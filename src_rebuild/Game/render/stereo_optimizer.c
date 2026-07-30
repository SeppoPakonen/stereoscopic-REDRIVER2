#include "stereo_optimizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global optimizer state
STEREO_OPTIMIZER g_stereo_optimizer;

// Cache storage for matrix optimization
static STEREO_MATRIX_CACHE_ENTRY g_matrix_cache;
static int g_optimizer_initialized = 0;

// Statistics tracking
static STEREO_OPT_STATS g_opt_stats;

// Viewport optimization buffer (remember last viewport settings)
typedef struct {
    int x, y, w, h;
    int valid;
} VIEWPORT_CACHE;

static VIEWPORT_CACHE g_viewport_cache;

// Initialize optimizer
void StereoOptimizer_Init(int optimization_flags)
{
    if (g_optimizer_initialized) {
        return;
    }

    memset(&g_stereo_optimizer, 0, sizeof(STEREO_OPTIMIZER));
    memset(&g_opt_stats, 0, sizeof(STEREO_OPT_STATS));
    memset(&g_matrix_cache, 0, sizeof(STEREO_MATRIX_CACHE_ENTRY));
    memset(&g_viewport_cache, 0, sizeof(VIEWPORT_CACHE));

    g_stereo_optimizer.enabled_optimizations = optimization_flags;
    g_stereo_optimizer.viewport_cache_valid = 0;
    g_stereo_optimizer.matrix_cache_hits = 0;
    g_stereo_optimizer.matrix_cache_misses = 0;

    g_optimizer_initialized = 1;

    printf("STEREO_OPTIMIZER: Initialized with flags=0x%x\n", optimization_flags);

    // Log enabled optimizations
    if (optimization_flags & STEREO_OPT_MATRIX_CACHING)
        printf("  - Matrix Caching enabled\n");
    if (optimization_flags & STEREO_OPT_SCISSOR_BATCHING)
        printf("  - Scissor Batching enabled\n");
    if (optimization_flags & STEREO_OPT_CLEAR_REDUCTION)
        printf("  - Clear Reduction enabled\n");
    if (optimization_flags & STEREO_OPT_VIEWPORT_CACHING)
        printf("  - Viewport Caching enabled\n");
    if (optimization_flags & STEREO_OPT_SHADER_PRECOMP)
        printf("  - Shader Precompilation enabled\n");
}

// Shutdown optimizer
void StereoOptimizer_Shutdown(void)
{
    if (!g_optimizer_initialized) {
        return;
    }

    printf("STEREO_OPTIMIZER: Shutting down\n");
    StereoOptimizer_PrintStats();

    g_optimizer_initialized = 0;
    memset(&g_stereo_optimizer, 0, sizeof(STEREO_OPTIMIZER));
}

// Enable specific optimization
void StereoOptimizer_EnableOptimization(STEREO_OPT_FLAG flag)
{
    g_stereo_optimizer.enabled_optimizations |= flag;
    printf("STEREO_OPTIMIZER: Enabled optimization flag 0x%x\n", flag);
}

// Disable specific optimization
void StereoOptimizer_DisableOptimization(STEREO_OPT_FLAG flag)
{
    g_stereo_optimizer.enabled_optimizations &= ~flag;
    printf("STEREO_OPTIMIZER: Disabled optimization flag 0x%x\n", flag);
}

// Check if optimization is enabled
int StereoOptimizer_IsOptimizationEnabled(STEREO_OPT_FLAG flag)
{
    return (g_stereo_optimizer.enabled_optimizations & flag) != 0;
}

// Viewport optimization: cache viewport calls to avoid redundant setup
int StereoOptimizer_SetViewportCached(int x, int y, int width, int height)
{
    g_opt_stats.total_viewport_calls++;

    // Check if optimization is enabled
    if (!StereoOptimizer_IsOptimizationEnabled(STEREO_OPT_VIEWPORT_CACHING)) {
        return 0; // Always set if optimization disabled
    }

    // Check if same as last viewport
    if (g_viewport_cache.valid &&
        g_viewport_cache.x == x &&
        g_viewport_cache.y == y &&
        g_viewport_cache.w == width &&
        g_viewport_cache.h == height) {
        // Viewport unchanged - skip setting
        g_opt_stats.viewport_calls_skipped++;
        return 1; // Return 1 = skip, caller should not call GR_SetViewPort
    }

    // Update cache
    g_viewport_cache.x = x;
    g_viewport_cache.y = y;
    g_viewport_cache.w = width;
    g_viewport_cache.h = height;
    g_viewport_cache.valid = 1;

    return 0; // Do not skip, caller should call GR_SetViewPort
}

// Clear optimization: determine if clear operation can be skipped
int StereoOptimizer_ShouldSkipClear(int mode)
{
    if (!StereoOptimizer_IsOptimizationEnabled(STEREO_OPT_CLEAR_REDUCTION)) {
        return 0; // Always do clear if optimization disabled
    }

    // For viewport-based stereo modes, we can potentially skip the full clear
    // since we're only rendering to specific regions
    // This is a conservative optimization - only skip if we're confident
    // about viewport coverage

    if (mode == 3 || mode == 4) { // SIDEBYSIDE or TOPBOTTOM
        // These modes render to non-overlapping viewport regions
        // A single clear at the start is sufficient
        g_opt_stats.clears_skipped++;
        return 1; // Skip additional clears
    }

    return 0; // Don't skip clear
}

// Scissor optimization: batch scissor setup
void StereoOptimizer_SetScissorOptimized(int enable, int x, int y, int w, int h)
{
    if (!StereoOptimizer_IsOptimizationEnabled(STEREO_OPT_SCISSOR_BATCHING)) {
        return; // No optimization
    }

    // Future: implement scissor batching (group multiple scissor calls)
    // For now, this is a placeholder for the optimization infrastructure
}

// Matrix caching: retrieve cached matrix if available
int StereoOptimizer_GetCachedMatrix(float separation, float convergence,
                                   STEREO_MATRIX_CACHE_ENTRY *out_cache)
{
    if (!StereoOptimizer_IsOptimizationEnabled(STEREO_OPT_MATRIX_CACHING)) {
        g_stereo_optimizer.matrix_cache_misses++;
        return 0; // Caching disabled
    }

    if (!g_matrix_cache.is_cached) {
        g_stereo_optimizer.matrix_cache_misses++;
        return 0; // No cached value
    }

    // Check if cache matches current parameters
    if (g_matrix_cache.separation == separation &&
        g_matrix_cache.convergence == convergence) {
        // Cache hit
        if (out_cache) {
            *out_cache = g_matrix_cache;
        }
        g_stereo_optimizer.matrix_cache_hits++;
        return 1; // Cache hit
    }

    g_stereo_optimizer.matrix_cache_misses++;
    return 0; // Cache miss (parameters changed)
}

// Cache transformation matrix
void StereoOptimizer_CacheMatrix(float separation, float convergence,
                                STEREO_MATRIX_CACHE_ENTRY *cache)
{
    if (!StereoOptimizer_IsOptimizationEnabled(STEREO_OPT_MATRIX_CACHING)) {
        return;
    }

    if (cache) {
        g_matrix_cache = *cache;
        g_matrix_cache.separation = separation;
        g_matrix_cache.convergence = convergence;
        g_matrix_cache.is_cached = 1;
    }
}

// Get statistics
void StereoOptimizer_GetStats(STEREO_OPT_STATS *out_stats)
{
    if (out_stats) {
        *out_stats = g_opt_stats;

        // Calculate cache hit rate
        int total_cache_accesses = g_stereo_optimizer.matrix_cache_hits +
                                  g_stereo_optimizer.matrix_cache_misses;
        if (total_cache_accesses > 0) {
            out_stats->cache_hit_rate =
                (double)g_stereo_optimizer.matrix_cache_hits / total_cache_accesses;
        }
    }
}

// Reset statistics
void StereoOptimizer_ResetStats(void)
{
    memset(&g_opt_stats, 0, sizeof(STEREO_OPT_STATS));
    g_stereo_optimizer.matrix_cache_hits = 0;
    g_stereo_optimizer.matrix_cache_misses = 0;
    printf("STEREO_OPTIMIZER: Statistics reset\n");
}

// Print statistics
void StereoOptimizer_PrintStats(void)
{
    STEREO_OPT_STATS stats;
    StereoOptimizer_GetStats(&stats);

    printf("\n=== STEREO_OPTIMIZER Statistics ===\n");
    printf("Viewport Calls: %d (Skipped: %d, %.1f%%)\n",
           stats.total_viewport_calls,
           stats.viewport_calls_skipped,
           stats.total_viewport_calls > 0 ?
               (double)stats.viewport_calls_skipped / stats.total_viewport_calls * 100 : 0);
    printf("Clear Operations: %d (Skipped: %d, %.1f%%)\n",
           stats.total_clears,
           stats.clears_skipped,
           stats.total_clears > 0 ?
               (double)stats.clears_skipped / stats.total_clears * 100 : 0);
    printf("Matrix Cache: Hits=%d, Misses=%d, Hit Rate=%.1f%%\n",
           stats.matrix_cache_hits,
           stats.matrix_cache_misses,
           stats.cache_hit_rate * 100);
    printf("====================================\n\n");
}

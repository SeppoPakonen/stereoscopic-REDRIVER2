#ifndef STEREO_OPTIMIZER_H
#define STEREO_OPTIMIZER_H

// Stereo rendering optimization system

// Optimization techniques
typedef enum {
    STEREO_OPT_MATRIX_CACHING = 1,
    STEREO_OPT_SCISSOR_BATCHING = 2,
    STEREO_OPT_CLEAR_REDUCTION = 4,
    STEREO_OPT_VIEWPORT_CACHING = 8,
    STEREO_OPT_SHADER_PRECOMP = 16
} STEREO_OPT_FLAG;

// Optimization state
typedef struct {
    int enabled_optimizations;
    int viewport_cache_valid;
    int last_viewport_x;
    int last_viewport_y;
    int last_viewport_w;
    int last_viewport_h;
    int viewport_clear_skipped;
    int matrix_cache_hits;
    int matrix_cache_misses;
} STEREO_OPTIMIZER;

extern STEREO_OPTIMIZER g_stereo_optimizer;

// Initialization and control
void StereoOptimizer_Init(int optimization_flags);
void StereoOptimizer_Shutdown(void);
void StereoOptimizer_EnableOptimization(STEREO_OPT_FLAG flag);
void StereoOptimizer_DisableOptimization(STEREO_OPT_FLAG flag);
int StereoOptimizer_IsOptimizationEnabled(STEREO_OPT_FLAG flag);

// Optimization routines

// Viewport optimization: skip redundant viewport setting
int StereoOptimizer_SetViewportCached(int x, int y, int width, int height);

// Clear optimization: avoid redundant clears for viewport-rendered modes
int StereoOptimizer_ShouldSkipClear(int mode);

// Scissor optimization: batch scissor test setup
void StereoOptimizer_SetScissorOptimized(int enable, int x, int y, int w, int h);

// Matrix caching: store and reuse transformation matrices
typedef struct {
    int is_cached;
    float separation;
    float convergence;
    // Cache actual matrices here when implemented
} STEREO_MATRIX_CACHE_ENTRY;

int StereoOptimizer_GetCachedMatrix(float separation, float convergence,
                                   STEREO_MATRIX_CACHE_ENTRY *out_cache);
void StereoOptimizer_CacheMatrix(float separation, float convergence,
                                STEREO_MATRIX_CACHE_ENTRY *cache);

// Statistics and reporting
typedef struct {
    int total_viewport_calls;
    int viewport_calls_skipped;
    int total_clears;
    int clears_skipped;
    int matrix_cache_hits;
    int matrix_cache_misses;
    double cache_hit_rate;
} STEREO_OPT_STATS;

void StereoOptimizer_GetStats(STEREO_OPT_STATS *out_stats);
void StereoOptimizer_ResetStats(void);
void StereoOptimizer_PrintStats(void);

#endif // STEREO_OPTIMIZER_H

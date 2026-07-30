#include "stereo_profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>

// Global profiler state
STEREO_PROFILER g_stereo_profiler;
int g_stereo_profiling_enabled = 0;
STEREO_MATRIX_CACHE g_camera_matrix_cache;

// Get current time in milliseconds with high precision
double StereoProfiler_GetTimeMs(void)
{
#ifdef _WIN32
    struct timeb time_buffer;
    ftime(&time_buffer);
    return (double)time_buffer.time * 1000.0 + time_buffer.millitm;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
#endif
}

// Initialize profiler
void StereoProfiler_Init(STEREO_PROF_MODE mode, int capacity)
{
    if (g_stereo_profiler.metrics != NULL) {
        return; // Already initialized
    }

    memset(&g_stereo_profiler, 0, sizeof(STEREO_PROFILER));
    g_stereo_profiler.mode = mode;
    g_stereo_profiler.enabled = (mode != STEREO_PROF_DISABLED);
    g_stereo_profiler.metrics_capacity = capacity;
    g_stereo_profiler.stats_capacity = capacity / 20; // Less stats than raw metrics
    g_stereo_profiler.frame_count = 0;
    g_stereo_profiler.baseline_frame_time_ms = 0.0;
    g_stereo_profiler.baseline_samples = 0;
    g_stereo_profiler.matrix_cache_valid = 0;

    // Allocate metric buffer
    g_stereo_profiler.metrics = (STEREO_PERF_METRIC *)malloc(
        capacity * sizeof(STEREO_PERF_METRIC));

    // Allocate stats buffer
    g_stereo_profiler.frame_stats = (STEREO_FRAME_STATS *)malloc(
        g_stereo_profiler.stats_capacity * sizeof(STEREO_FRAME_STATS));

    if (!g_stereo_profiler.metrics || !g_stereo_profiler.frame_stats) {
        printf("STEREO_PROFILER: Failed to allocate memory\n");
        if (g_stereo_profiler.metrics)
            free(g_stereo_profiler.metrics);
        if (g_stereo_profiler.frame_stats)
            free(g_stereo_profiler.frame_stats);
        memset(&g_stereo_profiler, 0, sizeof(STEREO_PROFILER));
        return;
    }

    // Initialize matrix cache
    StereoProfiler_InitMatrixCache();

    g_stereo_profiling_enabled = 1;
    printf("STEREO_PROFILER: Initialized with mode=%d, capacity=%d\n", mode, capacity);
}

// Record a profiling event
void StereoProfiler_RecordEvent(STEREO_PROF_EVENT event)
{
    if (!g_stereo_profiling_enabled || !g_stereo_profiler.metrics) {
        return;
    }

    if (g_stereo_profiler.metrics_count >= g_stereo_profiler.metrics_capacity) {
        // Circular buffer - wrap around and compute frame stats if at event boundary
        if (event == PROF_EVENT_FRAME_START) {
            StereoProfiler_EndFrame();
            g_stereo_profiler.metrics_count = 0;
        } else {
            return; // Buffer full, skip
        }
    }

    STEREO_PERF_METRIC *metric = &g_stereo_profiler.metrics[g_stereo_profiler.metrics_count];
    metric->start_time = StereoProfiler_GetTimeMs();
    metric->event_type = event;
    metric->end_time = 0.0;
    metric->duration_ms = 0.0;

    g_stereo_profiler.metrics_count++;
}

// Process profiling events and compute frame statistics
static void StereoProfiler_ComputeFrameStats(STEREO_FRAME_STATS *out_stats)
{
    if (!out_stats) return;

    memset(out_stats, 0, sizeof(STEREO_FRAME_STATS));

    // Scan through metrics and accumulate timing information
    double frame_start = 0.0;
    double frame_end = 0.0;
    int frame_started = 0;

    for (int i = 0; i < g_stereo_profiler.metrics_count; i++) {
        STEREO_PERF_METRIC *m = &g_stereo_profiler.metrics[i];

        if (m->event_type == PROF_EVENT_FRAME_START && !frame_started) {
            frame_start = m->start_time;
            frame_started = 1;
        } else if (m->event_type == PROF_EVENT_FRAME_END) {
            frame_end = m->start_time;
        }

        // Calculate durations for paired events
        if (i + 1 < g_stereo_profiler.metrics_count) {
            STEREO_PERF_METRIC *next = &g_stereo_profiler.metrics[i + 1];

            // Check if this is a start event and next is corresponding end
            if (m->event_type == PROF_EVENT_CAMERA_CALC_START &&
                next->event_type == PROF_EVENT_CAMERA_CALC_END) {
                out_stats->camera_calc_time_ms += (next->start_time - m->start_time);
                i++; // Skip the end event
            } else if (m->event_type == PROF_EVENT_VIEWPORT_SET_START &&
                       next->event_type == PROF_EVENT_VIEWPORT_SET_END) {
                out_stats->viewport_setup_time_ms += (next->start_time - m->start_time);
                i++;
            } else if (m->event_type == PROF_EVENT_RENDER_LEFT_START &&
                       next->event_type == PROF_EVENT_RENDER_LEFT_END) {
                out_stats->render_left_time_ms += (next->start_time - m->start_time);
                i++;
            } else if (m->event_type == PROF_EVENT_RENDER_RIGHT_START &&
                       next->event_type == PROF_EVENT_RENDER_RIGHT_END) {
                out_stats->render_right_time_ms += (next->start_time - m->start_time);
                i++;
            } else if (m->event_type == PROF_EVENT_SHADER_SETUP_START &&
                       next->event_type == PROF_EVENT_SHADER_SETUP_END) {
                out_stats->shader_setup_time_ms += (next->start_time - m->start_time);
                i++;
            } else if (m->event_type == PROF_EVENT_SCISSOR_SET_START &&
                       next->event_type == PROF_EVENT_SCISSOR_SET_END) {
                out_stats->scissor_setup_time_ms += (next->start_time - m->start_time);
                i++;
            } else if (m->event_type == PROF_EVENT_CLEAR_START &&
                       next->event_type == PROF_EVENT_CLEAR_END) {
                out_stats->clear_time_ms += (next->start_time - m->start_time);
                i++;
            } else if (m->event_type == PROF_EVENT_VIEWPORT_RESET_START &&
                       next->event_type == PROF_EVENT_VIEWPORT_RESET_END) {
                out_stats->viewport_reset_time_ms += (next->start_time - m->start_time);
                i++;
            }
        }
    }

    // Calculate total frame time
    if (frame_started && frame_end > 0.0) {
        out_stats->total_frame_time_ms = frame_end - frame_start;
    }

    // Calculate percentage breakdowns
    if (out_stats->total_frame_time_ms > 0.0) {
        out_stats->left_render_percent =
            (out_stats->render_left_time_ms / out_stats->total_frame_time_ms) * 100.0;
        out_stats->right_render_percent =
            (out_stats->render_right_time_ms / out_stats->total_frame_time_ms) * 100.0;
    }

    // Calculate stereo overhead based on baseline
    if (g_stereo_profiler.baseline_frame_time_ms > 0.0) {
        double overhead = out_stats->total_frame_time_ms - g_stereo_profiler.baseline_frame_time_ms;
        out_stats->stereo_overhead_percent =
            (overhead / g_stereo_profiler.baseline_frame_time_ms) * 100.0;
    }
}

// Get current frame statistics
const STEREO_FRAME_STATS* StereoProfiler_GetFrameStats(int frame_index)
{
    if (frame_index < 0 || frame_index >= g_stereo_profiler.stats_count) {
        return NULL;
    }
    return &g_stereo_profiler.frame_stats[frame_index];
}

// Get average statistics across all frames
void StereoProfiler_GetAverageStats(STEREO_FRAME_STATS *out_stats)
{
    if (!out_stats || g_stereo_profiler.stats_count == 0) {
        return;
    }

    memset(out_stats, 0, sizeof(STEREO_FRAME_STATS));

    // Sum all statistics
    for (int i = 0; i < g_stereo_profiler.stats_count; i++) {
        STEREO_FRAME_STATS *stats = &g_stereo_profiler.frame_stats[i];
        out_stats->total_frame_time_ms += stats->total_frame_time_ms;
        out_stats->camera_calc_time_ms += stats->camera_calc_time_ms;
        out_stats->viewport_setup_time_ms += stats->viewport_setup_time_ms;
        out_stats->render_left_time_ms += stats->render_left_time_ms;
        out_stats->render_right_time_ms += stats->render_right_time_ms;
        out_stats->shader_setup_time_ms += stats->shader_setup_time_ms;
        out_stats->scissor_setup_time_ms += stats->scissor_setup_time_ms;
        out_stats->clear_time_ms += stats->clear_time_ms;
        out_stats->viewport_reset_time_ms += stats->viewport_reset_time_ms;
        out_stats->stereo_overhead_percent += stats->stereo_overhead_percent;
    }

    // Compute averages
    if (g_stereo_profiler.stats_count > 0) {
        out_stats->total_frame_time_ms /= g_stereo_profiler.stats_count;
        out_stats->camera_calc_time_ms /= g_stereo_profiler.stats_count;
        out_stats->viewport_setup_time_ms /= g_stereo_profiler.stats_count;
        out_stats->render_left_time_ms /= g_stereo_profiler.stats_count;
        out_stats->render_right_time_ms /= g_stereo_profiler.stats_count;
        out_stats->shader_setup_time_ms /= g_stereo_profiler.stats_count;
        out_stats->scissor_setup_time_ms /= g_stereo_profiler.stats_count;
        out_stats->clear_time_ms /= g_stereo_profiler.stats_count;
        out_stats->viewport_reset_time_ms /= g_stereo_profiler.stats_count;
        out_stats->stereo_overhead_percent /= g_stereo_profiler.stats_count;

        // Recalculate render percentages
        if (out_stats->total_frame_time_ms > 0.0) {
            out_stats->left_render_percent =
                (out_stats->render_left_time_ms / out_stats->total_frame_time_ms) * 100.0;
            out_stats->right_render_percent =
                (out_stats->render_right_time_ms / out_stats->total_frame_time_ms) * 100.0;
        }
    }
}

// Performance report generation
void StereoProfiler_GenerateReport(const char *output_filename)
{
    FILE *report = fopen(output_filename, "w");
    if (!report) {
        printf("STEREO_PROFILER: Failed to open report file: %s\n", output_filename);
        return;
    }

    fprintf(report, "=== REDRIVER2 Stereo Rendering Performance Report ===\n\n");
    fprintf(report, "Profiler Mode: %d\n", g_stereo_profiler.mode);
    fprintf(report, "Total Frames Profiled: %d\n", g_stereo_profiler.frame_count);
    fprintf(report, "Total Metrics Collected: %d\n\n", g_stereo_profiler.metrics_count);

    // Calculate average statistics
    STEREO_FRAME_STATS avg_stats;
    StereoProfiler_GetAverageStats(&avg_stats);

    fprintf(report, "--- Average Frame Performance ---\n");
    fprintf(report, "Total Frame Time: %.3f ms (Target: ~16.67 ms for 60 FPS)\n",
            avg_stats.total_frame_time_ms);
    fprintf(report, "Camera Calculation: %.3f ms (%.1f%%)\n",
            avg_stats.camera_calc_time_ms,
            avg_stats.total_frame_time_ms > 0 ? (avg_stats.camera_calc_time_ms / avg_stats.total_frame_time_ms) * 100 : 0);
    fprintf(report, "Viewport Setup: %.3f ms (%.1f%%)\n",
            avg_stats.viewport_setup_time_ms,
            avg_stats.total_frame_time_ms > 0 ? (avg_stats.viewport_setup_time_ms / avg_stats.total_frame_time_ms) * 100 : 0);
    fprintf(report, "Left Eye Render: %.3f ms (%.1f%%)\n",
            avg_stats.render_left_time_ms, avg_stats.left_render_percent);
    fprintf(report, "Right Eye Render: %.3f ms (%.1f%%)\n",
            avg_stats.render_right_time_ms, avg_stats.right_render_percent);
    fprintf(report, "Shader Setup: %.3f ms\n", avg_stats.shader_setup_time_ms);
    fprintf(report, "Scissor Setup: %.3f ms\n", avg_stats.scissor_setup_time_ms);
    fprintf(report, "Clear Operations: %.3f ms\n", avg_stats.clear_time_ms);
    fprintf(report, "Viewport Reset: %.3f ms\n\n", avg_stats.viewport_reset_time_ms);

    fprintf(report, "--- Optimization Analysis ---\n");
    fprintf(report, "Baseline Frame Time (non-stereo): %.3f ms\n",
            g_stereo_profiler.baseline_frame_time_ms);
    fprintf(report, "Baseline Samples: %d\n", g_stereo_profiler.baseline_samples);
    fprintf(report, "Stereo Overhead: %.1f%%\n\n", avg_stats.stereo_overhead_percent);

    // Performance assessment
    fprintf(report, "--- Performance Assessment ---\n");
    if (avg_stats.stereo_overhead_percent < 20.0) {
        fprintf(report, "EXCELLENT: Stereo overhead is within target (< 20%%)\n");
    } else if (avg_stats.stereo_overhead_percent < 30.0) {
        fprintf(report, "GOOD: Stereo overhead is acceptable (20-30%%)\n");
    } else {
        fprintf(report, "WARNING: Stereo overhead exceeds target (> 30%%)\n");
        fprintf(report, "Recommended Optimizations:\n");
        fprintf(report, "  - Consider matrix caching implementation\n");
        fprintf(report, "  - Review viewport setup frequency\n");
        fprintf(report, "  - Evaluate scissor test optimization\n");
        fprintf(report, "  - Check shader compilation caching\n");
    }

    fprintf(report, "\n--- Per-Frame Performance (sample of first 20) ---\n");
    int sample_count = (g_stereo_profiler.stats_count < 20) ? g_stereo_profiler.stats_count : 20;
    for (int i = 0; i < sample_count; i++) {
        STEREO_FRAME_STATS *stats = &g_stereo_profiler.frame_stats[i];
        fprintf(report, "Frame %d: %.3f ms (Overhead: %.1f%%)\n",
                i, stats->total_frame_time_ms, stats->stereo_overhead_percent);
    }

    fprintf(report, "\n=== End of Report ===\n");
    fclose(report);

    printf("STEREO_PROFILER: Report generated: %s\n", output_filename);
}

// Start frame profiling
void StereoProfiler_StartFrame(void)
{
    StereoProfiler_RecordEvent(PROF_EVENT_FRAME_START);
}

// End frame profiling and store statistics
void StereoProfiler_EndFrame(void)
{
    if (!g_stereo_profiling_enabled) return;

    StereoProfiler_RecordEvent(PROF_EVENT_FRAME_END);

    // Compute and store frame statistics
    if (g_stereo_profiler.stats_count < g_stereo_profiler.stats_capacity) {
        STEREO_FRAME_STATS *stats = &g_stereo_profiler.frame_stats[g_stereo_profiler.stats_count];
        StereoProfiler_ComputeFrameStats(stats);
        g_stereo_profiler.stats_count++;
        g_stereo_profiler.frame_count++;
    }
}

// Reset profiler state
void StereoProfiler_Reset(void)
{
    if (!g_stereo_profiler.metrics) {
        return;
    }

    g_stereo_profiler.metrics_count = 0;
    g_stereo_profiler.stats_count = 0;
    g_stereo_profiler.frame_count = 0;
    printf("STEREO_PROFILER: Reset profiler state\n");
}

// Shutdown profiler
void StereoProfiler_Shutdown(void)
{
    if (g_stereo_profiler.metrics) {
        free(g_stereo_profiler.metrics);
        g_stereo_profiler.metrics = NULL;
    }

    if (g_stereo_profiler.frame_stats) {
        free(g_stereo_profiler.frame_stats);
        g_stereo_profiler.frame_stats = NULL;
    }

    g_stereo_profiling_enabled = 0;
    printf("STEREO_PROFILER: Shutdown complete\n");
}

// Matrix cache functions for optimization
void StereoProfiler_InitMatrixCache(void)
{
    memset(&g_camera_matrix_cache, 0, sizeof(STEREO_MATRIX_CACHE));
    g_camera_matrix_cache.is_valid = 0;
    g_camera_matrix_cache.frame_number = -1;
}

int StereoProfiler_IsMatrixCacheValid(float sep, float conv, int frame)
{
    return (g_camera_matrix_cache.is_valid &&
            g_camera_matrix_cache.separation == sep &&
            g_camera_matrix_cache.convergence == conv &&
            g_camera_matrix_cache.frame_number == frame);
}

void StereoProfiler_InvalidateMatrixCache(void)
{
    g_camera_matrix_cache.is_valid = 0;
}

int StereoProfiler_ShouldCacheMatrix(int current_frame)
{
    // Return 1 if matrix should be cached (cached value matches current conditions)
    return g_camera_matrix_cache.is_valid && g_camera_matrix_cache.frame_number == current_frame;
}

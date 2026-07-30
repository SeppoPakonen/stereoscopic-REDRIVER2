#ifndef STEREO_PROFILER_H
#define STEREO_PROFILER_H

#include <time.h>

// Performance profiling infrastructure for stereo rendering

// Profiling event types
typedef enum {
    PROF_EVENT_FRAME_START = 0,
    PROF_EVENT_FRAME_END,
    PROF_EVENT_CAMERA_CALC_START,
    PROF_EVENT_CAMERA_CALC_END,
    PROF_EVENT_VIEWPORT_SET_START,
    PROF_EVENT_VIEWPORT_SET_END,
    PROF_EVENT_RENDER_LEFT_START,
    PROF_EVENT_RENDER_LEFT_END,
    PROF_EVENT_RENDER_RIGHT_START,
    PROF_EVENT_RENDER_RIGHT_END,
    PROF_EVENT_SHADER_SETUP_START,
    PROF_EVENT_SHADER_SETUP_END,
    PROF_EVENT_SCISSOR_SET_START,
    PROF_EVENT_SCISSOR_SET_END,
    PROF_EVENT_CLEAR_START,
    PROF_EVENT_CLEAR_END,
    PROF_EVENT_VIEWPORT_RESET_START,
    PROF_EVENT_VIEWPORT_RESET_END,
    PROF_EVENT_COUNT
} STEREO_PROF_EVENT;

// Performance metric structure
typedef struct {
    double start_time;
    double end_time;
    double duration_ms;
    int event_type;
} STEREO_PERF_METRIC;

// Performance statistics
typedef struct {
    double total_frame_time_ms;
    double camera_calc_time_ms;
    double viewport_setup_time_ms;
    double render_left_time_ms;
    double render_right_time_ms;
    double shader_setup_time_ms;
    double scissor_setup_time_ms;
    double clear_time_ms;
    double viewport_reset_time_ms;

    // Derived metrics
    double stereo_overhead_percent;
    double left_render_percent;
    double right_render_percent;
} STEREO_FRAME_STATS;

// Profiling mode
typedef enum {
    STEREO_PROF_DISABLED = 0,
    STEREO_PROF_BASIC = 1,
    STEREO_PROF_DETAILED = 2
} STEREO_PROF_MODE;

// Profiler state
typedef struct {
    STEREO_PROF_MODE mode;
    int enabled;
    int frame_count;

    // Buffers for metrics
    STEREO_PERF_METRIC *metrics;
    int metrics_count;
    int metrics_capacity;

    // Accumulated statistics
    STEREO_FRAME_STATS *frame_stats;
    int stats_count;
    int stats_capacity;

    // Baseline (non-stereo) frame time
    double baseline_frame_time_ms;
    int baseline_samples;

    // Cached matrix state for optimization
    int matrix_cache_valid;
    int last_camera_frame;
} STEREO_PROFILER;

// Global profiler state
extern STEREO_PROFILER g_stereo_profiler;
extern int g_stereo_profiling_enabled;

// API Functions

// Initialize profiler with given capacity
void StereoProfiler_Init(STEREO_PROF_MODE mode, int capacity);

// Record a profiling event
void StereoProfiler_RecordEvent(STEREO_PROF_EVENT event);

// Get current frame statistics
const STEREO_FRAME_STATS* StereoProfiler_GetFrameStats(int frame_index);

// Get average statistics across all frames
void StereoProfiler_GetAverageStats(STEREO_FRAME_STATS *out_stats);

// Generate performance report
void StereoProfiler_GenerateReport(const char *output_filename);

// Reset profiler state
void StereoProfiler_Reset(void);

// Shutdown profiler and free resources
void StereoProfiler_Shutdown(void);

// Get current time in milliseconds (high precision)
double StereoProfiler_GetTimeMs(void);

// Optimization hint functions
int StereoProfiler_ShouldCacheMatrix(int current_frame);
void StereoProfiler_InvalidateMatrixCache(void);

// Performance monitoring
void StereoProfiler_StartFrame(void);
void StereoProfiler_EndFrame(void);

// Camera matrix caching for optimization
typedef struct {
    int is_valid;
    int frame_number;
    float separation;
    float convergence;
    // Matrices would be cached here
} STEREO_MATRIX_CACHE;

extern STEREO_MATRIX_CACHE g_camera_matrix_cache;

// Camera matrix cache functions
void StereoProfiler_InitMatrixCache(void);
int StereoProfiler_IsMatrixCacheValid(float sep, float conv, int frame);
void StereoProfiler_InvalidateMatrixCache(void);

#endif // STEREO_PROFILER_H

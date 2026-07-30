#include "stereo_performance_monitor.h"
#include "stereo.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// High-resolution timer implementation
#ifdef _WIN32
#include <windows.h>
typedef struct {
    LARGE_INTEGER start;
    LARGE_INTEGER frequency;
} HighResTimer;

static double GetElapsedMS(HighResTimer *timer) {
    LARGE_INTEGER end;
    QueryPerformanceCounter(&end);
    return (double)(end.QuadPart - timer->start.QuadPart) / timer->frequency.QuadPart * 1000.0;
}

static HighResTimer StartTimer(void) {
    HighResTimer timer;
    QueryPerformanceFrequency(&timer.frequency);
    QueryPerformanceCounter(&timer.start);
    return timer;
}

#else
#include <sys/time.h>
typedef struct {
    struct timeval start;
} HighResTimer;

static double GetElapsedMS(HighResTimer *timer) {
    struct timeval end;
    gettimeofday(&end, NULL);
    return (double)(end.tv_sec - timer->start.tv_sec) * 1000.0 +
           (double)(end.tv_usec - timer->start.tv_usec) / 1000.0;
}

static HighResTimer StartTimer(void) {
    HighResTimer timer;
    gettimeofday(&timer.start, NULL);
    return timer;
}
#endif

// Performance monitor state
static struct {
    STEREO_PERF_METRICS metrics;
    HighResTimer frame_timer;
    HighResTimer left_eye_timer;
    HighResTimer right_eye_timer;
    HighResTimer composite_timer;
    int initialized;

    // Running averages
    float avg_frame_time;
    float avg_left_eye_time;
    float avg_right_eye_time;
    float avg_composite_time;
    int sample_count;

    // Baseline (for comparison)
    float baseline_frame_time;
} g_perf_monitor;

void StereoPerformance_Init(void)
{
    if (g_perf_monitor.initialized)
        return;

    memset(&g_perf_monitor, 0, sizeof(g_perf_monitor));
    memset(&g_perf_monitor.metrics, 0, sizeof(STEREO_PERF_METRICS));

    g_perf_monitor.initialized = 1;
    g_perf_monitor.sample_count = 0;
    g_perf_monitor.baseline_frame_time = 0.0f;

    printf("StereoPerformance: Monitor initialized\n");
}

void StereoPerformance_Shutdown(void)
{
    g_perf_monitor.initialized = 0;
}

void StereoPerformance_BeginFrame(void)
{
    if (!g_perf_monitor.initialized)
        return;

    g_perf_monitor.frame_timer = StartTimer();
}

void StereoPerformance_EndFrame(void)
{
    if (!g_perf_monitor.initialized)
        return;

    float elapsed = (float)GetElapsedMS(&g_perf_monitor.frame_timer);
    g_perf_monitor.metrics.frame_time_ms = elapsed;

    // Update running average
    if (g_perf_monitor.sample_count < 60) {
        g_perf_monitor.avg_frame_time = (g_perf_monitor.avg_frame_time * g_perf_monitor.sample_count + elapsed) /
                                        (g_perf_monitor.sample_count + 1);
        g_perf_monitor.sample_count++;
    } else {
        // Exponential moving average after 60 samples
        g_perf_monitor.avg_frame_time = g_perf_monitor.avg_frame_time * 0.95f + elapsed * 0.05f;
    }

    g_perf_monitor.metrics.frames_rendered++;

    // Calculate improvement (assuming RTT reduces frame time by ~50%)
    if (g_perf_monitor.baseline_frame_time > 0.0f) {
        g_perf_monitor.metrics.frame_time_improvement =
            (1.0f - (elapsed / g_perf_monitor.baseline_frame_time)) * 100.0f;
    }
}

void StereoPerformance_BeginLeftEyeRender(void)
{
    if (!g_perf_monitor.initialized)
        return;

    g_perf_monitor.left_eye_timer = StartTimer();
}

void StereoPerformance_EndLeftEyeRender(void)
{
    if (!g_perf_monitor.initialized)
        return;

    float elapsed = (float)GetElapsedMS(&g_perf_monitor.left_eye_timer);
    g_perf_monitor.metrics.render_left_eye_time_ms = elapsed;

    if (g_perf_monitor.sample_count < 60) {
        g_perf_monitor.avg_left_eye_time = (g_perf_monitor.avg_left_eye_time * (g_perf_monitor.sample_count - 1) + elapsed) /
                                           g_perf_monitor.sample_count;
    }
}

void StereoPerformance_BeginRightEyeRender(void)
{
    if (!g_perf_monitor.initialized)
        return;

    g_perf_monitor.right_eye_timer = StartTimer();
}

void StereoPerformance_EndRightEyeRender(void)
{
    if (!g_perf_monitor.initialized)
        return;

    float elapsed = (float)GetElapsedMS(&g_perf_monitor.right_eye_timer);
    g_perf_monitor.metrics.render_right_eye_time_ms = elapsed;

    if (g_perf_monitor.sample_count < 60) {
        g_perf_monitor.avg_right_eye_time = (g_perf_monitor.avg_right_eye_time * (g_perf_monitor.sample_count - 1) + elapsed) /
                                            g_perf_monitor.sample_count;
    }
}

void StereoPerformance_BeginComposite(void)
{
    if (!g_perf_monitor.initialized)
        return;

    g_perf_monitor.composite_timer = StartTimer();
}

void StereoPerformance_EndComposite(void)
{
    if (!g_perf_monitor.initialized)
        return;

    float elapsed = (float)GetElapsedMS(&g_perf_monitor.composite_timer);
    g_perf_monitor.metrics.composite_time_ms = elapsed;

    if (g_perf_monitor.sample_count < 60) {
        g_perf_monitor.avg_composite_time = (g_perf_monitor.avg_composite_time * (g_perf_monitor.sample_count - 1) + elapsed) /
                                            g_perf_monitor.sample_count;
    }
}

STEREO_PERF_METRICS* StereoPerformance_GetMetrics(void)
{
    if (!g_perf_monitor.initialized)
        return NULL;

    return &g_perf_monitor.metrics;
}

void StereoPerformance_PrintReport(void)
{
    if (!g_perf_monitor.initialized) {
        printf("StereoPerformance: Monitor not initialized\n");
        return;
    }

    printf("\n");
    printf("=== STEREO PERFORMANCE REPORT ===\n");
    printf("Mode: %d (RTT %s)\n", g_perf_monitor.metrics.mode,
           g_perf_monitor.metrics.use_render_to_texture ? "enabled" : "disabled");
    printf("Frames analyzed: %d\n", g_perf_monitor.metrics.frames_rendered);
    printf("\n");

    printf("TIMING ANALYSIS:\n");
    printf("  Frame Time:         %.3f ms (avg: %.3f ms)\n",
           g_perf_monitor.metrics.frame_time_ms,
           g_perf_monitor.avg_frame_time);
    printf("  Left Eye Render:    %.3f ms (avg: %.3f ms)\n",
           g_perf_monitor.metrics.render_left_eye_time_ms,
           g_perf_monitor.avg_left_eye_time);
    printf("  Right Eye Render:   %.3f ms (avg: %.3f ms)\n",
           g_perf_monitor.metrics.render_right_eye_time_ms,
           g_perf_monitor.avg_right_eye_time);
    printf("  Composition Pass:   %.3f ms (avg: %.3f ms)\n",
           g_perf_monitor.metrics.composite_time_ms,
           g_perf_monitor.avg_composite_time);
    printf("\n");

    float total_render = g_perf_monitor.metrics.render_left_eye_time_ms +
                         g_perf_monitor.metrics.render_right_eye_time_ms;
    printf("BREAKDOWN:\n");
    printf("  Rendering:  %.1f%%\n", (total_render / g_perf_monitor.metrics.frame_time_ms) * 100.0f);
    printf("  Composite:  %.1f%%\n", (g_perf_monitor.metrics.composite_time_ms / g_perf_monitor.metrics.frame_time_ms) * 100.0f);
    printf("  Other:      %.1f%%\n",
           ((g_perf_monitor.metrics.frame_time_ms - total_render - g_perf_monitor.metrics.composite_time_ms) /
            g_perf_monitor.metrics.frame_time_ms) * 100.0f);
    printf("\n");

    if (g_perf_monitor.metrics.frame_time_improvement != 0.0f) {
        printf("IMPROVEMENT:\n");
        printf("  vs Baseline: %.1f%%\n", g_perf_monitor.metrics.frame_time_improvement);
    }

    // FPS calculation
    float fps = 1000.0f / g_perf_monitor.avg_frame_time;
    printf("\nFPS: %.1f (%.3f ms per frame)\n", fps, g_perf_monitor.avg_frame_time);
    printf("==============================\n\n");
}

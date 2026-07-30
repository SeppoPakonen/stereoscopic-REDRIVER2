#include "stereo_benchmark.h"
#include "stereo_profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Global benchmark session
STEREO_BENCHMARK_SESSION g_benchmark_session;

// Frame time buffer for statistics
typedef struct {
    double *frame_times;
    int count;
    int capacity;
} FRAME_TIME_BUFFER;

static FRAME_TIME_BUFFER g_frame_times;

// Initialize benchmark session
void StereoBenchmark_Init(STEREO_BENCHMARK_CONFIG *config)
{
    if (!config) {
        return;
    }

    memset(&g_benchmark_session, 0, sizeof(STEREO_BENCHMARK_SESSION));
    memcpy(&g_benchmark_session.config, config, sizeof(STEREO_BENCHMARK_CONFIG));

    // Allocate result buffer
    g_benchmark_session.result_capacity = 20; // Support up to 20 different test configurations
    g_benchmark_session.results = (STEREO_BENCHMARK_RESULT *)malloc(
        g_benchmark_session.result_capacity * sizeof(STEREO_BENCHMARK_RESULT));

    if (!g_benchmark_session.results) {
        printf("STEREO_BENCHMARK: Failed to allocate memory\n");
        return;
    }

    // Allocate frame time buffer
    g_frame_times.capacity = config->duration_frames + 10;
    g_frame_times.frame_times = (double *)malloc(g_frame_times.capacity * sizeof(double));

    if (!g_frame_times.frame_times) {
        printf("STEREO_BENCHMARK: Failed to allocate frame time buffer\n");
        free(g_benchmark_session.results);
        g_benchmark_session.results = NULL;
        return;
    }

    printf("STEREO_BENCHMARK: Initialized (scenario=%d, duration=%d frames)\n",
           config->scenario, config->duration_frames);
}

// Start benchmark
void StereoBenchmark_Start(void)
{
    if (!g_benchmark_session.results) {
        printf("STEREO_BENCHMARK: Not initialized\n");
        return;
    }

    g_benchmark_session.active = 1;
    g_benchmark_session.current_frame = 0;
    g_frame_times.count = 0;
    g_benchmark_session.test_start_time = StereoProfiler_GetTimeMs();

    printf("STEREO_BENCHMARK: Benchmark started\n");
}

// Record frame time
void StereoBenchmark_RecordFrame(double frame_time_ms)
{
    if (!g_benchmark_session.active) {
        return;
    }

    // Store frame time
    if (g_frame_times.count < g_frame_times.capacity) {
        g_frame_times.frame_times[g_frame_times.count] = frame_time_ms;
        g_frame_times.count++;
    }

    g_benchmark_session.current_frame++;

    // Check if we've reached the target number of frames
    if (g_benchmark_session.current_frame >= g_benchmark_session.config.duration_frames) {
        StereoBenchmark_Stop();
    }

    // Log per-frame if requested
    if (g_benchmark_session.config.log_per_frame && g_benchmark_session.current_frame % 30 == 0) {
        printf("BENCH: Frame %d, Time: %.2f ms\n",
               g_benchmark_session.current_frame, frame_time_ms);
    }
}

// Compute statistics for collected frame times
static void StereoBenchmark_ComputeStatistics(
    const double *frame_times, int count,
    double *out_avg, double *out_min, double *out_max, double *out_variance)
{
    if (!frame_times || count == 0) {
        return;
    }

    // Compute average
    double sum = 0.0;
    double min_time = frame_times[0];
    double max_time = frame_times[0];

    for (int i = 0; i < count; i++) {
        sum += frame_times[i];
        if (frame_times[i] < min_time)
            min_time = frame_times[i];
        if (frame_times[i] > max_time)
            max_time = frame_times[i];
    }

    double avg = sum / count;
    if (out_avg) *out_avg = avg;
    if (out_min) *out_min = min_time;
    if (out_max) *out_max = max_time;

    // Compute variance
    if (out_variance) {
        double variance_sum = 0.0;
        for (int i = 0; i < count; i++) {
            double diff = frame_times[i] - avg;
            variance_sum += diff * diff;
        }
        *out_variance = variance_sum / count;
    }
}

// Stop benchmark and compute results
void StereoBenchmark_Stop(void)
{
    if (!g_benchmark_session.active) {
        return;
    }

    g_benchmark_session.active = 0;

    // Compute statistics
    double avg_time = 0.0, min_time = 0.0, max_time = 0.0, variance = 0.0;
    StereoBenchmark_ComputeStatistics(
        g_frame_times.frame_times, g_frame_times.count,
        &avg_time, &min_time, &max_time, &variance);

    // Store result
    if (g_benchmark_session.result_count < g_benchmark_session.result_capacity) {
        STEREO_BENCHMARK_RESULT *result =
            &g_benchmark_session.results[g_benchmark_session.result_count];

        result->scenario = g_benchmark_session.config.scenario;
        result->avg_frame_time_ms = avg_time;
        result->min_frame_time_ms = min_time;
        result->max_frame_time_ms = max_time;
        result->frame_time_variance = variance;
        result->frame_count = g_frame_times.count;
        result->fps_average = (avg_time > 0.0) ? 1000.0 / avg_time : 0.0;

        g_benchmark_session.result_count++;

        printf("STEREO_BENCHMARK: Stopped - Results:\n");
        printf("  Frames: %d\n", result->frame_count);
        printf("  Avg Time: %.3f ms\n", result->avg_frame_time_ms);
        printf("  Min Time: %.3f ms\n", result->min_frame_time_ms);
        printf("  Max Time: %.3f ms\n", result->max_frame_time_ms);
        printf("  Variance: %.6f\n", result->frame_time_variance);
        printf("  FPS Average: %.1f\n", result->fps_average);
    }
}

// Check if benchmark is active
int StereoBenchmark_IsActive(void)
{
    return g_benchmark_session.active;
}

// Generate benchmark report
void StereoBenchmark_GenerateReport(const char *output_filename)
{
    if (!g_benchmark_session.results) {
        printf("STEREO_BENCHMARK: No results to report\n");
        return;
    }

    FILE *report = fopen(output_filename, "w");
    if (!report) {
        printf("STEREO_BENCHMARK: Failed to open report file: %s\n", output_filename);
        return;
    }

    fprintf(report, "=== REDRIVER2 Stereo Benchmark Report ===\n\n");
    fprintf(report, "Scenario: %d\n", g_benchmark_session.config.scenario);
    fprintf(report, "Total Results: %d\n\n", g_benchmark_session.result_count);

    // Write results
    for (int i = 0; i < g_benchmark_session.result_count; i++) {
        STEREO_BENCHMARK_RESULT *result = &g_benchmark_session.results[i];

        fprintf(report, "--- Result %d ---\n", i + 1);
        fprintf(report, "Scenario: %d\n", result->scenario);
        fprintf(report, "Stereo Mode: %d\n", result->stereo_mode);
        fprintf(report, "Stereo Enabled: %s\n", result->stereo_enabled ? "Yes" : "No");
        fprintf(report, "Frame Count: %d\n", result->frame_count);
        fprintf(report, "Avg Frame Time: %.3f ms\n", result->avg_frame_time_ms);
        fprintf(report, "Min Frame Time: %.3f ms\n", result->min_frame_time_ms);
        fprintf(report, "Max Frame Time: %.3f ms\n", result->max_frame_time_ms);
        fprintf(report, "Frame Time Variance: %.6f\n", result->frame_time_variance);
        fprintf(report, "Average FPS: %.1f\n", result->fps_average);
        fprintf(report, "Stereo Overhead: %.1f%%\n\n", result->stereo_overhead_percent);
    }

    fprintf(report, "=== Benchmark Summary ===\n");
    if (g_benchmark_session.result_count >= 2) {
        // Compare first two results (typically stereo vs non-stereo)
        STEREO_BENCHMARK_RESULT *stereo = &g_benchmark_session.results[0];
        STEREO_BENCHMARK_RESULT *mono = &g_benchmark_session.results[1];

        double overhead = ((stereo->avg_frame_time_ms - mono->avg_frame_time_ms) /
                          mono->avg_frame_time_ms) * 100.0;

        fprintf(report, "Stereo Overhead: %.1f%%\n", overhead);

        if (overhead < 20.0) {
            fprintf(report, "Status: EXCELLENT (within target)\n");
        } else if (overhead < 30.0) {
            fprintf(report, "Status: ACCEPTABLE\n");
        } else {
            fprintf(report, "Status: NEEDS OPTIMIZATION\n");
        }
    }

    fprintf(report, "\n=== End of Report ===\n");
    fclose(report);

    printf("STEREO_BENCHMARK: Report generated: %s\n", output_filename);
}

// Shutdown benchmark
void StereoBenchmark_Shutdown(void)
{
    if (g_benchmark_session.results) {
        free(g_benchmark_session.results);
        g_benchmark_session.results = NULL;
    }

    if (g_frame_times.frame_times) {
        free(g_frame_times.frame_times);
        g_frame_times.frame_times = NULL;
    }

    printf("STEREO_BENCHMARK: Shutdown complete\n");
}

// Print results to console
void StereoBenchmark_PrintResultsToConsole(void)
{
    printf("\n=== Benchmark Results ===\n");
    for (int i = 0; i < g_benchmark_session.result_count; i++) {
        STEREO_BENCHMARK_RESULT *result = &g_benchmark_session.results[i];
        printf("Result %d: %.1f FPS (Avg: %.2f ms)\n",
               i + 1, result->fps_average, result->avg_frame_time_ms);
    }
    printf("========================\n\n");
}

// Create default benchmark configuration
void StereoBenchmark_CreateDefaultConfig(STEREO_BENCHMARK_CONFIG *config)
{
    if (!config) return;

    memset(config, 0, sizeof(STEREO_BENCHMARK_CONFIG));
    config->scenario = BENCH_SCENARIO_COMPLEX;
    config->duration_frames = 300; // 5 seconds at 60 FPS
    config->stereo_modes_to_test = 0x3F; // All 6 modes
    config->test_stereo_enabled = 1;
    config->test_stereo_disabled = 1;
    config->log_per_frame = 0;
    strcpy(config->output_filename, "stereo_benchmark_report.txt");
}

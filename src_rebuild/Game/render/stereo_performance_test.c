// REDRIVER2 Stereo Rendering Performance Test Utility
// This module provides comprehensive performance testing infrastructure

#include "stereo_profiler.h"
#include "stereo_optimizer.h"
#include "stereo_benchmark.h"
#include "stereo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test configuration
typedef struct {
    int profile_enabled;
    int optimization_enabled;
    int benchmark_enabled;
    int verbose_logging;
} STEREO_TEST_CONFIG;

static STEREO_TEST_CONFIG test_config;

// Initialize performance testing framework
void StereoPerformanceTest_Init(int enable_all)
{
    memset(&test_config, 0, sizeof(STEREO_TEST_CONFIG));

    if (enable_all) {
        test_config.profile_enabled = 1;
        test_config.optimization_enabled = 1;
        test_config.benchmark_enabled = 1;
        test_config.verbose_logging = 1;
    }

    printf("\n=== REDRIVER2 Stereo Performance Test Suite ===\n");
    printf("Profiling: %s\n", test_config.profile_enabled ? "ENABLED" : "DISABLED");
    printf("Optimization: %s\n", test_config.optimization_enabled ? "ENABLED" : "DISABLED");
    printf("Benchmarking: %s\n", test_config.benchmark_enabled ? "ENABLED" : "DISABLED");
    printf("========================================\n\n");

    // Initialize profiler
    if (test_config.profile_enabled) {
        StereoProfiler_Init(STEREO_PROF_DETAILED, 10000);
    }

    // Initialize optimizer
    if (test_config.optimization_enabled) {
        int opt_flags = STEREO_OPT_MATRIX_CACHING |
                       STEREO_OPT_SCISSOR_BATCHING |
                       STEREO_OPT_CLEAR_REDUCTION |
                       STEREO_OPT_VIEWPORT_CACHING;
        StereoOptimizer_Init(opt_flags);
    }
}

// Run profiling test
void StereoPerformanceTest_RunProfileTest(int frame_count)
{
    if (!test_config.profile_enabled) {
        printf("Profiling not enabled\n");
        return;
    }

    printf("Running profiling test for %d frames...\n", frame_count);

    // Reset profiler
    StereoProfiler_Reset();

    // Simulate frame rendering
    for (int i = 0; i < frame_count; i++) {
        StereoProfiler_StartFrame();

        // Simulate camera calculations
        StereoProfiler_RecordEvent(PROF_EVENT_CAMERA_CALC_START);
        StereoProfiler_RecordEvent(PROF_EVENT_CAMERA_CALC_END);

        // Simulate viewport setup
        StereoProfiler_RecordEvent(PROF_EVENT_VIEWPORT_SET_START);
        StereoProfiler_RecordEvent(PROF_EVENT_VIEWPORT_SET_END);

        // Simulate rendering
        StereoProfiler_RecordEvent(PROF_EVENT_RENDER_LEFT_START);
        StereoProfiler_RecordEvent(PROF_EVENT_RENDER_LEFT_END);

        StereoProfiler_RecordEvent(PROF_EVENT_RENDER_RIGHT_START);
        StereoProfiler_RecordEvent(PROF_EVENT_RENDER_RIGHT_END);

        StereoProfiler_EndFrame();

        if (test_config.verbose_logging && i % 30 == 0) {
            printf("  Progress: %d/%d frames\n", i, frame_count);
        }
    }

    printf("Profiling test complete\n");
}

// Run optimization test
void StereoPerformanceTest_RunOptimizationTest(void)
{
    if (!test_config.optimization_enabled) {
        printf("Optimization not enabled\n");
        return;
    }

    printf("Running optimization test...\n");

    // Test viewport caching
    printf("  Testing viewport caching...\n");
    StereoOptimizer_ResetStats();

    for (int i = 0; i < 100; i++) {
        // Simulate repeated viewport calls
        StereoOptimizer_SetViewportCached(0, 0, 640, 480);
        StereoOptimizer_SetViewportCached(0, 0, 640, 480); // Same - should be skipped
        StereoOptimizer_SetViewportCached(320, 0, 320, 480); // Different - should not be skipped
    }

    STEREO_OPT_STATS opt_stats;
    StereoOptimizer_GetStats(&opt_stats);
    printf("    Total calls: %d, Skipped: %d (%.1f%%)\n",
           opt_stats.total_viewport_calls,
           opt_stats.viewport_calls_skipped,
           opt_stats.total_viewport_calls > 0 ?
               (double)opt_stats.viewport_calls_skipped / opt_stats.total_viewport_calls * 100 : 0);

    printf("Optimization test complete\n");
}

// Run benchmark test
void StereoPerformanceTest_RunBenchmark(void)
{
    if (!test_config.benchmark_enabled) {
        printf("Benchmarking not enabled\n");
        return;
    }

    printf("Running benchmark test...\n");

    // Create benchmark configuration
    STEREO_BENCHMARK_CONFIG config;
    StereoBenchmark_CreateDefaultConfig(&config);
    config.duration_frames = 100;
    config.log_per_frame = test_config.verbose_logging;

    // Initialize benchmark
    StereoBenchmark_Init(&config);

    // Run benchmark
    StereoBenchmark_Start();

    // Simulate frame rendering
    double baseline_frame_time = 16.67; // 60 FPS target
    for (int i = 0; i < config.duration_frames; i++) {
        // Simulate frame time with slight variance
        double frame_time = baseline_frame_time * (0.95 + (i % 10) * 0.01);
        StereoBenchmark_RecordFrame(frame_time);

        if (test_config.verbose_logging && i % 30 == 0) {
            printf("  Progress: %d/%d frames\n", i, config.duration_frames);
        }
    }

    printf("Benchmark test complete\n");

    // Generate report
    StereoBenchmark_GenerateReport("stereo_benchmark_results.txt");
    StereoBenchmark_PrintResultsToConsole();
}

// Generate comprehensive performance report
void StereoPerformanceTest_GenerateReport(const char *output_dir)
{
    printf("\nGenerating performance report...\n");

    if (test_config.profile_enabled) {
        char profiler_report[512];
        snprintf(profiler_report, sizeof(profiler_report), "%s/stereo_profiler_report.txt", output_dir);
        StereoProfiler_GenerateReport(profiler_report);
    }

    if (test_config.optimization_enabled) {
        char opt_report[512];
        snprintf(opt_report, sizeof(opt_report), "%s/stereo_optimizer_report.txt", output_dir);

        FILE *report = fopen(opt_report, "w");
        if (report) {
            fprintf(report, "=== REDRIVER2 Stereo Optimizer Report ===\n\n");
            StereoOptimizer_PrintStats();
            fprintf(report, "\n=== End of Report ===\n");
            fclose(report);
        }
    }

    printf("Report generation complete\n");
}

// Shutdown performance testing
void StereoPerformanceTest_Shutdown(void)
{
    printf("\nShutting down performance test suite...\n");

    if (test_config.profile_enabled) {
        StereoProfiler_Shutdown();
    }

    if (test_config.optimization_enabled) {
        StereoOptimizer_Shutdown();
    }

    if (test_config.benchmark_enabled) {
        StereoBenchmark_Shutdown();
    }

    printf("Performance test suite shutdown complete\n");
}

// Run all tests
void StereoPerformanceTest_RunAll(const char *output_dir)
{
    printf("\n=== Running All Performance Tests ===\n\n");

    StereoPerformanceTest_Init(1);

    printf("1. Running profiling test...\n");
    StereoPerformanceTest_RunProfileTest(300);

    printf("\n2. Running optimization test...\n");
    StereoPerformanceTest_RunOptimizationTest();

    printf("\n3. Running benchmark test...\n");
    StereoPerformanceTest_RunBenchmark();

    printf("\n4. Generating reports...\n");
    StereoPerformanceTest_GenerateReport(output_dir);

    printf("\n5. Shutting down...\n");
    StereoPerformanceTest_Shutdown();

    printf("\n=== All Performance Tests Complete ===\n");
    printf("Reports saved to: %s\n", output_dir);
}

// Standalone test entry point
#ifdef STEREO_PERF_TEST_STANDALONE
int main(int argc, char *argv[])
{
    const char *output_dir = argc > 1 ? argv[1] : ".";

    printf("REDRIVER2 Stereo Performance Test Suite\n");
    printf("Output directory: %s\n\n", output_dir);

    StereoPerformanceTest_RunAll(output_dir);

    return 0;
}
#endif

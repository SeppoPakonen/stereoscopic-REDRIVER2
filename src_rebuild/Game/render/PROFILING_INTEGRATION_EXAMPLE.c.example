// REDRIVER2 Stereo Profiling Integration Example
// This file demonstrates how to integrate the stereo profiling system
// into the game rendering loop

#include "stereo_profiler.h"
#include "stereo_optimizer.h"
#include "stereo_benchmark.h"
#include "stereo.h"
#include "stereo_performance_test.h"

// Example 1: Minimal Integration
// Add this to your game initialization:
void GameInit_WithProfiling(void)
{
    // Initialize profiling and optimization systems
    // Set both to 1 to enable profiling and optimization
    StereoPerformance_Init(1, 1);

    printf("Stereo rendering profiling initialized\n");
}

// Example 2: Game Loop Integration
// Modify your rendering loop like this:
void DrawGameFrame_WithProfiling(int player_num)
{
    int screenW = 320, screenH = 240;
#ifndef PSX
    PsyX_GetScreenSize(&screenW, &screenH);
#endif

    // Start frame profiling
    StereoRender_ProfileFrameStart();

    // Clear screen (uses optimization to skip redundant clears)
    StereoRender_ClearOptimized(0, 0, screenW, screenH, 0, 0, 0);

    // Handle different stereo modes with optimized rendering
    if (gStereoMode == STEREO_SIDEBYSIDE)
    {
        // Render left eye to left half
        StereoRender_SetViewPortOptimized(0, 0, screenW / 2, screenH);
        StereoCamera_Update(&player[player_num], STEREO_EYE_LEFT);
        RenderGame2(player_num);

        // Render right eye to right half
        StereoRender_SetViewPortOptimized(screenW / 2, 0, screenW / 2, screenH);
        StereoCamera_Update(&player[player_num], STEREO_EYE_RIGHT);
        RenderGame2(player_num);

        // Reset viewport
        StereoRender_SetViewPortOptimized(0, 0, screenW, screenH);
    }
    else if (gStereoMode == STEREO_TOPBOTTOM)
    {
        // Render left eye to top half
        StereoRender_SetViewPortOptimized(0, 0, screenW, screenH / 2);
        StereoCamera_Update(&player[player_num], STEREO_EYE_LEFT);
        RenderGame2(player_num);

        // Render right eye to bottom half
        StereoRender_SetViewPortOptimized(0, screenH / 2, screenW, screenH / 2);
        StereoCamera_Update(&player[player_num], STEREO_EYE_RIGHT);
        RenderGame2(player_num);

        // Reset viewport
        StereoRender_SetViewPortOptimized(0, 0, screenW, screenH);
    }
    else if (gStereoMode == STEREO_INTERLACED)
    {
        // Clear once
        StereoRender_ClearOptimized(0, 0, screenW, screenH, 0, 0, 0);

        // Render left eye full-screen
        StereoCamera_Update(&player[player_num], STEREO_EYE_LEFT);
        RenderGame2(player_num);

        // Render right eye full-screen (shader will interleave)
        StereoCamera_Update(&player[player_num], STEREO_EYE_RIGHT);
        RenderGame2(player_num);
    }
    else if (gStereoMode == STEREO_DISABLED)
    {
        // Normal non-stereo rendering
        StereoCamera_Update(&player[player_num], STEREO_EYE_MONO);
        RenderGame2(player_num);
    }
    else
    {
        // Anaglyph modes: render both eyes full-screen
        StereoCamera_Update(&player[player_num], STEREO_EYE_LEFT);
        RenderGame2(player_num);

        StereoCamera_Update(&player[player_num], STEREO_EYE_RIGHT);
        RenderGame2(player_num);
    }

    // End frame profiling
    StereoRender_ProfileFrameEnd();
}

// Example 3: Performance Monitoring During Gameplay
// Add this to your in-game debug display or console:
void DisplayPerformanceMetrics(void)
{
    static int frame_count = 0;
    frame_count++;

    // Display metrics every 30 frames (~0.5 seconds at 60 FPS)
    if (frame_count % 30 == 0) {
        STEREO_FRAME_STATS stats;
        StereoRender_GetPerformanceMetrics(&stats);

        printf("\n=== PERFORMANCE METRICS (Frame %d) ===\n", frame_count);
        printf("Frame Time: %.2f ms\n", stats.total_frame_time_ms);
        printf("FPS: %.1f\n", 1000.0 / stats.total_frame_time_ms);
        printf("Stereo Overhead: %.1f%%\n", stats.stereo_overhead_percent);
        printf("Left Eye: %.2f ms (%.1f%%)\n", stats.render_left_time_ms,
               stats.left_render_percent);
        printf("Right Eye: %.2f ms (%.1f%%)\n", stats.render_right_time_ms,
               stats.right_render_percent);
        printf("Camera Calc: %.2f ms\n", stats.camera_calc_time_ms);
        printf("Viewport Setup: %.2f ms\n", stats.viewport_setup_time_ms);
        printf("Clear Ops: %.2f ms\n", stats.clear_time_ms);
        printf("=====================================\n");
    }
}

// Example 4: Shutdown and Report Generation
void GameShutdown_WithProfiling(void)
{
    printf("\nGenerating performance reports...\n");

    // Generate detailed reports
    StereoPerformance_GenerateReports(".");

    // Print optimizer statistics
    StereoOptimizer_PrintStats();

    // Shutdown profiling systems
    StereoPerformance_Shutdown();

    printf("Performance reports generated. Check current directory for:\n");
    printf("  - stereo_profiler_report.txt\n");
    printf("  - stereo_optimizer_report.txt\n");
}

// Example 5: Running Standalone Benchmark
void RunStereoBenchmark_Example(void)
{
    // Create benchmark configuration
    STEREO_BENCHMARK_CONFIG config;
    StereoBenchmark_CreateDefaultConfig(&config);

    config.scenario = BENCH_SCENARIO_COMPLEX;  // Dense traffic
    config.duration_frames = 300;                // 5 seconds at 60 FPS
    config.log_per_frame = 1;                   // Print progress

    // Initialize benchmark
    StereoBenchmark_Init(&config);
    StereoBenchmark_Start();

    printf("Running stereo benchmark (%d frames)...\n", config.duration_frames);

    // In your game loop, record frame times
    // for (int i = 0; i < 300; i++) {
    //     double frame_time = MeasureFrameTime();
    //     StereoBenchmark_RecordFrame(frame_time);
    // }

    // When done:
    StereoBenchmark_Stop();
    StereoBenchmark_GenerateReport("stereo_benchmark_report.txt");
    StereoBenchmark_Shutdown();

    printf("Benchmark complete. Report saved to stereo_benchmark_report.txt\n");
}

// Example 6: Running Complete Test Suite
void RunCompletePerformanceTest_Example(void)
{
    printf("\n=== REDRIVER2 Stereo Performance Test Suite ===\n");
    printf("This will run profiling, optimization, and benchmark tests\n");
    printf("and generate comprehensive reports.\n");
    printf("================================================\n\n");

    // Initialize all testing components
    StereoPerformanceTest_Init(1); // Enable all tests

    // Run profiling test (300 frames)
    printf("Step 1: Running profiling test (300 frames)...\n");
    StereoPerformanceTest_RunProfileTest(300);

    // Run optimization test
    printf("Step 2: Running optimization test...\n");
    StereoPerformanceTest_RunOptimizationTest();

    // Run benchmark
    printf("Step 3: Running benchmark test...\n");
    StereoPerformanceTest_RunBenchmark();

    // Generate comprehensive reports
    printf("Step 4: Generating reports...\n");
    StereoPerformanceTest_GenerateReport(".");

    // Shutdown
    StereoPerformanceTest_Shutdown();

    printf("\nTest suite complete!\n");
    printf("Reports generated in current directory:\n");
    printf("  - stereo_profiler_report.txt\n");
    printf("  - stereo_benchmark_results.txt\n");
    printf("  - stereo_optimizer_report.txt\n");
}

// Example 7: Conditional Profiling (Enable/Disable at Runtime)
int enable_profiling = 0;  // Can be set from config or command line

void GameLoop_ConditionalProfiling(void)
{
    static int profiling_initialized = 0;

    if (enable_profiling && !profiling_initialized) {
        // Enable profiling on demand
        StereoPerformance_Init(1, 1);
        profiling_initialized = 1;
        printf("Stereo profiling ENABLED\n");
    }
    else if (!enable_profiling && profiling_initialized) {
        // Disable profiling and generate reports
        StereoPerformance_GenerateReports(".");
        StereoPerformance_Shutdown();
        profiling_initialized = 0;
        printf("Stereo profiling DISABLED - reports generated\n");
    }

    // ... normal game loop code ...
}

// Example 8: Performance-Based Optimization Control
void AdjustOptimizations_BasedOnPerformance(void)
{
    static int last_check_frame = 0;
    static int check_interval = 300;  // Check every 5 seconds

    int current_frame = GetCurrentFrame();

    if (current_frame - last_check_frame >= check_interval) {
        STEREO_FRAME_STATS stats;
        StereoRender_GetPerformanceMetrics(&stats);

        printf("Performance Check (Frame %d):\n", current_frame);
        printf("  Overhead: %.1f%%\n", stats.stereo_overhead_percent);

        if (stats.stereo_overhead_percent > 30.0) {
            printf("  WARNING: Overhead exceeds 30%%\n");
            printf("  Enabling additional optimizations...\n");

            StereoOptimizer_EnableOptimization(STEREO_OPT_MATRIX_CACHING);
            StereoOptimizer_EnableOptimization(STEREO_OPT_SCISSOR_BATCHING);
        }
        else if (stats.stereo_overhead_percent < 10.0) {
            printf("  EXCELLENT: Overhead below 10%%\n");
        }

        last_check_frame = current_frame;
    }
}

/*
=== INTEGRATION CHECKLIST ===

To integrate stereo profiling into REDRIVER2:

1. Add to game initialization (e.g., in main.c GameInit):
   GameInit_WithProfiling();

2. Modify main rendering loop (in DrawGame or similar):
   - Replace GR_SetViewPort() with StereoRender_SetViewPortOptimized()
   - Replace GR_Clear() with StereoRender_ClearOptimized()
   - Add StereoRender_ProfileFrameStart() at frame start
   - Add StereoRender_ProfileFrameEnd() at frame end

3. Add to performance monitoring (optional debug display):
   DisplayPerformanceMetrics();

4. Add to game shutdown (in game cleanup):
   GameShutdown_WithProfiling();

5. (Optional) Run performance tests:
   RunCompletePerformanceTest_Example();

6. Build and run game:
   - Game will initialize profiling automatically
   - After 300+ frames, performance reports will be generated
   - Check working directory for:
     * stereo_profiler_report.txt
     * stereo_optimizer_report.txt

=== EXPECTED OUTPUT ===

When profiling is enabled, you'll see in console:
```
Stereo profiling initialized
STEREO_PROFILER: Initialized with mode=2, capacity=10000
STEREO_OPTIMIZER: Initialized with flags=0x1f
  - Matrix Caching enabled
  - Scissor Batching enabled
  - Clear Reduction enabled
  - Viewport Caching enabled
  - Shader Precompilation enabled
```

After 300+ frames:
```
Generating performance reports...
STEREO_PROFILER: Report generated: stereo_profiler_report.txt
STEREO_OPTIMIZER: Statistics reset
STEREO_OPTIMIZER: Shutdown complete
StereoPerformance: Shutdown complete
```

Reports will show performance metrics and optimization effectiveness.

=== SAMPLE REPORT OUTPUT ===

From stereo_profiler_report.txt:
```
=== REDRIVER2 Stereo Rendering Performance Report ===

--- Average Frame Performance ---
Total Frame Time: 17.85 ms (Target: ~16.67 ms for 60 FPS)
Camera Calculation: 0.18 ms (1.0%)
Viewport Setup: 0.12 ms (0.7%)
Left Eye Render: 8.65 ms (48.5%)
Right Eye Render: 8.55 ms (47.9%)
Shader Setup: 0.04 ms (0.2%)
Scissor Setup: 0.01 ms (0.1%)
Clear Operations: 0.15 ms (0.8%)
Viewport Reset: 0.15 ms (0.8%)

--- Optimization Analysis ---
Baseline Frame Time (non-stereo): 16.67 ms
Stereo Overhead: 7.1%

--- Performance Assessment ---
EXCELLENT: Stereo overhead is within target (< 20%)
```

This confirms stereo rendering is performing optimally!
*/

#ifndef STEREO_BENCHMARK_H
#define STEREO_BENCHMARK_H

// Stereo rendering performance benchmarking system

// Benchmark scenario types
typedef enum {
    BENCH_SCENARIO_SIMPLE = 0,      // Few vehicles/pedestrians
    BENCH_SCENARIO_COMPLEX = 1,     // Dense traffic
    BENCH_SCENARIO_MOTION = 2,      // Fast camera movement
    BENCH_SCENARIO_STATIC = 3,      // Stationary camera
    BENCH_SCENARIO_MIXED = 4        // Mixed scenarios
} STEREO_BENCHMARK_SCENARIO;

// Benchmark configuration
typedef struct {
    STEREO_BENCHMARK_SCENARIO scenario;
    int duration_frames;            // How many frames to run benchmark
    int stereo_modes_to_test;       // Bitmask of modes to test
    int test_stereo_enabled;
    int test_stereo_disabled;
    int log_per_frame;
    char output_filename[256];
} STEREO_BENCHMARK_CONFIG;

// Benchmark result for one run
typedef struct {
    int scenario;
    int stereo_mode;
    int stereo_enabled;
    double avg_frame_time_ms;
    double min_frame_time_ms;
    double max_frame_time_ms;
    double frame_time_variance;
    int frame_count;
    double fps_average;
    double stereo_overhead_percent;
} STEREO_BENCHMARK_RESULT;

// Benchmark session
typedef struct {
    STEREO_BENCHMARK_CONFIG config;
    STEREO_BENCHMARK_RESULT *results;
    int result_count;
    int result_capacity;
    int active;
    int current_frame;
    double test_start_time;
} STEREO_BENCHMARK_SESSION;

extern STEREO_BENCHMARK_SESSION g_benchmark_session;

// Benchmark API

// Initialize benchmark session
void StereoBenchmark_Init(STEREO_BENCHMARK_CONFIG *config);

// Start running a benchmark
void StereoBenchmark_Start(void);

// Record frame time
void StereoBenchmark_RecordFrame(double frame_time_ms);

// Stop benchmark and compute results
void StereoBenchmark_Stop(void);

// Check if benchmark is active
int StereoBenchmark_IsActive(void);

// Generate benchmark report
void StereoBenchmark_GenerateReport(const char *output_filename);

// Shutdown benchmark
void StereoBenchmark_Shutdown(void);

// Utility functions
void StereoBenchmark_PrintResultsToConsole(void);
void StereoBenchmark_CreateDefaultConfig(STEREO_BENCHMARK_CONFIG *config);

#endif // STEREO_BENCHMARK_H

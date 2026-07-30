#ifndef STEREO_PERFORMANCE_TEST_H
#define STEREO_PERFORMANCE_TEST_H

// REDRIVER2 Stereo Performance Test Suite
// Provides comprehensive performance testing infrastructure

// Initialize performance testing framework
void StereoPerformanceTest_Init(int enable_all);

// Run individual tests
void StereoPerformanceTest_RunProfileTest(int frame_count);
void StereoPerformanceTest_RunOptimizationTest(void);
void StereoPerformanceTest_RunBenchmark(void);

// Generate performance report
void StereoPerformanceTest_GenerateReport(const char *output_dir);

// Shutdown performance testing
void StereoPerformanceTest_Shutdown(void);

// Run all tests
void StereoPerformanceTest_RunAll(const char *output_dir);

#endif // STEREO_PERFORMANCE_TEST_H

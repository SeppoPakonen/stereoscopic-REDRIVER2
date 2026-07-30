/*
 * REDRIVER2 Stereo Test Harness
 *
 * Comprehensive automated testing framework for stereo rendering system
 * Provides test registration, execution, reporting, and regression detection
 */

#ifndef STEREO_TEST_HARNESS_H
#define STEREO_TEST_HARNESS_H

#include <types.h>

// Forward declarations
typedef struct _PLAYER PLAYER;

// Test result types
typedef enum {
    TEST_RESULT_PASS = 0,
    TEST_RESULT_FAIL = 1,
    TEST_RESULT_BLOCKED = 2,
    TEST_RESULT_SKIP = 3,
    TEST_RESULT_ERROR = 4
} TEST_RESULT;

// Test severity levels
typedef enum {
    TEST_SEVERITY_INFO = 0,
    TEST_SEVERITY_LOW = 1,
    TEST_SEVERITY_MEDIUM = 2,
    TEST_SEVERITY_HIGH = 3,
    TEST_SEVERITY_CRITICAL = 4
} TEST_SEVERITY;

// Test category
typedef enum {
    TEST_CATEGORY_REGRESSION = 0,
    TEST_CATEGORY_STEREO_COMPAT = 1,
    TEST_CATEGORY_PERFORMANCE = 2,
    TEST_CATEGORY_INPUT = 3,
    TEST_CATEGORY_AUDIO = 4,
    TEST_CATEGORY_CONFIG = 5,
    TEST_CATEGORY_EDGE_CASE = 6,
    TEST_CATEGORY_PLATFORM = 7
} TEST_CATEGORY;

// Test statistics
typedef struct {
    int total_tests;
    int passed;
    int failed;
    int blocked;
    int skipped;
    int errors;
    unsigned int total_duration_ms;  // milliseconds
    float regression_count;
} TEST_STATS;

// Individual test result
typedef struct {
    int test_id;
    const char *test_name;
    TEST_CATEGORY category;
    TEST_RESULT result;
    TEST_SEVERITY severity;
    const char *expected;
    const char *actual;
    const char *error_message;
    unsigned int duration_ms;
    int is_regression;
} TEST_RECORD;

// Test function signature
typedef TEST_RESULT (*TEST_FUNC)(void);

// Test registry entry
typedef struct {
    int test_id;
    const char *test_name;
    TEST_CATEGORY category;
    TEST_SEVERITY severity;
    TEST_FUNC test_func;
    int enabled;
    int is_regression_test;
} TEST_ENTRY;

// Performance metrics
typedef struct {
    const char *test_name;
    float baseline_ms;
    float current_ms;
    float delta_percent;
    int passed;
} PERF_TEST_RESULT;

// Initialization and cleanup
void StereoTestHarness_Init(void);
void StereoTestHarness_Shutdown(void);

// Test execution
TEST_RESULT StereoTestHarness_RunTest(int test_id);
void StereoTestHarness_RunAllTests(void);
void StereoTestHarness_RunCategory(TEST_CATEGORY category);
void StereoTestHarness_RunRegressionTests(void);
void StereoTestHarness_RunPerformanceTests(void);

// Test registration
void StereoTestHarness_RegisterTest(
    int test_id,
    const char *test_name,
    TEST_CATEGORY category,
    TEST_SEVERITY severity,
    TEST_FUNC test_func
);

void StereoTestHarness_UnregisterTest(int test_id);
void StereoTestHarness_EnableTest(int test_id, int enable);

// Results access
TEST_STATS StereoTestHarness_GetStatistics(void);
TEST_RECORD *StereoTestHarness_GetTestResult(int test_id);
TEST_RECORD *StereoTestHarness_GetAllResults(int *out_count);

// Reporting
void StereoTestHarness_GenerateReport(const char *output_file);
void StereoTestHarness_GenerateRegressionReport(const char *output_file);
void StereoTestHarness_GenerateCSVReport(const char *output_file);

// Baseline comparison
void StereoTestHarness_SaveBaseline(const char *output_file);
void StereoTestHarness_LoadBaseline(const char *baseline_file);
int StereoTestHarness_CompareToBaseline(void);

// Utility functions
void StereoTestHarness_LogResult(
    int test_id,
    TEST_RESULT result,
    const char *expected,
    const char *actual,
    const char *message
);

void StereoTestHarness_LogPerformance(
    const char *test_name,
    float duration_ms
);

// Assertion helpers
int StereoTest_AssertTrue(int condition, const char *message);
int StereoTest_AssertFalse(int condition, const char *message);
int StereoTest_AssertEqual(int expected, int actual, const char *message);
int StereoTest_AssertFloatEqual(float expected, float actual, float tolerance, const char *message);
int StereoTest_AssertStringEqual(const char *expected, const char *actual, const char *message);
int StereoTest_AssertNotNull(const void *ptr, const char *message);
int StereoTest_AssertNull(const void *ptr, const char *message);

// Performance test helpers
typedef struct {
    unsigned int start_time_ms;
    unsigned int end_time_ms;
    unsigned int frame_count;
    float avg_frame_time_ms;
    float min_frame_time_ms;
    float max_frame_time_ms;
} PERF_MEASUREMENT;

PERF_MEASUREMENT *StereoTest_MeasurePerformance(const char *test_name, int frame_count);
void StereoTest_FreeMeasurement(PERF_MEASUREMENT *measurement);

// Stereo-specific test helpers
int StereoTest_VerifyStereoMode(int expected_mode);
int StereoTest_VerifyEyeRendering(int eye);
int StereoTest_VerifySeparationValue(float expected_separation);
int StereoTest_VerifyConvergenceValue(float expected_convergence);
int StereoTest_VerifyConfigPersistence(void);
int StereoTest_VerifyAudioSync(void);

// Debug utilities
void StereoTestHarness_PrintStatistics(void);
void StereoTestHarness_PrintFailures(void);
void StereoTestHarness_PrintRegressions(void);
void StereoTestHarness_EnableDebugLogging(int enable);

#endif // STEREO_TEST_HARNESS_H

/*
 * REDRIVER2 Stereo Test Harness Implementation
 *
 * Provides test framework infrastructure for stereo rendering validation
 */

#include "stereo_test_harness.h"
#include "stereo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Maximum tests that can be registered
#define MAX_TESTS 256

// Test registry
static TEST_ENTRY g_test_registry[MAX_TESTS];
static int g_test_count = 0;

// Test results storage
static TEST_RECORD g_test_results[MAX_TESTS];

// Statistics
static TEST_STATS g_test_stats = {0};

// Baseline data
static TEST_RECORD g_baseline_results[MAX_TESTS];
static int g_baseline_loaded = 0;
static int g_baseline_count = 0;

// Debug logging
static int g_debug_logging = 0;

// Local time tracking for performance measurements
static unsigned int _get_time_ms(void)
{
    // TODO: Implement platform-specific timer
    // For now, use a simple counter
    static unsigned int counter = 0;
    return counter++;
}

/*
 * Initialization and cleanup
 */

void StereoTestHarness_Init(void)
{
    memset(g_test_registry, 0, sizeof(g_test_registry));
    memset(g_test_results, 0, sizeof(g_test_results));
    memset(&g_test_stats, 0, sizeof(g_test_stats));
    memset(g_baseline_results, 0, sizeof(g_baseline_results));

    g_test_count = 0;
    g_baseline_loaded = 0;
    g_baseline_count = 0;

    if (g_debug_logging) {
        printf("[STEREO TEST] Harness initialized\n");
    }
}

void StereoTestHarness_Shutdown(void)
{
    // Cleanup resources
    g_test_count = 0;

    if (g_debug_logging) {
        printf("[STEREO TEST] Harness shutdown\n");
    }
}

/*
 * Test registration
 */

void StereoTestHarness_RegisterTest(
    int test_id,
    const char *test_name,
    TEST_CATEGORY category,
    TEST_SEVERITY severity,
    TEST_FUNC test_func
)
{
    if (g_test_count >= MAX_TESTS) {
        printf("[STEREO TEST] ERROR: Test registry full (max %d tests)\n", MAX_TESTS);
        return;
    }

    if (test_id < 0 || test_id >= MAX_TESTS) {
        printf("[STEREO TEST] ERROR: Invalid test ID %d\n", test_id);
        return;
    }

    if (g_test_registry[test_id].test_func != NULL) {
        printf("[STEREO TEST] ERROR: Test ID %d already registered\n", test_id);
        return;
    }

    TEST_ENTRY *entry = &g_test_registry[test_id];
    entry->test_id = test_id;
    entry->test_name = test_name;
    entry->category = category;
    entry->severity = severity;
    entry->test_func = test_func;
    entry->enabled = 1;
    entry->is_regression_test = 0;

    g_test_count++;

    if (g_debug_logging) {
        printf("[STEREO TEST] Registered test #%d: %s\n", test_id, test_name);
    }
}

void StereoTestHarness_UnregisterTest(int test_id)
{
    if (test_id < 0 || test_id >= MAX_TESTS) {
        return;
    }

    if (g_test_registry[test_id].test_func != NULL) {
        g_test_registry[test_id].test_func = NULL;
        g_test_count--;
    }
}

void StereoTestHarness_EnableTest(int test_id, int enable)
{
    if (test_id >= 0 && test_id < MAX_TESTS) {
        g_test_registry[test_id].enabled = enable;
    }
}

/*
 * Test execution
 */

TEST_RESULT StereoTestHarness_RunTest(int test_id)
{
    if (test_id < 0 || test_id >= MAX_TESTS) {
        return TEST_RESULT_ERROR;
    }

    TEST_ENTRY *entry = &g_test_registry[test_id];
    if (entry->test_func == NULL || !entry->enabled) {
        return TEST_RESULT_SKIP;
    }

    unsigned int start_ms = _get_time_ms();
    TEST_RESULT result = entry->test_func();
    unsigned int end_ms = _get_time_ms();

    // Store result
    TEST_RECORD *record = &g_test_results[test_id];
    record->test_id = test_id;
    record->test_name = entry->test_name;
    record->category = entry->category;
    record->result = result;
    record->severity = entry->severity;
    record->duration_ms = end_ms - start_ms;

    // Check if regression (compare to baseline)
    if (g_baseline_loaded && test_id < g_baseline_count) {
        TEST_RECORD *baseline = &g_baseline_results[test_id];
        if (baseline->result == TEST_RESULT_PASS && result != TEST_RESULT_PASS) {
            record->is_regression = 1;
            g_test_stats.regression_count += 1;
        }
    }

    // Update statistics
    g_test_stats.total_tests++;
    g_test_stats.total_duration_ms += record->duration_ms;

    switch (result) {
        case TEST_RESULT_PASS:
            g_test_stats.passed++;
            break;
        case TEST_RESULT_FAIL:
            g_test_stats.failed++;
            break;
        case TEST_RESULT_BLOCKED:
            g_test_stats.blocked++;
            break;
        case TEST_RESULT_SKIP:
            g_test_stats.skipped++;
            break;
        case TEST_RESULT_ERROR:
            g_test_stats.errors++;
            break;
    }

    if (g_debug_logging) {
        const char *result_str[] = {"PASS", "FAIL", "BLOCKED", "SKIP", "ERROR"};
        printf("[STEREO TEST] Test #%d: %s - %s (%u ms)\n",
               test_id, entry->test_name, result_str[result], record->duration_ms);
    }

    return result;
}

void StereoTestHarness_RunAllTests(void)
{
    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        if (g_test_registry[i].test_func != NULL) {
            StereoTestHarness_RunTest(i);
        }
    }
}

void StereoTestHarness_RunCategory(TEST_CATEGORY category)
{
    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        TEST_ENTRY *entry = &g_test_registry[i];
        if (entry->test_func != NULL && entry->category == category) {
            StereoTestHarness_RunTest(i);
        }
    }
}

void StereoTestHarness_RunRegressionTests(void)
{
    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        TEST_ENTRY *entry = &g_test_registry[i];
        if (entry->test_func != NULL && entry->is_regression_test) {
            StereoTestHarness_RunTest(i);
        }
    }
}

void StereoTestHarness_RunPerformanceTests(void)
{
    StereoTestHarness_RunCategory(TEST_CATEGORY_PERFORMANCE);
}

/*
 * Results access
 */

TEST_STATS StereoTestHarness_GetStatistics(void)
{
    return g_test_stats;
}

TEST_RECORD *StereoTestHarness_GetTestResult(int test_id)
{
    if (test_id >= 0 && test_id < MAX_TESTS) {
        return &g_test_results[test_id];
    }
    return NULL;
}

TEST_RECORD *StereoTestHarness_GetAllResults(int *out_count)
{
    if (out_count != NULL) {
        *out_count = g_test_stats.total_tests;
    }
    return g_test_results;
}

/*
 * Reporting
 */

void StereoTestHarness_GenerateReport(const char *output_file)
{
    FILE *f = fopen(output_file, "w");
    if (f == NULL) {
        printf("[STEREO TEST] ERROR: Cannot open report file: %s\n", output_file);
        return;
    }

    fprintf(f, "# REDRIVER2 Stereo Regression Test Report\n\n");
    fprintf(f, "## Summary\n");
    fprintf(f, "- Total Tests: %d\n", g_test_stats.total_tests);
    fprintf(f, "- Passed: %d\n", g_test_stats.passed);
    fprintf(f, "- Failed: %d\n", g_test_stats.failed);
    fprintf(f, "- Blocked: %d\n", g_test_stats.blocked);
    fprintf(f, "- Skipped: %d\n", g_test_stats.skipped);
    fprintf(f, "- Errors: %d\n", g_test_stats.errors);
    fprintf(f, "- Regressions: %.0f\n", g_test_stats.regression_count);
    fprintf(f, "- Total Duration: %u ms\n\n", g_test_stats.total_duration_ms);

    int pass_rate = (g_test_stats.total_tests > 0) ?
        (100 * g_test_stats.passed / g_test_stats.total_tests) : 0;
    fprintf(f, "**Overall Status**: %d%% Pass Rate\n\n", pass_rate);

    // Results by category
    fprintf(f, "## Results by Category\n\n");
    fprintf(f, "| Category | Tests | Passed | Failed |\n");
    fprintf(f, "|----------|-------|--------|--------|\n");

    TEST_CATEGORY cat;
    for (cat = 0; cat < 8; cat++) {
        int total = 0, passed = 0;
        int i;
        for (i = 0; i < MAX_TESTS; i++) {
            if (g_test_registry[i].category == cat && g_test_registry[i].test_func != NULL) {
                total++;
                if (g_test_results[i].result == TEST_RESULT_PASS) {
                    passed++;
                }
            }
        }

        if (total > 0) {
            const char *cat_name[] = {
                "Regression", "Stereo Compat", "Performance", "Input",
                "Audio", "Config", "Edge Cases", "Platform"
            };
            fprintf(f, "| %s | %d | %d | %d |\n",
                    cat_name[cat], total, passed, total - passed);
        }
    }

    fprintf(f, "\n## Detailed Results\n\n");
    fprintf(f, "| Test ID | Name | Result | Duration (ms) |\n");
    fprintf(f, "|---------|------|--------|---------------|\n");

    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        if (g_test_registry[i].test_func != NULL) {
            TEST_RECORD *rec = &g_test_results[i];
            const char *result_str[] = {"PASS", "FAIL", "BLOCKED", "SKIP", "ERROR"};
            fprintf(f, "| %d | %s | %s | %u |\n",
                    rec->test_id, rec->test_name,
                    result_str[rec->result], rec->duration_ms);
        }
    }

    fprintf(f, "\n## Regressions\n\n");
    if (g_test_stats.regression_count == 0) {
        fprintf(f, "No regressions detected.\n");
    } else {
        fprintf(f, "| Test | Previous Result | Current Result |\n");
        fprintf(f, "|------|-----------------|----------------|\n");
        for (i = 0; i < MAX_TESTS; i++) {
            if (g_test_results[i].is_regression) {
                const char *result_str[] = {"PASS", "FAIL", "BLOCKED", "SKIP", "ERROR"};
                fprintf(f, "| %s | %s | %s |\n",
                        g_test_results[i].test_name,
                        result_str[g_baseline_results[i].result],
                        result_str[g_test_results[i].result]);
            }
        }
    }

    fclose(f);
    printf("[STEREO TEST] Report generated: %s\n", output_file);
}

void StereoTestHarness_GenerateRegressionReport(const char *output_file)
{
    FILE *f = fopen(output_file, "w");
    if (f == NULL) {
        printf("[STEREO TEST] ERROR: Cannot open regression report file: %s\n", output_file);
        return;
    }

    fprintf(f, "# REDRIVER2 Stereo Regression Report\n\n");
    fprintf(f, "## Detected Regressions: %.0f\n\n", g_test_stats.regression_count);

    int i;
    int regression_count = 0;
    for (i = 0; i < MAX_TESTS; i++) {
        if (g_test_results[i].is_regression) {
            regression_count++;
            fprintf(f, "### Regression #%d: %s\n\n", regression_count, g_test_results[i].test_name);
            fprintf(f, "- Test ID: %d\n", g_test_results[i].test_id);
            fprintf(f, "- Category: %d\n", g_test_results[i].category);
            fprintf(f, "- Severity: %d\n", g_test_results[i].severity);

            if (g_test_results[i].error_message) {
                fprintf(f, "- Error: %s\n", g_test_results[i].error_message);
            }
            fprintf(f, "\n");
        }
    }

    if (regression_count == 0) {
        fprintf(f, "No regressions detected in this test run.\n");
    }

    fclose(f);
    printf("[STEREO TEST] Regression report generated: %s\n", output_file);
}

void StereoTestHarness_GenerateCSVReport(const char *output_file)
{
    FILE *f = fopen(output_file, "w");
    if (f == NULL) {
        printf("[STEREO TEST] ERROR: Cannot open CSV report file: %s\n", output_file);
        return;
    }

    fprintf(f, "Test ID,Test Name,Category,Result,Duration MS,Is Regression\n");

    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        if (g_test_registry[i].test_func != NULL) {
            TEST_RECORD *rec = &g_test_results[i];
            const char *result_str[] = {"PASS", "FAIL", "BLOCKED", "SKIP", "ERROR"};
            fprintf(f, "%d,%s,%d,%s,%u,%d\n",
                    rec->test_id, rec->test_name, rec->category,
                    result_str[rec->result], rec->duration_ms,
                    rec->is_regression);
        }
    }

    fclose(f);
    printf("[STEREO TEST] CSV report generated: %s\n", output_file);
}

/*
 * Baseline comparison
 */

void StereoTestHarness_SaveBaseline(const char *output_file)
{
    FILE *f = fopen(output_file, "w");
    if (f == NULL) {
        printf("[STEREO TEST] ERROR: Cannot save baseline: %s\n", output_file);
        return;
    }

    // Write baseline header
    fprintf(f, "# REDRIVER2 Stereo Test Baseline\n");
    fprintf(f, "# Generated: %s\n", __DATE__);
    fprintf(f, "# Total Tests: %d\n\n", g_test_stats.total_tests);

    // Write each test result
    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        if (g_test_registry[i].test_func != NULL) {
            TEST_RECORD *rec = &g_test_results[i];
            const char *result_str[] = {"PASS", "FAIL", "BLOCKED", "SKIP", "ERROR"};
            fprintf(f, "%d,%s,%s\n", rec->test_id, rec->test_name, result_str[rec->result]);
        }
    }

    fclose(f);
    printf("[STEREO TEST] Baseline saved: %s\n", output_file);
}

void StereoTestHarness_LoadBaseline(const char *baseline_file)
{
    FILE *f = fopen(baseline_file, "r");
    if (f == NULL) {
        printf("[STEREO TEST] ERROR: Cannot load baseline: %s\n", baseline_file);
        return;
    }

    char line[512];
    int count = 0;

    while (fgets(line, sizeof(line), f) && count < MAX_TESTS) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }

        // Parse CSV: test_id,test_name,result
        int test_id;
        char test_name[256];
        char result_str[32];

        if (sscanf(line, "%d,%255[^,],%31s", &test_id, test_name, result_str) == 3) {
            TEST_RECORD *rec = &g_baseline_results[count];
            rec->test_id = test_id;
            rec->test_name = test_name;

            // Convert result string to enum
            if (strcmp(result_str, "PASS") == 0) {
                rec->result = TEST_RESULT_PASS;
            } else if (strcmp(result_str, "FAIL") == 0) {
                rec->result = TEST_RESULT_FAIL;
            } else if (strcmp(result_str, "BLOCKED") == 0) {
                rec->result = TEST_RESULT_BLOCKED;
            } else if (strcmp(result_str, "SKIP") == 0) {
                rec->result = TEST_RESULT_SKIP;
            } else {
                rec->result = TEST_RESULT_ERROR;
            }

            count++;
        }
    }

    g_baseline_count = count;
    g_baseline_loaded = 1;
    fclose(f);

    printf("[STEREO TEST] Baseline loaded: %s (%d tests)\n", baseline_file, count);
}

int StereoTestHarness_CompareToBaseline(void)
{
    if (!g_baseline_loaded) {
        printf("[STEREO TEST] ERROR: No baseline loaded\n");
        return -1;
    }

    int regressions = 0;
    int i;

    for (i = 0; i < g_baseline_count && i < g_test_stats.total_tests; i++) {
        if (g_baseline_results[i].result == TEST_RESULT_PASS &&
            g_test_results[i].result != TEST_RESULT_PASS) {
            regressions++;
        }
    }

    return regressions;
}

/*
 * Utility functions
 */

void StereoTestHarness_LogResult(
    int test_id,
    TEST_RESULT result,
    const char *expected,
    const char *actual,
    const char *message
)
{
    if (test_id >= 0 && test_id < MAX_TESTS) {
        TEST_RECORD *rec = &g_test_results[test_id];
        rec->result = result;
        rec->expected = expected;
        rec->actual = actual;
        rec->error_message = message;
    }
}

void StereoTestHarness_LogPerformance(
    const char *test_name,
    float duration_ms
)
{
    if (g_debug_logging) {
        printf("[STEREO TEST] Performance: %s = %.2f ms\n", test_name, duration_ms);
    }
}

/*
 * Assertion helpers
 */

int StereoTest_AssertTrue(int condition, const char *message)
{
    if (!condition && g_debug_logging) {
        printf("[STEREO TEST ASSERT] FAILED: %s\n", message);
        return 0;
    }
    return condition;
}

int StereoTest_AssertFalse(int condition, const char *message)
{
    if (condition && g_debug_logging) {
        printf("[STEREO TEST ASSERT] FAILED: %s\n", message);
        return 0;
    }
    return !condition;
}

int StereoTest_AssertEqual(int expected, int actual, const char *message)
{
    if (expected != actual && g_debug_logging) {
        printf("[STEREO TEST ASSERT] FAILED: %s (expected %d, got %d)\n",
               message, expected, actual);
        return 0;
    }
    return expected == actual;
}

int StereoTest_AssertFloatEqual(float expected, float actual, float tolerance, const char *message)
{
    float diff = expected - actual;
    if (diff < 0) diff = -diff;

    if (diff > tolerance && g_debug_logging) {
        printf("[STEREO TEST ASSERT] FAILED: %s (expected %.2f, got %.2f, tolerance %.2f)\n",
               message, expected, actual, tolerance);
        return 0;
    }
    return diff <= tolerance;
}

int StereoTest_AssertStringEqual(const char *expected, const char *actual, const char *message)
{
    if (expected == NULL || actual == NULL) {
        return expected == actual;
    }

    if (strcmp(expected, actual) != 0 && g_debug_logging) {
        printf("[STEREO TEST ASSERT] FAILED: %s (expected '%s', got '%s')\n",
               message, expected, actual);
        return 0;
    }
    return strcmp(expected, actual) == 0;
}

int StereoTest_AssertNotNull(const void *ptr, const char *message)
{
    if (ptr == NULL && g_debug_logging) {
        printf("[STEREO TEST ASSERT] FAILED: %s (pointer is NULL)\n", message);
        return 0;
    }
    return ptr != NULL;
}

int StereoTest_AssertNull(const void *ptr, const char *message)
{
    if (ptr != NULL && g_debug_logging) {
        printf("[STEREO TEST ASSERT] FAILED: %s (pointer is not NULL)\n", message);
        return 0;
    }
    return ptr == NULL;
}

/*
 * Performance test helpers
 */

PERF_MEASUREMENT *StereoTest_MeasurePerformance(const char *test_name, int frame_count)
{
    PERF_MEASUREMENT *measurement = (PERF_MEASUREMENT *)malloc(sizeof(PERF_MEASUREMENT));
    if (measurement == NULL) {
        return NULL;
    }

    memset(measurement, 0, sizeof(PERF_MEASUREMENT));

    measurement->start_time_ms = _get_time_ms();
    measurement->frame_count = frame_count;

    // TODO: Run test for frame_count frames and measure time

    measurement->end_time_ms = _get_time_ms();
    measurement->avg_frame_time_ms =
        (float)(measurement->end_time_ms - measurement->start_time_ms) / frame_count;

    return measurement;
}

void StereoTest_FreeMeasurement(PERF_MEASUREMENT *measurement)
{
    if (measurement != NULL) {
        free(measurement);
    }
}

/*
 * Stereo-specific test helpers
 */

int StereoTest_VerifyStereoMode(int expected_mode)
{
    return StereoTest_AssertEqual(expected_mode, (int)gStereoMode,
                                  "Stereo mode mismatch");
}

int StereoTest_VerifyEyeRendering(int eye)
{
    // Verify that the correct eye is being rendered
    // This would check internal rendering state
    return 1; // TODO: Implement
}

int StereoTest_VerifySeparationValue(float expected_separation)
{
    return StereoTest_AssertFloatEqual(expected_separation, gStereoSeparation, 0.01f,
                                       "Stereo separation mismatch");
}

int StereoTest_VerifyConvergenceValue(float expected_convergence)
{
    return StereoTest_AssertFloatEqual(expected_convergence, gStereoConvergence, 0.1f,
                                       "Stereo convergence mismatch");
}

int StereoTest_VerifyConfigPersistence(void)
{
    // This would test config save/load
    // For now, return placeholder
    return 1;
}

int StereoTest_VerifyAudioSync(void)
{
    // This would verify audio synchronization
    // For now, return placeholder
    return 1;
}

/*
 * Debug utilities
 */

void StereoTestHarness_PrintStatistics(void)
{
    printf("\n=== STEREO TEST STATISTICS ===\n");
    printf("Total Tests: %d\n", g_test_stats.total_tests);
    printf("Passed: %d\n", g_test_stats.passed);
    printf("Failed: %d\n", g_test_stats.failed);
    printf("Blocked: %d\n", g_test_stats.blocked);
    printf("Skipped: %d\n", g_test_stats.skipped);
    printf("Errors: %d\n", g_test_stats.errors);
    printf("Regressions: %.0f\n", g_test_stats.regression_count);
    printf("Total Duration: %u ms\n", g_test_stats.total_duration_ms);

    int pass_rate = (g_test_stats.total_tests > 0) ?
        (100 * g_test_stats.passed / g_test_stats.total_tests) : 0;
    printf("Pass Rate: %d%%\n", pass_rate);
    printf("===============================\n\n");
}

void StereoTestHarness_PrintFailures(void)
{
    printf("\n=== FAILED TESTS ===\n");

    int count = 0;
    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        if (g_test_registry[i].test_func != NULL && g_test_results[i].result == TEST_RESULT_FAIL) {
            count++;
            printf("#%d: %s\n", g_test_results[i].test_id, g_test_results[i].test_name);
            if (g_test_results[i].error_message) {
                printf("  Error: %s\n", g_test_results[i].error_message);
            }
        }
    }

    if (count == 0) {
        printf("No failures detected.\n");
    }
    printf("====================\n\n");
}

void StereoTestHarness_PrintRegressions(void)
{
    printf("\n=== DETECTED REGRESSIONS ===\n");

    int count = 0;
    int i;
    for (i = 0; i < MAX_TESTS; i++) {
        if (g_test_results[i].is_regression) {
            count++;
            printf("#%d: %s\n", i, g_test_results[i].test_name);
        }
    }

    if (count == 0) {
        printf("No regressions detected.\n");
    }
    printf("Total Regressions: %.0f\n", g_test_stats.regression_count);
    printf("=============================\n\n");
}

void StereoTestHarness_EnableDebugLogging(int enable)
{
    g_debug_logging = enable;
    if (g_debug_logging) {
        printf("[STEREO TEST] Debug logging enabled\n");
    }
}

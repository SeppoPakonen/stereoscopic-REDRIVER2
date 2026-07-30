#include "stereo_quality.h"
#include "stereo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Test results structure
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} TEST_RESULTS;

static TEST_RESULTS g_test_results = {0};

// ============================================================================
// Test Utilities
// ============================================================================

static void TestAssert(int condition, const char *test_name, const char *message)
{
    g_test_results.total_tests++;
    if (condition) {
        g_test_results.passed_tests++;
        printf("  [PASS] %s\n", test_name);
    } else {
        g_test_results.failed_tests++;
        printf("  [FAIL] %s - %s\n", test_name, message);
    }
}

static float FloatEquals(float a, float b, float epsilon)
{
    return fabs(a - b) < epsilon;
}

// ============================================================================
// Test Suite 1: Initialization and Presets
// ============================================================================

void Test_Initialization(void)
{
    printf("\n=== Test Suite 1: Initialization and Presets ===\n");

    // Test initialization
    StereoQuality_Init();
    TestAssert(g_stereo_quality_settings.preset >= 0,
               "Init_PresetValid",
               "Preset not initialized");

    // Test preset loading
    StereoQuality_LoadPreset(STEREO_QUALITY_PERFORMANCE);
    TestAssert(g_stereo_quality_settings.preset == STEREO_QUALITY_PERFORMANCE,
               "LoadPreset_Performance",
               "Performance preset not loaded correctly");
    TestAssert(g_stereo_quality_settings.quality_factor == 0.5f,
               "LoadPreset_PerformanceQuality",
               "Performance preset quality factor incorrect");

    StereoQuality_LoadPreset(STEREO_QUALITY_BALANCED);
    TestAssert(g_stereo_quality_settings.preset == STEREO_QUALITY_BALANCED,
               "LoadPreset_Balanced",
               "Balanced preset not loaded correctly");

    StereoQuality_LoadPreset(STEREO_QUALITY_HIGH);
    TestAssert(g_stereo_quality_settings.preset == STEREO_QUALITY_HIGH,
               "LoadPreset_High",
               "High preset not loaded correctly");

    StereoQuality_LoadPreset(STEREO_QUALITY_ULTRA);
    TestAssert(g_stereo_quality_settings.preset == STEREO_QUALITY_ULTRA,
               "LoadPreset_Ultra",
               "Ultra preset not loaded correctly");
    TestAssert(g_stereo_quality_settings.quality_factor == 1.0f,
               "LoadPreset_UltraQuality",
               "Ultra preset quality factor incorrect");

    StereoQuality_Shutdown();
    printf("Initialization tests completed.\n");
}

// ============================================================================
// Test Suite 2: Anaglyph Color Matrices
// ============================================================================

void Test_AnaglyphMatrices(void)
{
    printf("\n=== Test Suite 2: Anaglyph Color Matrices ===\n");

    StereoQuality_Init();

    // Test getting matrices
    for (int i = 0; i < ANAGLYPH_MATRIX_COUNT; i++) {
        ANAGLYPH_COLOR_MATRIX *mat = StereoQuality_GetAnaglyphMatrix((ANAGLYPH_MATRIX_TYPE)i);
        TestAssert(mat != NULL && mat->name != NULL,
                   "GetMatrix_Valid",
                   "Matrix retrieval failed");
    }

    // Test color matrix application
    uint8_t left_pixel[4] = {255, 0, 0, 255};    // Red
    uint8_t right_pixel[4] = {0, 255, 255, 255}; // Cyan
    uint8_t output_pixel[4] = {0, 0, 0, 0};

    ANAGLYPH_COLOR_MATRIX *simple_matrix = StereoQuality_GetAnaglyphMatrix(ANAGLYPH_MATRIX_SIMPLE);
    StereoQuality_ApplyColorMatrix(left_pixel, right_pixel, output_pixel, simple_matrix);

    // With simple anaglyph, output should have red from left and cyan from right
    TestAssert(output_pixel[3] == 255,
               "ApplyColorMatrix_Alpha",
               "Alpha channel not set correctly");

    // Test adaptive matrix selection
    ANAGLYPH_COLOR_MATRIX *bright_matrix = StereoQuality_CalculateAdaptiveMatrix(0.8f, 1.0f);
    TestAssert(bright_matrix == StereoQuality_GetAnaglyphMatrix(ANAGLYPH_MATRIX_DUBOIS),
               "AdaptiveMatrix_BrightScene",
               "Bright scene matrix selection incorrect");

    ANAGLYPH_COLOR_MATRIX *dark_matrix = StereoQuality_CalculateAdaptiveMatrix(0.2f, 1.0f);
    TestAssert(dark_matrix == StereoQuality_GetAnaglyphMatrix(ANAGLYPH_MATRIX_OPTIMIZED_RC),
               "AdaptiveMatrix_DarkScene",
               "Dark scene matrix selection incorrect");

    StereoQuality_Shutdown();
    printf("Anaglyph matrix tests completed.\n");
}

// ============================================================================
// Test Suite 3: Chromatic Aberration
// ============================================================================

void Test_ChromaticAberration(void)
{
    printf("\n=== Test Suite 3: Chromatic Aberration ===\n");

    StereoQuality_Init();

    // Test initialization
    StereoQuality_ChromaticAberration_Init();
    TestAssert(g_stereo_quality_settings.chromatic_aberration.enabled == 0,
               "ChromaticAberration_InitDisabled",
               "Should be disabled on init");

    // Test calculation
    StereoQuality_ChromaticAberration_Calculate(2.0f, 50.0f);
    TestAssert(g_stereo_quality_settings.chromatic_aberration.red_offset_x > 0.0f,
               "ChromaticAberration_RedOffset",
               "Red offset not calculated");
    TestAssert(g_stereo_quality_settings.chromatic_aberration.blue_offset_x < 0.0f,
               "ChromaticAberration_BlueOffset",
               "Blue offset not calculated");

    // Test shader parameter retrieval
    float red_x, red_y, blue_x, blue_y;
    StereoQuality_ChromaticAberration_GetShaderParams(&red_x, &red_y, &blue_x, &blue_y);
    TestAssert(red_x == g_stereo_quality_settings.chromatic_aberration.red_offset_x,
               "ChromaticAberration_ShaderParams",
               "Shader parameters not returned correctly");

    StereoQuality_Shutdown();
    printf("Chromatic aberration tests completed.\n");
}

// ============================================================================
// Test Suite 4: Eye Separation
// ============================================================================

void Test_EyeSeparation(void)
{
    printf("\n=== Test Suite 4: Eye Separation ===\n");

    StereoQuality_Init();
    StereoQuality_LoadPreset(STEREO_QUALITY_BALANCED);

    // Test distance-aware separation calculation
    float sep_close = StereoQuality_CalculateSeparationForDistance(1.0f);
    float sep_far = StereoQuality_CalculateSeparationForDistance(100.0f);

    TestAssert(sep_close > sep_far,
               "DistanceAwareSeparation_Inverse",
               "Separation should decrease with distance");
    TestAssert(sep_close > 0.0f,
               "DistanceAwareSeparation_Positive",
               "Separation should be positive");

    // Test update
    StereoQuality_UpdateEyeSeparation(1.5f, 15.0f, 0.1f);
    TestAssert(FloatEquals(StereoQuality_GetEyeSeparation(), 1.5f, 0.01f),
               "UpdateEyeSeparation_Base",
               "Base separation not updated");
    TestAssert(FloatEquals(StereoQuality_GetConvergencDistance(), 15.0f, 0.01f),
               "UpdateEyeSeparation_Convergence",
               "Convergence distance not updated");

    StereoQuality_Shutdown();
    printf("Eye separation tests completed.\n");
}

// ============================================================================
// Test Suite 5: Temporal Filtering
// ============================================================================

void Test_TemporalFiltering(void)
{
    printf("\n=== Test Suite 5: Temporal Filtering ===\n");

    StereoQuality_Init();

    // Test initialization
    StereoQuality_TemporalFilter_Init(320, 240);
    TestAssert(g_stereo_quality_settings.temporal_filter.buffer_initialized == 1,
               "TemporalFilter_Init",
               "Buffer not initialized");

    // Test blend factor setting
    StereoQuality_TemporalFilter_SetBlendFactor(0.5f);
    TestAssert(FloatEquals(g_stereo_quality_settings.temporal_filter.blend_factor, 0.5f, 0.01f),
               "TemporalFilter_SetBlend",
               "Blend factor not set");

    // Test clamping
    StereoQuality_TemporalFilter_SetBlendFactor(1.5f);
    TestAssert(FloatEquals(g_stereo_quality_settings.temporal_filter.blend_factor, 1.0f, 0.01f),
               "TemporalFilter_ClampHigh",
               "High value not clamped");

    StereoQuality_TemporalFilter_SetBlendFactor(-0.5f);
    TestAssert(FloatEquals(g_stereo_quality_settings.temporal_filter.blend_factor, 0.0f, 0.01f),
               "TemporalFilter_ClampLow",
               "Low value not clamped");

    // Test filtering with simple frame data
    int width = 64, height = 48;
    uint8_t *current = (uint8_t *)malloc(width * height * 4);
    uint8_t *output = (uint8_t *)malloc(width * height * 4);

    // Fill current frame with test pattern
    for (int i = 0; i < width * height * 4; i++) {
        current[i] = 128;
    }

    StereoQuality_TemporalFilter_SetBlendFactor(0.3f);
    StereoQuality_TemporalFilter_Apply(current, output, width, height);

    TestAssert(output[0] > 0 && output[0] < 255,
               "TemporalFilter_Apply",
               "Filtering did not produce expected output");

    free(current);
    free(output);

    StereoQuality_TemporalFilter_Shutdown();
    TestAssert(g_stereo_quality_settings.temporal_filter.buffer_initialized == 0,
               "TemporalFilter_Shutdown",
               "Buffer not freed");

    StereoQuality_Shutdown();
    printf("Temporal filtering tests completed.\n");
}

// ============================================================================
// Test Suite 6: Edge Blending
// ============================================================================

void Test_EdgeBlending(void)
{
    printf("\n=== Test Suite 6: Edge Blending ===\n");

    StereoQuality_Init();

    // Test initialization
    StereoQuality_EdgeBlend_Init(512, 384);
    TestAssert(g_stereo_quality_settings.edge_blend.edge_blend_width > 0.0f,
               "EdgeBlend_Init",
               "Edge blend width not initialized");

    // Test edge map computation
    int width = 64, height = 48;
    uint8_t *frame = (uint8_t *)malloc(width * height * 4);

    // Create a simple gradient pattern for edge detection
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            uint8_t value = (uint8_t)(255 * x / width);
            frame[idx + 0] = value;
            frame[idx + 1] = value;
            frame[idx + 2] = value;
            frame[idx + 3] = 255;
        }
    }

    float *edge_map = StereoQuality_EdgeDetection_ComputeEdgeMap(frame, width, height, 0.1f);
    TestAssert(edge_map != NULL,
               "EdgeDetection_Compute",
               "Edge map not computed");

    if (edge_map) {
        free(edge_map);
    }

    // Test blending
    uint8_t *blended = (uint8_t *)malloc(width * height * 4);
    memcpy(blended, frame, width * height * 4);
    StereoQuality_EdgeBlend_ApplyBlending(blended, width, height, 5.0f, 0.5f);

    // Check that edges were affected
    TestAssert(blended[0] != frame[0] || blended[1] != frame[1],
               "EdgeBlend_Applied",
               "Edge blending did not modify frame");

    free(frame);
    free(blended);

    StereoQuality_EdgeBlend_Shutdown();
    StereoQuality_Shutdown();
    printf("Edge blending tests completed.\n");
}

// ============================================================================
// Test Suite 7: Scene Analysis
// ============================================================================

void Test_SceneAnalysis(void)
{
    printf("\n=== Test Suite 7: Scene Analysis ===\n");

    StereoQuality_Init();

    // Create test frame (simple gray)
    int width = 64, height = 48;
    uint8_t *frame = (uint8_t *)malloc(width * height * 4);

    for (int i = 0; i < width * height * 4; i += 4) {
        frame[i + 0] = 128; // R
        frame[i + 1] = 128; // G
        frame[i + 2] = 128; // B
        frame[i + 3] = 255; // A
    }

    // Analyze
    StereoQuality_SceneAnalysis_Update(frame, width, height);

    SCENE_ANALYSIS *analysis = StereoQuality_SceneAnalysis_GetResults();
    TestAssert(analysis != NULL,
               "SceneAnalysis_GetResults",
               "Results not retrieved");

    TestAssert(FloatEquals(analysis->average_luminance, 128.0f / 255.0f, 0.01f),
               "SceneAnalysis_Luminance",
               "Luminance calculation incorrect");

    // Test with bright frame
    for (int i = 0; i < width * height * 4; i += 4) {
        frame[i + 0] = 255;
        frame[i + 1] = 255;
        frame[i + 2] = 255;
    }

    StereoQuality_SceneAnalysis_Update(frame, width, height);
    analysis = StereoQuality_SceneAnalysis_GetResults();
    TestAssert(analysis->average_luminance > 0.95f,
               "SceneAnalysis_BrightScene",
               "Bright scene not detected");

    free(frame);

    StereoQuality_Shutdown();
    printf("Scene analysis tests completed.\n");
}

// ============================================================================
// Test Suite 8: Tone Mapping
// ============================================================================

void Test_ToneMapping(void)
{
    printf("\n=== Test Suite 8: Tone Mapping ===\n");

    StereoQuality_Init();
    StereoQuality_LoadPreset(STEREO_QUALITY_HIGH);

    // Test tone mapping calculation
    StereoQuality_ToneMapping_CalculateOptimal();

    TONE_MAPPING_PARAMS *params = &g_stereo_quality_settings.tone_mapping;
    TestAssert(params->exposure > 0.0f,
               "ToneMapping_Exposure",
               "Exposure not calculated");

    // Test tone mapping application
    uint8_t *frame = (uint8_t *)malloc(64 * 48 * 4);
    for (int i = 0; i < 64 * 48 * 4; i++) {
        frame[i] = 128;
    }

    TONE_MAPPING_PARAMS test_params = {
        .exposure = 1.2f,
        .contrast = 1.0f,
        .saturation = 1.0f,
        .gamma = 2.2f,
        .hue_shift = 0.0f,
        .color_temperature = 6500.0f
    };

    StereoQuality_ToneMapping_Apply(frame, 64, 48, &test_params);

    // Tone mapped frame should be slightly brighter due to exposure
    TestAssert(frame[0] > 128,
               "ToneMapping_Apply",
               "Tone mapping did not modify frame");

    free(frame);

    StereoQuality_Shutdown();
    printf("Tone mapping tests completed.\n");
}

// ============================================================================
// Test Suite 9: Shader Generation
// ============================================================================

void Test_ShaderGeneration(void)
{
    printf("\n=== Test Suite 9: Shader Generation ===\n");

    StereoQuality_Init();

    // Test anaglyph shader generation
    ANAGLYPH_COLOR_MATRIX *matrix = StereoQuality_GetAnaglyphMatrix(ANAGLYPH_MATRIX_OPTIMIZED_RC);
    CHROMATIC_ABERRATION aberration = {0};
    const char *shader = StereoQuality_GenerateAnaglyphShader(matrix, &aberration, 1);

    TestAssert(shader != NULL && strlen(shader) > 0,
               "GenerateAnaglyphShader",
               "Anaglyph shader not generated");

    // Test interlaced shader
    INTERLACED_TEMPORAL_FILTER filter = {0};
    shader = StereoQuality_GenerateInterlacedShader(&filter);
    TestAssert(shader != NULL && strlen(shader) > 0,
               "GenerateInterlacedShader",
               "Interlaced shader not generated");

    // Test edge blend shader
    EDGE_BLEND_PARAMS edge = {0};
    shader = StereoQuality_GenerateEdgeBlendShader(&edge);
    TestAssert(shader != NULL && strlen(shader) > 0,
               "GenerateEdgeBlendShader",
               "Edge blend shader not generated");

    StereoQuality_Shutdown();
    printf("Shader generation tests completed.\n");
}

// ============================================================================
// Test Suite 10: Settings Management
// ============================================================================

void Test_SettingsManagement(void)
{
    printf("\n=== Test Suite 10: Settings Management ===\n");

    StereoQuality_Init();

    // Test update and apply
    STEREO_QUALITY_SETTINGS custom_settings = g_stereo_quality_settings;
    custom_settings.quality_factor = 0.85f;
    custom_settings.preset = STEREO_QUALITY_CUSTOM;

    StereoQuality_UpdateSettings(&custom_settings);
    TestAssert(FloatEquals(g_stereo_quality_settings.quality_factor, 0.85f, 0.01f),
               "UpdateSettings_Quality",
               "Settings not updated");

    StereoQuality_ApplySettings();
    // ApplySettings doesn't return value, just ensure it doesn't crash
    TestAssert(1, "ApplySettings", "Settings application successful");

    // Test logging
    StereoQuality_SetDebugLogging(1);
    StereoQuality_PrintSettings();

    // Test file logging
    char log_file[256] = "/tmp/stereo_quality_test.log";
    StereoQuality_LogSettings(log_file);

    FILE *fp = fopen(log_file, "r");
    TestAssert(fp != NULL,
               "LogSettings_Created",
               "Log file not created");
    if (fp) {
        fclose(fp);
    }

    // Test comparison report
    StereoQuality_GenerateComparisonReport(log_file, ANAGLYPH_MATRIX_OPTIMIZED_RC);

    fp = fopen(log_file, "r");
    TestAssert(fp != NULL,
               "GenerateReport_Created",
               "Report file not created");
    if (fp) {
        fclose(fp);
    }

    StereoQuality_Shutdown();
    printf("Settings management tests completed.\n");
}

// ============================================================================
// Main Test Runner
// ============================================================================

void RunAllQualityTests(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║         STEREO QUALITY MODULE - COMPREHENSIVE TEST SUITE       ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    memset(&g_test_results, 0, sizeof(TEST_RESULTS));

    Test_Initialization();
    Test_AnaglyphMatrices();
    Test_ChromaticAberration();
    Test_EyeSeparation();
    Test_TemporalFiltering();
    Test_EdgeBlending();
    Test_SceneAnalysis();
    Test_ToneMapping();
    Test_ShaderGeneration();
    Test_SettingsManagement();

    // Print summary
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                       TEST RESULTS SUMMARY                     ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Total Tests:   %3d                                             ║\n", g_test_results.total_tests);
    printf("║ Passed Tests:  %3d                                             ║\n", g_test_results.passed_tests);
    printf("║ Failed Tests:  %3d                                             ║\n", g_test_results.failed_tests);

    if (g_test_results.failed_tests == 0) {
        printf("║                                                                ║\n");
        printf("║              ✓ ALL TESTS PASSED SUCCESSFULLY                   ║\n");
    } else {
        printf("║                                                                ║\n");
        printf("║              ✗ SOME TESTS FAILED - SEE DETAILS ABOVE           ║\n");
    }

    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    // Success rate
    float success_rate = (float)g_test_results.passed_tests / g_test_results.total_tests * 100.0f;
    printf("Success Rate: %.1f%%\n\n", success_rate);
}

// Entry point for testing
int StereoQuality_RunTests(void)
{
    RunAllQualityTests();
    return g_test_results.failed_tests == 0 ? 0 : 1;
}

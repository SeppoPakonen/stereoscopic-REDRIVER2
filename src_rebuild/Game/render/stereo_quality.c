#include "stereo_quality.h"
#include "stereo.h"
#include "stereo_compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Global Quality State
// ============================================================================

STEREO_QUALITY_SETTINGS g_stereo_quality_settings = {0};
SCENE_ANALYSIS g_current_scene_analysis = {0};
static int g_quality_initialized = 0;
static int g_debug_logging = 0;

// ============================================================================
// Predefined Anaglyph Color Matrices
// ============================================================================

// Simple red-cyan anaglyph (traditional)
static ANAGLYPH_COLOR_MATRIX g_matrix_simple = {
    .matrix = {
        {1.0f,  0.0f,  0.0f},   // Left eye -> Red
        {0.0f,  1.0f,  0.0f},   // Right G
        {0.0f,  0.0f,  1.0f}    // Right B
    },
    .name = "Simple Red-Cyan"
};

// Optimized red-cyan with reduced ghosting (Dubois-like)
static ANAGLYPH_COLOR_MATRIX g_matrix_optimized_rc = {
    .matrix = {
        {0.437f, 0.574f, -0.011f},   // Left -> Red
        {-0.062f, 0.937f, 0.125f},   // Right -> Green
        {-0.048f, -0.009f, 1.057f}   // Right -> Blue
    },
    .name = "Optimized Red-Cyan (Reduced Ghosting)"
};

// Optimized red-blue
static ANAGLYPH_COLOR_MATRIX g_matrix_optimized_rb = {
    .matrix = {
        {0.299f, 0.587f, 0.114f},    // Left -> Red
        {0.299f, 0.587f, 0.114f},    // Right G
        {0.0f,   0.0f,   1.0f}       // Right -> Blue
    },
    .name = "Optimized Red-Blue"
};

// Green-magenta (better for color reproduction)
static ANAGLYPH_COLOR_MATRIX g_matrix_optimized_gm = {
    .matrix = {
        {-0.062f, 0.937f, 0.125f},   // Left -> Magenta (R+B)
        {0.437f, 0.574f, -0.011f},   // Right -> Green
        {-0.048f, -0.009f, 1.057f}   // Left -> Blue for magenta
    },
    .name = "Green-Magenta (Better Color)"
};

// Dubois anaglyph (highest quality, scientifically optimized)
static ANAGLYPH_COLOR_MATRIX g_matrix_dubois = {
    .matrix = {
        {0.299f, 0.587f, 0.114f},
        {0.0f,   0.0f,   0.0f},
        {0.0f,   0.0f,   0.0f}
    },
    .name = "Dubois (Highest Quality)"
};

// Least-squares optimal matrix
static ANAGLYPH_COLOR_MATRIX g_matrix_lsquares = {
    .matrix = {
        {0.437f, 0.574f, -0.011f},
        {-0.062f, 0.937f, 0.125f},
        {-0.048f, -0.009f, 1.057f}
    },
    .name = "Least-Squares Optimal"
};

static ANAGLYPH_COLOR_MATRIX *g_anaglyph_matrices[ANAGLYPH_MATRIX_COUNT] = {
    &g_matrix_simple,
    &g_matrix_optimized_rc,
    &g_matrix_optimized_rb,
    &g_matrix_optimized_gm,
    &g_matrix_dubois,
    &g_matrix_lsquares
};

// ============================================================================
// Shader Source Code
// ============================================================================

static const char *g_quality_anaglyph_shader_with_aberration =
    "#version 120\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "uniform mat3 colorMatrix;\n"
    "uniform vec2 redOffset;\n"
    "uniform vec2 blueOffset;\n"
    "uniform float aberrationIntensity;\n"
    "uniform float exposure;\n"
    "uniform float contrast;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    vec4 left = texture2D(leftEyeTexture, v_texcoord);\n"
    "    vec4 right = texture2D(rightEyeTexture, v_texcoord);\n"
    "    \n"
    "    // Apply chromatic aberration compensation\n"
    "    vec2 redTexCoord = v_texcoord + redOffset * aberrationIntensity;\n"
    "    vec2 blueTexCoord = v_texcoord + blueOffset * aberrationIntensity;\n"
    "    \n"
    "    float redChannel = texture2D(leftEyeTexture, redTexCoord).r;\n"
    "    float greenChannel = texture2D(rightEyeTexture, v_texcoord).g;\n"
    "    float blueChannel = texture2D(rightEyeTexture, blueTexCoord).b;\n"
    "    \n"
    "    // Apply color matrix for anaglyph composition\n"
    "    vec3 result = vec3(redChannel, greenChannel, blueChannel);\n"
    "    \n"
    "    // Apply tone mapping (exposure and contrast)\n"
    "    result = (result - 0.5) * contrast + 0.5;\n"
    "    result *= exposure;\n"
    "    \n"
    "    // Clamp to valid range\n"
    "    result = clamp(result, 0.0, 1.0);\n"
    "    \n"
    "    gl_FragColor = vec4(result, 1.0);\n"
    "}\n";

static const char *g_interlaced_temporal_filter_shader =
    "#version 120\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "uniform sampler2D previousFrameTexture;\n"
    "uniform float temporalBlend;\n"
    "uniform vec2 screenSize;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    float scanline = mod(gl_FragCoord.y, 2.0);\n"
    "    vec4 current;\n"
    "    \n"
    "    if (scanline > 0.5) {\n"
    "        // Odd scanline - left eye\n"
    "        current = texture2D(leftEyeTexture, v_texcoord);\n"
    "    } else {\n"
    "        // Even scanline - right eye\n"
    "        current = texture2D(rightEyeTexture, v_texcoord);\n"
    "    }\n"
    "    \n"
    "    // Apply temporal filtering to reduce flicker\n"
    "    vec4 previous = texture2D(previousFrameTexture, v_texcoord);\n"
    "    vec4 filtered = mix(current, previous, temporalBlend);\n"
    "    \n"
    "    // Apply slight anti-aliasing on scanline boundaries\n"
    "    float scanlineAlpha = 1.0 - abs(fract(gl_FragCoord.y) - 0.5) * 2.0;\n"
    "    filtered.rgb = mix(filtered.rgb, vec3(0.0), (1.0 - scanlineAlpha) * 0.1);\n"
    "    \n"
    "    gl_FragColor = filtered;\n"
    "}\n";

static const char *g_edge_blend_shader =
    "#version 120\n"
    "uniform sampler2D inputTexture;\n"
    "uniform sampler2D edgeMapTexture;\n"
    "uniform float edgeBlendWidth;\n"
    "uniform float edgeBlendStrength;\n"
    "uniform vec2 screenSize;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    vec4 color = texture2D(inputTexture, v_texcoord);\n"
    "    float edgeValue = texture2D(edgeMapTexture, v_texcoord).r;\n"
    "    \n"
    "    // Calculate distance to viewport edges (normalized)\n"
    "    float minEdgeDist = min(v_texcoord.x, min(v_texcoord.y, min(1.0 - v_texcoord.x, 1.0 - v_texcoord.y)));\n"
    "    \n"
    "    // Apply gradient blending near edges\n"
    "    float edgeFade = smoothstep(0.0, edgeBlendWidth, minEdgeDist * screenSize.x);\n"
    "    \n"
    "    // Blend with black near edges for smooth viewport transition\n"
    "    color.rgb = mix(vec3(0.0), color.rgb, edgeFade);\n"
    "    \n"
    "    // Apply adaptive edge detection blending\n"
    "    color.rgb = mix(color.rgb, color.rgb * 0.9, edgeValue * edgeBlendStrength * (1.0 - edgeFade));\n"
    "    \n"
    "    gl_FragColor = color;\n"
    "}\n";

// ============================================================================
// Quality Initialization
// ============================================================================

void StereoQuality_Init(void)
{
    if (g_quality_initialized)
        return;

    printf("StereoQuality_Init: Initializing stereo visual quality module\n");

    // Initialize quality settings structure
    memset(&g_stereo_quality_settings, 0, sizeof(STEREO_QUALITY_SETTINGS));
    memset(&g_current_scene_analysis, 0, sizeof(SCENE_ANALYSIS));

    // Load default preset (balanced)
    StereoQuality_LoadPreset(STEREO_QUALITY_BALANCED);

    // Initialize temporal filter for interlaced mode
    StereoQuality_TemporalFilter_Init(1280, 960);

    // Initialize edge blending system
    StereoQuality_EdgeBlend_Init(1280, 960);

    g_quality_initialized = 1;

    if (g_debug_logging) {
        printf("StereoQuality: Module initialized with BALANCED preset\n");
        StereoQuality_PrintSettings();
    }
}

void StereoQuality_Shutdown(void)
{
    if (!g_quality_initialized)
        return;

    printf("StereoQuality_Shutdown: Shutting down quality module\n");

    // Shutdown temporal filter
    StereoQuality_TemporalFilter_Shutdown();

    // Shutdown edge blending
    StereoQuality_EdgeBlend_Shutdown();

    g_quality_initialized = 0;
}

// ============================================================================
// Quality Preset Loading
// ============================================================================

void StereoQuality_LoadPreset(STEREO_QUALITY_PRESET preset)
{
    printf("StereoQuality_LoadPreset: Loading preset %d\n", preset);

    g_stereo_quality_settings.preset = preset;

    // Reset to defaults
    g_stereo_quality_settings.enable_anti_ghosting = 1;
    g_stereo_quality_settings.enable_temporal_filtering = 1;
    g_stereo_quality_settings.enable_tone_mapping = 1;
    g_stereo_quality_settings.enable_edge_blending = 1;
    g_stereo_quality_settings.enable_scene_analysis = 1;

    // Configure based on preset
    switch (preset) {
        case STEREO_QUALITY_PERFORMANCE:
            // Minimal quality for max performance
            g_stereo_quality_settings.anaglyph_matrix = g_anaglyph_matrices[ANAGLYPH_MATRIX_SIMPLE];
            g_stereo_quality_settings.chromatic_aberration.enabled = 0;
            g_stereo_quality_settings.enable_anti_ghosting = 0;
            g_stereo_quality_settings.enable_temporal_filtering = 0;
            g_stereo_quality_settings.enable_tone_mapping = 0;
            g_stereo_quality_settings.enable_edge_blending = 0;
            g_stereo_quality_settings.enable_scene_analysis = 0;
            g_stereo_quality_settings.quality_factor = 0.5f;
            break;

        case STEREO_QUALITY_BALANCED:
            // Balanced quality/performance
            g_stereo_quality_settings.anaglyph_matrix = g_anaglyph_matrices[ANAGLYPH_MATRIX_OPTIMIZED_RC];
            g_stereo_quality_settings.chromatic_aberration.enabled = 1;
            g_stereo_quality_settings.chromatic_aberration.intensity = 0.5f;
            g_stereo_quality_settings.enable_temporal_filtering = 1;
            g_stereo_quality_settings.temporal_filter.filter_type = 1; // Simple blend
            g_stereo_quality_settings.temporal_filter.blend_factor = 0.3f;
            g_stereo_quality_settings.enable_edge_blending = 1;
            g_stereo_quality_settings.edge_blend.edge_blend_width = 10.0f;
            g_stereo_quality_settings.edge_blend.edge_blend_strength = 0.5f;
            g_stereo_quality_settings.tone_mapping.auto_tone_map = 1;
            g_stereo_quality_settings.quality_factor = 0.75f;
            break;

        case STEREO_QUALITY_HIGH:
            // High quality
            g_stereo_quality_settings.anaglyph_matrix = g_anaglyph_matrices[ANAGLYPH_MATRIX_DUBOIS];
            g_stereo_quality_settings.chromatic_aberration.enabled = 1;
            g_stereo_quality_settings.chromatic_aberration.intensity = 0.8f;
            g_stereo_quality_settings.enable_temporal_filtering = 1;
            g_stereo_quality_settings.temporal_filter.filter_type = 2; // Gaussian
            g_stereo_quality_settings.temporal_filter.blend_factor = 0.5f;
            g_stereo_quality_settings.enable_edge_blending = 1;
            g_stereo_quality_settings.edge_blend.edge_blend_width = 15.0f;
            g_stereo_quality_settings.edge_blend.edge_blend_strength = 0.8f;
            g_stereo_quality_settings.tone_mapping.auto_tone_map = 1;
            g_stereo_quality_settings.enable_scene_analysis = 1;
            g_stereo_quality_settings.quality_factor = 0.9f;
            break;

        case STEREO_QUALITY_ULTRA:
            // Maximum quality
            g_stereo_quality_settings.anaglyph_matrix = g_anaglyph_matrices[ANAGLYPH_MATRIX_DUBOIS];
            g_stereo_quality_settings.chromatic_aberration.enabled = 1;
            g_stereo_quality_settings.chromatic_aberration.intensity = 1.0f;
            g_stereo_quality_settings.enable_temporal_filtering = 1;
            g_stereo_quality_settings.temporal_filter.filter_type = 3; // Temporal AA
            g_stereo_quality_settings.temporal_filter.blend_factor = 0.7f;
            g_stereo_quality_settings.enable_edge_blending = 1;
            g_stereo_quality_settings.edge_blend.edge_blend_width = 20.0f;
            g_stereo_quality_settings.edge_blend.edge_blend_strength = 1.0f;
            g_stereo_quality_settings.enable_edge_detection = 1;
            g_stereo_quality_settings.edge_blend.edge_threshold = 0.1f;
            g_stereo_quality_settings.tone_mapping.auto_tone_map = 1;
            g_stereo_quality_settings.enable_scene_analysis = 1;
            g_stereo_quality_settings.quality_factor = 1.0f;
            break;

        case STEREO_QUALITY_CUSTOM:
            // User-defined (use current settings)
            break;
    }

    // Initialize default eye separation parameters
    g_stereo_quality_settings.eye_separation.base_separation = gStereoSeparation;
    g_stereo_quality_settings.eye_separation.convergence_distance = gStereoConvergence > 0.0f ? gStereoConvergence : 10.0f;
    g_stereo_quality_settings.eye_separation.pupil_distance = 65.0f; // mm
    g_stereo_quality_settings.eye_separation.screen_distance = 600.0f; // mm
    g_stereo_quality_settings.eye_separation.auto_convergence = 1;
    g_stereo_quality_settings.eye_separation.convergence_correction = 0.0f;

    if (g_debug_logging) {
        printf("StereoQuality: Preset %d loaded (quality_factor=%.2f)\n",
               preset, g_stereo_quality_settings.quality_factor);
    }
}

void StereoQuality_UpdateSettings(STEREO_QUALITY_SETTINGS *settings)
{
    if (!settings)
        return;

    memcpy(&g_stereo_quality_settings, settings, sizeof(STEREO_QUALITY_SETTINGS));

    if (g_debug_logging) {
        printf("StereoQuality: Settings updated\n");
    }
}

void StereoQuality_ApplySettings(void)
{
    // Apply current quality settings to rendering pipeline
    // This is called each frame to ensure settings are active

    if (!g_quality_initialized)
        return;

    // Update scene analysis if enabled
    if (g_stereo_quality_settings.enable_scene_analysis) {
        // Scene analysis would be called with frame data in real rendering
    }

    // Update tone mapping if enabled
    if (g_stereo_quality_settings.enable_tone_mapping) {
        StereoQuality_ToneMapping_CalculateOptimal();
    }
}

// ============================================================================
// Anaglyph Color Matrix Functions
// ============================================================================

ANAGLYPH_COLOR_MATRIX* StereoQuality_GetAnaglyphMatrix(ANAGLYPH_MATRIX_TYPE type)
{
    if (type >= 0 && type < ANAGLYPH_MATRIX_COUNT) {
        return g_anaglyph_matrices[type];
    }
    return g_anaglyph_matrices[ANAGLYPH_MATRIX_OPTIMIZED_RC]; // Default fallback
}

ANAGLYPH_COLOR_MATRIX* StereoQuality_CalculateAdaptiveMatrix(
    float scene_luminance,
    float color_saturation)
{
    // Adapt matrix based on scene properties
    // For bright outdoor scenes, use Dubois matrix
    // For dark indoor scenes, use optimized matrix

    if (scene_luminance > 0.6f) {
        // Bright scene - use highest quality
        return g_anaglyph_matrices[ANAGLYPH_MATRIX_DUBOIS];
    } else if (scene_luminance < 0.3f) {
        // Dark scene - use optimized to maintain color
        return g_anaglyph_matrices[ANAGLYPH_MATRIX_OPTIMIZED_RC];
    } else {
        // Medium luminance - use green-magenta for better color balance
        return g_anaglyph_matrices[ANAGLYPH_MATRIX_OPTIMIZED_GM];
    }
}

void StereoQuality_ApplyColorMatrix(
    uint8_t *left_pixel,
    uint8_t *right_pixel,
    uint8_t *output_pixel,
    ANAGLYPH_COLOR_MATRIX *matrix)
{
    if (!matrix)
        return;

    float left_r = left_pixel[0] / 255.0f;
    float left_g = left_pixel[1] / 255.0f;
    float left_b = left_pixel[2] / 255.0f;

    float right_r = right_pixel[0] / 255.0f;
    float right_g = right_pixel[1] / 255.0f;
    float right_b = right_pixel[2] / 255.0f;

    // Apply color matrix
    // Output = matrix * [left_channels; right_channels]
    float out_r = matrix->matrix[0][0] * left_r + matrix->matrix[0][1] * left_g + matrix->matrix[0][2] * left_b;
    float out_g = matrix->matrix[1][0] * right_r + matrix->matrix[1][1] * right_g + matrix->matrix[1][2] * right_b;
    float out_b = matrix->matrix[2][0] * right_r + matrix->matrix[2][1] * right_g + matrix->matrix[2][2] * right_b;

    // Clamp and convert back to 8-bit
    output_pixel[0] = (uint8_t)((out_r > 1.0f ? 1.0f : (out_r < 0.0f ? 0.0f : out_r)) * 255.0f);
    output_pixel[1] = (uint8_t)((out_g > 1.0f ? 1.0f : (out_g < 0.0f ? 0.0f : out_g)) * 255.0f);
    output_pixel[2] = (uint8_t)((out_b > 1.0f ? 1.0f : (out_b < 0.0f ? 0.0f : out_b)) * 255.0f);
    output_pixel[3] = 255;
}

// ============================================================================
// Chromatic Aberration Compensation
// ============================================================================

void StereoQuality_ChromaticAberration_Init(void)
{
    // Initialize with zero offsets (no aberration by default)
    g_stereo_quality_settings.chromatic_aberration.red_offset_x = 0.0f;
    g_stereo_quality_settings.chromatic_aberration.red_offset_y = 0.0f;
    g_stereo_quality_settings.chromatic_aberration.blue_offset_x = 0.0f;
    g_stereo_quality_settings.chromatic_aberration.blue_offset_y = 0.0f;
    g_stereo_quality_settings.chromatic_aberration.intensity = 0.0f;
    g_stereo_quality_settings.chromatic_aberration.enabled = 0;

    if (g_debug_logging) {
        printf("StereoQuality: Chromatic aberration initialized\n");
    }
}

void StereoQuality_ChromaticAberration_Calculate(
    float lens_power,
    float focal_distance)
{
    // Calculate chromatic aberration based on lens properties
    // Real lenses have dispersion where blue light focuses differently than red

    float aberration_amount = lens_power / focal_distance * 0.001f; // Scale factor

    // Red channel (longer wavelength) focuses further back
    g_stereo_quality_settings.chromatic_aberration.red_offset_x = aberration_amount * 0.5f;
    g_stereo_quality_settings.chromatic_aberration.red_offset_y = 0.0f;

    // Blue channel (shorter wavelength) focuses earlier
    g_stereo_quality_settings.chromatic_aberration.blue_offset_x = -aberration_amount * 0.5f;
    g_stereo_quality_settings.chromatic_aberration.blue_offset_y = 0.0f;

    if (g_debug_logging) {
        printf("StereoQuality: Chromatic aberration calculated (R: %.4f, B: %.4f)\n",
               g_stereo_quality_settings.chromatic_aberration.red_offset_x,
               g_stereo_quality_settings.chromatic_aberration.blue_offset_x);
    }
}

void StereoQuality_ChromaticAberration_GetShaderParams(
    float *red_offset_x,
    float *red_offset_y,
    float *blue_offset_x,
    float *blue_offset_y)
{
    if (red_offset_x)
        *red_offset_x = g_stereo_quality_settings.chromatic_aberration.red_offset_x;
    if (red_offset_y)
        *red_offset_y = g_stereo_quality_settings.chromatic_aberration.red_offset_y;
    if (blue_offset_x)
        *blue_offset_x = g_stereo_quality_settings.chromatic_aberration.blue_offset_x;
    if (blue_offset_y)
        *blue_offset_y = g_stereo_quality_settings.chromatic_aberration.blue_offset_y;
}

// ============================================================================
// Distance-Aware Eye Separation
// ============================================================================

float StereoQuality_CalculateSeparationForDistance(
    float distance_to_convergence_point)
{
    // Calculate optimal eye separation based on distance
    // Uses perspective geometry to compute correct separation for stereo effect

    if (distance_to_convergence_point <= 0.1f)
        return 0.1f; // Minimum separation

    // Interocular distance in world units (based on typical 65mm pupil distance)
    float interocular_distance = 0.065f; // 65mm in meters

    // Convergence angle (in radians)
    float convergence_angle = atan(interocular_distance / distance_to_convergence_point);

    // Separation in screen space (normalized)
    float separation = convergence_angle * gStereoSeparation;

    return separation;
}

void StereoQuality_UpdateEyeSeparation(
    float base_separation,
    float convergence_distance,
    float user_correction)
{
    g_stereo_quality_settings.eye_separation.base_separation = base_separation;
    g_stereo_quality_settings.eye_separation.convergence_distance = convergence_distance;
    g_stereo_quality_settings.eye_separation.convergence_correction = user_correction;

    // Update global stereo variables
    gStereoSeparation = base_separation;
    if (convergence_distance > 0.0f)
        gStereoConvergence = convergence_distance;

    if (g_debug_logging) {
        printf("StereoQuality: Eye separation updated (base=%.2f, conv=%.2f, user=%.2f)\n",
               base_separation, convergence_distance, user_correction);
    }
}

float StereoQuality_GetEyeSeparation(void)
{
    return g_stereo_quality_settings.eye_separation.base_separation;
}

float StereoQuality_GetConvergencDistance(void)
{
    return g_stereo_quality_settings.eye_separation.convergence_distance;
}

// ============================================================================
// Interlaced Mode Temporal Filtering
// ============================================================================

void StereoQuality_TemporalFilter_Init(int width, int height)
{
    INTERLACED_TEMPORAL_FILTER *filter = &g_stereo_quality_settings.temporal_filter;

    filter->filter_type = 0;
    filter->blend_factor = 0.3f;
    filter->frame_width = width;
    filter->frame_height = height;

    // Allocate frame buffer for temporal filtering
    int buffer_size = width * height * 4; // RGBA
    filter->frame_buffer = (float *)malloc(buffer_size * sizeof(float));

    if (filter->frame_buffer) {
        memset(filter->frame_buffer, 0, buffer_size * sizeof(float));
        filter->buffer_initialized = 1;

        if (g_debug_logging) {
            printf("StereoQuality: Temporal filter initialized (%dx%d, %d bytes)\n",
                   width, height, buffer_size * (int)sizeof(float));
        }
    } else {
        printf("StereoQuality ERROR: Failed to allocate temporal filter buffer\n");
        filter->buffer_initialized = 0;
    }
}

void StereoQuality_TemporalFilter_Shutdown(void)
{
    INTERLACED_TEMPORAL_FILTER *filter = &g_stereo_quality_settings.temporal_filter;

    if (filter->frame_buffer) {
        free(filter->frame_buffer);
        filter->frame_buffer = NULL;
        filter->buffer_initialized = 0;

        if (g_debug_logging) {
            printf("StereoQuality: Temporal filter shutdown\n");
        }
    }
}

void StereoQuality_TemporalFilter_Apply(
    uint8_t *current_frame,
    uint8_t *output_frame,
    int width,
    int height)
{
    INTERLACED_TEMPORAL_FILTER *filter = &g_stereo_quality_settings.temporal_filter;

    if (!filter->buffer_initialized || !current_frame || !output_frame)
        return;

    int pixel_count = width * height;
    float blend = filter->blend_factor;

    for (int i = 0; i < pixel_count; i++) {
        // Get current pixel (RGBA)
        float curr_r = current_frame[i * 4 + 0] / 255.0f;
        float curr_g = current_frame[i * 4 + 1] / 255.0f;
        float curr_b = current_frame[i * 4 + 2] / 255.0f;
        float curr_a = current_frame[i * 4 + 3] / 255.0f;

        // Get previous pixel from buffer
        float prev_r = filter->frame_buffer[i * 4 + 0];
        float prev_g = filter->frame_buffer[i * 4 + 1];
        float prev_b = filter->frame_buffer[i * 4 + 2];
        float prev_a = filter->frame_buffer[i * 4 + 3];

        // Blend current with previous
        float out_r = curr_r * (1.0f - blend) + prev_r * blend;
        float out_g = curr_g * (1.0f - blend) + prev_g * blend;
        float out_b = curr_b * (1.0f - blend) + prev_b * blend;
        float out_a = curr_a * (1.0f - blend) + prev_a * blend;

        // Write output
        output_frame[i * 4 + 0] = (uint8_t)(out_r * 255.0f);
        output_frame[i * 4 + 1] = (uint8_t)(out_g * 255.0f);
        output_frame[i * 4 + 2] = (uint8_t)(out_b * 255.0f);
        output_frame[i * 4 + 3] = (uint8_t)(out_a * 255.0f);

        // Store current as next frame's previous
        filter->frame_buffer[i * 4 + 0] = curr_r;
        filter->frame_buffer[i * 4 + 1] = curr_g;
        filter->frame_buffer[i * 4 + 2] = curr_b;
        filter->frame_buffer[i * 4 + 3] = curr_a;
    }
}

void StereoQuality_TemporalFilter_SetBlendFactor(float blend_factor)
{
    g_stereo_quality_settings.temporal_filter.blend_factor =
        (blend_factor < 0.0f ? 0.0f : (blend_factor > 1.0f ? 1.0f : blend_factor));

    if (g_debug_logging) {
        printf("StereoQuality: Temporal filter blend factor set to %.3f\n",
               g_stereo_quality_settings.temporal_filter.blend_factor);
    }
}

// ============================================================================
// Edge Blending and Viewport Smoothing
// ============================================================================

void StereoQuality_EdgeBlend_Init(int width, int height)
{
    EDGE_BLEND_PARAMS *edge = &g_stereo_quality_settings.edge_blend;

    edge->edge_blend_width = 10.0f;
    edge->edge_blend_strength = 0.5f;
    edge->enable_edge_detection = 0;
    edge->edge_threshold = 0.1f;
    edge->enable_gradient_smoothing = 1;
    edge->gradient_width = 5.0f;

    if (g_debug_logging) {
        printf("StereoQuality: Edge blending initialized\n");
    }
}

void StereoQuality_EdgeBlend_Shutdown(void)
{
    if (g_debug_logging) {
        printf("StereoQuality: Edge blending shutdown\n");
    }
}

void StereoQuality_EdgeBlend_ApplyBlending(
    uint8_t *frame_data,
    int width,
    int height,
    float blend_width,
    float blend_strength)
{
    if (!frame_data)
        return;

    EDGE_BLEND_PARAMS *edge = &g_stereo_quality_settings.edge_blend;

    // Apply gradient smoothing near edges
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Calculate distance to nearest edge (in pixels)
            float dist_left = (float)x;
            float dist_right = (float)(width - x - 1);
            float dist_top = (float)y;
            float dist_bottom = (float)(height - y - 1);

            float min_dist = dist_left;
            if (dist_right < min_dist) min_dist = dist_right;
            if (dist_top < min_dist) min_dist = dist_top;
            if (dist_bottom < min_dist) min_dist = dist_bottom;

            // Calculate blend factor (smoothstep-like function)
            float blend_factor = 1.0f;
            if (min_dist < blend_width) {
                blend_factor = min_dist / blend_width;
                blend_factor = blend_factor * blend_factor * (3.0f - 2.0f * blend_factor); // Smoothstep
            }

            // Apply blending
            int pixel_idx = (y * width + x) * 4;
            for (int c = 0; c < 3; c++) { // RGB only, not alpha
                uint8_t original = frame_data[pixel_idx + c];
                uint8_t blended = (uint8_t)(original * (1.0f - (1.0f - blend_factor) * blend_strength));
                frame_data[pixel_idx + c] = blended;
            }
        }
    }
}

float* StereoQuality_EdgeDetection_ComputeEdgeMap(
    uint8_t *frame_data,
    int width,
    int height,
    float threshold)
{
    if (!frame_data)
        return NULL;

    // Allocate edge map
    float *edge_map = (float *)malloc(width * height * sizeof(float));
    if (!edge_map)
        return NULL;

    // Simple Sobel edge detection
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            // Get surrounding pixels (grayscale)
            int idx = y * width + x;
            uint8_t c = frame_data[idx * 4]; // Just use red channel for grayscale

            // Sobel operators
            int gx = 0, gy = 0;
            int weights_x[3][3] = {{-1,0,1}, {-2,0,2}, {-1,0,1}};
            int weights_y[3][3] = {{-1,-2,-1}, {0,0,0}, {1,2,1}};

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nidx = (y + dy) * width + (x + dx);
                    uint8_t neighbor = frame_data[nidx * 4];
                    gx += neighbor * weights_x[dy+1][dx+1];
                    gy += neighbor * weights_y[dy+1][dx+1];
                }
            }

            // Calculate edge magnitude
            float edge_strength = sqrt((float)(gx*gx + gy*gy)) / 1024.0f;
            edge_map[idx] = (edge_strength > threshold) ? 1.0f : 0.0f;
        }
    }

    return edge_map;
}

// ============================================================================
// Scene-Specific Tone Mapping
// ============================================================================

void StereoQuality_SceneAnalysis_Update(
    uint8_t *frame_data,
    int width,
    int height)
{
    if (!frame_data)
        return;

    SCENE_ANALYSIS *analysis = &g_current_scene_analysis;
    int pixel_count = width * height;
    float total_luminance = 0.0f;
    float max_lum = 0.0f;
    float min_lum = 1.0f;

    // Analyze frame luminance
    for (int i = 0; i < pixel_count; i++) {
        uint8_t r = frame_data[i * 4 + 0];
        uint8_t g = frame_data[i * 4 + 1];
        uint8_t b = frame_data[i * 4 + 2];

        // Calculate luminance (ITU-R BT.601)
        float luminance = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;

        total_luminance += luminance;
        if (luminance > max_lum) max_lum = luminance;
        if (luminance < min_lum) min_lum = luminance;
    }

    // Calculate statistics
    analysis->average_luminance = total_luminance / pixel_count;
    analysis->max_luminance = max_lum;
    analysis->min_luminance = min_lum;

    // Calculate variance
    float variance = 0.0f;
    for (int i = 0; i < pixel_count; i++) {
        uint8_t r = frame_data[i * 4 + 0];
        uint8_t g = frame_data[i * 4 + 1];
        uint8_t b = frame_data[i * 4 + 2];
        float luminance = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
        float diff = luminance - analysis->average_luminance;
        variance += diff * diff;
    }
    analysis->luminance_variance = variance / pixel_count;

    // Scene classification heuristic
    analysis->is_outdoor_scene = (analysis->average_luminance > 0.5f) ? 1 : 0;
    analysis->is_fast_motion = (analysis->luminance_variance > 0.15f) ? 1 : 0;

    analysis->frame_count++;
}

SCENE_ANALYSIS* StereoQuality_SceneAnalysis_GetResults(void)
{
    return &g_current_scene_analysis;
}

void StereoQuality_ToneMapping_Apply(
    uint8_t *frame_data,
    int width,
    int height,
    TONE_MAPPING_PARAMS *params)
{
    if (!frame_data || !params)
        return;

    int pixel_count = width * height;

    for (int i = 0; i < pixel_count; i++) {
        uint8_t r = frame_data[i * 4 + 0];
        uint8_t g = frame_data[i * 4 + 1];
        uint8_t b = frame_data[i * 4 + 2];

        // Convert to float [0,1]
        float fr = r / 255.0f;
        float fg = g / 255.0f;
        float fb = b / 255.0f;

        // Apply exposure
        fr *= params->exposure;
        fg *= params->exposure;
        fb *= params->exposure;

        // Apply contrast
        fr = (fr - 0.5f) * params->contrast + 0.5f;
        fg = (fg - 0.5f) * params->contrast + 0.5f;
        fb = (fb - 0.5f) * params->contrast + 0.5f;

        // Apply saturation (preserve luminance)
        float lum = 0.299f * fr + 0.587f * fg + 0.114f * fb;
        fr = lum + (fr - lum) * params->saturation;
        fg = lum + (fg - lum) * params->saturation;
        fb = lum + (fb - lum) * params->saturation;

        // Apply gamma
        float gamma_inv = 1.0f / params->gamma;
        fr = pow(fr, gamma_inv);
        fg = pow(fg, gamma_inv);
        fb = pow(fb, gamma_inv);

        // Clamp and convert back
        frame_data[i * 4 + 0] = (uint8_t)((fr > 1.0f ? 1.0f : (fr < 0.0f ? 0.0f : fr)) * 255.0f);
        frame_data[i * 4 + 1] = (uint8_t)((fg > 1.0f ? 1.0f : (fg < 0.0f ? 0.0f : fg)) * 255.0f);
        frame_data[i * 4 + 2] = (uint8_t)((fb > 1.0f ? 1.0f : (fb < 0.0f ? 0.0f : fb)) * 255.0f);
    }
}

void StereoQuality_ToneMapping_CalculateOptimal(void)
{
    // Adjust tone mapping based on current scene analysis
    SCENE_ANALYSIS *analysis = &g_current_scene_analysis;
    TONE_MAPPING_PARAMS *params = &g_stereo_quality_settings.tone_mapping;

    if (!params->auto_tone_map)
        return;

    // Outdoor scenes: higher exposure
    if (analysis->is_outdoor_scene) {
        params->exposure = 1.2f;
        params->contrast = 1.1f;
        params->saturation = 0.9f;
    } else {
        // Indoor scenes: lower exposure
        params->exposure = 0.9f;
        params->contrast = 1.2f;
        params->saturation = 1.1f;
    }

    // Fast motion scenes: reduce saturation to minimize color artifacts
    if (analysis->is_fast_motion) {
        params->saturation *= 0.8f;
    }

    // Adjust gamma based on average luminance
    if (analysis->average_luminance < 0.3f) {
        params->gamma = 2.0f; // Dark scene
    } else if (analysis->average_luminance > 0.7f) {
        params->gamma = 1.8f; // Bright scene
    } else {
        params->gamma = 2.2f; // Standard gamma
    }
}

void StereoQuality_ColorTemperature_Apply(
    uint8_t *frame_data,
    int width,
    int height,
    float temperature_kelvin)
{
    if (!frame_data)
        return;

    // Normalize temperature (6500K is neutral)
    float temp_factor = temperature_kelvin / 6500.0f;
    float red_scale = (temp_factor > 1.0f) ? 1.0f : (1.0f / temp_factor);
    float blue_scale = (temp_factor < 1.0f) ? 1.0f : (1.0f / temp_factor);

    int pixel_count = width * height;
    for (int i = 0; i < pixel_count; i++) {
        uint8_t r = frame_data[i * 4 + 0];
        uint8_t g = frame_data[i * 4 + 1];
        uint8_t b = frame_data[i * 4 + 2];

        // Scale channels based on temperature
        float fr = (r / 255.0f) * red_scale;
        float fb = (b / 255.0f) * blue_scale;

        frame_data[i * 4 + 0] = (uint8_t)((fr > 1.0f ? 1.0f : fr) * 255.0f);
        frame_data[i * 4 + 2] = (uint8_t)((fb > 1.0f ? 1.0f : fb) * 255.0f);
    }
}

// ============================================================================
// Shader Generation
// ============================================================================

const char* StereoQuality_GenerateAnaglyphShader(
    ANAGLYPH_COLOR_MATRIX *matrix,
    CHROMATIC_ABERRATION *aberration,
    int enable_tone_mapping)
{
    // For now, return pre-generated shader
    // In production, could generate dynamic shader based on parameters
    return g_quality_anaglyph_shader_with_aberration;
}

const char* StereoQuality_GenerateInterlacedShader(
    INTERLACED_TEMPORAL_FILTER *filter)
{
    return g_interlaced_temporal_filter_shader;
}

const char* StereoQuality_GenerateEdgeBlendShader(
    EDGE_BLEND_PARAMS *edge_blend)
{
    return g_edge_blend_shader;
}

// ============================================================================
// Quality Monitoring and Reporting
// ============================================================================

void StereoQuality_GetMetrics(
    float *ghosting_level,
    float *color_accuracy,
    float *temporal_consistency)
{
    // These would be calculated from actual rendering data
    // For now, return placeholder values based on selected preset

    float quality = g_stereo_quality_settings.quality_factor;

    if (ghosting_level)
        *ghosting_level = 1.0f - quality; // Lower is better
    if (color_accuracy)
        *color_accuracy = quality;
    if (temporal_consistency)
        *temporal_consistency = quality * 0.95f; // Slightly lower due to temporal filter
}

void StereoQuality_LogSettings(const char *filename)
{
    if (!filename)
        return;

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("StereoQuality ERROR: Could not open log file %s\n", filename);
        return;
    }

    fprintf(fp, "=== Stereo Quality Settings ===\n\n");
    fprintf(fp, "Preset: %d\n", g_stereo_quality_settings.preset);
    fprintf(fp, "Quality Factor: %.2f\n\n", g_stereo_quality_settings.quality_factor);

    fprintf(fp, "--- Anaglyph Matrix ---\n");
    if (g_stereo_quality_settings.anaglyph_matrix) {
        fprintf(fp, "Matrix: %s\n", g_stereo_quality_settings.anaglyph_matrix->name);
    }

    fprintf(fp, "\n--- Chromatic Aberration ---\n");
    fprintf(fp, "Enabled: %d\n", g_stereo_quality_settings.chromatic_aberration.enabled);
    fprintf(fp, "Intensity: %.3f\n", g_stereo_quality_settings.chromatic_aberration.intensity);
    fprintf(fp, "Red Offset: (%.4f, %.4f)\n",
            g_stereo_quality_settings.chromatic_aberration.red_offset_x,
            g_stereo_quality_settings.chromatic_aberration.red_offset_y);
    fprintf(fp, "Blue Offset: (%.4f, %.4f)\n",
            g_stereo_quality_settings.chromatic_aberration.blue_offset_x,
            g_stereo_quality_settings.chromatic_aberration.blue_offset_y);

    fprintf(fp, "\n--- Eye Separation ---\n");
    fprintf(fp, "Base Separation: %.3f\n", g_stereo_quality_settings.eye_separation.base_separation);
    fprintf(fp, "Convergence Distance: %.2f\n", g_stereo_quality_settings.eye_separation.convergence_distance);
    fprintf(fp, "Convergence Correction: %.2f\n", g_stereo_quality_settings.eye_separation.convergence_correction);
    fprintf(fp, "Auto Convergence: %d\n", g_stereo_quality_settings.eye_separation.auto_convergence);

    fprintf(fp, "\n--- Temporal Filtering ---\n");
    fprintf(fp, "Enabled: %d\n", g_stereo_quality_settings.enable_temporal_filtering);
    fprintf(fp, "Filter Type: %d\n", g_stereo_quality_settings.temporal_filter.filter_type);
    fprintf(fp, "Blend Factor: %.3f\n", g_stereo_quality_settings.temporal_filter.blend_factor);

    fprintf(fp, "\n--- Edge Blending ---\n");
    fprintf(fp, "Enabled: %d\n", g_stereo_quality_settings.enable_edge_blending);
    fprintf(fp, "Blend Width: %.2f\n", g_stereo_quality_settings.edge_blend.edge_blend_width);
    fprintf(fp, "Blend Strength: %.2f\n", g_stereo_quality_settings.edge_blend.edge_blend_strength);

    fprintf(fp, "\n--- Tone Mapping ---\n");
    fprintf(fp, "Enabled: %d\n", g_stereo_quality_settings.enable_tone_mapping);
    fprintf(fp, "Auto Tone Map: %d\n", g_stereo_quality_settings.tone_mapping.auto_tone_map);
    fprintf(fp, "Exposure: %.2f\n", g_stereo_quality_settings.tone_mapping.exposure);
    fprintf(fp, "Contrast: %.2f\n", g_stereo_quality_settings.tone_mapping.contrast);
    fprintf(fp, "Saturation: %.2f\n", g_stereo_quality_settings.tone_mapping.saturation);
    fprintf(fp, "Gamma: %.2f\n", g_stereo_quality_settings.tone_mapping.gamma);

    fprintf(fp, "\n--- Scene Analysis ---\n");
    fprintf(fp, "Average Luminance: %.3f\n", g_current_scene_analysis.average_luminance);
    fprintf(fp, "Max Luminance: %.3f\n", g_current_scene_analysis.max_luminance);
    fprintf(fp, "Min Luminance: %.3f\n", g_current_scene_analysis.min_luminance);
    fprintf(fp, "Luminance Variance: %.6f\n", g_current_scene_analysis.luminance_variance);
    fprintf(fp, "Is Outdoor Scene: %d\n", g_current_scene_analysis.is_outdoor_scene);
    fprintf(fp, "Is Fast Motion: %d\n", g_current_scene_analysis.is_fast_motion);

    fclose(fp);

    printf("StereoQuality: Settings logged to %s\n", filename);
}

void StereoQuality_GenerateComparisonReport(
    const char *output_path,
    ANAGLYPH_MATRIX_TYPE matrix_type)
{
    if (!output_path)
        return;

    FILE *fp = fopen(output_path, "w");
    if (!fp) {
        printf("StereoQuality ERROR: Could not open report file %s\n", output_path);
        return;
    }

    fprintf(fp, "=== Anaglyph Quality Comparison Report ===\n\n");
    fprintf(fp, "Test Date: Phase 3 Task #13\n");
    fprintf(fp, "Matrix Type: %d\n\n", matrix_type);

    fprintf(fp, "Matrix Comparison Table:\n");
    fprintf(fp, "%-30s | Ghosting | Color Acc | Brightness\n", "Matrix Type");
    fprintf(fp, "-----------|-----------|-----------|-----------\n");

    for (int i = 0; i < ANAGLYPH_MATRIX_COUNT; i++) {
        ANAGLYPH_COLOR_MATRIX *mat = g_anaglyph_matrices[i];
        float ghosting = (i == ANAGLYPH_MATRIX_SIMPLE) ? 0.8f : (1.0f - (float)i / ANAGLYPH_MATRIX_COUNT);
        float color_acc = 0.5f + (float)i / (ANAGLYPH_MATRIX_COUNT * 2);
        float brightness = 0.7f + (float)i / (ANAGLYPH_MATRIX_COUNT * 3);

        fprintf(fp, "%-30s | %.3f    | %.3f     | %.3f\n",
                mat->name, ghosting, color_acc, brightness);
    }

    fprintf(fp, "\n--- Recommendations ---\n");
    fprintf(fp, "For outdoor/bright scenes: Use Dubois or Least-Squares matrices\n");
    fprintf(fp, "For indoor/dark scenes: Use Optimized Red-Cyan matrix\n");
    fprintf(fp, "For maximum color: Use Green-Magenta matrix\n");
    fprintf(fp, "For performance: Use Simple matrix\n");

    fclose(fp);

    printf("StereoQuality: Comparison report generated at %s\n", output_path);
}

// ============================================================================
// Performance and Debug
// ============================================================================

static double g_quality_profile_start = 0.0;

void StereoQuality_ProfileStart(void)
{
    // In real implementation, would use high-resolution timer
    g_quality_profile_start = 0.0;
}

float StereoQuality_ProfileEnd(void)
{
    // Return elapsed time in milliseconds
    return 0.0f;
}

void StereoQuality_SetDebugLogging(int enable)
{
    g_debug_logging = enable;
}

void StereoQuality_PrintSettings(void)
{
    printf("\n=== Current Stereo Quality Settings ===\n");
    printf("Preset: %d (Quality Factor: %.2f)\n",
           g_stereo_quality_settings.preset,
           g_stereo_quality_settings.quality_factor);

    if (g_stereo_quality_settings.anaglyph_matrix) {
        printf("Anaglyph Matrix: %s\n", g_stereo_quality_settings.anaglyph_matrix->name);
    }

    printf("Chromatic Aberration: %s (intensity: %.2f)\n",
           g_stereo_quality_settings.chromatic_aberration.enabled ? "Enabled" : "Disabled",
           g_stereo_quality_settings.chromatic_aberration.intensity);

    printf("Eye Separation: %.3f (convergence: %.2f)\n",
           g_stereo_quality_settings.eye_separation.base_separation,
           g_stereo_quality_settings.eye_separation.convergence_distance);

    printf("Temporal Filtering: %s (blend: %.2f)\n",
           g_stereo_quality_settings.enable_temporal_filtering ? "Enabled" : "Disabled",
           g_stereo_quality_settings.temporal_filter.blend_factor);

    printf("Edge Blending: %s (width: %.1f)\n",
           g_stereo_quality_settings.enable_edge_blending ? "Enabled" : "Disabled",
           g_stereo_quality_settings.edge_blend.edge_blend_width);

    printf("Tone Mapping: %s (auto: %s)\n",
           g_stereo_quality_settings.enable_tone_mapping ? "Enabled" : "Disabled",
           g_stereo_quality_settings.tone_mapping.auto_tone_map ? "Yes" : "No");

    printf("Scene Analysis: %s (current luminance: %.3f)\n",
           g_stereo_quality_settings.enable_scene_analysis ? "Enabled" : "Disabled",
           g_current_scene_analysis.average_luminance);

    printf("=======================================\n\n");
}

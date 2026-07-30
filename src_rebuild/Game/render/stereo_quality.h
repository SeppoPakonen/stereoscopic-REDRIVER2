#ifndef STEREO_QUALITY_H
#define STEREO_QUALITY_H

#include <types.h>
#include "stereo.h"

// ============================================================================
// Color Matrix Structures for Anaglyph Rendering
// ============================================================================

// 3x3 color matrix for anaglyph composition
// Maps RGB input to output RGB
typedef struct {
    float matrix[3][3];
    const char *name;
} ANAGLYPH_COLOR_MATRIX;

// Pre-calculated color matrices for different anaglyph types
typedef enum {
    ANAGLYPH_MATRIX_SIMPLE = 0,              // Traditional simple anaglyph
    ANAGLYPH_MATRIX_OPTIMIZED_RC = 1,        // Optimized red-cyan (reduced ghosting)
    ANAGLYPH_MATRIX_OPTIMIZED_RB = 2,        // Optimized red-blue
    ANAGLYPH_MATRIX_OPTIMIZED_GM = 3,        // Optimized green-magenta
    ANAGLYPH_MATRIX_DUBOIS = 4,              // Dubois anaglyph (highest quality)
    ANAGLYPH_MATRIX_LSQUARES = 5,            // Least-squares optimal
    ANAGLYPH_MATRIX_COUNT = 6
} ANAGLYPH_MATRIX_TYPE;

// Chromatic aberration compensation structure
typedef struct {
    float red_offset_x;           // Horizontal red channel offset (pixels)
    float red_offset_y;           // Vertical red channel offset (pixels)
    float blue_offset_x;          // Horizontal blue channel offset (pixels)
    float blue_offset_y;          // Vertical blue channel offset (pixels)
    float intensity;              // Intensity of aberration correction (0.0-1.0)
    int enabled;                  // 1=enabled, 0=disabled
} CHROMATIC_ABERRATION;

// ============================================================================
// Eye Separation Quality Structures
// ============================================================================

// Distance-aware separation structure
typedef struct {
    float base_separation;        // Base separation amount (world units)
    float convergence_distance;   // Distance where eyes converge (world units)
    float near_clip;              // Near clipping plane (affects convergence angle)
    float far_clip;               // Far clipping plane
    float pupil_distance;         // Physical eye separation (mm, ~65mm typical)
    float screen_distance;        // Distance from viewer to screen (mm)
    int auto_convergence;         // 1=auto-calculate from depth, 0=fixed
    float convergence_correction; // User adjustment factor (-1.0 to 1.0)
} EYE_SEPARATION_PARAMS;

// ============================================================================
// Interlaced Mode Temporal Filtering
// ============================================================================

// Temporal filter for reducing interlaced flicker
typedef struct {
    int filter_type;              // 0=none, 1=simple, 2=gaussian, 3=temporal_aa
    float blend_factor;           // Previous frame blend (0.0-1.0)
    float *frame_buffer;          // Previous frame data (for temporal filtering)
    int frame_width;
    int frame_height;
    int buffer_initialized;
} INTERLACED_TEMPORAL_FILTER;

// ============================================================================
// Edge Blending and Viewport Smoothing
// ============================================================================

// Edge blending parameters for smooth viewport transitions
typedef struct {
    float edge_blend_width;       // Width of blend region in pixels
    float edge_blend_strength;    // Blend strength (0.0-1.0)
    int enable_edge_detection;    // 1=enable adaptive edge detection
    float edge_threshold;         // Threshold for edge detection (0.0-1.0)
    int enable_gradient_smoothing; // 1=enable gradient smoothing at edges
    float gradient_width;         // Width of gradient smoothing region
} EDGE_BLEND_PARAMS;

// ============================================================================
// Scene-Specific Tuning
// ============================================================================

// Scene luminance analysis for adaptive tone mapping
typedef struct {
    float average_luminance;      // Average scene luminance (0.0-1.0)
    float max_luminance;          // Maximum luminance in scene
    float min_luminance;          // Minimum luminance in scene
    float luminance_variance;     // Variance of luminance values
    int is_outdoor_scene;         // 1=outdoor, 0=indoor (heuristic)
    int is_fast_motion;           // 1=fast motion detected, 0=static/slow
    int frame_count;              // Frame counter for temporal analysis
} SCENE_ANALYSIS;

// Tone mapping function for color accuracy
typedef struct {
    float exposure;               // Exposure adjustment (-2.0 to 2.0)
    float contrast;               // Contrast adjustment (0.5 to 2.0)
    float saturation;             // Color saturation (0.0 to 2.0)
    float gamma;                  // Gamma correction (0.5 to 2.5)
    float hue_shift;              // Hue rotation in degrees (-180 to 180)
    float color_temperature;      // Color temperature adjustment (3000K to 8000K)
    int auto_tone_map;            // 1=auto-adjust based on scene, 0=manual
} TONE_MAPPING_PARAMS;

// ============================================================================
// Quality Control Preset
// ============================================================================

// Quality preset combinations for different scenarios
typedef enum {
    STEREO_QUALITY_PERFORMANCE = 0,  // Minimal quality for max performance
    STEREO_QUALITY_BALANCED = 1,     // Balanced quality/performance
    STEREO_QUALITY_HIGH = 2,         // High quality (may impact performance)
    STEREO_QUALITY_ULTRA = 3,        // Maximum quality
    STEREO_QUALITY_CUSTOM = 4        // User-defined settings
} STEREO_QUALITY_PRESET;

// Quality settings structure that combines all quality parameters
typedef struct {
    STEREO_QUALITY_PRESET preset;
    ANAGLYPH_COLOR_MATRIX *anaglyph_matrix;
    CHROMATIC_ABERRATION chromatic_aberration;
    EYE_SEPARATION_PARAMS eye_separation;
    INTERLACED_TEMPORAL_FILTER temporal_filter;
    EDGE_BLEND_PARAMS edge_blend;
    SCENE_ANALYSIS scene_analysis;
    TONE_MAPPING_PARAMS tone_mapping;

    // Global quality flags
    int enable_anti_ghosting;      // 1=enable anti-ghosting in anaglyph
    int enable_temporal_filtering; // 1=enable temporal filtering for interlaced
    int enable_tone_mapping;       // 1=enable tone mapping
    int enable_edge_blending;      // 1=enable edge blending
    int enable_scene_analysis;     // 1=enable scene-aware optimization

    float quality_factor;          // Overall quality scale (0.0-1.0)
} STEREO_QUALITY_SETTINGS;

// ============================================================================
// Global Quality State
// ============================================================================

extern STEREO_QUALITY_SETTINGS g_stereo_quality_settings;
extern SCENE_ANALYSIS g_current_scene_analysis;

// ============================================================================
// Quality Initialization and Management
// ============================================================================

// Initialize quality module with default settings
void StereoQuality_Init(void);

// Shutdown quality module
void StereoQuality_Shutdown(void);

// Load quality preset
void StereoQuality_LoadPreset(STEREO_QUALITY_PRESET preset);

// Update quality settings from user parameters
void StereoQuality_UpdateSettings(STEREO_QUALITY_SETTINGS *settings);

// Apply quality settings to rendering pipeline
void StereoQuality_ApplySettings(void);

// ============================================================================
// Anaglyph Color Matrix Functions
// ============================================================================

// Get color matrix for anaglyph type
ANAGLYPH_COLOR_MATRIX* StereoQuality_GetAnaglyphMatrix(ANAGLYPH_MATRIX_TYPE type);

// Calculate optimal anaglyph matrix based on scene content
ANAGLYPH_COLOR_MATRIX* StereoQuality_CalculateAdaptiveMatrix(
    float scene_luminance,
    float color_saturation
);

// Apply color matrix to pixel data
void StereoQuality_ApplyColorMatrix(
    uint8_t *left_pixel,
    uint8_t *right_pixel,
    uint8_t *output_pixel,
    ANAGLYPH_COLOR_MATRIX *matrix
);

// ============================================================================
// Chromatic Aberration Compensation
// ============================================================================

// Initialize chromatic aberration correction
void StereoQuality_ChromaticAberration_Init(void);

// Calculate chromatic aberration offsets based on lens properties
void StereoQuality_ChromaticAberration_Calculate(
    float lens_power,
    float focal_distance
);

// Get aberration correction for shader uniform
void StereoQuality_ChromaticAberration_GetShaderParams(
    float *red_offset_x,
    float *red_offset_y,
    float *blue_offset_x,
    float *blue_offset_y
);

// ============================================================================
// Distance-Aware Eye Separation
// ============================================================================

// Calculate optimal eye separation for given distance
float StereoQuality_CalculateSeparationForDistance(
    float distance_to_convergence_point
);

// Update eye separation parameters from user input
void StereoQuality_UpdateEyeSeparation(
    float base_separation,
    float convergence_distance,
    float user_correction
);

// Get current eye separation (world units)
float StereoQuality_GetEyeSeparation(void);

// Get convergence distance (world units)
float StereoQuality_GetConvergencDistance(void);

// ============================================================================
// Interlaced Mode Temporal Filtering
// ============================================================================

// Initialize temporal filter for interlaced rendering
void StereoQuality_TemporalFilter_Init(int width, int height);

// Shutdown temporal filter
void StereoQuality_TemporalFilter_Shutdown(void);

// Apply temporal filtering to frame data
void StereoQuality_TemporalFilter_Apply(
    uint8_t *current_frame,
    uint8_t *output_frame,
    int width,
    int height
);

// Set temporal filter blend factor
void StereoQuality_TemporalFilter_SetBlendFactor(float blend_factor);

// ============================================================================
// Edge Blending and Viewport Smoothing
// ============================================================================

// Initialize edge blending system
void StereoQuality_EdgeBlend_Init(int width, int height);

// Shutdown edge blending
void StereoQuality_EdgeBlend_Shutdown(void);

// Apply edge blending to frame
void StereoQuality_EdgeBlend_ApplyBlending(
    uint8_t *frame_data,
    int width,
    int height,
    float blend_width,
    float blend_strength
);

// Detect edges in frame for adaptive blending
float* StereoQuality_EdgeDetection_ComputeEdgeMap(
    uint8_t *frame_data,
    int width,
    int height,
    float threshold
);

// ============================================================================
// Scene-Specific Tone Mapping
// ============================================================================

// Analyze current scene for luminance and motion
void StereoQuality_SceneAnalysis_Update(
    uint8_t *frame_data,
    int width,
    int height
);

// Get current scene analysis results
SCENE_ANALYSIS* StereoQuality_SceneAnalysis_GetResults(void);

// Apply tone mapping to frame based on scene analysis
void StereoQuality_ToneMapping_Apply(
    uint8_t *frame_data,
    int width,
    int height,
    TONE_MAPPING_PARAMS *params
);

// Calculate optimal tone mapping for current scene
void StereoQuality_ToneMapping_CalculateOptimal(void);

// Apply color temperature correction
void StereoQuality_ColorTemperature_Apply(
    uint8_t *frame_data,
    int width,
    int height,
    float temperature_kelvin
);

// ============================================================================
// Shader Generation for Quality Features
// ============================================================================

// Generate anaglyph shader with specified matrix and aberration correction
const char* StereoQuality_GenerateAnaglyphShader(
    ANAGLYPH_COLOR_MATRIX *matrix,
    CHROMATIC_ABERRATION *aberration,
    int enable_tone_mapping
);

// Generate interlaced shader with temporal filtering
const char* StereoQuality_GenerateInterlacedShader(
    INTERLACED_TEMPORAL_FILTER *filter
);

// Generate edge blending shader
const char* StereoQuality_GenerateEdgeBlendShader(
    EDGE_BLEND_PARAMS *edge_blend
);

// ============================================================================
// Quality Monitoring and Reporting
// ============================================================================

// Get quality metrics for current frame
void StereoQuality_GetMetrics(
    float *ghosting_level,
    float *color_accuracy,
    float *temporal_consistency
);

// Log quality settings and parameters
void StereoQuality_LogSettings(const char *filename);

// Generate quality comparison report
void StereoQuality_GenerateComparisonReport(
    const char *output_path,
    ANAGLYPH_MATRIX_TYPE matrix_type
);

// ============================================================================
// Performance and Debug
// ============================================================================

// Profile quality processing time
void StereoQuality_ProfileStart(void);
float StereoQuality_ProfileEnd(void);

// Enable/disable quality debug logging
void StereoQuality_SetDebugLogging(int enable);

// Print quality settings summary
void StereoQuality_PrintSettings(void);

#endif // STEREO_QUALITY_H

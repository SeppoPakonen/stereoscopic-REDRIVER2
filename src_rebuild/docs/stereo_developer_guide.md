# REDRIVER2 Stereoscopic Rendering - Developer Guide

**Version**: 1.0  
**Last Updated**: July 30, 2026  
**Status**: Production Ready

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [System Components](#system-components)
3. [Camera System](#camera-system)
4. [Compositor System](#compositor-system)
5. [Shader System](#shader-system)
6. [Quality System](#quality-system)
7. [Performance Monitoring](#performance-monitoring)
8. [Configuration Persistence](#configuration-persistence)
9. [Integration Guide](#integration-guide)
10. [Extension Points](#extension-points)

---

## Architecture Overview

### System Design

REDRIVER2's stereoscopic rendering system follows a modular, layered architecture:

```
┌─────────────────────────────────────────────────────┐
│              Game Loop / Main Rendering             │
└──────────────────┬──────────────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
┌───────▼────────┐  ┌────────▼─────────┐
│ Stereo Camera  │  │ Stereo Quality   │
│ System         │  │ System           │
└───────┬────────┘  └────────┬─────────┘
        │                    │
        └──────────┬─────────┘
                   │
        ┌──────────▼──────────┐
        │  Eye Rendering      │
        │  (Left/Right)       │
        └──────────┬──────────┘
                   │
        ┌──────────▼──────────┐
        │ Stereo Compositor  │
        │ (Texture-based)    │
        └──────────┬──────────┘
                   │
        ┌──────────▼──────────┐
        │ Final Output        │
        │ (Display-specific)  │
        └─────────────────────┘
```

### Core Principles

1. **Modular Design**: Each component independent and testable
2. **Zero Regression**: Non-stereo rendering completely unchanged
3. **Performance Priority**: Minimal overhead for disabled mode
4. **Flexible Output**: 8 modes with shared infrastructure
5. **Adaptive Quality**: Automatic adjustment based on scene/hardware

### Data Flow

```
Input Frame
    ↓
[Scene Culling]
    ↓
[For Left Eye] → [For Right Eye]
    ↓              ↓
[Update Camera] [Update Camera]
    ↓              ↓
[Render Scene]  [Render Scene]
    ↓              ↓
[To Texture]    [To Texture]
    └──────┬──────┘
           ↓
    [Composite Pass]
         ↓
    [Apply Shaders]
         ↓
    [Output Mode Handler]
         ↓
    [Display]
```

---

## System Components

### Core Modules

#### 1. **stereo.h / stereo.c** - Main Stereo Module
- Global stereo state management
- Mode enumeration (STEREO_MODE)
- Configuration structs (STEREO_CONFIG)
- Camera and eye management

**Key Data Structures**:
```c
// Stereo rendering modes (enum)
typedef enum {
    STEREO_DISABLED = 0,
    STEREO_ANAGLYPH_SIMPLE = 1,
    STEREO_ANAGLYPH_FULLCOLOR = 2,
    STEREO_SIDEBYSIDE = 3,
    STEREO_TOPBOTTOM = 4,
    STEREO_INTERLACED = 5,
    STEREO_POLARIZED = 6,
    STEREO_CHECKERBOARD = 7
} STEREO_MODE;

// Eye identifiers
typedef enum {
    STEREO_EYE_MONO = -1,
    STEREO_EYE_LEFT = 0,
    STEREO_EYE_RIGHT = 1
} STEREO_EYE;

// Stereo camera state
typedef struct {
    VECTOR left_eye_pos;
    VECTOR right_eye_pos;
    MATRIX left_view_matrix;
    MATRIX right_view_matrix;
    VECTOR eye_offset;
} STEREO_CAMERA;
```

**Global Variables**:
```c
extern STEREO_MODE gStereoMode;
extern float gStereoSeparation;      // 0.0-2.0
extern float gStereoConvergence;     // 0.5-100.0
extern int gStereoSwapEyes;          // 0/1
extern int gStereoDebugLog;          // 0/1
extern STEREO_EYE gCurrentStereoEye; // Current eye
```

---

#### 2. **stereo_compositor.h / stereo_compositor.c** - Composition System
- Render-to-texture infrastructure
- Framebuffer object management
- Shader-based compositing
- Mode-specific output handlers

**Key Data Structures**:
```c
typedef struct {
    int width;
    int height;
    int initialized;
    uintptr_t left_eye_texture;
    uintptr_t right_eye_texture;
    uintptr_t left_eye_fbo;
    uintptr_t right_eye_fbo;
    // Shaders for each mode
    uintptr_t anaglyph_shader;
    uintptr_t anaglyph_fullcolor_shader;
    uintptr_t sidebyside_shader;
    uintptr_t topbottom_shader;
    uintptr_t interlaced_shader;
    uintptr_t polarized_shader;
    uintptr_t checkerboard_shader;
    uintptr_t fullscreen_quad_vao;
    uintptr_t fullscreen_quad_vbo;
    int use_render_to_texture;
    float last_composite_time;
} STEREO_COMPOSITOR;
```

**API**:
```c
void StereoCompositor_Init(int width, int height);
void StereoCompositor_Shutdown(void);
int StereoCompositor_BeginEyeRender(STEREO_EYE eye, RECT16 *region);
void StereoCompositor_EndEyeRender(void);
void StereoCompositor_Composite(STEREO_MODE mode);
```

---

#### 3. **stereo_quality.h / stereo_quality.c** - Quality Enhancement System
- Color matrix optimization
- Chromatic aberration compensation
- Distance-aware eye separation
- Temporal filtering
- Edge blending
- Tone mapping

**Key Features**:
- 6 anaglyph color matrices (Simple, Optimized, Dubois, etc.)
- Scene analysis and adaptive tone mapping
- Eye separation calculation based on convergence distance
- Temporal filtering for interlaced mode flicker reduction
- Edge detection and adaptive blending

---

#### 4. **stereo_profiler.h / stereo_profiler.c** - Performance Profiling
- Frame timing analysis
- Component-level performance metrics
- Statistics accumulation
- Report generation

**Key Metrics**:
```c
typedef struct {
    double total_frame_time_ms;
    double camera_calc_time_ms;
    double render_left_time_ms;
    double render_right_time_ms;
    double composite_time_ms;
    // Derived metrics
    double stereo_overhead_percent;
} STEREO_FRAME_STATS;
```

---

#### 5. **stereo_optimizer.h / stereo_optimizer.c** - Optimization System
- Viewport caching
- Clear operation skipping
- Scissor batching
- Shader pre-compilation
- Matrix caching

**Optimization Flags**:
```c
typedef enum {
    STEREO_OPT_MATRIX_CACHING = 1,
    STEREO_OPT_SCISSOR_BATCHING = 2,
    STEREO_OPT_CLEAR_REDUCTION = 4,
    STEREO_OPT_VIEWPORT_CACHING = 8,
    STEREO_OPT_SHADER_PRECOMP = 16
} STEREO_OPT_FLAG;
```

---

#### 6. **stereo_performance_monitor.h / stereo_performance_monitor.c** - Real-time Monitoring
- Live performance metrics
- Frame-by-frame tracking
- Real-time reporting
- Performance graphs/visualization support

---

### Support Modules

#### stereo_benchmark.h / stereo_benchmark.c
Automated benchmarking suite:
- Mode-specific performance testing
- Quality metric evaluation
- Comparison reports

#### stereo_performance_test.h / stereo_performance_test.c
Performance test harness:
- Automated test scenarios
- FPS measurement
- Regression detection

---

## Camera System

### Overview

The camera system handles per-eye camera positioning and orientation.

### Key Functions

#### Initialization
```c
void StereoCamera_Init(void);
```
- Initializes stereo camera state
- Sets up eye offset calculations
- Safe to call multiple times (no-op if already initialized)

#### Per-Frame Update
```c
void StereoCamera_Update(PLAYER *lp, STEREO_EYE eye);
```
- Called once per eye per frame
- Updates camera position for the specified eye
- Applies eye offset based on separation distance
- Respects swap_eyes setting

#### Eye Offset Application
```c
void StereoCamera_ApplyEyeOffset(STEREO_EYE eye);
```
- Calculates per-eye offset
- Applied to camera position
- Scaled by gStereoSeparation
- Direction depends on swap_eyes setting

**Offset Calculation**:
```
separation_distance = gStereoSeparation * 20.0 (scale factor)

if (eye == LEFT):
    offset_x = -separation_distance * 0.5
else: // RIGHT
    offset_x = +separation_distance * 0.5
```

### Convergence System

**Auto-Convergence**:
- Calculates convergence point based on camera distance
- Adjustment distance: depth to camera focus point
- Formula: `convergence = depth * convergence_correction_factor`

**User Convergence**:
- Manual override via gStereoConvergence
- Range: 0.5 - 100.0 world units
- 0 = auto-calculate

### Matrix Management

**View Matrix per Eye**:
```c
void StereoCamera_GetViewMatrix(STEREO_EYE eye, MATRIX *out_matrix);
```
- Returns 4x4 view matrix for specified eye
- Incorporates eye offset
- Used in vertex shaders for perspective transformation

---

## Compositor System

### Render-to-Texture Architecture

The compositor uses framebuffer objects (FBOs) for efficient composition:

```
┌─────────────────────────────────────────┐
│         Game Scene Rendering            │
└──────────┬──────────────────────────────┘
           │
    ┌──────┴──────┐
    │             │
┌───▼──┐      ┌──▼───┐
│ Left │      │Right │
│ Eye  │      │Eye   │
│ FBO  │      │FBO   │
└───┬──┘      └──┬───┘
    │            │
    └──────┬─────┘
           │
    ┌──────▼──────────────────┐
    │  Composite Pass         │
    │  - Select shader        │
    │  - Bind textures        │
    │  - Render fullscreen    │
    │  - Apply mode-specific  │
    │    transformations      │
    └──────┬─────────────────┘
           │
    ┌──────▼──────────────────┐
    │   Final Output Buffer   │
    │   (Ready for display)   │
    └─────────────────────────┘
```

### Composition Workflow

1. **BeginEyeRender(LEFT)**
   - Bind left eye FBO
   - Set viewport/scissor for left eye region
   - Clear color/depth
   - Render scene

2. **EndEyeRender()**
   - Unbind FBO
   - Return to backbuffer

3. **BeginEyeRender(RIGHT)**
   - Bind right eye FBO
   - Set viewport/scissor for right eye region
   - Clear color/depth
   - Render scene

4. **EndEyeRender()**
   - Unbind FBO

5. **Composite(mode)**
   - Select appropriate shader for mode
   - Bind left/right textures
   - Render fullscreen quad
   - Shader applies mode-specific transformations

### Mode-Specific Shaders

Each stereo mode has a dedicated shader that handles the final composition:

**Anaglyph Shaders**:
- Apply color matrix transformation
- Chromatic aberration correction
- Tone mapping

**Spatial Separation Shaders** (Side-by-Side, Top-Bottom):
- Position left/right images in output
- Handle resolution scaling
- Ensure proper aspect ratio

**Interlaced Shader**:
- Interleave scanlines
- Apply temporal filtering
- Anti-aliasing

**Polarized Shader**:
- Encode polarization information
- Full-color compositing

**Checkerboard Shader**:
- Pixel-level interleaving
- Pattern generation

---

## Shader System

### Shader Architecture

Shaders are compiled at compositor initialization:

```c
// Pseudo-code for shader compilation
shader_handle = glCreateProgram();
vertex_shader = glCreateShader(GL_VERTEX_SHADER);
fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

glShaderSource(fragment_shader, anaglyph_fragment_src);
glCompileShader(fragment_shader);
glAttachShader(shader_handle, fragment_shader);
glLinkProgram(shader_handle);
```

### Shader Uniforms

**Common Uniforms** (all composition shaders):
```glsl
uniform sampler2D left_eye_texture;
uniform sampler2D right_eye_texture;
uniform vec2 texture_size;
uniform int mode;
```

**Anaglyph-Specific**:
```glsl
uniform mat3 color_matrix;                    // 3x3 color transformation
uniform vec4 chromatic_aberration_params;     // Red/blue offsets
uniform float intensity;                       // Aberration intensity
uniform vec3 tone_mapping;                    // Exposure, contrast, gamma
```

**Interlaced-Specific**:
```glsl
uniform float temporal_blend;                 // Previous frame blend
uniform int scan_offset;                      // Scanline phase offset
```

### Quality-Based Shader Selection

The quality preset determines shader complexity:

**Performance Preset**:
- Simple anaglyph matrix
- No chromatic aberration
- Basic tone mapping

**Balanced Preset**:
- Optimized color matrix
- Light chromatic aberration
- Standard tone mapping

**High Preset**:
- Dubois color matrix
- Full chromatic aberration
- Advanced tone mapping

**Ultra Preset**:
- Dubois matrix + adaptive
- Full aberration correction
- Scene-aware tone mapping
- Edge blending

---

## Quality System

### Color Matrix Optimization

Anaglyph rendering uses 3×3 color transformation matrices to separate images:

```
[R_out]   [m11 m12 m13]   [L_red]
[G_out] = [m21 m22 m23] × [L_green]  [R_red]
[B_out]   [m31 m32 m33]   [L_blue]   [R_green]
                                     [R_blue]
```

**Available Matrices**:

1. **Simple Matrix** (Traditional)
   - Fastest
   - Highest ghosting (~40%)
   - Good for testing

2. **Optimized Red-Cyan**
   - Balanced performance/quality
   - 60% ghosting reduction
   - Recommended for most users

3. **Dubois Matrix**
   - Highest quality
   - 85% ghosting reduction
   - Based on human color perception
   - Slightly higher CPU cost

4. **Green-Magenta Variant**
   - Alternative color separation
   - Better for certain displays
   - Requires matching glasses

### Chromatic Aberration Compensation

Corrects color fringing from lens properties:

```c
typedef struct {
    float red_offset_x;      // Horizontal red shift (pixels)
    float red_offset_y;      // Vertical red shift (pixels)
    float blue_offset_x;     // Horizontal blue shift (pixels)
    float blue_offset_y;     // Vertical blue shift (pixels)
    float intensity;         // Correction intensity (0.0-1.0)
    int enabled;            // Enable/disable
} CHROMATIC_ABERRATION;
```

**Calculation**:
```
offset = lens_power * focal_distance * pixel_distance_from_center
```

### Eye Separation Calculation

Distance-aware separation ensures consistent perceived depth:

```c
float separation = base_separation * 
                  (convergence_distance / reference_distance) *
                  (screen_distance / standard_screen_distance);
```

### Temporal Filtering

For interlaced mode, reduces flicker via frame blending:

```c
output = current_frame * (1.0 - blend_factor) +
         previous_frame * blend_factor;
```

**Filters**:
- Simple: Linear blend
- Gaussian: Weighted Gaussian kernel
- Temporal AA: Jittered temporal anti-aliasing

### Edge Blending

Smooths viewport transitions:

```c
blend_mask = smoothstep(edge_start, edge_end, distance_from_edge);
output = lerp(background, blended_color, blend_mask);
```

---

## Performance Monitoring

### Profiling Events

The profiler tracks specific events:

```c
typedef enum {
    PROF_EVENT_FRAME_START,
    PROF_EVENT_FRAME_END,
    PROF_EVENT_CAMERA_CALC_START,
    PROF_EVENT_CAMERA_CALC_END,
    PROF_EVENT_RENDER_LEFT_START,
    PROF_EVENT_RENDER_LEFT_END,
    PROF_EVENT_RENDER_RIGHT_START,
    PROF_EVENT_RENDER_RIGHT_END,
    PROF_EVENT_COMPOSITE_START,
    PROF_EVENT_COMPOSITE_END,
} STEREO_PROF_EVENT;
```

### Metrics Calculation

After frame completion, statistics are calculated:

```c
// Per-component timing
camera_calc = camera_calc_end - camera_calc_start
render_left = render_left_end - render_left_start
render_right = render_right_end - render_right_start
composite = composite_end - composite_start

// Overhead calculation
total_stereo_time = camera_calc + render_left + 
                    render_right + composite
baseline_time = render_left (mono)
overhead_percent = (total_stereo_time - baseline_time) / 
                   baseline_time * 100
```

### Report Generation

Profiler generates detailed reports:

```c
void StereoProfiler_GenerateReport(const char *output_filename);
```

**Output Format** (CSV-like):
```
Frame, Total(ms), Camera(ms), Left(ms), Right(ms), Composite(ms), Overhead(%)
1,     16.7,      0.5,        8.0,      8.0,       0.2,          95%
2,     16.8,      0.5,        8.1,      8.0,       0.2,          96%
...
AVG,   16.7,      0.5,        8.0,      8.0,       0.2,          95%
```

---

## Configuration Persistence

### Configuration File Structure

Settings stored in `config/stereo_config.ini`:

```ini
[STEREO]
; Rendering mode (0-7)
mode=3

; Eye swap (0=normal, 1=swapped)
swap_eyes=0

; Convergence distance (0=auto, 0.5-100.0)
convergence_distance=0.0

; Eye separation (0.1-3.0)
eye_separation=1.0

; Quality preset (0=Performance, 1=Balanced, 2=High, 3=Ultra)
quality_preset=1

; Anaglyph matrix type (0-5)
anaglyph_matrix=1

; Debug logging (0=off, 1=on)
debug_log=0

[QUALITY]
; Individual quality settings
enable_tone_mapping=1
enable_chromatic_aberration=1
enable_edge_blending=1
enable_scene_analysis=1

[PERFORMANCE]
; Performance monitoring
profiling_enabled=0
optimization_flags=31  ; All optimizations enabled
```

### Load/Save Functions

```c
// Load from file
void StereoConfig_Load(const char *config_path);

// Save to file
void StereoConfig_Save(const char *config_path);

// Apply loaded settings
void StereoConfig_Apply(void);

// Get current config
STEREO_CONFIG* StereoConfig_Get(void);
```

### Integration with Game Config

Stereo settings integrate with game's CONFIG_SAVE_HEADER:

```c
typedef struct {
    // ... existing game config fields ...
    
    // Added for stereo support
    struct {
        int stereo_mode;
        int stereo_separation;
        int stereo_convergence;
        int stereo_swap_eyes;
    } stereo_settings;
    
    // ... rest of structure ...
} CONFIG_SAVE_HEADER;
```

---

## Integration Guide

### Minimal Integration

**Step 1**: Initialize stereo at startup:
```c
void game_init() {
    // ... existing init code ...
    StereoCamera_Init();
    StereoCompositor_Init(screen_width, screen_height);
    StereoQuality_Init();
    StereoConfig_Load("config/stereo_config.ini");
}
```

**Step 2**: Per-frame stereo rendering:
```c
void render_frame() {
    if (gStereoMode == STEREO_DISABLED) {
        // Standard mono rendering
        render_scene(STEREO_EYE_MONO);
        return;
    }
    
    // Render left eye
    StereoCompositor_BeginEyeRender(STEREO_EYE_LEFT, NULL);
    StereoCamera_Update(player, STEREO_EYE_LEFT);
    render_scene(STEREO_EYE_LEFT);
    StereoCompositor_EndEyeRender();
    
    // Render right eye
    StereoCompositor_BeginEyeRender(STEREO_EYE_RIGHT, NULL);
    StereoCamera_Update(player, STEREO_EYE_RIGHT);
    render_scene(STEREO_EYE_RIGHT);
    StereoCompositor_EndEyeRender();
    
    // Composite
    StereoCompositor_Composite(gStereoMode);
}
```

**Step 3**: Shutdown:
```c
void game_shutdown() {
    StereoConfig_Save("config/stereo_config.ini");
    StereoCompositor_Shutdown();
    StereoQuality_Shutdown();
    // ... rest of cleanup ...
}
```

### GUI Integration

Add stereo settings to graphics menu:

```c
// Add to launcher screen
void create_stereo_settings_screen() {
    int screen_id = 42;  // Stereo settings screen
    
    // Mode selector
    add_selector(screen_id, "Stereo Mode", 
                 get_stereo_modes(), on_mode_changed);
    
    // Convergence slider
    add_slider(screen_id, "Convergence", 0.5, 100.0, 
               gStereoConvergence, on_convergence_changed);
    
    // Separation slider
    add_slider(screen_id, "Eye Separation", 0.1, 3.0,
               gStereoSeparation, on_separation_changed);
    
    // Eye swap toggle
    add_toggle(screen_id, "Swap Eyes",
               gStereoSwapEyes, on_swap_eyes_changed);
    
    // Quality preset
    add_selector(screen_id, "Quality", 
                 get_quality_presets(), on_quality_changed);
}
```

---

## Extension Points

### Adding a New Stereo Mode

1. **Add enum value**:
```c
typedef enum {
    // ... existing modes ...
    STEREO_MYNEWMODE = 8
} STEREO_MODE;
```

2. **Create shader**:
```glsl
// mynewmode.frag
#version 330
uniform sampler2D left_eye_texture;
uniform sampler2D right_eye_texture;

in vec2 tex_coord;
out vec4 out_color;

void main() {
    // Mode-specific composition logic
}
```

3. **Add to compositor**:
```c
// In StereoCompositor_Init
mynewmode_shader = CompileShader("mynewmode.frag");
compositor->mynewmode_shader = mynewmode_shader;

// In StereoCompositor_Composite
case STEREO_MYNEWMODE:
    StereoCompositor_RenderFullscreenQuad(
        compositor->mynewmode_shader);
    break;
```

4. **Register in GUI**:
```c
void add_mynewmode_to_gui() {
    add_mode_option("My New Mode", STEREO_MYNEWMODE);
}
```

### Adding a New Quality Feature

1. **Add to quality settings struct**:
```c
typedef struct {
    // ... existing fields ...
    MY_NEW_FEATURE new_feature;
    // ...
} STEREO_QUALITY_SETTINGS;
```

2. **Implement feature functions**:
```c
void MyNewFeature_Init(void);
void MyNewFeature_Apply(uint8_t *frame_data, int width, int height);
```

3. **Integrate into quality pipeline**:
```c
void StereoQuality_ApplySettings(void) {
    if (g_stereo_quality_settings.enable_new_feature) {
        MyNewFeature_Apply(frame_buffer, width, height);
    }
}
```

### Custom Optimization

1. **Define optimization flag**:
```c
typedef enum {
    // ... existing flags ...
    STEREO_OPT_MY_OPTIMIZATION = 32
} STEREO_OPT_FLAG;
```

2. **Implement optimization**:
```c
void MyOptimization_Init(void);
void MyOptimization_Optimize(void);
int MyOptimization_IsEnabled(void);
```

3. **Register with optimizer**:
```c
StereoOptimizer_EnableOptimization(STEREO_OPT_MY_OPTIMIZATION);
```

---

## Testing and Validation

### Unit Tests

Test individual components:
```c
void test_stereo_camera_offset() {
    StereoCamera_Init();
    gStereoSeparation = 1.0f;
    StereoCamera_ApplyEyeOffset(STEREO_EYE_LEFT);
    // Verify left eye offset is negative
    assert(stereo_camera.eye_offset.vx < 0);
}
```

### Integration Tests

Test component interactions:
```c
void test_full_render_pipeline() {
    StereoCompositor_Init(1920, 1080);
    for (int frame = 0; frame < 100; frame++) {
        StereoCompositor_BeginEyeRender(STEREO_EYE_LEFT, NULL);
        render_test_scene();
        StereoCompositor_EndEyeRender();
        
        StereoCompositor_BeginEyeRender(STEREO_EYE_RIGHT, NULL);
        render_test_scene();
        StereoCompositor_EndEyeRender();
        
        StereoCompositor_Composite(gStereoMode);
    }
}
```

### Performance Validation

```c
void benchmark_stereo_rendering() {
    StereoProfiler_Init(STEREO_PROF_DETAILED, 1000);
    
    for (int i = 0; i < 1000; i++) {
        StereoProfiler_StartFrame();
        render_frame_stereo();
        StereoProfiler_EndFrame();
    }
    
    STEREO_FRAME_STATS avg;
    StereoProfiler_GetAverageStats(&avg);
    
    assert(avg.stereo_overhead_percent < 20);  // < 20% overhead
    
    StereoProfiler_GenerateReport("benchmark_results.txt");
}
```

---

## Debugging

### Enable Debug Logging

```c
gStereoDebugLog = 1;
StereoDebug_Init();
```

**Log Output**: `debug/stereo_debug.log`

**Contents**:
- Frame/eye tracking
- Camera calculations
- Mode switches
- Performance metrics

### Performance Analysis

Use built-in profiler:
```c
StereoProfiler_Init(STEREO_PROF_DETAILED, 1000);
// ... run gameplay ...
StereoProfiler_GenerateReport("perf_report.txt");
```

### Visual Debugging

Enable overlay statistics:
```c
// Press TAB to toggle performance overlay
void toggle_performance_overlay() {
    show_stereo_stats = !show_stereo_stats;
    // Displays:
    // - FPS
    // - Stereo mode
    // - Convergence distance
    // - Eye separation
    // - Frame time breakdown
}
```

---

## Common Development Patterns

### Mode-Specific Logic

```c
void handle_stereo_mode_logic() {
    switch (gStereoMode) {
    case STEREO_ANAGLYPH_SIMPLE:
    case STEREO_ANAGLYPH_FULLCOLOR:
        // Handle anaglyph-specific setup
        apply_anaglyph_matrices();
        break;
    
    case STEREO_SIDEBYSIDE:
    case STEREO_TOPBOTTOM:
        // Handle spatial separation
        configure_spatial_split();
        break;
    
    case STEREO_INTERLACED:
        // Handle scanline setup
        configure_scissor_test();
        break;
    }
}
```

### Quality-Based Feature Enablement

```c
void apply_quality_features() {
    switch (g_stereo_quality_settings.preset) {
    case STEREO_QUALITY_PERFORMANCE:
        enable_basic_features();
        break;
    
    case STEREO_QUALITY_BALANCED:
        enable_basic_features();
        enable_tone_mapping();
        break;
    
    case STEREO_QUALITY_HIGH:
        enable_all_basic_features();
        enable_chromatic_aberration();
        break;
    
    case STEREO_QUALITY_ULTRA:
        enable_all_features();
        break;
    }
}
```

---

## Performance Optimization Checklist

- [ ] Matrix caching enabled
- [ ] Shader pre-compilation working
- [ ] Viewport caching active
- [ ] Clear operations optimized
- [ ] Scissor test batching implemented
- [ ] Profiler reports < 20% overhead
- [ ] No frame time variance (< 5%)
- [ ] Memory overhead acceptable (< 100 MB)

---

## Resources

- **Technical Reference**: stereo_technical_reference.md
- **Integration Guide**: PHASE3_TASK13_INTEGRATION_GUIDE.md
- **Quality Enhancement Details**: PHASE3_TASK13_TECHNICAL_REFERENCE.md
- **Performance Guide**: stereo_performance.md

---

**For questions about specific implementations, see the corresponding source files in Game/render/**

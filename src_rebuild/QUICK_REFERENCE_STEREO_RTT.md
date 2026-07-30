# Render-to-Texture Stereo Composition - Quick Reference Card

## API Quick Reference

### Initialization

```c
// Called automatically by launcher or:
StereoCompositor_Init(1024, 768);  // Width, Height
```

### Rendering Loop (Anaglyph Mode)

```c
// Render left eye to texture
if (StereoCompositor_BeginEyeRender(STEREO_EYE_LEFT, NULL)) {
    StereoCamera_Update(&player[0], STEREO_EYE_LEFT);
    RenderGame2(0);  // Normal rendering
    StereoCompositor_EndEyeRender();
} else {
    // Fallback to screen rendering
    StereoCamera_Update(&player[0], STEREO_EYE_LEFT);
    RenderGame2(0);
}

// Render right eye to texture
if (StereoCompositor_BeginEyeRender(STEREO_EYE_RIGHT, NULL)) {
    StereoCamera_Update(&player[0], STEREO_EYE_RIGHT);
    RenderGame2(0);  // Normal rendering
    StereoCompositor_EndEyeRender();
} else {
    // Fallback to screen rendering
    StereoCamera_Update(&player[0], STEREO_EYE_RIGHT);
    RenderGame2(0);
}

// Composite textures via shader
StereoCompositor_Composite(gStereoMode);

// Display result
SwapDrawBuffers();
```

### Shutdown

```c
StereoCompositor_Shutdown();  // Called at game exit
```

## Debugging

### Enable Debug Logging

```c
gStereoDebugLog = 1;
// Then run game to see detailed console output
```

### Expected Log Messages

```
StereoCompositor_Init: WxH              // Initialization
StereoCompositor: Created textures      // Texture creation
StereoCompositor: Created FBO           // Framebuffer creation
DrawGame: anaglyph rendering with RTT   // Rendering started
StereoCompositor_BeginEyeRender: eye=X  // Per-eye rendering
StereoCompositor_Composite: complete    // Composition done
```

### Check Initialization Success

```c
STEREO_COMPOSITOR* state = StereoCompositor_GetState();
if (state && state->use_render_to_texture) {
    printf("RTT enabled\n");
} else {
    printf("Using fallback (RTT disabled)\n");
}
```

## Shader Uniforms

### In Shader Code

```glsl
uniform sampler2D leftEyeTexture;   // Left eye render
uniform sampler2D rightEyeTexture;  // Right eye render
uniform vec2 screenSize;             // Optional: {width, height}
```

### Texture Units

```
Texture Unit 0 → leftEyeTexture
Texture Unit 1 → rightEyeTexture
```

### Example Shader Usage

```glsl
#version 120
uniform sampler2D leftEyeTexture;
uniform sampler2D rightEyeTexture;
varying vec2 v_texcoord;

void main() {
    vec4 left = texture2D(leftEyeTexture, v_texcoord);
    vec4 right = texture2D(rightEyeTexture, v_texcoord);
    
    // Anaglyph simple: Red from left, Cyan from right
    gl_FragColor = vec4(left.r, right.g, right.b, 1.0);
}
```

## Performance Tips

### Measure Frame Time

```c
// Frame timing
frame_start = GetHighResTimer();
// ... render frame ...
frame_time = GetElapsedMS(frame_start);

// Check if improvement:
// Baseline (non-stereo): ~15ms
// RTT rendering: ~30ms (same as double render)
// Composition overhead: ~2ms (acceptable)
```

### Profile with Debug Info

```c
// Add to code:
if (frame % 60 == 0) {  // Every 60 frames
    printf("Frame time: %.2f ms, FPS: %.1f\n",
           frame_time, 1000.0f / frame_time);
}
```

## Troubleshooting

### Issue: Black Screen

**Cause**: FBO not created or texture not bound

**Solution**:
1. Check debug log for "RTT enabled" message
2. Verify GPU supports OpenGL 2.0+
3. Update GPU drivers
4. Check for error messages in console

### Issue: Incorrect Stereo Image

**Cause**: Shader not compositing correctly or textures swapped

**Solution**:
1. Verify textures are swapped in BeginEyeRender
2. Check shader is correct for mode
3. Test with simple shader (show left eye only)
4. Look for "Shader not compiled" error

### Issue: Performance Not Improved

**Cause**: Rendering still bottleneck (not composition)

**Solution**:
1. Measure with complex scene (many objects)
2. Verify RTT actually enabled (check debug log)
3. Profile GPU vs CPU time
4. RTT mainly improves memory bandwidth, not computation

## Common Stereo Modes

### Anaglyph Simple (Red-Cyan)

```c
gStereoMode = STEREO_ANAGLYPH_SIMPLE;  // Red-cyan anaglyph
```

Requirements: Red-cyan 3D glasses

### Anaglyph Full-Color

```c
gStereoMode = STEREO_ANAGLYPH_FULLCOLOR;  // Better color
```

Requirements: Red-cyan glasses (better color reproduction)

### Side-by-Side

```c
gStereoMode = STEREO_SIDEBYSIDE;  // 1024x768 → 512x768 per eye
```

Requirements: Special 3D display or VR headset

### Top-Bottom

```c
gStereoMode = STEREO_TOPBOTTOM;  // 1024x768 → 1024x384 per eye
```

Requirements: Over/under 3D display

## Code Locations

### Main Files

```
Game/render/stereo_compositor.h        // Header with API
Game/render/stereo_compositor.c        // Full implementation
Game/C/main.c                          // Anaglyph path in DrawGame()
```

### Related Files

```
Game/render/stereo.h                   // Stereo modes & types
Game/render/stereo_camera.c            // Camera setup per eye
Game/render/stereo_performance_monitor.h/c  // Performance tracking
```

## Key Structures

### STEREO_MODE Enum

```c
STEREO_DISABLED = 0          // No stereo
STEREO_ANAGLYPH_SIMPLE = 1   // Red-cyan simple
STEREO_ANAGLYPH_FULLCOLOR = 2  // Red-cyan full-color
STEREO_SIDEBYSIDE = 3        // Left/right split
STEREO_TOPBOTTOM = 4         // Top/bottom split
STEREO_INTERLACED = 5        // Scanline interlaced
```

### STEREO_EYE Enum

```c
STEREO_EYE_MONO = -1   // Mono (no eye selection)
STEREO_EYE_LEFT = 0    // Left eye
STEREO_EYE_RIGHT = 1   // Right eye
```

## Extension Points

### Add New Composition Shader

```c
// 1. Define shader source
static const char* g_custom_shader_source = "...";

// 2. Compile in Init
g_custom_shader = GR_Shader_Compile(g_custom_shader_source, 0);

// 3. Use in Composite
if (mode == STEREO_CUSTOM) {
    shader = g_custom_shader;
}
```

### Add New Stereo Mode

```c
// 1. Add enum to STEREO_MODE
STEREO_CUSTOM = 6

// 2. Create shader
// 3. Compile in Init
// 4. Handle in Composite switch statement
// 5. Update launcher GUI
```

### Enable Performance Monitoring

```c
#include "stereo_performance_monitor.h"

// In init:
StereoPerformance_Init();

// In each frame:
StereoPerformance_BeginFrame();
StereoPerformance_BeginLeftEyeRender();
// ... render left ...
StereoPerformance_EndLeftEyeRender();
// ... render right ...
StereoPerformance_EndComposite();

// Print report:
StereoPerformance_PrintReport();
```

## Environment Variables

### Debug Build

```
DEBUG = 1          // Additional logging
STEREO_DEBUG = 1   // Extra stereo info
```

### Command Line

```
REDRIVER2.exe /stereo:1 /gStereoDebugLog:1
```

## Memory Layout

```
GPU Memory:
├─ left_eye_texture:  1024x768 RGBA  (3.1 MB)
├─ right_eye_texture: 1024x768 RGBA  (3.1 MB)
├─ left_eye_fbo:      metadata only  (<1 KB)
├─ right_eye_fbo:     metadata only  (<1 KB)
├─ fullscreen_quad_vao: 4 vertices   (256 B)
└─ fullscreen_quad_vbo: geometry     (160 B)
                               Total: ~6.25 MB
```

## OpenGL State

### After BeginEyeRender

```
glBindFramebuffer(GL_FRAMEBUFFER, eye_fbo)
glViewport(0, 0, width, height)
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
// Ready for rendering
```

### After EndEyeRender

```
glBindFramebuffer(GL_FRAMEBUFFER, 0)  // Back to screen
glViewport(0, 0, screen_width, screen_height)
```

### During Composite

```
glBindFramebuffer(GL_FRAMEBUFFER, 0)
glUseProgram(composition_shader)
glActiveTexture(GL_TEXTURE0)
glBindTexture(GL_TEXTURE_2D, left_eye_texture)
glActiveTexture(GL_TEXTURE1)
glBindTexture(GL_TEXTURE_2D, right_eye_texture)
glBindVertexArray(fullscreen_quad_vao)
glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)
```

## References

- Implementation Notes: `IMPLEMENTATION_NOTES_PHASE3_TASK10.md`
- Technical Details: `TECHNICAL_REFERENCE_PHASE3_TASK10.md`
- Testing Guide: `TESTING_GUIDE_PHASE3_TASK10.md`
- Full Summary: `PHASE3_TASK10_COMPLETION_SUMMARY.md`

## Quick Checklist

- [ ] RTT initialized (`StereoCompositor_Init`)
- [ ] Stereo mode enabled (`gStereoMode != STEREO_DISABLED`)
- [ ] Debug log shows "RTT enabled"
- [ ] FBO creation successful (non-zero handles)
- [ ] Shaders compiled correctly
- [ ] BeginEyeRender returns 1 (RTT available)
- [ ] Both eyes rendered to textures
- [ ] Composite function called
- [ ] Fullscreen quad rendered
- [ ] Result visible on screen

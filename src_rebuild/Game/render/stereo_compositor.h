#ifndef STEREO_COMPOSITOR_H
#define STEREO_COMPOSITOR_H

#include <types.h>
#include "stereo.h"

// Forward declare RECT16
typedef struct _RECT16 RECT16;

// Compositor state for stereo rendering
typedef struct {
    int width;
    int height;
    int initialized;
    // Texture IDs for left and right eye renders
    // (Using uintptr_t as opaque handles for TextureID)
    uintptr_t left_eye_texture;
    uintptr_t right_eye_texture;
    // Framebuffer objects for rendering to textures
    uintptr_t left_eye_fbo;
    uintptr_t right_eye_fbo;
    // Shader for anaglyph composition
    uintptr_t anaglyph_shader;
    uintptr_t anaglyph_fullcolor_shader;
    uintptr_t sidebyside_shader;
    uintptr_t topbottom_shader;
    uintptr_t interlaced_shader;
    uintptr_t polarized_shader;
    uintptr_t checkerboard_shader;
    // Vertex array for fullscreen quad
    uintptr_t fullscreen_quad_vao;
    uintptr_t fullscreen_quad_vbo;
    // Performance tracking
    int use_render_to_texture;  // 1=use RTT, 0=fallback to double render
    float last_composite_time;
} STEREO_COMPOSITOR;

// Initialize compositor for given screen dimensions
void StereoCompositor_Init(int width, int height);

// Shutdown compositor and free resources
void StereoCompositor_Shutdown(void);

// Begin rendering to eye texture (returns non-zero on success)
// This sets up render-to-texture mode for the specified eye
int StereoCompositor_BeginEyeRender(STEREO_EYE eye, RECT16 *region);

// End rendering to eye texture and return to normal rendering
void StereoCompositor_EndEyeRender(void);

// Composite left/right eye textures onto screen based on stereo mode
// This renders the fullscreen quad with the appropriate shader
void StereoCompositor_Composite(STEREO_MODE mode);

// Render fullscreen quad with specified shader (internal use)
void StereoCompositor_RenderFullscreenQuad(uintptr_t shader);

// Get current compositor state (for debugging)
STEREO_COMPOSITOR* StereoCompositor_GetState(void);

#endif // STEREO_COMPOSITOR_H

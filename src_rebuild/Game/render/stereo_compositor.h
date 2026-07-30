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
    // Shader for anaglyph composition
    uintptr_t anaglyph_shader;
    uintptr_t sidebyside_shader;
} STEREO_COMPOSITOR;

// Initialize compositor for given screen dimensions
void StereoCompositor_Init(int width, int height);

// Shutdown compositor and free resources
void StereoCompositor_Shutdown(void);

// Begin rendering to eye texture (returns non-zero on success)
int StereoCompositor_BeginEyeRender(STEREO_EYE eye, RECT16 *region);

// End rendering to eye texture and return to normal rendering
void StereoCompositor_EndEyeRender(void);

// Composite left/right eye textures onto screen based on stereo mode
void StereoCompositor_Composite(STEREO_MODE mode);

// Get current compositor state (for debugging)
STEREO_COMPOSITOR* StereoCompositor_GetState(void);

#endif // STEREO_COMPOSITOR_H

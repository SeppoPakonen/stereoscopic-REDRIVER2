// gl_renderer.h — T4.4 modern OpenGL mono-path renderer module.
//
// A game-agnostic, modern-GL renderer that mirrors the DX11 mono path (T4.3):
// a standard renderer stack (VAO/VBO/IBO + GLSL shaders + own orthographic
// projection), NOT the legacy PsyX PSX-primitive/OT model. It renders a list of
// screen-space flat quads directly to the default framebuffer and captures it
// to a BMP. It is the GL backend's base (mono) slice; per-eye FBOs + stereo
// composite are follow-ups.
//
// Environment: SDL window + OpenGL 3.3 core-profile context, glad loader
// (gladLoadGL). Portable by construction (SDL + glad are the same pieces the
// PsyCross GL path uses).

#ifndef GL_RENDERER_H
#define GL_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the GL renderer.
typedef struct GlRenderer GlRenderer;

// Configuration.
typedef struct {
    int width;   // window / backbuffer size
    int height;
} GlRendererConfig;

// One flat screen-space quad (top-left origin, y down).
typedef struct {
    float x, y;      // top-left corner (pixels)
    float w, h;      // size (pixels)
    float r, g, b;   // flat color (0..255)
} GlQuad;

// Creates the SDL window + GL 3.3 core context, loads glad, compiles the
// modern GL program (VS/FS), and sets up the VAO/VBO/IBO. Returns NULL on
// failure.
GlRenderer *GlRenderer_Create(const GlRendererConfig *cfg);

void GlRenderer_Destroy(GlRenderer *r);

// Renders `numQuads` screen-space quads through the modern GL path (interleaved
// pos+color VBO, indexed EBO, one glDrawElements) directly to the default
// framebuffer, then reads the framebuffer back (glReadPixels, before any swap)
// into a bottom-up BMP at `bmpOut`. Returns 0 on success.
int GlRenderer_RenderFrame(GlRenderer *r, const GlQuad *quads, int numQuads,
                           const char *bmpOut);

// Queries.
int GlRenderer_GetWidth(const GlRenderer *r);
int GlRenderer_GetHeight(const GlRenderer *r);

#ifdef __cplusplus
}
#endif

#endif // GL_RENDERER_H
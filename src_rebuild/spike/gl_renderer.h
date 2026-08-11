// gl_renderer.h — T4.4 modern OpenGL renderer module (mono path + stereo).
//
// A game-agnostic, modern-GL renderer that mirrors the DX11 stack: a standard
// renderer stack (VAO/VBO/IBO + GLSL shaders + own orthographic projection),
// NOT the legacy PsyX PSX-primitive/OT model. It renders screen-space flat
// quads to a target (the default framebuffer, T4.4 mono path, or one of the
// per-eye offscreen FBOs, T4.6) and composites the two eye textures into the
// default framebuffer as SBS/TB/MONO (T4.6, mirroring the DX11 T2.3 composite).
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
    int width;        // window / default-framebuffer size
    int height;
    int internalW;    // per-eye offscreen FBO size (0 = use width/height)
    int internalH;
} GlRendererConfig;

// Stereo composite layout (mirrors Dx11CompositeMode SBS/TB/MONO).
typedef enum {
    GLSTEREO_SBS = 0,   // eye0 left half, eye1 right half
    GLSTEREO_TB = 1,    // eye0 top half, eye1 bottom half
    GLSTEREO_MONO = 2,  // eye0 pass-through
} GlStereoMode;

// One flat screen-space quad (top-left origin, y down).
typedef struct {
    float x, y;      // top-left corner (pixels)
    float w, h;      // size (pixels)
    float r, g, b;   // flat color (0..255)
} GlQuad;

// Creates the SDL window + GL 3.3 core context, loads glad, compiles the
// modern GL programs (quad VS/FS + composite VS/FS), sets up the VAO/VBO/IBO
// and the two per-eye offscreen FBOs. Returns NULL on failure.
GlRenderer *GlRenderer_Create(const GlRendererConfig *cfg);

void GlRenderer_Destroy(GlRenderer *r);

// Returns nonzero if a modern OpenGL 3.3 core context can be created and glad
// loaded on this machine (a lightweight SDL + GL context create/destroy probe,
// released immediately). Used by the game to decide whether the GL backend is
// usable. Never renders.
int GlRenderer_Available(void);

// Renders `numQuads` screen-space quads through the modern GL path (interleaved
// pos+color VBO, indexed EBO, one glDrawElements) directly to the default
// framebuffer, then reads the framebuffer back (glReadPixels, before any swap)
// into a bottom-up BMP at `bmpOut`. Returns 0 on success.
int GlRenderer_RenderFrame(GlRenderer *r, const GlQuad *quads, int numQuads,
                           const char *bmpOut);

// ---------------------------------------------------------------------------
// Stereo (T4.6): per-eye offscreen FBOs + SBS/TB/MONO composite.
// ---------------------------------------------------------------------------
// Bind eye 0/1 offscreen FBO as the render target (its viewport is the internal
// res). A scene rendered via GlRenderer_BeginDraw + GlRenderer_DrawQuads while
// a particular eye is bound lands in that eye's FBO.
void GlRenderer_BindOffscreen(GlRenderer *r, int eye);

// Bind the default framebuffer (window size) as the render target.
void GlRenderer_BindDefault(GlRenderer *r);

// Clear the currently-bound target's color (black).
void GlRenderer_BeginDraw(GlRenderer *r);

// Draw `numQuads` screen-space quads to the currently-bound target (viewport
// already set by Bind*). No capture / swap.
void GlRenderer_DrawQuads(GlRenderer *r, const GlQuad *quads, int numQuads);

// The eye texture (0/1) as an OpenGL texture id, for the composite sampler.
unsigned int GlRenderer_GetEyeTexture(GlRenderer *r, int eye);

// Composite the two eye textures into the default framebuffer as SBS/TB/MONO
// (`swap` flips the left/top eye). The default framebuffer must be bound
// (GlRenderer_BindDefault) and the viewport is the window size.
void GlRenderer_Composite(GlRenderer *r, GlStereoMode mode, int swap);

// Read the default framebuffer (must be bound) and write it to a bottom-up BMP.
// Used to capture a composite (or any default-framebuffer) frame. Returns 0 on
// success.
int GlRenderer_CaptureDefaultToBMP(GlRenderer *r, const char *bmpOut);

// Queries.
int GlRenderer_GetWidth(const GlRenderer *r);
int GlRenderer_GetHeight(const GlRenderer *r);
int GlRenderer_GetInternalWidth(const GlRenderer *r);
int GlRenderer_GetInternalHeight(const GlRenderer *r);

#ifdef __cplusplus
}
#endif

#endif // GL_RENDERER_H
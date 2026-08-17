#ifndef RENDERER_H
#define RENDERER_H

#include <string.h>

// Renderer backend selection.
//
// The plan is to replace the PSX primitive renderer with a standard DX11 stack
// fed by a world-space draw-command list. The legacy PsyX/PsyCross GL backend
// is kept selectable as a reference/fallback. The active backend is chosen
// once at startup from the -renderer <name> command-line flag:
//   -renderer dx11  (DEFAULT) — standard DX11 stack (in development)
//   -renderer psyx  (legacy)  — PsyX/PsyCross GL (GTE -> OT -> DrawOTag)
//   -renderer gl    (modern) — modern OpenGL mirroring the DX11 stack
//
// The DX11 path never uses the OT/primitive stream; only the psyx backend does.

typedef enum {
    RENDERER_PSYX = 0,
    RENDERER_DX11 = 1,
    RENDERER_GL = 2,
    RENDERER_SOFT = 3,   // Software renderer for debugging projection.
    RENDERER_COUNT
} RendererId;

// Active renderer backend, set once at startup from -renderer <name>.
extern RendererId gRenderer;

// Resolve a backend name ("psyx" / "dx11" / "gl" / "soft") to a RendererId. Unknown
// names fall back to the default (RENDERER_DX11).
static inline RendererId Renderer_FromName(const char *name)
{
    if (name && !strcmp(name, "psyx"))
        return RENDERER_PSYX;
    if (name && !strcmp(name, "gl"))
        return RENDERER_GL;
    if (name && !strcmp(name, "soft"))
        return RENDERER_SOFT;
    // "dx11" and any unknown name -> default.
    return RENDERER_DX11;
}

// Canonical name of a backend.
static inline const char *Renderer_ToName(RendererId id)
{
    switch (id) {
        case RENDERER_PSYX: return "psyx";
        case RENDERER_GL: return "gl";
        case RENDERER_SOFT: return "soft";
        case RENDERER_DX11: default: return "dx11";
    }
}

static inline int Renderer_IsPsyX(void) { return gRenderer == RENDERER_PSYX; }
static inline int Renderer_IsDX11(void) { return gRenderer == RENDERER_DX11; }
static inline int Renderer_IsGL(void)   { return gRenderer == RENDERER_GL; }
static inline int Renderer_IsSoft(void) { return gRenderer == RENDERER_SOFT; }

// Returns true if the draw-command feed should be populated (DX11 or soft).
// The DX11 and soft backends both consume the DrawCommand arena; the psyx/gl
// backends use the legacy OT/primitive stream.
static inline int Renderer_IsFeedActive(void) { return gRenderer == RENDERER_DX11 || gRenderer == RENDERER_SOFT; }

#endif // RENDERER_H
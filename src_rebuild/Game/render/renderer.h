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
//   -renderer gl    (planned) — modern OpenGL mirroring the DX11 stack
//
// The DX11 path never uses the OT/primitive stream; only the psyx backend does.

typedef enum {
    RENDERER_PSYX = 0,
    RENDERER_DX11 = 1,
    RENDERER_COUNT
} RendererId;

// Active renderer backend, set once at startup from -renderer <name>.
extern RendererId gRenderer;

// Resolve a backend name ("psyx" / "dx11") to a RendererId. Unknown names fall
// back to the default (RENDERER_DX11).
static inline RendererId Renderer_FromName(const char *name)
{
    if (name && !strcmp(name, "psyx"))
        return RENDERER_PSYX;
    // "dx11" and any unknown name -> default.
    return RENDERER_DX11;
}

// Canonical name of a backend.
static inline const char *Renderer_ToName(RendererId id)
{
    switch (id) {
        case RENDERER_PSYX: return "psyx";
        case RENDERER_DX11: default: return "dx11";
    }
}

static inline int Renderer_IsPsyX(void) { return gRenderer == RENDERER_PSYX; }
static inline int Renderer_IsDX11(void) { return gRenderer == RENDERER_DX11; }

#endif // RENDERER_H
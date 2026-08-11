// dx11_backendab_state.h — T4.2 common store/load world-state.
//
// The game-agnostic render-scene snapshot shared by the two backend harnesses
// (`dx11_backendab_psyx` and `dx11_backendab_dx11`). It is a simple binary
// "world-state": a screen resolution plus a list of flat screen-space quads.
// This is the smallest common denominator both backends can render identically
// — the psyx side via POLY_F4 + addPrim/DrawOTag, the DX11 side via the
// draw-command executor with an orthographic screen->NDC projection.
//
// Store/load contract (emulator-style, see T4.2):
//   * one backend "engine iteration" renders the scene and writes the state
//     (ABState_Write) + a screenshot (the harness BMP capture);
//   * the other backend loads the state (ABState_Read) and renders it
//     immediately (no engine iterations) to a screenshot.
// A -compare step then proves both reproduced the same quads at the same
// screen positions with the same colors.

#ifndef DX11_BACKENDAB_STATE_H
#define DX11_BACKENDAB_STATE_H

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DX11AB_STATE_MAGIC 0x32424152u   // "RAB2"
#define DX11AB_STATE_VERSION 1
#define DX11AB_MAX_QUADS 64

// One flat screen-space quad (top-left origin, y down).
typedef struct {
    float x, y;      // top-left corner (pixels)
    float w, h;      // size (pixels)
    unsigned char r, g, b; // flat color
    unsigned char pad;
} Dx11AbQuad;

typedef struct {
    unsigned int resW, resH;   // screen resolution
    unsigned int numQuads;
    Dx11AbQuad quads[DX11AB_MAX_QUADS];
} Dx11AbScene;

// Builds a small representative reference scene (used by -store when no state
// is loaded): a couple of flat quads at known screen positions.
static inline void Dx11AbScene_FillDefault(Dx11AbScene *s, unsigned int resW,
                                           unsigned int resH) {
    memset(s, 0, sizeof(*s));
    s->resW = resW;
    s->resH = resH;
    s->numQuads = 3;
    // A full-height left bar (red), a centred square (green), a right bar (blue).
    s->quads[0].x = 0;           s->quads[0].y = 0;
    s->quads[0].w = (float)resW * 0.20f; s->quads[0].h = (float)resH;
    s->quads[0].r = 220; s->quads[0].g = 20;  s->quads[0].b = 20;
    s->quads[1].x = (float)resW * 0.40f; s->quads[1].y = (float)resH * 0.30f;
    s->quads[1].w = (float)resW * 0.20f; s->quads[1].h = (float)resH * 0.40f;
    s->quads[1].r = 20;  s->quads[1].g = 200; s->quads[1].b = 40;
    s->quads[2].x = (float)resW * 0.80f; s->quads[2].y = 0;
    s->quads[2].w = (float)resW * 0.20f; s->quads[2].h = (float)resH;
    s->quads[2].r = 30;  s->quads[2].g = 60;  s->quads[2].b = 220;
}

// Writes the world-state to `path`. Returns 0 on success, nonzero on error.
static inline int Dx11AbState_Write(const Dx11AbScene *s, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    unsigned int magic = DX11AB_STATE_MAGIC, ver = DX11AB_STATE_VERSION;
    int ok = 1;
    ok &= (fwrite(&magic, 4, 1, f) == 1);
    ok &= (fwrite(&ver, 4, 1, f) == 1);
    ok &= (fwrite(&s->resW, 4, 1, f) == 1);
    ok &= (fwrite(&s->resH, 4, 1, f) == 1);
    ok &= (fwrite(&s->numQuads, 4, 1, f) == 1);
    for (unsigned int i = 0; i < s->numQuads && ok; ++i)
        ok &= (fwrite(&s->quads[i], sizeof(Dx11AbQuad), 1, f) == 1);
    fclose(f);
    return ok ? 0 : 1;
}

// Reads the world-state from `path`. Returns 0 on success (and validates magic/
// version/count), nonzero on error.
static inline int Dx11AbState_Read(Dx11AbScene *s, const char *path) {
    memset(s, 0, sizeof(*s));
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    unsigned int magic = 0, ver = 0;
    int ok = 1;
    ok &= (fread(&magic, 4, 1, f) == 1);
    ok &= (fread(&ver, 4, 1, f) == 1);
    ok &= (fread(&s->resW, 4, 1, f) == 1);
    ok &= (fread(&s->resH, 4, 1, f) == 1);
    ok &= (fread(&s->numQuads, 4, 1, f) == 1);
    if (magic != DX11AB_STATE_MAGIC || ver != DX11AB_STATE_VERSION)
        ok = 0;
    if (s->numQuads > DX11AB_MAX_QUADS)
        ok = 0;
    for (unsigned int i = 0; i < s->numQuads && ok; ++i)
        ok &= (fread(&s->quads[i], sizeof(Dx11AbQuad), 1, f) == 1);
    fclose(f);
    return ok ? 0 : 1;
}

#ifdef __cplusplus
}
#endif

#endif // DX11_BACKENDAB_STATE_H
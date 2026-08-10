// dx11_modeladapter.h — T1.6 game-side MODEL -> arena adapter.
//
// Bridges the game's MODEL mesh data into the DX11 executor's input: world-space
// geometry pushed into the dx11_resources arena, per-poly texture_set/texture_id
// resolved (via a callback) to a PSX tpage/clut region and baked to an SRV, flat
// vs gouraud shading, a world transform + local bbox, and translucent sort keys.
//
// The adapter is deliberately GAME-AGNOSTIC: it consumes a raw, uniform
// poly/vertex description that mirrors the game's resolved MODEL data (int16
// vertices like SVECTOR, per-poly vertex indices + tpage-local texel UVs +
// texture_set/texture_id + resolved flat/gouraud color + state). The game's plot
// functions (T1.7) compute lighting colors and supply a texture-resolve hook;
// the variable-size poly-block walk itself stays game-side. This module owns the
// real conversion the T1.7 shim will call:
//   * int16 model-local vertices -> float (world applied via the command CB);
//   * triangle split (v0,v1,v3)+(v0,v3,v2 for quads);
//   * UV texel -> normalized (divide by the baked region size);
//   * texture bake + SRV handle (white substitute when untextured);
//   * flat (poly color via the executor's flatColor CB) vs gouraud (per-vertex);
//   * local bbox for the executor's frustum cull;
//   * translucent/opaque + blend + sortKey passthrough.
//
// Vertices are duplicated per poly so consecutive same-material polys are
// index-contiguous in the arena -> the T1.5 executor batches them into one
// DrawIndexed.
//
// Matrix convention: row-vector x column-major (v' = v*M), as dx11_renderer.

#ifndef DX11_MODELADAPTER_H
#define DX11_MODELADAPTER_H

#include "dx11_drawcmdexec.h"
#include "dx11_resources.h"
#include "dx11_textures.h"

#include <d3d11.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Raw input types (mirror the game's resolved MODEL data)
// ---------------------------------------------------------------------------

// A model-local vertex (mirrors SVECTOR). Per-vertex rgb is used for gouraud
// shading; flat polys ignore it.
typedef struct {
    short x, y, z;
    unsigned char r, g, b;
} Dx11ModelVertex;

// One resolved poly. vi3 == vi2 for triangles. Quad vertex indices follow the
// game's winding: vi0, vi1, vi3, vi2 are the corners in order (TL, TR, BR, BL),
// and the adapter emits (vi0, vi1, vi3) + (vi0, vi3, vi2). UVs are tpage-local
// texels (the adapter normalizes them by the baked region size). shade / blend
// use the dx11_shaders enums (DX11SH_COLOR_*, DX11SH_BLEND_*).
typedef struct {
    unsigned char vi0, vi1, vi2, vi3;   // vertex indices (vi3==vi2 => triangle)
    unsigned char u0, v0, u1, v1, u2, v2, u3, v3; // tpage-local texel UVs
    unsigned char r, g, b;              // flat color (shade == FLAT)
    unsigned char shade;                // DX11SH_COLOR_FLAT / GOURAUD
    unsigned char blend;                // DX11SH_BLEND_*
    unsigned char twoSided;
    unsigned char nodepth;
    unsigned char texture_set;          // -> resolved via the callback
    unsigned char texture_id;
    int sortKey;                        // translucent depth key
} Dx11ModelPoly;

// A raw mesh reference.
typedef struct {
    const Dx11ModelVertex *verts;
    int num_verts;
    const Dx11ModelPoly *polys;
    int num_polys;
} Dx11ModelMesh;

// The PSX region to bake (region offset/size in texels within the tpage). The
// game/harness fills the VRAM staging (Dx11Tex_CopyVRAM) before Submit.
typedef struct {
    unsigned short tpage;
    unsigned short clut;
    int tex_x, tex_y;       // offset within the tpage (texels)
    int width, height;      // region size (texels)
} Dx11ModelTexture;

// Resolve (texture_set, texture_id) -> a PSX texture region. Returns 0 on
// success (and fills *out). Any nonzero return treats the poly as untextured
// (white substitute). `user` is passed straight through from Submit.
typedef int (*Dx11ModelTexResolve)(void *user, unsigned char set,
                                   unsigned char id, Dx11ModelTexture *out);

// ---------------------------------------------------------------------------
// Submit
// ---------------------------------------------------------------------------
// Converts `mesh` (placed in world space by `world`) into the arena + executor
// commands: pushes vertices/indices into `res`, bakes textures via `tex`, and
// submits one Dx11DrawCmdItem per poly to `cmds`. Returns 0 on success (and
// sets *outPolys to the number of commands submitted), or nonzero on error
// (arena/texture/cache limit). `world` may be NULL (identity).
int Dx11ModelAdapter_Submit(Dx11Res *res, Dx11Tex *tex, Dx11DrawCmds *cmds,
                            const Dx11ModelMesh *mesh, const float world[4][4],
                            void *texUser, Dx11ModelTexResolve texResolve,
                            int *outPolys);

#ifdef __cplusplus
}
#endif

#endif // DX11_MODELADAPTER_H
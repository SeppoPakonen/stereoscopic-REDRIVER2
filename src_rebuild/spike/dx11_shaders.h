// dx11_shaders.h — T1.4 DX11 shaders + render state.
//
// Reproduces the PSX/PsyX appearance for the draw-command renderer: flat &
// gouraud shading, texture sampling (via the T1.3 SRVs), and the PSX blend /
// depth / rasterizer states. One universal VS (world->view->projection via the
// b0/b1 CBs from T1.2) + two pixel shaders:
//   * flat PS     — constant color from a per-draw `b2` CB, x texture.
//   * gouraud PS  — interpolated vertex color, x texture.
// Untextured surfaces bind the T1.3 white 1x1 substitute so `white * color =
// color` (one textured PS covers both, mirroring the psyx `color * v_color`).
//
// Blend states mirror `GR_SetBlendMode` / the PSX `BlendMode` table:
//   NONE (no blend, depth write on), AVERAGE (src*srcAlpha + dst*(1-srcAlpha)),
//   ADD (src+dst), SUBTRACT (dst-src), ADD_QUATER (src*0.5 + dst).
// Translucent polys use depth-write-off (ordering is the T1.5 sort-key job).

#ifndef DX11_SHADERS_H
#define DX11_SHADERS_H

#include <d3d11.h>
#include <dxgi.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the shader/render-state set.
typedef struct Dx11Shaders Dx11Shaders;

// PSX / PsyCross blend modes (BM_NONE..BM_ADD_QUATER_SOURCE).
typedef enum {
    DX11SH_BLEND_NONE = 0,
    DX11SH_BLEND_AVERAGE,
    DX11SH_BLEND_ADD,
    DX11SH_BLEND_SUBTRACT,
    DX11SH_BLEND_ADD_QUATER,
} Dx11ShadersBlend;

// Color source: flat (per-draw b2 constant) vs gouraud (interpolated vertex).
typedef enum {
    DX11SH_COLOR_FLAT = 0,
    DX11SH_COLOR_GOURAUD,
} Dx11ShadersColor;

typedef enum {
    DX11SH_OK = 0,
    DX11SH_ERR_COMPILE,
    DX11SH_ERR_DEVICE,
    DX11SH_ERR_ARG,
} Dx11ShadersResult;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
// Compiles the VS + both PS, creates the input layout (matches Dx11ResVertex:
// pos@0, color@12, uv@28), the blend/depth-stencil/rasterizer states and the
// flat-color CB (b2). `ctx` is borrowed (owned by the caller). Returns NULL on
// failure; *outResult carries the code.
Dx11Shaders *Dx11Shaders_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                                Dx11ShadersResult *outResult);

void Dx11Shaders_Destroy(Dx11Shaders *s);

// ---------------------------------------------------------------------------
// Bind helpers
// ---------------------------------------------------------------------------
// Binds the VS + the PS for `color` (flat or gouraud) + the input layout and
// triangle-list topology. The caller still binds b0/b1 (view-proj/world) and a
// texture SRV (t0) + sampler (s0) — e.g. via the T1.2 resource manager and the
// T1.3 texture system.
void Dx11Shaders_Bind(Dx11Shaders *s, ID3D11DeviceContext *ctx,
                      Dx11ShadersColor color);

// Flat color (b2): used by DX11SH_COLOR_FLAT. Updates and binds the CB.
void Dx11Shaders_SetFlatColor(Dx11Shaders *s, ID3D11DeviceContext *ctx,
                              float r, float g, float b, float a);

// Binds the blend state matching a PSX blend mode.
void Dx11Shaders_SetBlend(Dx11Shaders *s, ID3D11DeviceContext *ctx,
                          Dx11ShadersBlend blend);

// Binds the depth-stencil state: opaque (depth write on) or translucent
// (depth write off; depth test stays on).
void Dx11Shaders_SetDepthOpaque(Dx11Shaders *s, ID3D11DeviceContext *ctx,
                                int opaque);

// Binds the rasterizer state: twoSided=1 -> CULL_NONE, else CULL_BACK.
void Dx11Shaders_SetRaster(Dx11Shaders *s, ID3D11DeviceContext *ctx,
                           int twoSided);

// The input layout (matches Dx11ResVertex). For binding geometry.
ID3D11InputLayout *Dx11Shaders_GetLayout(Dx11Shaders *s);

#ifdef __cplusplus
}
#endif

#endif // DX11_SHADERS_H
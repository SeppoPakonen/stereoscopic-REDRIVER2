// dx11_composite.h — T2.3 DX11 stereo composite.
//
// A game-agnostic composite pass that samples the two per-eye offscreen SRVs
// (T2.1) into the backbuffer as Side-by-Side or Top-and-Bottom, replacing the
// legacy GL blit (StereoCompositor_Composite). A fullscreen triangle (no
// vertex buffer) + a pixel shader that, per output pixel, picks which eye
// fills the current half and resamples it. The same per-pixel shader path is
// the foundation for Phase 3's color modes.
//
// Modes:
//   DX11C_MODE_SBS  — left half = eye0, right half = eye1.
//   DX11C_MODE_TB   — top half  = eye0, bottom half = eye1.
//   DX11C_MODE_MONO — pass-through of eye0 across the full screen.
//   DX11C_MODE_ANAGLYPH / _FULLCOLOR — red-cyan / luminance-blended anaglyph.
//   DX11C_MODE_INTERLACED — odd rows = eye0, even rows = eye1.
//   DX11C_MODE_POLARIZED  — scanline encoding (complementary to INTERLACED).
//   DX11C_MODE_CHECKERBOARD — pixel interleave (mod(x+y,2)).
// swap=1 swaps which eye fills the left/top half / channel / row.

#ifndef DX11_COMPOSITE_H
#define DX11_COMPOSITE_H

#include <d3d11.h>
#include <dxgi.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the composite pass.
typedef struct Dx11Composite Dx11Composite;

typedef enum {
    DX11C_MODE_SBS = 0,
    DX11C_MODE_TB  = 1,
    DX11C_MODE_MONO = 2,
    DX11C_MODE_ANAGLYPH = 3,
    DX11C_MODE_ANAGLYPH_FULLCOLOR = 4,
    DX11C_MODE_INTERLACED = 5,
    DX11C_MODE_POLARIZED = 6,
    DX11C_MODE_CHECKERBOARD = 7,
} Dx11CompositeMode;

typedef enum {
    DX11C_OK = 0,
    DX11C_ERR_COMPILE,
    DX11C_ERR_DEVICE,
    DX11C_ERR_ARG,
} Dx11CompositeResult;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
// Compiles the composite VS + PS, creates the params CB (b0), a point sampler,
// a no-cull rasterizer and a depth-disabled state. `dev`/`ctx` are borrowed.
// Returns NULL on failure; *outResult carries the code.
Dx11Composite *Dx11Composite_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                                    Dx11CompositeResult *outResult);

void Dx11Composite_Destroy(Dx11Composite *c);

// ---------------------------------------------------------------------------
// Composite
// ---------------------------------------------------------------------------
// Binds the composite shaders, sets t0/t1 (eye0/eye1 SRVs) + the sampler,
// updates the params CB (mode, swap), sets a viewport of w×h and draws the
// fullscreen triangle. The caller must have already bound the target RTV
// (e.g. the backbuffer via Dx11Renderer_BindBackbuffer). `eye0`/`eye1` may be
// NULL (in which case that eye samples black).
void Dx11Composite_Composite(Dx11Composite *c, ID3D11DeviceContext *ctx,
                             Dx11CompositeMode mode, int swap,
                             ID3D11ShaderResourceView *eye0,
                             ID3D11ShaderResourceView *eye1,
                             int w, int h);

#ifdef __cplusplus
}
#endif

#endif // DX11_COMPOSITE_H
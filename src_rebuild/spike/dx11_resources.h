// dx11_resources.h — T1.2 DX11 resource management.
//
// Per-frame resource management for the draw-command renderer:
//   * a growable CPU vertex arena (packed once per frame from submitted draw
//     commands) uploaded into a D3D11 dynamic vertex/index buffer (WRITE_DISCARD,
//     reused every frame — no per-frame allocation);
//   * a constant-buffer pool: one 64-byte DEFAULT CB per per-draw world-matrix
//     slot (updated via UpdateSubresource), plus a view/proj CB;
//   * a default (point) sampler + SRV bind helper (textures themselves are
//     T1.3; this manages binding state).
//
// All CPU arenas grow geometrically up to a hard cap, so per-frame allocation
// is bounded (verified by the harness stats). Uses the same row-vector x
// column-major matrix convention as dx11_renderer (v' = v*M).

#ifndef DX11_RESOURCES_H
#define DX11_RESOURCES_H

#include <d3d11.h>

#ifdef __cplusplus
extern "C" {
#endif

// A packed vertex fed to the dynamic VB. Position+color+uv; stride 36 bytes.
#pragma pack(push, 1)
typedef struct {
    float x, y, z;      // model-local position (world applied via CB)
    float r, g, b, a;   // vertex color (gouraud / flat)
    float u, v;         // texture uv
} Dx11ResVertex;
#pragma pack(pop)

typedef struct Dx11Res Dx11Res;

typedef enum {
    DX11RES_OK = 0,
    DX11RES_ERR_CB_CONTEXT1,   // could not get ID3D11DeviceContext1
    DX11RES_ERR_VB,
    DX11RES_ERR_IB,
    DX11RES_ERR_CB,
    DX11RES_ERR_OOM,           // arena exhausted / realloc failed
} Dx11ResResult;

typedef struct {
    int vertex_capacity;   // initial vertex arena vertex count (grows geometrically)
    int vertex_max;        // hard cap (0 = use default)
    int index_capacity;    // initial index arena count
    int index_max;         // hard cap (0 = use default)
    int cb_slots;          // constant-buffer slot count (each slot = one 4x4 matrix)
} Dx11ResConfig;

// Defaults used when a config field is 0.
Dx11ResConfig Dx11Res_DefaultConfig(void);

// Create resource manager bound to `dev` and the immediate context `ctx`.
// `ctx1` (for VSSetConstantBuffers1) is obtained internally from `ctx`.
Dx11Res *Dx11Res_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                        const Dx11ResConfig *cfg, Dx11ResResult *outResult);
void Dx11Res_Destroy(Dx11Res *r);

// ---------------------------------------------------------------------------
// Per-frame reset. Clears the vertex/index arena and resets the CB slot cursor.
// ---------------------------------------------------------------------------
void Dx11Res_BeginFrame(Dx11Res *r);

// ---------------------------------------------------------------------------
// Vertex arena (CPU). Returns the vertex/index index, or -1 if the arena is
// full (bounded).
// ---------------------------------------------------------------------------
int Dx11Res_PushVertex(Dx11Res *r, const Dx11ResVertex *v);
int Dx11Res_PushIndex(Dx11Res *r, unsigned short idx);
int Dx11Res_VertexCount(Dx11Res *r);
int Dx11Res_IndexCount(Dx11Res *r);

// ---------------------------------------------------------------------------
// Constant-buffer arena.
// ---------------------------------------------------------------------------
// Allocate a 4x4 matrix slot for the current frame. Returns slot index or -1.
int Dx11Res_AllocCB(Dx11Res *r);
// Write a world matrix into a previously allocated slot.
void Dx11Res_SetCB(Dx11Res *r, int slot, const float m[4][4]);
// Set the per-frame view-projection matrix (b0). Pass NULL to skip.
void Dx11Res_SetViewProj(Dx11Res *r, const float m[4][4]);

// ---------------------------------------------------------------------------
// Upload CPU arenas -> dynamic GPU buffers. Call once per frame after all
// commands are submitted, before drawing. Pushes the CB arena and view-proj
// to b1/b0.
// ---------------------------------------------------------------------------
void Dx11Res_Upload(Dx11Res *r, ID3D11DeviceContext *ctx);

// Bind VB+IB for drawing (b0 = view/proj, b1 = per-draw world via slot).
void Dx11Res_BindGeometry(Dx11Res *r, ID3D11DeviceContext *ctx);
// Bind b1 at a specific world slot for a draw (after BindGeometry).
void Dx11Res_BindWorldSlot(Dx11Res *r, ID3D11DeviceContext *ctx, int slot);

// ---------------------------------------------------------------------------
// Sampler + SRV.
// ---------------------------------------------------------------------------
// Default (point) sampler, reusable across draws.
ID3D11SamplerState *Dx11Res_GetSampler(Dx11Res *r);
// Bind an SRV to the pixel shader texture slot 0 (NULL unbinds).
void Dx11Res_BindSRV(Dx11Res *r, ID3D11DeviceContext *ctx,
                     ID3D11ShaderResourceView *srv);

// ---------------------------------------------------------------------------
// Stats (bounded-growth verification).
// ---------------------------------------------------------------------------
int Dx11Res_VertexCapacity(Dx11Res *r);   // current vertex arena capacity
int Dx11Res_IndexCapacity(Dx11Res *r);    // current index arena capacity
int Dx11Res_CBSlotCount(Dx11Res *r);      // configured CB slot count
int Dx11Res_CBSlotsUsed(Dx11Res *r);      // slots allocated so far this frame
int Dx11Res_TotalVertsSubmitted(Dx11Res *r); // cumulative across frames
int Dx11Res_TotalFrames(Dx11Res *r);

// DEBUG (verification): read back the uploaded VB/view-proj and write a small
// dump to `path`. Not part of the production API.
void Dx11Res_DebugDump(Dx11Res *r, ID3D11DeviceContext *ctx, const char *path);

#ifdef __cplusplus
}
#endif

#endif // DX11_RESOURCES_H
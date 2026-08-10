// dx11_drawcmdexec.h — T1.5 draw-command executor.
//
// The renderer-side logic that consumes a per-frame list of draw commands and
// owns projection (per-command world CB from the T1.2 arena), frustum culling,
// sorting (opaque by material/state front-to-back, translucent back-to-front by
// sortKey), batching (same material/state/world + index-contiguous into one
// DrawIndexed) and the final emission on top of dx11_resources / dx11_textures
// / dx11_shaders. Game-agnostic: the game pushes geometry into the T1.2 arena
// and submits commands referencing those vertex/index ranges (the game-side
// DrawCommand/MODEL adapter is T1.6).
//
// Conventions:
//   * row-vector x column-major matrices (v' = v*M), world at [0..2][3].
//   * viewProj is set once per frame (T1.2 SetViewProj); per-command world CBs
//     are allocated from the arena's CB pool during Execute.
//   * translucent commands sort back-to-front by sortKey (largest = far = drawn
//     first, matching the PSX OT order); opaque commands draw before them.

#ifndef DX11_DRAWCMDEXEC_H
#define DX11_DRAWCMDEXEC_H

#include "dx11_resources.h"
#include "dx11_textures.h"
#include "dx11_shaders.h"

#include <d3d11.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the executor.
typedef struct Dx11DrawCmds Dx11DrawCmds;

// One draw command fed to the executor. Geometry is already in the T1.2 arena.
typedef struct {
    int vertexOffset;            // base vertex (arena index) for this command
    int vertexCount;
    int indexOffset;             // base index into the arena's index buffer
    int indexCount;
    float bboxMin[3], bboxMax[3];// local-space bbox (culled via world*viewProj)
    float world[4][4];           // world transform (row-vector, column-major)
    Dx11TexHandle texture;       // SRV handle from dx11_textures (-1 = white sub)
    float flatColor[4];          // flat shading color (used when shade==FLAT)
    unsigned char blend;         // Dx11ShadersBlend
    unsigned char shade;         // Dx11ShadersColor (FLAT / GOURAUD)
    unsigned char twoSided;
    unsigned char nodepth;
    unsigned char translucent;   // 1 = translucent (B2F by sortKey), 0 = opaque
    int sortKey;                 // depth key for translucent ordering
} Dx11DrawCmdItem;

typedef struct {
    int max_commands;            // capacity of the per-frame command buffer
} Dx11DrawCmdsConfig;

typedef enum {
    DX11DCMD_OK = 0,
    DX11DCMD_ERR_ARG,
    DX11DCMD_ERR_DEVICE,
} Dx11DrawCmdsResult;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
// Creates the executor. `res`, `tex`, `sh` are borrowed (owned by the caller;
// not released here). Returns NULL on failure; *outResult carries the code.
Dx11DrawCmds *Dx11DrawCmds_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                                  Dx11Res *res, Dx11Tex *tex, Dx11Shaders *sh,
                                  const Dx11DrawCmdsConfig *cfg,
                                  Dx11DrawCmdsResult *outResult);

void Dx11DrawCmds_Destroy(Dx11DrawCmds *c);

// Resets the per-frame command buffer.
void Dx11DrawCmds_BeginFrame(Dx11DrawCmds *c);

// Sets the view/proj matrix for the frame (via the T1.2 SetViewProj).
void Dx11DrawCmds_SetViewProj(Dx11DrawCmds *c, const float m[4][4]);

// Appends a command. Returns 0 on success, nonzero if the buffer is full.
int Dx11DrawCmds_Submit(Dx11DrawCmds *c, const Dx11DrawCmdItem *item);

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------
// Sorts, culls, uploads the arena, and draws every non-culled command to the
// currently bound render target. Returns the number of emitted DrawIndexed
// calls (after batching). Stats are queryable via CulledCount / DrawCallCount.
int Dx11DrawCmds_Execute(Dx11DrawCmds *c, ID3D11DeviceContext *ctx);

// Commands frustum-culled by the last Execute.
int Dx11DrawCmds_CulledCount(Dx11DrawCmds *c);

// DrawIndexed calls emitted by the last Execute (after batching).
int Dx11DrawCmds_DrawCallCount(Dx11DrawCmds *c);

// Total commands submitted this frame.
int Dx11DrawCmds_SubmittedCount(Dx11DrawCmds *c);

#ifdef __cplusplus
}
#endif

#endif // DX11_DRAWCMDEXEC_H
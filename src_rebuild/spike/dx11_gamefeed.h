// dx11_gamefeed.h — T5.1 in-game DX11 renderer integration.
//
// The game-facing renderer integration: consumes the game's real `DrawCommand`
// list (drawcmd.h) and drives the full DX11 pipeline — MODEL -> Dx11ModelAdapter
// -> arena -> executor -> per-eye view -> eye RTs -> composite. This is the
// renderer half of DrawGame's "DrawGame -> draw-command -> per-eye -> composite"
// path: the function DrawGame's `-renderer dx11` branch will call each frame.
//
// The MODEL -> Dx11ModelMesh conversion handles flat-textured-quad polys
// (PL_POLYFT4, poly type `id & 31` = 11/21/23, stride PolySizes[type]) — the
// dominant terrain/tile/car case. Full shade/gouraud/LOD + all poly types are a
// follow-up.

#ifndef DX11_GAMEFEED_H
#define DX11_GAMEFEED_H

#include "dx11_renderer.h"
#include "dx11_resources.h"
#include "dx11_textures.h"
#include "dx11_shaders.h"
#include "dx11_drawcmdexec.h"
#include "dx11_modeladapter.h"
#include "dx11_stereo.h"
#include "dx11_composite.h"
#include "drawcmd.h"

#include <d3d11.h>

#ifdef __cplusplus
extern "C" {
#endif

// Convert a MODEL's flat-textured-quad polys (PL_POLYFT4) into the adapter's raw
// mesh. `verts`/`polys` are caller buffers (>= model->num_vertices / model->num_polys).
// `flatRGB` is the flat color applied to every converted poly (the shading color
// the game would compute for the model). Returns 0 on success. Only flat quad
// polys are converted; other types are skipped (the walk still advances by its
// PolySizes stride).
int Dx11GameFeed_ModelToMesh(const struct MODEL *model, const unsigned char flatRGB[3],
                             Dx11ModelVertex *verts, int vertCap,
                             Dx11ModelPoly *polys, int polyCap,
                             int *outVerts, int *outPolys);

// Render a DrawCommand[] list through the full DX11 path: for each command with a
// mesh, convert + submit via Dx11ModelAdapter, then render both eyes into their
// offscreen RTs and composite SBS/TB/MONO into the backbuffer, captured to BMP.
// `proj` is the shared per-eye projection (row-vector, column-major). `composite`
// is a pre-created Dx11Composite. `cmdColors[i]` is the flat color for command i
// (NULL -> white); the real game would supply the shading color it computes per
// model. Returns 0 on success.
int Dx11GameFeed_RenderFrame(Dx11Renderer *ren, Dx11Res *res, Dx11Tex *tex,
                             Dx11Shaders *sh, Dx11DrawCmds *cmds,
                             Dx11Composite *composite,
                             const float proj[4][4],
                             const DrawCommand *drawCmds, int numCmds,
                             const unsigned char (*cmdColors)[3],
                             const float camPos[3], float yawRad, float sep,
                             int swap, Dx11CompositeMode mode,
                             void *texUser, Dx11ModelTexResolve texResolve,
                             const char *bmpOut);

#ifdef __cplusplus
}
#endif

#endif // DX11_GAMEFEED_H
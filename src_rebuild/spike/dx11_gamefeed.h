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
#include "render/drawcmd.h"

#include <d3d11.h>

#ifdef __cplusplus
extern "C" {
#endif

// Convert a MODEL's flat-textured-quad polys (PL_POLYFT4) into the adapter's raw
// mesh. `verts`/`polys` are caller buffers (>= model->num_vertices / model->num_polys).
// `flatRGB` is the flat color applied to every converted poly (the shading color
// the game would compute for the model). `tpages` is the game's texture_pages[128]
// array (or NULL): when non-NULL the per-poly UV X is scaled by the tpage format
// (page width 64/128/256 texels / 256) so the tpage-page-relative UVs (0..255)
// land in the full-page region the tex-resolve bakes; V is unchanged. Returns 0
// on success. Only flat quad polys are converted; other types are skipped (the
// walk still advances by its PolySizes stride).
int Dx11GameFeed_ModelToMesh(const struct MODEL *model, const unsigned char flatRGB[3],
                             Dx11ModelVertex *verts, int vertCap,
                             Dx11ModelPoly *polys, int polyCap,
                             int *outVerts, int *outPolys,
                             const unsigned short *tpages);

// ---------------------------------------------------------------------------
// Sky horizon feed (T5.2): the sky horizon MODEL's polys are textured per-poly
// from the game's sky tables (skytpage/skyclut/skytexuv) via
// HorizonTextures[horizOffset + polyIndex], NOT the model's texture_set/id.
// ---------------------------------------------------------------------------
typedef struct {
    unsigned char u0, v0, u1, v1, u2, v2, u3, v3;   // page-relative texel UVs
} Dx11SkyUV;

typedef struct {
    const unsigned short *skytpage;        // [28] VRAM tpage per sky texture
    const unsigned short *skyclut;         // [28] VRAM clut per sky texture
    const Dx11SkyUV       *skytexuv;       // [28] UVs per sky texture
    const unsigned char  *horizonTextures; // [40] horizon poly -> sky texture idx
} Dx11SkyTextures;

// Convert a game sky horizon MODEL into the adapter's raw mesh, texturing each
// poly from the sky tables (via HorizonTextures[horizOffset + polyIndex]) — the
// per-poly carTpage/carClut direct-bake path with the skytexuv UV remap
// (u2,u3,u0,u1, matching PlotSkyPoly). `verts`/`polys` are caller buffers
// (>= model->num_vertices / model->num_polys). Returns 0 on success.
int Dx11GameFeed_SkyModelToMesh(const struct MODEL *model, const Dx11SkyTextures *sky,
                                int horizOffset, Dx11ModelVertex *verts, int vertCap,
                                Dx11ModelPoly *polys, int polyCap,
                                int *outVerts, int *outPolys);

// Convert a game CAR_MODEL (dented vlist + GT3/FT3/B3 triangle lists) into the
// adapter's raw mesh. `carModel` is the game's CAR_MODEL* (passed via
// DrawCommand.carModel); `verts` is a caller buffer >= 256; `polys` >=
// car->numGT3+numFT3+numB3. `palette` selects the civ_clut color variant;
// `civClut` is the game's civ_clut[8][32][6] (NULL -> GT3 polys untextured).
// Returns 0 on success.
int Dx11GameFeed_CarModelToMesh(const void *carModel, int palette,
                                const u_short (*civClut)[32][6],
                                Dx11ModelVertex *verts, int vertCap,
                                Dx11ModelPoly *polys, int polyCap,
                                int *outVerts, int *outPolys);

// Convert a single-primitive billboard DrawCommand (mesh == NULL + material)
// into the adapter's raw mesh (4 verts + 1 quad poly). `center` is the world
// position (the command's world translation); `orient` is a BillboardOrient
// (camera-facing vs world-ground); `halfX`/`halfY` are the quad half-extents;
// `mat` carries the tpage/clut/blend; `uv` are the 8 page-relative texel UVs
// (u0,v0,u1,v1,u2,v2,u3,v3); `rgb` is the flat color; `sortKey` is the depth
// key; `camPos` drives the camera-facing basis. The verts are model-local
// (centered at origin) so the caller's `world` matrix (containing `center`)
// places them; the UV X is scaled into the full-page texel region the direct
// bake covers. Returns 0 on success.
int Dx11GameFeed_BillboardToMesh(const float center[3], int orient,
                                 short halfX, short halfY,
                                 const MaterialRef *mat, const unsigned char uv[8],
                                 const unsigned char rgb[3], int sortKey,
                                 const float camPos[3],
                                 Dx11ModelVertex *verts, Dx11ModelPoly *poly);

// Render a DrawCommand[] list through the full DX11 path: for each command with a
// mesh, convert + submit via Dx11ModelAdapter, then render both eyes into their
// offscreen RTs and composite SBS/TB/MONO into the backbuffer, captured to BMP.
// `proj` is the shared per-eye projection (row-vector, column-major). `composite`
// is a pre-created Dx11Composite. `cmdColors[i]` is the flat color for command i
// (NULL -> white); the real game would supply the shading color it computes per
// model. `bmpOut` captures the composite backbuffer to a BMP (NULL -> skip; the
// in-game consumer passes NULL). `tpages` is the game's texture_pages[128] (or
// NULL for no UV scaling). Returns 0 on success.
int Dx11GameFeed_RenderFrame(Dx11Renderer *ren, Dx11Res *res, Dx11Tex *tex,
                             Dx11Shaders *sh, Dx11DrawCmds *cmds,
                             Dx11Composite *composite,
                             const float proj[4][4],
                             const DrawCommand *drawCmds, int numCmds,
                             const unsigned char (*cmdColors)[3],
                             const float camPos[3], float yawRad, float sep,
                             int swap, Dx11CompositeMode mode,
                             void *texUser, Dx11ModelTexResolve texResolve,
                             const unsigned short *tpages,
                             const u_short (*civClut)[32][6],
                             const Dx11SkyTextures *skyTex,
                             const char *bmpOut,
                             const float (*customView)[4]);

#ifdef __cplusplus
}
#endif

#endif // DX11_GAMEFEED_H
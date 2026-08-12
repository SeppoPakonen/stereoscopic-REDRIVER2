// dx11_gamefeed.c — T5.1 in-game DX11 renderer integration.
//
// Implements Dx11GameFeed (see dx11_gamefeed.h): the game-facing renderer
// integration that consumes the real DrawCommand list and drives the full DX11
// pipeline (MODEL -> Dx11ModelAdapter -> arena -> executor -> per-eye -> composite).
// Compiled as C++ by premake (compileas "C++").

#include "dx11_gamefeed.h"

#include "engine/mdl.h"    // MODEL, PL_POLYFT4, SVECTOR
#include "libgte.h"        // MATRIX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Car body feed (T5.2): the game's car body is a CAR_MODEL (dr2types.h) — a
// triangulated list of CAR_POLYs (FT3/GT3/B3) over a shared dented vlist, with
// textures from the game's civ_clut CLUT table (NOT the terrain texture_pages/
// texture_cluts resolve). The spike stays self-contained (headless-testable) so
// we mirror the two structs here; the game passes its CAR_MODEL* via
// DrawCommand.carModel and we reinterpret it as this mirror (same layout).
typedef struct {
    int vindices;      // packed 3 vertex indices: v0=low8, v1=mid8, v2=high8
    int nindices;      // packed 3 normal indices (unused by the flat feed)
    int clut_uv0;      // (clut<<16) | (u0 | v0<<8)  — clut = addr (FT3) or index (GT3)
    int tpage_uv1;     // (tpage<<16) | (u1 | v1<<8)
    int uv3_uv2;       // (u2 | v2<<8)
    short originalindex;
} GFCarPoly;

typedef struct {
    int numFT3; GFCarPoly* pFT3;
    int numGT3; GFCarPoly* pGT3;
    int numB3;  GFCarPoly* pB3;
    SVECTOR* vlist;
    SVECTOR* nlist;
} GFCarModel;

// Max car-body vertices (the game's denting buffer, dr2limits.h). Vertex
// indices are 8-bit so this bounds them.
#define GFCAR_MAX_VERTS 256

// Number of civ_clut "palette slots" per car model (each 6 colors/variants).
#define GFCAR_CLUT_STRIDE 6
#define GFCAR_CLUT_GROUP  (32 * GFCAR_CLUT_STRIDE)   // 192 shorts per carid group

// ---------------------------------------------------------------------------
// PolySizes[56] — the game's per-poly stride table (from draw.c). Walk advances
// by PolySizes[ptype] where ptype = id & 31.
// ---------------------------------------------------------------------------
static const int kPolySizes[56] = {
    8,  12, 16, 24,
    20, 20, 28, 32,
    8,  12, 16, 16,
    0,  0,  0,  0,
    12, 12, 12, 16,
    20, 20, 24, 24,
    0,  0,  0,  0,
    0,  0,  0,  0,
    8,  12, 16, 24,
    20, 24, 28, 32,
    0,  0,  0,  0,
    0,  0,  0,  0,
    12, 12, 12, 16,
    20, 20, 24, 24,
};

// Flat textured quad poly types (from PlotModelSubdivNxN's `ptype != 11 && != 21
// && != 23` skip).
#define IS_FLAT_QUAD(ptype) ((ptype) == 11 || (ptype) == 21 || (ptype) == 23)

// Tpage page width (texels) and UV-X scale for the tpage format.
static int PageWidthForTpage(unsigned short tpage) {
    int fmt = (tpage >> 7) & 3;
    return (fmt == 0) ? 64 : (fmt == 1) ? 128 : 256;
}

// ---------------------------------------------------------------------------
// MODEL -> adapter raw mesh (flat-textured-quad subset).
// ---------------------------------------------------------------------------
int Dx11GameFeed_ModelToMesh(const struct MODEL *model, const unsigned char flatRGB[3],
                             Dx11ModelVertex *verts, int vertCap,
                             Dx11ModelPoly *polys, int polyCap,
                             int *outVerts, int *outPolys,
                             const unsigned short *tpages) {
    if (!model) return 1;
    int ov = 0, op = 0;

    // Relocatable model layout: vertices / poly_block are BYTE OFFSETS from the
    // MODEL base (PC convention). int16 SVECTOR vertices.
    const SVECTOR *srcVerts = (const SVECTOR *)((const unsigned char *)model + model->vertices);
    int nv = model->num_vertices;
    if (nv > vertCap) nv = vertCap;
    for (int i = 0; i < nv; ++i) {
        verts[i].x = srcVerts[i].vx;
        verts[i].y = srcVerts[i].vy;
        verts[i].z = srcVerts[i].vz;
        verts[i].r = verts[i].g = verts[i].b = 0;   // gouraud unused for flat
    }
    ov = nv;

    // Walk the poly_block (PL_POLYFT4 base; stride per type).
    const unsigned char *p = (const unsigned char *)model + model->poly_block;
    int n = model->num_polys;
    while (n > 0) {
        const PL_POLYFT4 *poly = (const PL_POLYFT4 *)p;
        int ptype = poly->id & 31;
        if (IS_FLAT_QUAD(ptype)) {
            if (op < polyCap) {
                Dx11ModelPoly *mp = &polys[op];
                memset(mp, 0, sizeof(*mp));
                // Game winding (vi0,vi1,vi3,vi2) = (v0,v1,v3,v2) = TL,TR,BR,BL.
                mp->vi0 = poly->v0; mp->vi1 = poly->v1;
                mp->vi3 = poly->v3; mp->vi2 = poly->v2;
                if (tpages) {
                    // Scale UV X from page-relative (0..255) into the full page
                    // texel region (width = pageW) the resolve bakes; V is 1:1.
                    int pageW = PageWidthForTpage(tpages[poly->texture_set]);
                    mp->u0 = (unsigned char)((poly->uv0.u * pageW) >> 8);
                    mp->u1 = (unsigned char)((poly->uv1.u * pageW) >> 8);
                    mp->u2 = (unsigned char)((poly->uv2.u * pageW) >> 8);
                    mp->u3 = (unsigned char)((poly->uv3.u * pageW) >> 8);
                    mp->v0 = poly->uv0.v; mp->v1 = poly->uv1.v;
                    mp->v2 = poly->uv2.v; mp->v3 = poly->uv3.v;
                } else {
                    mp->u0 = poly->uv0.u; mp->v0 = poly->uv0.v;
                    mp->u1 = poly->uv1.u; mp->v1 = poly->uv1.v;
                    mp->u2 = poly->uv2.u; mp->v2 = poly->uv2.v;
                    mp->u3 = poly->uv3.u; mp->v3 = poly->uv3.v;
                }
                mp->r = flatRGB[0]; mp->g = flatRGB[1]; mp->b = flatRGB[2];
                mp->shade = DX11SH_COLOR_FLAT;
                mp->blend = DX11SH_BLEND_NONE;
                mp->twoSided = 1;
                mp->texture_set = poly->texture_set;
                mp->texture_id = poly->texture_id;
                ++op;
            }
        }
        int stride = kPolySizes[ptype];
        if (stride <= 0) stride = 16;
        p += stride;
        --n;
    }

    if (outVerts) *outVerts = ov;
    if (outPolys) *outPolys = op;
    return 0;
}

// ---------------------------------------------------------------------------
// CAR_MODEL -> adapter raw mesh (car body: GT3/FT3/B3 triangles).
// ---------------------------------------------------------------------------
// Emits one triangle poly for a car CAR_POLY. `mode` selects the clut source:
//   0 = GT3 (clut = civClut[carid][texid][palette], a civ_clut index)
//   1 = FT3 (clut = clut_uv0>>16, already a VRAM clut address)
//   2 = B3  (untextured flat, no clut/tpage)
static void EmitCarTri(const GFCarPoly* cp, int mode, int palette,
                       const u_short (*civClut)[32][6], Dx11ModelPoly* mp) {
    memset(mp, 0, sizeof(*mp));
    int iv0 = cp->vindices & 0xff;
    int iv1 = (cp->vindices >> 8) & 0xff;
    int iv2 = (cp->vindices >> 16) & 0xff;
    mp->vi0 = (unsigned char)iv0;
    mp->vi1 = (unsigned char)iv1;
    mp->vi2 = (unsigned char)iv2;
    mp->vi3 = (unsigned char)iv2;   // triangle (vi3 == vi2)

    // Page-relative UVs (0..255), X scaled into the full-page texel region.
    int u0 = cp->clut_uv0 & 0xff,  v0 = (cp->clut_uv0 >> 8) & 0xff;
    int u1 = cp->tpage_uv1 & 0xff, v1 = (cp->tpage_uv1 >> 8) & 0xff;
    int u2 = cp->uv3_uv2 & 0xff,   v2 = (cp->uv3_uv2 >> 8) & 0xff;
    int tpage = (cp->tpage_uv1 >> 16) & 0xffff;
    int fmt = (tpage >> 7) & 3;
    int pageW = (fmt == 0) ? 64 : (fmt == 1) ? 128 : 256;
    mp->u0 = (unsigned char)((u0 * pageW) >> 8); mp->v0 = (unsigned char)v0;
    mp->u1 = (unsigned char)((u1 * pageW) >> 8); mp->v1 = (unsigned char)v1;
    mp->u2 = (unsigned char)((u2 * pageW) >> 8); mp->v2 = (unsigned char)v2;
    mp->u3 = mp->u2; mp->v3 = mp->v2;

    mp->r = mp->g = mp->b = 255;
    mp->shade = DX11SH_COLOR_FLAT;
    mp->blend = DX11SH_BLEND_NONE;
    mp->twoSided = 1;

    if (mode == 2) {
        // B3: untextured flat — mark untextured so the adapter uses a white
        // substitute (an out-of-range texture_set makes the resolve fail).
        mp->texture_set = 200;
        mp->texture_id = 0;
        return;
    }

    mp->carTexture = 1;
    mp->carTpage = (unsigned short)tpage;
    if (mode == 1) {
        // FT3: clut is already a VRAM clut address (from texture_cluts).
        mp->carClut = (unsigned short)((cp->clut_uv0 >> 16) & 0xffff);
    } else {
        // GT3: clut = civClut[carid][texid][palette]. carid = 1 + idx/GROUP,
        // texid = (idx%GROUP)/STRIDE, from the packed clut index.
        if (civClut) {
            int idx = (cp->clut_uv0 >> 16) & 0xffff;
            int carid = 1 + idx / GFCAR_CLUT_GROUP;
            int texid = (idx % GFCAR_CLUT_GROUP) / GFCAR_CLUT_STRIDE;
            if (carid < 0) carid = 0;
            if (carid > 7) carid = 7;
            if (texid < 0) texid = 0;
            if (texid > 31) texid = 31;
            if (palette < 0) palette = 0;
            if (palette > 5) palette = 5;
            mp->carClut = civClut[carid][texid][palette];
        } else {
            mp->carTexture = 0;
            mp->texture_set = 200;   // untextured fallback
        }
    }
}

// Convert a game CAR_MODEL (dented vlist + GT3/FT3/B3 triangle lists) into the
// adapter's raw mesh. `verts` is a caller buffer >= GFCAR_MAX_VERTS (256);
// `polys` >= car->numGT3+numFT3+numB3. `palette` selects the civ_clut color
// variant; `civClut` is the game's civ_clut[8][32][6] (NULL -> GT3 untextured).
int Dx11GameFeed_CarModelToMesh(const void* carModel, int palette,
                                const u_short (*civClut)[32][6],
                                Dx11ModelVertex* verts, int vertCap,
                                Dx11ModelPoly* polys, int polyCap,
                                int* outVerts, int* outPolys) {
    const GFCarModel* car = (const GFCarModel*)carModel;
    if (!car || !car->vlist) return 1;

    // Vertices: the dented vlist (up to the 256-vert cap / denting buffer).
    int nv = GFCAR_MAX_VERTS;
    if (nv > vertCap) nv = vertCap;
    for (int i = 0; i < nv; ++i) {
        verts[i].x = car->vlist[i].vx;
        verts[i].y = car->vlist[i].vy;
        verts[i].z = car->vlist[i].vz;
        verts[i].r = verts[i].g = verts[i].b = 0;
    }

    int op = 0;
    // GT3 body, then FT3 bottom, then B3 bottom (order irrelevant to sort).
    for (int i = 0; i < car->numGT3 && op < polyCap; ++i)
        EmitCarTri(&car->pGT3[i], 0, palette, civClut, &polys[op++]);
    for (int i = 0; i < car->numFT3 && op < polyCap; ++i)
        EmitCarTri(&car->pFT3[i], 1, palette, civClut, &polys[op++]);
    for (int i = 0; i < car->numB3 && op < polyCap; ++i)
        EmitCarTri(&car->pB3[i], 2, palette, civClut, &polys[op++]);

    if (outVerts) *outVerts = nv;
    if (outPolys) *outPolys = op;
    return 0;
}

// ---------------------------------------------------------------------------
// MATRIX (int16 rot /4096, int32 trans) -> float row-vector world[4][4].
// ---------------------------------------------------------------------------
static void MatWorldFromGte(const MATRIX *m, float world[4][4]) {
    memset(world, 0, 16 * sizeof(float));
    // Row-vector convention (mul(pos, world) in the shader): rotation in the
    // top-left 3x3, translation in the LAST ROW (world[3][0..2]). The GTE MATRIX
    // is fixed-point (4096 = 1.0).
    world[0][0] = m->m[0][0] / 4096.0f; world[0][1] = m->m[0][1] / 4096.0f; world[0][2] = m->m[0][2] / 4096.0f;
    world[1][0] = m->m[1][0] / 4096.0f; world[1][1] = m->m[1][1] / 4096.0f; world[1][2] = m->m[1][2] / 4096.0f;
    world[2][0] = m->m[2][0] / 4096.0f; world[2][1] = m->m[2][1] / 4096.0f; world[2][2] = m->m[2][2] / 4096.0f;
    world[3][0] = (float)m->t[0]; world[3][1] = (float)m->t[1]; world[3][2] = (float)m->t[2];
    world[3][3] = 1.0f;
}

// out = a * b (row-vector).
static void MatMul(const float a[4][4], const float b[4][4], float out[4][4]) {
    float t[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[i][k] * b[k][j];
            t[i][j] = s;
        }
    memcpy(out, t, sizeof(t));
}

// ---------------------------------------------------------------------------
// Render a DrawCommand[] list through per-eye -> composite.
// ---------------------------------------------------------------------------
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
                             const char *bmpOut,
                             const float (*customView)[4]) {
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);
    int iw = Dx11Renderer_GetInternalWidth(ren), ih = Dx11Renderer_GetInternalHeight(ren);
    int w = Dx11Renderer_GetWindowWidth(ren), h = Dx11Renderer_GetWindowHeight(ren);

    Dx11StereoEye eyes[2] = { DX11STEREO_EYE_LEFT, DX11STEREO_EYE_RIGHT };

    for (int e = 0; e < 2; ++e) {
        if (Dx11Renderer_BeginFrame(ren) != 0) return 1;
        Dx11Renderer_BindOffscreen(ren, e);
        float base[4] = { 0, 0, 0, 1 };
        ctx->ClearRenderTargetView(Dx11Renderer_GetOffscreenRTV(ren, e), base);
        ctx->ClearDepthStencilView(Dx11Renderer_GetDSV(ren),
                                   D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        float view[4][4], vp[4][4];
        if (customView) {
            memcpy(view, customView, sizeof(view));
        } else {
            Dx11Stereo_ViewMatrix(camPos, yawRad, eyes[e], sep, swap, view);
        }
        MatMul(view, proj, vp);

        Dx11DrawCmds_BeginFrame(cmds);
        Dx11Res_BeginFrame(res);
        Dx11DrawCmds_SetViewProj(cmds, vp);

        for (int i = 0; i < numCmds; ++i) {
            const DrawCommand *dc = &drawCmds[i];
            if (!dc->mesh && !dc->carModel) continue;

            Dx11ModelVertex *verts = NULL;
            Dx11ModelPoly *mpolys = NULL;
            int ov = 0, op = 0;
            if (dc->carModel) {
                // Car body: convert the CAR_MODEL (GT3/FT3/B3 triangles + dented
                // vlist) via the car mesh path; palette picks the civ_clut color.
                const GFCarModel *car = (const GFCarModel *)dc->carModel;
                int np = car->numGT3 + car->numFT3 + car->numB3;
                verts = (Dx11ModelVertex *)malloc((size_t)GFCAR_MAX_VERTS * sizeof(Dx11ModelVertex));
                mpolys = (Dx11ModelPoly *)malloc((size_t)(np ? np : 1) * sizeof(Dx11ModelPoly));
                if (!verts || !mpolys) { free(mpolys); free(verts); return 1; }
                Dx11GameFeed_CarModelToMesh(dc->carModel, dc->palette, civClut,
                                            verts, GFCAR_MAX_VERTS, mpolys, np, &ov, &op);
            } else {
                int nv = dc->mesh->num_vertices, np = dc->mesh->num_polys;
                verts = (Dx11ModelVertex *)malloc((size_t)nv * sizeof(Dx11ModelVertex));
                mpolys = (Dx11ModelPoly *)malloc((size_t)np * sizeof(Dx11ModelPoly));
                if (!verts || !mpolys) { free(mpolys); free(verts); return 1; }
                unsigned char flat[3] = { 255, 255, 255 };
                if (cmdColors) { flat[0] = cmdColors[i][0]; flat[1] = cmdColors[i][1]; flat[2] = cmdColors[i][2]; }
                Dx11GameFeed_ModelToMesh(dc->mesh, flat, verts, nv, mpolys, np, &ov, &op, tpages);
            }
            Dx11ModelMesh mesh = { verts, ov, mpolys, op };
            float world[4][4];
            MatWorldFromGte(&dc->world, world);
            int submitted = 0;
            Dx11ModelAdapter_Submit(res, tex, cmds, &mesh, world, texUser, texResolve, &submitted);
            free(mpolys);
            free(verts);
        }

        Dx11DrawCmds_Execute(cmds, ctx);
    }

    // Composite the two eye RTs into the backbuffer and capture.
    Dx11Renderer_BindBackbuffer(ren);
    float base[4] = { 0, 0, 0, 1 };
    ctx->ClearRenderTargetView(Dx11Renderer_GetBackbufferRTV(ren), base);
    Dx11Composite_Composite(composite, ctx, mode, swap,
                            Dx11Renderer_GetOffscreenSRV(ren, 0),
                            Dx11Renderer_GetOffscreenSRV(ren, 1),
                            w, h);
    if (bmpOut)   // optional: the in-game consumer passes NULL (no per-frame BMP)
        Dx11Renderer_CaptureToBMP(ren, NULL, bmpOut, NULL);
    Dx11Renderer_Present(ren);
    return 0;
}
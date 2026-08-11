// dx11_gamefeed.c — T5.1 in-game DX11 renderer integration.
//
// Implements Dx11GameFeed (see dx11_gamefeed.h): the game-facing renderer
// integration that consumes the real DrawCommand list and drives the full DX11
// pipeline (MODEL -> Dx11ModelAdapter -> arena -> executor -> per-eye -> composite).
// Compiled as C++ by premake (compileas "C++").

#include "dx11_gamefeed.h"

#include "mdl.h"           // MODEL, PL_POLYFT4, SVECTOR
#include "libgte.h"        // MATRIX

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// ---------------------------------------------------------------------------
// MODEL -> adapter raw mesh (flat-textured-quad subset).
// ---------------------------------------------------------------------------
int Dx11GameFeed_ModelToMesh(const struct MODEL *model, const unsigned char flatRGB[3],
                             Dx11ModelVertex *verts, int vertCap,
                             Dx11ModelPoly *polys, int polyCap,
                             int *outVerts, int *outPolys) {
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
                mp->u0 = poly->uv0.u; mp->v0 = poly->uv0.v;
                mp->u1 = poly->uv1.u; mp->v1 = poly->uv1.v;
                mp->u2 = poly->uv2.u; mp->v2 = poly->uv2.v;
                mp->u3 = poly->uv3.u; mp->v3 = poly->uv3.v;
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
// MATRIX (int16 rot /4096, int32 trans) -> float row-vector world[4][4].
// ---------------------------------------------------------------------------
static void MatWorldFromGte(const MATRIX *m, float world[4][4]) {
    memset(world, 0, 16 * sizeof(float));
    world[0][0] = m->m[0][0] / 4096.0f; world[0][1] = m->m[0][1] / 4096.0f; world[0][2] = m->m[0][2] / 4096.0f; world[0][3] = (float)m->t[0];
    world[1][0] = m->m[1][0] / 4096.0f; world[1][1] = m->m[1][1] / 4096.0f; world[1][2] = m->m[1][2] / 4096.0f; world[1][3] = (float)m->t[1];
    world[2][0] = m->m[2][0] / 4096.0f; world[2][1] = m->m[2][1] / 4096.0f; world[2][2] = m->m[2][2] / 4096.0f; world[2][3] = (float)m->t[2];
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
                             const char *bmpOut) {
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
        Dx11Stereo_ViewMatrix(camPos, yawRad, eyes[e], sep, swap, view);
        MatMul(view, proj, vp);

        Dx11DrawCmds_BeginFrame(cmds);
        Dx11Res_BeginFrame(res);
        Dx11DrawCmds_SetViewProj(cmds, vp);

        for (int i = 0; i < numCmds; ++i) {
            const DrawCommand *dc = &drawCmds[i];
            if (!dc->mesh) continue;
            int nv = dc->mesh->num_vertices, np = dc->mesh->num_polys;
            Dx11ModelVertex *verts = (Dx11ModelVertex *)malloc((size_t)nv * sizeof(Dx11ModelVertex));
            Dx11ModelPoly *mpolys = (Dx11ModelPoly *)malloc((size_t)np * sizeof(Dx11ModelPoly));
            if (!verts || !mpolys) { free(mpolys); free(verts); return 1; }
            int ov = 0, op = 0;
            unsigned char flat[3] = { 255, 255, 255 };
            if (cmdColors) { flat[0] = cmdColors[i][0]; flat[1] = cmdColors[i][1]; flat[2] = cmdColors[i][2]; }
            Dx11GameFeed_ModelToMesh(dc->mesh, flat, verts, nv, mpolys, np, &ov, &op);
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
    Dx11Renderer_CaptureToBMP(ren, NULL, bmpOut, NULL);
    Dx11Renderer_Present(ren);
    return 0;
}
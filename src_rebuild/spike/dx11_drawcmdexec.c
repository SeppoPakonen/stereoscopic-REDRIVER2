// dx11_drawcmdexec.c — T1.5 draw-command executor implementation.
//
// Consumes a per-frame list of draw commands (geometry already in the T1.2
// arena as vertex/index ranges), owns projection (per-command world CB),
// frustum culling, sorting (opaque by material/state front-to-back, translucent
// back-to-front by sortKey), batching and the final DrawIndexed emission on top
// of dx11_resources / dx11_textures / dx11_shaders. Compiled as C++ by premake.

#define WIN32_LEAN_AND_MEAN
#include "dx11_drawcmdexec.h"

#include <d3d11_1.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DX11DCMD_DEFAULT_MAX_COMMANDS 4096

// One executor record: a submitted command + its world-CB slot.
typedef struct {
    Dx11DrawCmdItem *item;
    int worldSlot;
    int culled;
} Dx11CmdRec;

struct Dx11DrawCmds {
    ID3D11Device        *dev;
    ID3D11DeviceContext *ctx;
    Dx11Res    *res;
    Dx11Tex    *tex;
    Dx11Shaders *sh;

    Dx11DrawCmdItem *items;
    int count, cap;

    Dx11CmdRec *recs;      // per-command records (sorted in place during Execute)
    float vp[16];
    int vpSet;
    int culledCount;
    int drawCallCount;
};

// ---------------------------------------------------------------------------
// Matrix helpers (row-vector x column-major).
// ---------------------------------------------------------------------------
// Replicates HLSL `mul(p, M)` on a matrix stored in the C++ row-major layout
// that the device passes to the shader (which reads it transposed). The shader
// computes clip[j] = sum_i p[i]*M[j][i], so we access M[j][i] here.
static void XformPoint(const float p[4], const float m[4][4], float out[4]) {
    for (int j = 0; j < 4; ++j)
        out[j] = p[0] * m[j][0] + p[1] * m[j][1] + p[2] * m[j][2] + p[3] * m[j][3];
}

// Frustum cull: transform the 8 bbox corners through world then viewProj (as
// the VS does) and test the 6 clip planes. Returns 1 if fully outside any.
static int IsCulled(const Dx11DrawCmdItem *it, const float vp[16]) {
    float mn[3] = { it->bboxMin[0], it->bboxMin[1], it->bboxMin[2] };
    float mx[3] = { it->bboxMax[0], it->bboxMax[1], it->bboxMax[2] };
    float clip[8][4];
    int ci = 0;
    for (int ix = 0; ix < 2; ++ix)
        for (int iy = 0; iy < 2; ++iy)
            for (int iz = 0; iz < 2; ++iz) {
                float p[4] = { ix ? mx[0] : mn[0], iy ? mx[1] : mn[1], iz ? mx[2] : mn[2], 1 };
                float wp[4];
                XformPoint(p, (const float (*)[4])it->world, wp);
                XformPoint(wp, (const float (*)[4])vp, clip[ci++]);
            }

    // For each plane, count how many corners are outside.
    int outside[6] = { 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < 8; ++i) {
        float x = clip[i][0], y = clip[i][1], z = clip[i][2], w = clip[i][3];
        if (x < -w) outside[0]++;
        if (x >  w) outside[1]++;
        if (y < -w) outside[2]++;
        if (y >  w) outside[3]++;
        if (z <  0) outside[4]++;
        if (z >  w) outside[5]++;
    }
    for (int p = 0; p < 6; ++p)
        if (outside[p] == 8)
            return 1;
    return 0;
}

// ---------------------------------------------------------------------------
// Sorting.
// ---------------------------------------------------------------------------
// Material/state key for opaque batching + sorting.
static unsigned long long MaterialKey(const Dx11DrawCmdItem *it) {
    return ((unsigned long long)(unsigned char)it->texture << 32) |
           (((unsigned long long)it->blend) << 24) |
           (((unsigned long long)it->shade) << 16) |
           (((unsigned long long)it->twoSided) << 8) |
           ((unsigned long long)it->nodepth);
}

// Order records: opaque (smaller) before translucent (larger); opaque by
// materialKey then sortKey asc; translucent by sortKey desc (back-to-front).
static int CompareRecs(const Dx11CmdRec *a, const Dx11CmdRec *b) {
    int ta = a->item->translucent, tb = b->item->translucent;
    if (ta != tb) return ta - tb;            // opaque first
    if (!ta) {
        unsigned long long ka = MaterialKey(a->item), kb = MaterialKey(b->item);
        if (ka != kb) return ka < kb ? -1 : 1;
        if (a->item->sortKey != b->item->sortKey)
            return a->item->sortKey - b->item->sortKey;   // front-to-back
        return 0;
    }
    // translucent: larger sortKey first (far drawn first).
    if (a->item->sortKey != b->item->sortKey)
        return a->item->sortKey > b->item->sortKey ? -1 : 1;
    return 0;
}

// Insertion sort (small per-frame lists; stable-ish, matches the comparator).
static void SortRecs(Dx11CmdRec *recs, int n) {
    for (int i = 1; i < n; ++i) {
        Dx11CmdRec key = recs[i];
        int j = i - 1;
        while (j >= 0 && CompareRecs(&recs[j], &key) > 0) {
            recs[j + 1] = recs[j];
            --j;
        }
        recs[j + 1] = key;
    }
}

// Same material/state + same world transform + index-contiguous (batchable).
static int CanBatch(const Dx11CmdRec *a, const Dx11CmdRec *b) {
    const Dx11DrawCmdItem *x = a->item, *y = b->item;
    if (x->texture != y->texture || x->blend != y->blend || x->shade != y->shade ||
        x->twoSided != y->twoSided || x->nodepth != y->nodepth)
        return 0;
    if (memcmp(x->world, y->world, sizeof(x->world)) != 0)
        return 0;
    if (memcmp(x->flatColor, y->flatColor, sizeof(x->flatColor)) != 0)
        return 0;
    // index-contiguous: this command starts where the previous ended.
    return (x->indexOffset + x->indexCount == y->indexOffset);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
Dx11DrawCmds *Dx11DrawCmds_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                                  Dx11Res *res, Dx11Tex *tex, Dx11Shaders *sh,
                                  const Dx11DrawCmdsConfig *cfg,
                                  Dx11DrawCmdsResult *outResult) {
    if (outResult) *outResult = DX11DCMD_ERR_ARG;
    int max = DX11DCMD_DEFAULT_MAX_COMMANDS;
    if (cfg && cfg->max_commands > 0) max = cfg->max_commands;

    Dx11DrawCmds *c = (Dx11DrawCmds *)calloc(1, sizeof(Dx11DrawCmds));
    if (!c) return NULL;
    c->dev = dev; c->ctx = ctx; c->res = res; c->tex = tex; c->sh = sh;
    c->cap = max;
    c->items = (Dx11DrawCmdItem *)calloc((size_t)max, sizeof(Dx11DrawCmdItem));
    c->recs = (Dx11CmdRec *)calloc((size_t)max, sizeof(Dx11CmdRec));
    if (!c->items || !c->recs) { free(c->items); free(c->recs); free(c); return NULL; }
    if (outResult) *outResult = DX11DCMD_OK;
    return c;
}

void Dx11DrawCmds_Destroy(Dx11DrawCmds *c) {
    if (!c) return;
    free(c->recs);
    free(c->items);
    free(c);
}

void Dx11DrawCmds_BeginFrame(Dx11DrawCmds *c) {
    c->count = 0;
    c->vpSet = 0;
    c->culledCount = 0;
    c->drawCallCount = 0;
}

void Dx11DrawCmds_SetViewProj(Dx11DrawCmds *c, const float m[4][4]) {
    if (m) memcpy(c->vp, m, 16 * sizeof(float));
    c->vpSet = 1;
    Dx11Res_SetViewProj(c->res, m);
}

int Dx11DrawCmds_Submit(Dx11DrawCmds *c, const Dx11DrawCmdItem *item) {
    if (!c || !item || c->count >= c->cap)
        return 1;
    c->items[c->count] = *item;
    ++c->count;
    return 0;
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------
int Dx11DrawCmds_Execute(Dx11DrawCmds *c, ID3D11DeviceContext *ctx) {
    if (!c) return 0;
    if (!ctx) ctx = c->ctx;
    c->culledCount = 0;
    c->drawCallCount = 0;

    int n = c->count;
    if (n == 0) return 0;

    // Allocate a world-CB slot per command and compute culling.
    for (int i = 0; i < n; ++i) {
        Dx11CmdRec *r = &c->recs[i];
        r->item = &c->items[i];
        r->culled = (c->vpSet && IsCulled(r->item, c->vp)) ? 1 : 0;
        r->worldSlot = r->culled ? -1 : Dx11Res_AllocCB(c->res);
        if (r->worldSlot >= 0)
            Dx11Res_SetCB(c->res, r->worldSlot, (const float (*)[4])r->item->world);
        if (r->culled)
            ++c->culledCount;
    }

    // Sort the records (opaque before translucent, etc.).
    SortRecs(c->recs, n);

    // Upload the arena once (VB/IB + world CBs + viewProj).
    Dx11Res_Upload(c->res, ctx);

    // Walk the sorted records, batch contiguous same-material runs, emit.
    int emitted = 0;
    int batchStart = -1, batchIndexOff = 0, batchIndexCount = 0, batchSlot = -1;
    for (int i = 0; i < n; ++i) {
        Dx11CmdRec *r = &c->recs[i];
        if (r->culled || r->worldSlot < 0) {
            if (batchStart >= 0) {
                ctx->DrawIndexed((UINT)batchIndexCount, (UINT)batchIndexOff, 0);
                ++emitted;
                batchStart = -1;
            }
            continue;
        }
        const Dx11DrawCmdItem *it = r->item;

        if (batchStart >= 0 && CanBatch(&c->recs[batchStart], r)) {
            // extend batch
            batchIndexCount += it->indexCount;
            continue;
        }

        // flush previous batch
        if (batchStart >= 0) {
            ctx->DrawIndexed((UINT)batchIndexCount, (UINT)batchIndexOff, 0);
            ++emitted;
        }

        // start a new batch: bind state + geometry + world slot.
        Dx11Res_BindGeometry(c->res, ctx);
        Dx11Res_BindWorldSlot(c->res, ctx, r->worldSlot);
        Dx11Shaders_Bind(c->sh, ctx, (Dx11ShadersColor)it->shade);
        if (it->shade == DX11SH_COLOR_FLAT)
            Dx11Shaders_SetFlatColor(c->sh, ctx, it->flatColor[0], it->flatColor[1],
                                     it->flatColor[2], it->flatColor[3]);
        Dx11Shaders_SetBlend(c->sh, ctx, (Dx11ShadersBlend)it->blend);
        Dx11Shaders_SetRaster(c->sh, ctx, it->twoSided);
        Dx11Shaders_SetDepthOpaque(c->sh, ctx, (it->translucent || it->nodepth) ? 0 : 1);
        ID3D11SamplerState *samp = Dx11Res_GetSampler(c->res);
        ctx->PSSetSamplers(0, 1, &samp);
        ID3D11ShaderResourceView *srv = (it->texture >= 0)
            ? Dx11Tex_GetSRV(c->tex, it->texture)
            : Dx11Tex_GetWhiteSRV(c->tex);
        ctx->PSSetShaderResources(0, 1, &srv);

        batchStart = i;
        batchIndexOff = it->indexOffset;
        batchIndexCount = it->indexCount;
        batchSlot = r->worldSlot;
    }
    if (batchStart >= 0) {
        ctx->DrawIndexed((UINT)batchIndexCount, (UINT)batchIndexOff, 0);
        ++emitted;
    }

    c->drawCallCount = emitted;
    return emitted;
}

int Dx11DrawCmds_CulledCount(Dx11DrawCmds *c)    { return c ? c->culledCount : 0; }
int Dx11DrawCmds_DrawCallCount(Dx11DrawCmds *c)  { return c ? c->drawCallCount : 0; }
int Dx11DrawCmds_SubmittedCount(Dx11DrawCmds *c) { return c ? c->count : 0; }
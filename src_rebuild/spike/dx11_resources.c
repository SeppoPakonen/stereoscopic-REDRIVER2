// dx11_resources.c — T1.2 DX11 resource management implementation.
//
// Manages per-frame CPU arenas + their GPU counterparts (dynamic VB/IB, a
// constant-buffer arena for per-draw world matrices, and a view/proj CB).
// Compiled as C++ by premake (compileas "C++", like dx11_renderer.c).

#define WIN32_LEAN_AND_MEAN
#include "dx11_resources.h"

#include <d3d11_1.h>
#include <windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DX11RES_DEFAULT_VERT_MAX  (1 << 16)   // 65536 vertices
#define DX11RES_DEFAULT_IDX_MAX   (1 << 18)   // 262144 indices
#define DX11RES_DEFAULT_CB_SLOTS  4096        // 4096 x 4x4 world matrices

struct Dx11Res {
    ID3D11Device        *dev;
    ID3D11DeviceContext *ctx;

    // Vertex arena (CPU) + dynamic VB.
    Dx11ResVertex *verts;
    int vertCount, vertCap, vertMax;
    Dx11ResVertex *vbMappedSlot; // not used; VB is fixed-size at vertMax

    // Index arena (CPU) + dynamic IB.
    unsigned short *inds;
    int idxCount, idxCap, idxMax;

    ID3D11Buffer *vb;
    ID3D11Buffer *ib;

    // Constant-buffer pool (b1): one 64-byte DEFAULT CB per world-matrix slot.
    // A small pool of per-draw CBs is more robust than a single arena bound
    // with VSSetConstantBuffers1 (which proved unreliable at runtime here).
    float (*cbMirror)[16];   // cbSlots x 16 floats
    int cbSlots, cbCursor;
    ID3D11Buffer **cbPool;   // cbSlots x ID3D11Buffer*
    ID3D11Buffer *cbViewProj; // b0: view/proj
    float vp[16];
    int vpSet;

    ID3D11SamplerState *sampler;

    // Stats.
    int totalVertsSubmitted;
    int totalFrames;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
Dx11ResConfig Dx11Res_DefaultConfig(void) {
    Dx11ResConfig c;
    c.vertex_capacity = 1024;
    c.vertex_max = DX11RES_DEFAULT_VERT_MAX;
    c.index_capacity = 2048;
    c.index_max = DX11RES_DEFAULT_IDX_MAX;
    c.cb_slots = DX11RES_DEFAULT_CB_SLOTS;
    return c;
}

static int GrowVertices(Dx11Res *r) {
    if (r->vertCap >= r->vertMax)
        return 0; // at hard cap
    int newCap = r->vertCap ? r->vertCap * 2 : 1024;
    if (newCap > r->vertMax)
        newCap = r->vertMax;
    Dx11ResVertex *grown = (Dx11ResVertex *)realloc(
        r->verts, (size_t)newCap * sizeof(Dx11ResVertex));
    if (!grown)
        return 0;
    r->verts = grown;
    r->vertCap = newCap;
    return 1;
}

static int GrowIndices(Dx11Res *r) {
    if (r->idxCap >= r->idxMax)
        return 0;
    int newCap = r->idxCap ? r->idxCap * 2 : 2048;
    if (newCap > r->idxMax)
        newCap = r->idxMax;
    unsigned short *grown = (unsigned short *)realloc(
        r->inds, (size_t)newCap * sizeof(unsigned short));
    if (!grown)
        return 0;
    r->inds = grown;
    r->idxCap = newCap;
    return 1;
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------
Dx11Res *Dx11Res_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                        const Dx11ResConfig *cfg, Dx11ResResult *outResult) {
    if (outResult) *outResult = DX11RES_ERR_CB_CONTEXT1;

    Dx11ResConfig d = Dx11Res_DefaultConfig();
    if (cfg) {
        if (cfg->vertex_capacity > 0) d.vertex_capacity = cfg->vertex_capacity;
        if (cfg->vertex_max > 0)      d.vertex_max = cfg->vertex_max;
        if (cfg->index_capacity > 0)  d.index_capacity = cfg->index_capacity;
        if (cfg->index_max > 0)       d.index_max = cfg->index_max;
        if (cfg->cb_slots > 0)        d.cb_slots = cfg->cb_slots;
    }

    Dx11Res *r = (Dx11Res *)calloc(1, sizeof(Dx11Res));
    if (!r)
        return NULL;
    r->dev = dev;
    r->ctx = ctx ? ctx : NULL;

    // Hoisted locals (declared before any goto so the cleanup labels don't
    // cross initializations).
    D3D11_BUFFER_DESC bd = {};
    D3D11_SUBRESOURCE_DATA sd = {};
    int stride = (int)sizeof(Dx11ResVertex);
    float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    D3D11_SAMPLER_DESC sdsc = {};

    r->vertMax = d.vertex_max;
    r->idxMax = d.index_max;
    r->cbSlots = d.cb_slots;
    r->vertCap = d.vertex_capacity;   // initial arena capacity (grows geometrically)
    r->idxCap = d.index_capacity;
    r->verts = (Dx11ResVertex *)malloc((size_t)d.vertex_capacity * sizeof(Dx11ResVertex));
    r->inds = (unsigned short *)malloc((size_t)d.index_capacity * sizeof(unsigned short));
    if (!r->verts || !r->inds) { if (outResult) *outResult = DX11RES_ERR_OOM; goto fail_alloc; }

    // GPU dynamic vertex buffer sized to the hard cap (bounded).
    bd.ByteWidth = (UINT)((size_t)d.vertex_max * stride);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    {
        HRESULT h = dev->CreateBuffer(&bd, NULL, &r->vb);
        if (FAILED(h)) { if (outResult) *outResult = DX11RES_ERR_VB; goto fail_vb; }
    }

    bd.ByteWidth = (UINT)((size_t)d.index_max * sizeof(unsigned short));
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    {
        HRESULT h = dev->CreateBuffer(&bd, NULL, &r->ib);
        if (FAILED(h)) { if (outResult) *outResult = DX11RES_ERR_VB; goto fail_ib; }
    }

    // Per-draw world CB pool (b1): cbSlots DEFAULT buffers of 64 bytes each.
    r->cbPool = (ID3D11Buffer **)calloc((size_t)d.cb_slots, sizeof(ID3D11Buffer *));
    if (!r->cbPool) { if (outResult) *outResult = DX11RES_ERR_OOM; goto fail_cb; }
    bd.ByteWidth = 64;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.CPUAccessFlags = 0;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sd.pSysMem = ident;
    for (int i = 0; i < d.cb_slots; ++i) {
        HRESULT h = dev->CreateBuffer(&bd, &sd, &r->cbPool[i]);
        if (FAILED(h)) { if (outResult) *outResult = DX11RES_ERR_CB; goto fail_cb; }
    }

    // View/proj CB (b0): 64 bytes. DEFAULT usage (updated via UpdateSubresource,
    // which is invalid on a dynamic buffer). CPUAccessFlags must be 0 for a
    // DEFAULT buffer (it was left WRITE from the dynamic buffers above).
    bd.ByteWidth = 64;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.CPUAccessFlags = 0;
    sd.pSysMem = ident;
    {
        HRESULT h = dev->CreateBuffer(&bd, &sd, &r->cbViewProj);
        if (FAILED(h)) { if (outResult) *outResult = DX11RES_ERR_CB; goto fail_vp; }
    }

    // Default (point) sampler.
    sdsc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sdsc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sdsc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sdsc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sdsc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(dev->CreateSamplerState(&sdsc, &r->sampler))) { if (outResult) *outResult = DX11RES_ERR_CB; goto fail_sampler; }

    r->cbMirror = (float (*)[16])calloc((size_t)d.cb_slots, 16 * sizeof(float));
    if (!r->cbMirror) { if (outResult) *outResult = DX11RES_ERR_OOM; goto fail_mirror; }

    if (outResult) *outResult = DX11RES_OK;
    return r;

fail_mirror:
    r->sampler->Release();
fail_sampler:
    r->cbViewProj->Release();
fail_vp:
    if (r->cbPool) {
        for (int i = 0; i < r->cbSlots; ++i)
            if (r->cbPool[i]) r->cbPool[i]->Release();
        free(r->cbPool);
    }
fail_cb:
    r->ib->Release();
fail_ib:
    r->vb->Release();
fail_vb:
fail_ctx1:
    free(r->verts);
    free(r->inds);
    free(r);
    return NULL;
fail_alloc:
    free(r->verts);
    free(r->inds);
    free(r);
    return NULL;
}

void Dx11Res_Destroy(Dx11Res *r) {
    if (!r)
        return;
    free(r->cbMirror);
    free(r->inds);
    free(r->verts);
    if (r->sampler) r->sampler->Release();
    if (r->cbViewProj) r->cbViewProj->Release();
    if (r->cbPool) {
        for (int i = 0; i < r->cbSlots; ++i)
            if (r->cbPool[i]) r->cbPool[i]->Release();
        free(r->cbPool);
    }
    if (r->ib) r->ib->Release();
    if (r->vb) r->vb->Release();
    // ctx is borrowed (owned by the caller); do not Release it.
    free(r);
}

// ---------------------------------------------------------------------------
// Per-frame / arena
// ---------------------------------------------------------------------------
void Dx11Res_BeginFrame(Dx11Res *r) {
    r->vertCount = 0;
    r->idxCount = 0;
    r->cbCursor = 0;
    r->vpSet = 0;
    ++r->totalFrames;
}

int Dx11Res_PushVertex(Dx11Res *r, const Dx11ResVertex *v) {
    if (r->vertCount >= r->vertCap)
        if (!GrowVertices(r))
            return -1;
    r->verts[r->vertCount++] = *v;
    ++r->totalVertsSubmitted;
    return r->vertCount - 1;
}

int Dx11Res_PushIndex(Dx11Res *r, unsigned short idx) {
    if (r->idxCount >= r->idxCap)
        if (!GrowIndices(r))
            return -1;
    r->inds[r->idxCount++] = idx;
    return r->idxCount - 1;
}

int Dx11Res_VertexCount(Dx11Res *r) { return r->vertCount; }
int Dx11Res_IndexCount(Dx11Res *r)  { return r->idxCount; }

int Dx11Res_AllocCB(Dx11Res *r) {
    if (r->cbCursor >= r->cbSlots)
        return -1;
    return r->cbCursor++;
}

void Dx11Res_SetCB(Dx11Res *r, int slot, const float m[4][4]) {
    if (slot < 0 || slot >= r->cbCursor)
        return;
    memcpy(r->cbMirror[slot], m, 16 * sizeof(float));
}

void Dx11Res_SetViewProj(Dx11Res *r, const float m[4][4]) {
    if (m)
        memcpy(r->vp, m, 16 * sizeof(float));
    r->vpSet = 1;
}

// ---------------------------------------------------------------------------
// Upload (CPU -> GPU), once per frame after submission.
// ---------------------------------------------------------------------------
void Dx11Res_Upload(Dx11Res *r, ID3D11DeviceContext *ctx) {
    if (!ctx)
        ctx = r->ctx;

    // Vertex buffer (WRITE_DISCARD).
    D3D11_MAPPED_SUBRESOURCE map = {};
    if (r->vertCount > 0 &&
        SUCCEEDED(ctx->Map(r->vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
        memcpy(map.pData, r->verts, (size_t)r->vertCount * sizeof(Dx11ResVertex));
        ctx->Unmap(r->vb, 0);
    }

    // Index buffer (WRITE_DISCARD).
    if (r->idxCount > 0 &&
        SUCCEEDED(ctx->Map(r->ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
        memcpy(map.pData, r->inds, (size_t)r->idxCount * sizeof(unsigned short));
        ctx->Unmap(r->ib, 0);
    }

    // Per-draw world CBs (b1): update each used slot's buffer.
    for (int i = 0; i < r->cbCursor; ++i)
        ctx->UpdateSubresource(r->cbPool[i], 0, NULL, r->cbMirror[i], 0, 0);

    // View/proj CB (b0).
    if (r->vpSet) {
        ctx->UpdateSubresource(r->cbViewProj, 0, NULL, r->vp, 0, 0);
        r->vpSet = 0;
    }
}

void Dx11Res_BindGeometry(Dx11Res *r, ID3D11DeviceContext *ctx) {
    if (!ctx)
        ctx = r->ctx;
    UINT stride = (UINT)sizeof(Dx11ResVertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &r->vb, &stride, &offset);
    ctx->IASetIndexBuffer(r->ib, DXGI_FORMAT_R16_UINT, 0);
    ctx->VSSetConstantBuffers(0, 1, &r->cbViewProj); // b0 = view/proj
}

void Dx11Res_BindWorldSlot(Dx11Res *r, ID3D11DeviceContext *ctx, int slot) {
    if (!ctx)
        ctx = r->ctx;
    if (slot < 0 || slot >= r->cbCursor)
        slot = 0;
    ctx->VSSetConstantBuffers(1, 1, &r->cbPool[slot]);
}

ID3D11SamplerState *Dx11Res_GetSampler(Dx11Res *r) { return r->sampler; }

void Dx11Res_BindSRV(Dx11Res *r, ID3D11DeviceContext *ctx, ID3D11ShaderResourceView *srv) {
    if (!ctx)
        ctx = r->ctx;
    ctx->PSSetShaderResources(0, 1, &srv);
}

int Dx11Res_VertexCapacity(Dx11Res *r)   { return r->vertCap; }
int Dx11Res_IndexCapacity(Dx11Res *r)    { return r->idxCap; }
int Dx11Res_CBSlotCount(Dx11Res *r)      { return r->cbSlots; }
int Dx11Res_CBSlotsUsed(Dx11Res *r)      { return r->cbCursor; }
int Dx11Res_TotalVertsSubmitted(Dx11Res *r) { return r->totalVertsSubmitted; }
int Dx11Res_TotalFrames(Dx11Res *r)      { return r->totalFrames; }

// DEBUG: read back the first few uploaded vertices and the view-proj matrix.
void Dx11Res_DebugDump(Dx11Res *r, ID3D11DeviceContext *ctx, const char *path) {
    if (!ctx) ctx = r->ctx;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "vp={%.3f,%.3f,%.3f|%.3f} set=%d\n",
            r->vp[0], r->vp[5], r->vp[10], r->vp[15], r->vpSet);
    fprintf(f, "verts=%d idx=%d cbCursor=%d\n", r->vertCount, r->idxCount, r->cbCursor);

    // Read back the VB to verify the uploaded data.
    D3D11_BUFFER_DESC bd = {};
    r->vb->GetDesc(&bd);
    bd.Usage = D3D11_USAGE_STAGING;
    bd.BindFlags = 0;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Buffer *stage = NULL;
    if (SUCCEEDED(r->dev->CreateBuffer(&bd, NULL, &stage))) {
        ctx->CopyResource(stage, r->vb);
        D3D11_MAPPED_SUBRESOURCE map = {};
        if (SUCCEEDED(ctx->Map(stage, 0, D3D11_MAP_READ, 0, &map))) {
            const Dx11ResVertex *v = (const Dx11ResVertex *)map.pData;
            int n = r->vertCount < 4 ? r->vertCount : 4;
            for (int i = 0; i < n; ++i)
                fprintf(f, "v[%d] pos=(%.3f,%.3f,%.3f) col=(%.2f,%.2f,%.2f) uv=(%.2f,%.2f)\n",
                        i, v[i].x, v[i].y, v[i].z, v[i].r, v[i].g, v[i].b, v[i].u, v[i].v);
            ctx->Unmap(stage, 0);
        }
        stage->Release();
    }
    fclose(f);
}
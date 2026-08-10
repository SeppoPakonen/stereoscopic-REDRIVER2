// dx11_textures.c — T1.3 DX11 texture system implementation.
//
// PSX VRAM staging + tpage/clut decode-to-RGBA baking into per-region
// R8G8B8A8_UNORM textures + SRVs, plus a white 1x1 substitute. Compiled as
// C++ by premake (compileas "C++", like dx11_renderer.c / dx11_resources.c).

#define WIN32_LEAN_AND_MEAN
#include "dx11_textures.h"

#include <d3d11_1.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DX11TEX_DEFAULT_MAX_TEXTURES 256

// One baked region: key + the GPU texture/SRV.
typedef struct {
    unsigned short tpage, clut;
    int texX, texY, w, h;
    ID3D11Texture2D *tex;
    ID3D11ShaderResourceView *srv;
    int used;
} Dx11TexEntry;

struct Dx11Tex {
    ID3D11Device        *dev;
    ID3D11DeviceContext *ctx;

    // PSX VRAM staging (raw 16-bit words).
    unsigned short *vram;

    // Baked SRV cache.
    Dx11TexEntry *entries;
    int entryCount, entryCap;

    // White 1x1 substitute for untextured surfaces.
    ID3D11Texture2D *whiteTex;
    ID3D11ShaderResourceView *whiteSRV;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
Dx11TexConfig Dx11Tex_DefaultConfig(void) {
    Dx11TexConfig c;
    c.max_textures = DX11TEX_DEFAULT_MAX_TEXTURES;
    return c;
}

// Bounds-clamped VRAM read.
static unsigned short VWord(Dx11Tex *t, int x, int y) {
    if (x < 0)       x = 0;
    if (x >= DX11TEX_VRAM_W) x = DX11TEX_VRAM_W - 1;
    if (y < 0)       y = 0;
    if (y >= DX11TEX_VRAM_H) y = DX11TEX_VRAM_H - 1;
    return t->vram[y * DX11TEX_VRAM_W + x];
}

// ---------------------------------------------------------------------------
// Decode: tpage/clut region -> RGBA (the exact PSX / psyx decode math).
// ---------------------------------------------------------------------------
//  * GET_TPAGE_FORMAT(tpage) = (tpage>>7)&0x3
//  * tpage X = (tpage<<6)&0x3c0, tpage Y = ((tpage<<4)&0x100)+((tpage>>2)&0x200)
//  * clut X = (clut&0x3f)<<4, clut Y = clut>>6
//  * 4-bit: 4 px/word (byte=(x&2)>>1, nibble=x&1, low nibble=even pixel)
//  * 8-bit: 2 px/word (low byte=even pixel, high byte=odd pixel)
//  * 16-bit: 1 px/word (direct RGB555)
//  * palette index -> vram[clutY*1024 + clutX + idx]
//  * RGB555 -> RGBA: R=(c&31)<<3, G=((c>>5)&31)<<3, B=((c>>10)&31)<<3, A=255
static void DecodeToRGBA(Dx11Tex *t, unsigned short tpage, unsigned short clut,
                         int texX, int texY, int w, int h, unsigned char *rgba) {
    int fmt = (tpage >> 7) & 3;
    int tpX = (tpage << 6) & 0x3c0;
    int tpY = ((tpage << 4) & 0x100) + ((tpage >> 2) & 0x200);
    int cX  = (clut & 0x3f) << 4;
    int cY  = clut >> 6;
    int px0 = tpX + texX;
    int py0 = tpY + texY;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int ax = px0 + x, ay = py0 + y;
            unsigned short col;
            if (fmt == DX11TEX_FMT_4BIT) {
                unsigned short word = VWord(t, ax / 4, ay);
                int byte = (word >> (((ax & 2) >> 1) * 8)) & 0xff;
                int idx  = (ax & 1) ? (byte >> 4) : (byte & 0x0f);
                col = VWord(t, cX + idx, cY);
            } else if (fmt == DX11TEX_FMT_8BIT) {
                unsigned short word = VWord(t, ax / 2, ay);
                int byte = (word >> ((ax & 1) * 8)) & 0xff;
                col = VWord(t, cX + byte, cY);
            } else { // 16-bit: direct RGB555
                col = VWord(t, ax, ay);
            }
            unsigned char *p = rgba + (y * w + x) * 4;
            p[0] = (unsigned char)((col & 31) << 3);
            p[1] = (unsigned char)(((col >> 5) & 31) << 3);
            p[2] = (unsigned char)(((col >> 10) & 31) << 3);
            p[3] = 255;
        }
    }
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------
Dx11Tex *Dx11Tex_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                        const Dx11TexConfig *cfg, Dx11TexResult *outResult) {
    if (outResult) *outResult = DX11TEX_ERR_ARG;

    Dx11TexConfig c = Dx11Tex_DefaultConfig();
    if (cfg && cfg->max_textures > 0)
        c.max_textures = cfg->max_textures;

    Dx11Tex *t = (Dx11Tex *)calloc(1, sizeof(Dx11Tex));
    if (!t) { if (outResult) *outResult = DX11TEX_ERR_OUT_OF_MEMORY; return NULL; }
    t->dev = dev;
    t->ctx = ctx;

    // Hoisted locals (declared before any goto so the cleanup labels don't
    // cross initializations).
    D3D11_TEXTURE2D_DESC td = {};
    D3D11_SUBRESOURCE_DATA srd = {};
    unsigned char whitePx[4] = { 255, 255, 255, 255 };

    t->vram = (unsigned short *)calloc((size_t)DX11TEX_VRAM_W * DX11TEX_VRAM_H,
                                       sizeof(unsigned short));
    t->entries = (Dx11TexEntry *)calloc((size_t)c.max_textures, sizeof(Dx11TexEntry));
    if (!t->vram || !t->entries) { if (outResult) *outResult = DX11TEX_ERR_OUT_OF_MEMORY; goto fail_alloc; }
    t->entryCap = c.max_textures;

    // White 1x1 substitute texture + SRV.
    td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    srd.pSysMem = whitePx;
    srd.SysMemPitch = 4;
    if (FAILED(dev->CreateTexture2D(&td, &srd, &t->whiteTex))) { if (outResult) *outResult = DX11TEX_ERR_DEVICE; goto fail_white; }
    if (FAILED(dev->CreateShaderResourceView(t->whiteTex, NULL, &t->whiteSRV))) { if (outResult) *outResult = DX11TEX_ERR_DEVICE; goto fail_white; }

    if (outResult) *outResult = DX11TEX_OK;
    return t;

fail_white:
    if (t->whiteSRV) t->whiteSRV->Release();
    if (t->whiteTex) t->whiteTex->Release();
fail_alloc:
    free(t->entries);
    free(t->vram);
    free(t);
    return NULL;
}

void Dx11Tex_Destroy(Dx11Tex *t) {
    if (!t)
        return;
    for (int i = 0; i < t->entryCount; ++i) {
        if (t->entries[i].srv) t->entries[i].srv->Release();
        if (t->entries[i].tex) t->entries[i].tex->Release();
    }
    free(t->entries);
    if (t->whiteSRV) t->whiteSRV->Release();
    if (t->whiteTex) t->whiteTex->Release();
    free(t->vram);
    // ctx is borrowed (owned by the caller); do not Release it.
    free(t);
}

// ---------------------------------------------------------------------------
// VRAM staging
// ---------------------------------------------------------------------------
void Dx11Tex_CopyVRAM(Dx11Tex *t, const unsigned short *src,
                      int x, int y, int w, int h, int dst_x, int dst_y) {
    if (!t || w <= 0 || h <= 0)
        return;
    int srcStride = src ? w : DX11TEX_VRAM_W;
    if (!src)
        src = t->vram;
    for (int j = 0; j < h; ++j) {
        int sy = y + j, dy = dst_y + j;
        if (sy < 0) sy = 0;
        if (sy >= DX11TEX_VRAM_H) sy = DX11TEX_VRAM_H - 1;
        if (dy < 0) dy = 0;
        if (dy >= DX11TEX_VRAM_H) dy = DX11TEX_VRAM_H - 1;
        for (int i = 0; i < w; ++i) {
            int sx = x + i, dx = dst_x + i;
            if (sx < 0) sx = 0;
            if (sx >= DX11TEX_VRAM_W) sx = DX11TEX_VRAM_W - 1;
            if (dx < 0) dx = 0;
            if (dx >= DX11TEX_VRAM_W) dx = DX11TEX_VRAM_W - 1;
            t->vram[dy * DX11TEX_VRAM_W + dx] = src[sy * srcStride + sx];
        }
    }
}

unsigned short Dx11Tex_GetVRAMWord(Dx11Tex *t, int x, int y) {
    if (!t) return 0;
    return VWord(t, x, y);
}

// ---------------------------------------------------------------------------
// Bake + SRV
// ---------------------------------------------------------------------------
static int FindEntry(Dx11Tex *t, unsigned short tpage, unsigned short clut,
                     int texX, int texY, int w, int h) {
    for (int i = 0; i < t->entryCount; ++i) {
        Dx11TexEntry *e = &t->entries[i];
        if (e->tpage == tpage && e->clut == clut && e->texX == texX &&
            e->texY == texY && e->w == w && e->h == h)
            return i;
    }
    return -1;
}

Dx11TexHandle Dx11Tex_Bake(Dx11Tex *t, unsigned short tpage, unsigned short clut,
                           int tex_x, int tex_y, int width, int height) {
    if (!t || width <= 0 || height <= 0)
        return -1;

    int idx = FindEntry(t, tpage, clut, tex_x, tex_y, width, height);
    if (idx >= 0)
        return idx;

    if (t->entryCount >= t->entryCap)
        return -1; // cache full

    // Hoisted locals.
    D3D11_TEXTURE2D_DESC td = {};
    D3D11_SUBRESOURCE_DATA srd = {};
    unsigned char *rgba = (unsigned char *)malloc((size_t)width * height * 4);
    if (!rgba)
        return -1;

    DecodeToRGBA(t, tpage, clut, tex_x, tex_y, width, height, rgba);

    td.Width = (UINT)width; td.Height = (UINT)height;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    srd.pSysMem = rgba;
    srd.SysMemPitch = (UINT)width * 4;

    ID3D11Texture2D *tex = NULL;
    ID3D11ShaderResourceView *srv = NULL;
    if (FAILED(t->dev->CreateTexture2D(&td, &srd, &tex))) { free(rgba); return -1; }
    if (FAILED(t->dev->CreateShaderResourceView(tex, NULL, &srv))) {
        tex->Release(); free(rgba); return -1;
    }
    free(rgba);

    Dx11TexEntry *e = &t->entries[t->entryCount];
    e->tpage = tpage; e->clut = clut;
    e->texX = tex_x; e->texY = tex_y;
    e->w = width; e->h = height;
    e->tex = tex; e->srv = srv; e->used = 1;
    return t->entryCount++;
}

ID3D11ShaderResourceView *Dx11Tex_GetSRV(Dx11Tex *t, Dx11TexHandle h) {
    if (!t || h < 0 || h >= t->entryCount)
        return NULL;
    return t->entries[h].srv;
}

ID3D11ShaderResourceView *Dx11Tex_GetWhiteSRV(Dx11Tex *t) {
    return t ? t->whiteSRV : NULL;
}

int Dx11Tex_GetSize(Dx11Tex *t, Dx11TexHandle h, int *outW, int *outH) {
    if (!t || h < 0 || h >= t->entryCount)
        return 1;
    if (outW) *outW = t->entries[h].w;
    if (outH) *outH = t->entries[h].h;
    return 0;
}

// Read the baked texture back to CPU (staging copy).
int Dx11Tex_ReadBack(Dx11Tex *t, ID3D11DeviceContext *ctx, Dx11TexHandle h,
                     unsigned char *out, int maxBytes) {
    if (!t || h < 0 || h >= t->entryCount || !out)
        return 1;
    if (!ctx) ctx = t->ctx;
    Dx11TexEntry *e = &t->entries[h];
    if ((size_t)e->w * e->h * 4 > (size_t)maxBytes)
        return 1;

    D3D11_TEXTURE2D_DESC td = {};
    e->tex->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *stage = NULL;
    if (FAILED(t->dev->CreateTexture2D(&td, NULL, &stage)))
        return 1;
    ctx->CopyResource(stage, e->tex);
    D3D11_MAPPED_SUBRESOURCE map = {};
    int ok = 1;
    if (SUCCEEDED(ctx->Map(stage, 0, D3D11_MAP_READ, 0, &map))) {
        const unsigned char *src = (const unsigned char *)map.pData;
        for (int y = 0; y < e->h; ++y) {
            memcpy(out + (size_t)y * e->w * 4,
                   src + (size_t)y * map.RowPitch,
                   (size_t)e->w * 4);
        }
        ctx->Unmap(stage, 0);
        ok = 0;
    }
    stage->Release();
    return ok;
}

// Read-only debug dump of the VRAM + a baked texture's first pixels.
void Dx11Tex_DebugDump(Dx11Tex *t, ID3D11DeviceContext *ctx, Dx11TexHandle h,
                       const char *path) {
    if (!t) return;
    if (!ctx) ctx = t->ctx;
    FILE *f = fopen(path, "w");
    if (!f) return;

    if (h >= 0 && h < t->entryCount) {
        Dx11TexEntry *e = &t->entries[h];
        fprintf(f, "bake tpage=%04X clut=%04X texXY=(%d,%d) size=%dx%d\n",
                e->tpage, e->clut, e->texX, e->texY, e->w, e->h);
        int n = (e->w * e->h < 4) ? e->w * e->h : 4;
        unsigned char *rgba = (unsigned char *)malloc((size_t)e->w * e->h * 4);
        if (rgba && Dx11Tex_ReadBack(t, ctx, h, rgba, e->w * e->h * 4) == 0) {
            for (int i = 0; i < n; ++i)
                fprintf(f, "px[%d]=(%u,%u,%u,%u)\n", i,
                        rgba[i*4+0], rgba[i*4+1], rgba[i*4+2], rgba[i*4+3]);
        }
        free(rgba);
    }
    fprintf(f, "vram[8*1024+0]=%04X vram[8*1024+1]=%04X\n",
            VWord(t, 0, 8), VWord(t, 1, 8));
    fclose(f);
}
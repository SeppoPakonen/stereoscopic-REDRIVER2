// dx11_textures_test.cpp — T1.3 texture-system harness.
//
// Builds a small PSX VRAM containing a 4-bit paletted texture (with its 16-
// entry CLUT) and an 8-bit paletted texture (with its CLUT), using the real
// getTPage/getClut packing. It bakes each region to an R8G8B8A8 SRV via the
// dx11_textures module, draws one quad per texture sampling the SRVs, captures
// the backbuffer to BMP, and read-backs the baked textures to assert the
// decoded colors equal the expected palette (white/red/green/blue checker).
//
// Verification (headless):
//   * decode correctness: read-back of each baked texture matches the expected
//     RGB555->RGBA colors (result file PASS/FAIL);
//   * SRV binding works: the backbuffer shows two distinct colored quads.

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_textures.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Shaders (t0/s0 = texture/sampler; vertex positions are already clip space).
// ---------------------------------------------------------------------------
static const char *kVS = R"(
struct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos, 1.0f);
    o.uv = i.uv;
    return o;
}
)";

static const char *kPS = R"(
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD; };
float4 main(VSOut i) : SV_Target {
    return tex.Sample(samp, i.uv);
}
)";

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_textures_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

// ---------------------------------------------------------------------------
// VRAM construction helpers (standard PSX packing)
// ---------------------------------------------------------------------------
// 4-bit: 4 px per 16-bit word (byte=(x&2)>>1, nibble x&1, low nibble=even px).
static void PutTex4(unsigned short *v, int baseX, int baseY, int w, int h,
                    int (*idxAt)(int x, int y)) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int ax = baseX + x, ay = baseY + y;
            int idx = idxAt(x, y) & 0x0f;
            int wordCol = ax / 4;
            int byteShift = ((ax & 2) >> 1) * 8;
            int nibShift = (ax & 1) ? 4 : 0;
            v[ay * 1024 + wordCol] |= (unsigned short)(idx << (nibShift + byteShift));
        }
}

// 8-bit: 2 px per word (low byte = even px, high byte = odd px).
static void PutTex8(unsigned short *v, int baseX, int baseY, int w, int h,
                    int (*idxAt)(int x, int y)) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int ax = baseX + x, ay = baseY + y;
            int idx = idxAt(x, y) & 0xff;
            int wordCol = ax / 2;
            int byteShift = (ax & 1) * 8;
            v[ay * 1024 + wordCol] |= (unsigned short)(idx << byteShift);
        }
}

static void PutClut(unsigned short *v, int clutX, int clutY,
                    const unsigned short *colors, int n) {
    for (int i = 0; i < n; ++i)
        v[clutY * 1024 + clutX + i] = colors[i];
}

// Checker pattern: index = (x + y) % 4.
static int CheckerIdx(int x, int y) { return (x + y) % 4; }

// Palette used by both textures (RGB555). Expected RGBA below.
static const unsigned short kPal3[] = {
    0x7FFF, // 0 white
    0x001F, // 1 red
    0x03E0, // 2 green
    0x7C00, // 3 blue
    0x03FF, // 4 yellow
    0x7FE0, // 5 cyan
    0x7C1F, // 6 magenta
    0x0000, // 7 black
};
static const unsigned char kCol[4][4] = {
    { 248, 248, 248, 255 }, // white
    { 248,   0,   0, 255 }, // red
    {   0, 248,   0, 255 }, // green
    {   0,   0, 248, 255 }, // blue
};

// ---------------------------------------------------------------------------
// Arg parsing: -res WxH.
// ---------------------------------------------------------------------------
static int ParsePair(const char *s, int *a, int *b) {
    return (sscanf(s, "%dx%d", a, b) == 2 && *a > 0 && *b > 0) ? 1 : 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    Dx11RendererConfig rcfg = { 800, 600, 320, 240, 0, 0 };
    {
        char buf[400];
        strncpy(buf, lpCmdLine ? lpCmdLine : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, " \t");
        while (tok) {
            if (!strcmp(tok, "-res") && (tok = strtok(NULL, " \t"))) {
                int w, h; if (ParsePair(tok, &w, &h)) { rcfg.windowW = w; rcfg.windowH = h; }
            }
            tok = strtok(NULL, " \t");
        }
    }

    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_textures_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11TexResult tr;
    Dx11Tex *tmod = Dx11Tex_Create(dev, ctx, NULL, &tr);
    if (!tmod) { MessageBoxA(NULL, "Dx11Tex_Create failed", "dx11_textures_test", MB_OK | MB_ICONERROR); return 2; }

    // ------------------------------------------------------------------
    // Build VRAM.
    //   4-bit texture: tpage 0 (tpX=0, tpY=0), texXY=(0,0) 8x8 -> rows 0-7, cols 0-1.
    //   4-bit CLUT:    clutX=0, clutY=8                      -> clut = (8<<6)|0 = 512.
    //   8-bit texture: tpage 1 (tpX=64, tpY=0), texXY=(0,16) -> rows 16-23, cols 64-67.
    //   8-bit CLUT:    clutX=80, clutY=16                    -> clut = (16<<6)|5 = 1029.
    // ------------------------------------------------------------------
    unsigned short vram[DX11TEX_VRAM_W * DX11TEX_VRAM_H] = { 0 };

    PutClut(vram, 0, 8, kPal3, 8);                       // 4-bit CLUT (16-entry, fill 8)
    PutTex4(vram, 0, 0, 8, 8, CheckerIdx);               // 4-bit texture

    unsigned short pal8[256] = { 0 };
    for (int i = 0; i < 8; ++i) pal8[i] = kPal3[i];
    PutClut(vram, 80, 16, pal8, 256);                    // 8-bit CLUT (256-entry)
    PutTex8(vram, 64, 16, 8, 8, CheckerIdx);             // 8-bit texture

    Dx11Tex_CopyVRAM(tmod, vram, 0, 0, DX11TEX_VRAM_W, DX11TEX_VRAM_H, 0, 0);

    // ------------------------------------------------------------------
    // Bake both regions.
    //   4-bit tpage=0 (fmt 0, tpX=0, tpY=0), clut 512 (clutX=0, clutY=8).
    //   8-bit tpage=0x81 (fmt 1, tpX=64, tpY=0 -> bit7 set + bit0 set),
    //        clut 1029 (clutX=80, clutY=16).
    // ------------------------------------------------------------------
    Dx11TexHandle h4 = Dx11Tex_Bake(tmod, 0, 512, 0, 0, 8, 8);        // 4-bit
    Dx11TexHandle h8 = Dx11Tex_Bake(tmod, 0x81, 1029, 0, 16, 8, 8);   // 8-bit
    if (h4 < 0 || h8 < 0) { MessageBoxA(NULL, "Dx11Tex_Bake failed", "dx11_textures_test", MB_OK | MB_ICONERROR); return 2; }

    // ------------------------------------------------------------------
    // Verify the decode: read back each baked texture and compare to kCol.
    // ------------------------------------------------------------------
    FILE *res = fopen("dx11_textures_result.txt", "w");
    if (!res) { MessageBoxA(NULL, "cannot open result file", "dx11_textures_test", MB_OK); return 2; }
    int totalBad = 0;
    for (int pass = 0; pass < 2; ++pass) {
        Dx11TexHandle h = pass ? h8 : h4;
        int w = 0, hh = 0;
        Dx11Tex_GetSize(tmod, h, &w, &hh);
        unsigned char *rgba = (unsigned char *)malloc((size_t)w * hh * 4);
        if (!rgba) { fclose(res); return 2; }
        if (Dx11Tex_ReadBack(tmod, ctx, h, rgba, w * hh * 4) != 0) {
            fprintf(res, "pass=%d READBACK_FAILED\n", pass);
            totalBad = 1;
        } else {
            int bad = 0;
            for (int y = 0; y < hh; ++y)
                for (int x = 0; x < w; ++x) {
                    int idx = CheckerIdx(x, y);
                    unsigned char *p = rgba + (y * w + x) * 4;
                    if (p[0] != kCol[idx][0] || p[1] != kCol[idx][1] ||
                        p[2] != kCol[idx][2] || p[3] != kCol[idx][3])
                        ++bad;
                }
            if (pass == 0)
                fprintf(res, "4-BIT mismatch=%d (expect white/red/green/blue checker)\n", bad);
            else
                fprintf(res, "8-BIT mismatch=%d (expect white/red/green/blue checker)\n", bad);
            if (bad == 0) {
                fprintf(res, "  px(0,0)=%u,%u,%u,%u  px(1,0)=%u,%u,%u,%u  px(2,0)=%u,%u,%u,%u  px(3,0)=%u,%u,%u,%u\n",
                        rgba[0], rgba[1], rgba[2], rgba[3],
                        rgba[4], rgba[5], rgba[6], rgba[7],
                        rgba[8], rgba[9], rgba[10], rgba[11],
                        rgba[12], rgba[13], rgba[14], rgba[15]);
            }
            totalBad += bad;
        }
        free(rgba);
    }
    fprintf(res, "TOTAL_MISMATCH=%d DECODE=%s\n", totalBad, totalBad == 0 ? "PASS" : "FAIL");
    fclose(res);

    Dx11Tex_DebugDump(tmod, ctx, h4, "dx11_textures_bake_4bit.txt");
    Dx11Tex_DebugDump(tmod, ctx, h8, "dx11_textures_bake_8bit.txt");

    // ------------------------------------------------------------------
    // Pipeline: position+uv -> sample t0.
    // ------------------------------------------------------------------
    ID3DBlob *vsBlob = NULL, *psBlob = NULL, *err = NULL;
    HRESULT hr = D3DCompile(kVS, strlen(kVS), "vs", NULL, NULL, "main", "vs_4_0", 0, 0, &vsBlob, &err);
    if (FAILED(hr)) Fail("D3DCompile(VS)", hr);
    hr = D3DCompile(kPS, strlen(kPS), "ps", NULL, NULL, "main", "ps_4_0", 0, 0, &psBlob, &err);
    if (FAILED(hr)) Fail("D3DCompile(PS)", hr);

    ID3D11VertexShader *vs = NULL;
    ID3D11PixelShader *ps = NULL;
    if (FAILED(dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &vs))) Fail("CreateVertexShader", hr);
    if (FAILED(dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &ps))) Fail("CreatePixelShader", hr);

    D3D11_INPUT_ELEMENT_DESC elems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout *layout = NULL;
    if (FAILED(dev->CreateInputLayout(elems, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layout))) Fail("CreateInputLayout", hr);
    vsBlob->Release(); psBlob->Release();

    // Point sampler.
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState *samp = NULL;
    if (FAILED(dev->CreateSamplerState(&sd, &samp))) Fail("CreateSamplerState", hr);

    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    ID3D11RasterizerState *rs = NULL;
    if (FAILED(dev->CreateRasterizerState(&rsd, &rs))) Fail("CreateRasterizerState", hr);

    // Two quads in clip space: left quad = 4-bit tex, right quad = 8-bit tex.
    struct { float x, y, z, u, v; } verts[8] = {
        { -1, -1, 0, 0, 1 }, { 0, -1, 0, 1, 1 }, { 0, 1, 0, 1, 0 }, { -1, 1, 0, 0, 0 },
        {  0, -1, 0, 0, 1 }, { 1, -1, 0, 1, 1 }, { 1, 1, 0, 1, 0 }, {  0, 1, 0, 0, 0 },
    };
    unsigned short idx[12] = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
    };
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = sizeof(verts); vbd.Usage = D3D11_USAGE_DEFAULT; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vsrd = {}; vsrd.pSysMem = verts;
    ID3D11Buffer *vb = NULL;
    if (FAILED(dev->CreateBuffer(&vbd, &vsrd, &vb))) Fail("CreateBuffer(VB)", hr);
    D3D11_BUFFER_DESC ibd = {};
    ibd.ByteWidth = sizeof(idx); ibd.Usage = D3D11_USAGE_DEFAULT; ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA isrd = {}; isrd.pSysMem = idx;
    ID3D11Buffer *ib = NULL;
    if (FAILED(dev->CreateBuffer(&ibd, &isrd, &ib))) Fail("CreateBuffer(IB)", hr);

    ID3D11ShaderResourceView *srv4 = Dx11Tex_GetSRV(tmod, h4);
    ID3D11ShaderResourceView *srv8 = Dx11Tex_GetSRV(tmod, h8);

    // Draw one frame.
    if (Dx11Renderer_BeginFrame(ren) == 0) {
        Dx11Renderer_BindBackbuffer(ren);
        ctx->RSSetState(rs);
        ctx->IASetInputLayout(layout);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs, NULL, 0);
        ctx->PSSetShader(ps, NULL, 0);
        ctx->PSSetSamplers(0, 1, &samp);
        UINT stride = 20, offset = 0;
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);

        // Left quad (4-bit).
        ctx->PSSetShaderResources(0, 1, &srv4);
        ctx->DrawIndexed(6, 0, 0);
        // Right quad (8-bit).
        ctx->PSSetShaderResources(0, 1, &srv8);
        ctx->DrawIndexed(6, 6, 0);

        Dx11Renderer_CaptureToBMP(ren, NULL, "dx11_textures_frame.bmp",
                                  "dx11_textures_frame.txt");
        Dx11Renderer_Present(ren);
    }

    // Cleanup.
    ib->Release(); vb->Release();
    rs->Release(); samp->Release(); layout->Release(); ps->Release(); vs->Release();
    Dx11Tex_Destroy(tmod);
    Dx11Renderer_Destroy(ren);
    return 0;
}
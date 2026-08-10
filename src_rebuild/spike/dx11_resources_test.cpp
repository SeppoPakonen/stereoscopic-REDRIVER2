// dx11_resources_test.cpp — T1.2 resource-management harness.
//
// Drives dx11_renderer (device/window/swapchain) + dx11_resources (per-frame
// vertex arena + dynamic VB, constant-buffer arena for per-draw world matrices,
// SRV + sampler). Each frame the harness simulates the game submitting a grid
// of draw commands (a quad = 4 vertices + 6 indices + one world-matrix CB slot
// each), then uploads and draws every command with its own world slot.
//
// Verification (headless):
//   * quad grid renders to the backbuffer (captured to BMP, SRV+sampler bound);
//   * a stats file records arena capacity per frame — after the first frame it
//     must stay constant (bounded growth, no per-frame leak).

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_resources.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Shaders (b0 = view/proj, b1 = per-draw world, t0/s0 = texture/sampler).
// ---------------------------------------------------------------------------
static const char *kVS = R"(
cbuffer ViewProj : register(b0) { float4x4 viewProj; };
cbuffer PerDraw : register(b1) { float4x4 world; };
struct VSIn { float3 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD; };
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(mul(float4(i.pos, 1.0f), world), viewProj);
    o.col = i.col;
    o.uv = i.uv;
    return o;
}
)";

static const char *kPS = R"(
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD; };
float4 main(VSOut i) : SV_Target {
    return tex.Sample(samp, i.uv) * i.col;
}
)";

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_resources_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

// Column-major matrix helpers (row-vector x column-major).
static void MatIdentity(float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    for (int i = 0; i < 4; ++i) m[i][i] = 1.0f;
}
static void MatTranslate(float tx, float ty, float tz, float m[4][4]) {
    MatIdentity(m);
    m[0][3] = tx;  // row-vector: translation at column 3
    m[1][3] = ty;
    m[2][3] = tz;
}
static void MatPerspectiveRH(float fovY, float aspect, float zn, float zf, float m[4][4]) {
    MatIdentity(m);
    float f = 1.0f / tanf(fovY * 0.5f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = zf / (zn - zf);
    m[2][3] = zn * zf / (zn - zf);
    m[3][2] = -1.0f;
}

// Simple pipeline kit (VS/PS/layout) for the grid draws.
struct Pipe {
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11InputLayout *layout;
    ID3D11Texture2D *tex;
    ID3D11ShaderResourceView *srv;
    ID3D11RasterizerState *rs;
};

static void InitPipe(ID3D11Device *dev, Pipe *p) {
    ID3DBlob *vsBlob = NULL, *psBlob = NULL, *err = NULL;
    HRESULT hr = D3DCompile(kVS, strlen(kVS), "vs", NULL, NULL, "main", "vs_4_0", 0, 0, &vsBlob, &err);
    if (FAILED(hr)) { if (err) MessageBoxA(NULL, (const char *)err->GetBufferPointer(), "VS compile", MB_OK); Fail("D3DCompile(VS)", hr); }
    hr = D3DCompile(kPS, strlen(kPS), "ps", NULL, NULL, "main", "ps_4_0", 0, 0, &psBlob, &err);
    if (FAILED(hr)) { if (err) MessageBoxA(NULL, (const char *)err->GetBufferPointer(), "PS compile", MB_OK); Fail("D3DCompile(PS)", hr); }

    hr = dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &p->vs);
    if (FAILED(hr)) Fail("CreateVertexShader", hr);
    hr = dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &p->ps);
    if (FAILED(hr)) Fail("CreatePixelShader", hr);

    D3D11_INPUT_ELEMENT_DESC elems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = dev->CreateInputLayout(elems, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &p->layout);
    vsBlob->Release(); psBlob->Release();
    if (FAILED(hr)) Fail("CreateInputLayout", hr);

    // 2x2 checkerboard texture -> SRV (proves SRV + sampler binding; full
    // texture system is T1.3).
    unsigned char px[4 * 4] = {
        255,255,255,255,   0,0,255,255,
        0,0,255,255,       255,255,255,255,
    };
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 2; td.Height = 2; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = px;
    srd.SysMemPitch = 2 * 4;
    hr = dev->CreateTexture2D(&td, &srd, &p->tex);
    if (FAILED(hr)) Fail("CreateTexture2D(checker)", hr);
    hr = dev->CreateShaderResourceView(p->tex, NULL, &p->srv);
    if (FAILED(hr)) Fail("CreateSRV(checker)", hr);

    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    hr = dev->CreateRasterizerState(&rsd, &p->rs);
    if (FAILED(hr)) Fail("CreateRasterizerState", hr);
}

// ---------------------------------------------------------------------------
// Arg parsing: -res WxH, -vsync N.
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
            } else if (!strcmp(tok, "-vsync") && (tok = strtok(NULL, " \t"))) {
                rcfg.vsync = atoi(tok);
            }
            tok = strtok(NULL, " \t");
        }
    }

    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_resources_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    // Resources: small initial arena so the first frame forces growth, then it
    // must plateau (bounded).
    Dx11ResConfig rsc = Dx11Res_DefaultConfig();
    rsc.vertex_capacity = 64;
    rsc.index_capacity = 64;
    Dx11ResResult rres;
    Dx11Res *rs = Dx11Res_Create(dev, ctx, &rsc, &rres);
    if (!rs) { MessageBoxA(NULL, "Dx11Res_Create failed", "dx11_resources_test", MB_OK | MB_ICONERROR); return 2; }

    Pipe pipe = {};
    InitPipe(dev, &pipe);

    FILE *stats = fopen("dx11_resources_stats.txt", "w");
    if (!stats) { MessageBoxA(NULL, "cannot open stats file", "dx11_resources_test", MB_OK); return 2; }

    // Grid of quads: each quad = one draw command (4 verts + 6 idx + 1 CB slot).
    const int GRIDX = 6, GRIDY = 4;
    const int QUADS = GRIDX * GRIDY;
    const float SPACING = 1.0f;

    float vp[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f,
                     (float)rcfg.windowW / (float)rcfg.windowH, 0.1f, 100.0f, vp);

    // Debug: dump the first world matrix translation to the stats file.
    {
        float wm[4][4];
        MatTranslate((-0.5f) * SPACING, (-1.0f) * SPACING, -4.0f, wm);
        fprintf(stats, "debug quad0 world trans=(%.3f,%.3f,%.3f)\n",
                wm[0][3], wm[1][3], wm[2][3]);
    }

    const int FRAMES = 40;
    int lastCap = -1, stableFrames = 0;
    for (int f = 0; f < FRAMES; ++f) {
        if (Dx11Renderer_BeginFrame(ren))
            break;

        Dx11Res_BeginFrame(rs);
        Dx11Res_SetViewProj(rs, vp);

        // Submit the grid (simulating the game's plot functions).
        for (int i = 0; i < QUADS; ++i) {
            int col = i % GRIDX, row = i / GRIDX;
            float cx = (col - (GRIDX - 1) * 0.5f) * SPACING;
            float cy = (row - (GRIDY - 1) * 0.5f) * SPACING;
            float z = -4.0f;

            // 4 model-local vertices (unit quad, centered, uv 0..1).
            Dx11ResVertex v[4];
            const float h = 0.45f;
            const float colr = 0.6f + 0.4f * (col % 2), colg = 0.6f + 0.4f * (row % 2), colb = 1.0f;
            int vi[4];
            v[0] = { -h, -h, 0, colr, colg, colb, 1, 0, 0 };
            v[1] = {  h, -h, 0, colr, colg, colb, 1, 1, 0 };
            v[2] = {  h,  h, 0, colr, colg, colb, 1, 1, 1 };
            v[3] = { -h,  h, 0, colr, colg, colb, 1, 0, 1 };
            for (int k = 0; k < 4; ++k) {
                vi[k] = Dx11Res_PushVertex(rs, &v[k]);
            }
            // Indices are global vertex indices (PushVertex returns global ids).
            unsigned short ind[6] = { (unsigned short)vi[0], (unsigned short)vi[1], (unsigned short)vi[2],
                                      (unsigned short)vi[0], (unsigned short)vi[2], (unsigned short)vi[3] };
            for (int k = 0; k < 6; ++k) Dx11Res_PushIndex(rs, ind[k]);
            int slot = Dx11Res_AllocCB(rs);
            float wm[4][4];
            MatTranslate(cx, cy, z, wm);
            Dx11Res_SetCB(rs, slot, wm);
        }

        Dx11Res_Upload(rs, ctx);
        if (f == 0)
            Dx11Res_DebugDump(rs, ctx, "dx11_resources_dbg.txt");

        // Draw each command with its own world slot.
        Dx11Renderer_BindBackbuffer(ren);
        ctx->RSSetState(pipe.rs);
        ctx->IASetInputLayout(pipe.layout);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(pipe.vs, NULL, 0);
        ctx->PSSetShader(pipe.ps, NULL, 0);
        ID3D11SamplerState *psSamp = (ID3D11SamplerState *)Dx11Res_GetSampler(rs);
        ctx->PSSetSamplers(0, 1, &psSamp);
        Dx11Res_BindSRV(rs, ctx, pipe.srv);

        Dx11Res_BindGeometry(rs, ctx);
        for (int i = 0; i < QUADS; ++i) {
            Dx11Res_BindWorldSlot(rs, ctx, i);
            ctx->DrawIndexed(6, i * 6, 0);   // global indices, base 0
        }

        if (f == 0) {
            Dx11Renderer_CaptureToBMP(ren, NULL, "dx11_resources_frame.bmp",
                                      "dx11_resources_frame.txt");
        }
        Dx11Renderer_Present(ren);

        // Bounded-growth tracking.
        int cap = Dx11Res_VertexCapacity(rs);
        if (cap == lastCap) ++stableFrames; else stableFrames = 0;
        lastCap = cap;
        fprintf(stats, "frame=%d verts=%d idx=%d vertCap=%d cbSlotsUsed=%d\n",
                f, Dx11Res_VertexCount(rs), Dx11Res_IndexCount(rs), cap,
                Dx11Res_CBSlotsUsed(rs));
    }
    int bounded = (stableFrames >= FRAMES / 3);
    fprintf(stats, "stableFrames=%d boundedGrowth=%s totalFrames=%d totalVerts=%d\n",
            stableFrames, bounded ? "YES" : "NO", Dx11Res_TotalFrames(rs),
            Dx11Res_TotalVertsSubmitted(rs));
    fclose(stats);

    Dx11Res_Destroy(rs);
    Dx11Renderer_Destroy(ren);
    return 0;
}
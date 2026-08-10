// dx11_foundation.cpp — T1.1 DX11 renderer foundation harness.
//
// Drives the dx11_renderer module to prove the full T1.1 stack end-to-end:
//   * native Win32 window + DXGI swapchain created by the module (no SDL)
//   * backbuffer RTV + depth/stencil + viewport
//   * offscreen RTV/SRV pair at the internal resolution (T0.4)
//   * clear -> draw -> present per frame
//   * resize (swapchain backbuffer recreated) 
//   * -res WxH (window) and -ires WxH (internal resolution)
//
// Verification is headless: on selected frames the backbuffer and an offscreen
// RT are captured to BMP + pixel-statistics files (before Present for the
// backbuffer, per the DISCARD convention). The window itself may not be visible
// in some launch contexts; the captures are the ground truth.

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Embedded HLSL for the verification quad (same as the T0.5 spike).
// ---------------------------------------------------------------------------
static const char *kVS = R"(
struct VSIn { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
cbuffer Matrices : register(b0) { float4x4 worldViewProj; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0f), worldViewProj);
    o.col = i.col;
    return o;
}
)";

static const char *kPS = R"(
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
float4 main(VSOut i) : SV_Target { return i.col; }
)";

struct Vertex { float x, y, z; float r, g, b, a; };

// ---------------------------------------------------------------------------
// Column-major matrix helpers (row-vector x column-major; see T0.4/T0.5).
// ---------------------------------------------------------------------------
static void MatIdentity(float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    for (int i = 0; i < 4; ++i) m[i][i] = 1.0f;
}

static void MatPerspectiveRH(float fovY, float aspect, float zn, float zf,
                             float m[4][4]) {
    MatIdentity(m);
    float f = 1.0f / tanf(fovY * 0.5f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = zf / (zn - zf);
    m[2][3] = zn * zf / (zn - zf);
    m[3][2] = -1.0f;
}

static void MatRotZ(float a, float m[4][4]) {
    MatIdentity(m);
    float c = cosf(a), s = sinf(a);
    m[0][0] = c;  m[1][0] = -s;
    m[0][1] = s;  m[1][1] = c;
}

static void MatMul(const float a[4][4], const float b[4][4], float dst[4][4]) {
    float t[4][4];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            t[c][r] = 0.0f;
            for (int k = 0; k < 4; ++k)
                t[c][r] += a[k][r] * b[c][k];
        }
    memcpy(dst, t, 16 * sizeof(float));
}

// Build worldViewProj for a quad 2 units in front of the camera (-Z forward).
static void BuildMVP(float aspect, float angle, float mvp[4][4]) {
    float proj[4][4], rot[4][4], world[4][4], tmp[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 100.0f, proj);
    MatRotZ(angle, rot);

    MatIdentity(world);
    world[2][3] = -2.0f;               // z translation at [2][3]
    MatMul(rot, world, tmp);           // tmp = rot * trans
    MatMul(tmp, proj, mvp);            // mvp = world * proj
}

// ---------------------------------------------------------------------------
// Quad scene resources (VB/IB/CB/shaders) built once, reused per target.
// ---------------------------------------------------------------------------
struct QuadScene {
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11InputLayout *layout;
    ID3D11Buffer *vb;
    ID3D11Buffer *ib;
    ID3D11Buffer *cb;
    ID3D11RasterizerState *rs;
};

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_foundation: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

static void InitQuad(ID3D11Device *dev, QuadScene *q) {
    ID3DBlob *vsBlob = NULL, *psBlob = NULL, *err = NULL;
    HRESULT hr = D3DCompile(kVS, strlen(kVS), "vs", NULL, NULL, "main", "vs_4_0",
                            0, 0, &vsBlob, &err);
    if (FAILED(hr)) {
        if (err) MessageBoxA(NULL, (const char *)err->GetBufferPointer(), "VS compile", MB_OK);
        Fail("D3DCompile(VS) failed", hr);
    }
    hr = D3DCompile(kPS, strlen(kPS), "ps", NULL, NULL, "main", "ps_4_0",
                    0, 0, &psBlob, &err);
    if (FAILED(hr)) {
        if (err) MessageBoxA(NULL, (const char *)err->GetBufferPointer(), "PS compile", MB_OK);
        Fail("D3DCompile(PS) failed", hr);
    }

    hr = dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                 NULL, &q->vs);
    if (FAILED(hr)) Fail("CreateVertexShader failed", hr);
    hr = dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                NULL, &q->ps);
    if (FAILED(hr)) Fail("CreatePixelShader failed", hr);

    D3D11_INPUT_ELEMENT_DESC elems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = dev->CreateInputLayout(elems, 2, vsBlob->GetBufferPointer(),
                                vsBlob->GetBufferSize(), &q->layout);
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr)) Fail("CreateInputLayout failed", hr);

    Vertex verts[4] = {
        { -0.6f, -0.6f, 0.0f, 1.0f, 0.2f, 0.2f, 1.0f },
        {  0.6f, -0.6f, 0.0f, 0.2f, 1.0f, 0.2f, 1.0f },
        {  0.6f,  0.6f, 0.0f, 0.2f, 0.2f, 1.0f, 1.0f },
        { -0.6f,  0.6f, 0.0f, 1.0f, 1.0f, 0.2f, 1.0f },
    };
    unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC bd = {};
    D3D11_SUBRESOURCE_DATA sd = {};
    bd.ByteWidth = sizeof(verts);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    sd.pSysMem = verts;
    hr = dev->CreateBuffer(&bd, &sd, &q->vb);
    if (FAILED(hr)) Fail("CreateBuffer(VB) failed", hr);

    bd.ByteWidth = sizeof(idx);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sd.pSysMem = idx;
    hr = dev->CreateBuffer(&bd, &sd, &q->ib);
    if (FAILED(hr)) Fail("CreateBuffer(IB) failed", hr);

    float cbInit[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    bd.ByteWidth = 64;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sd.pSysMem = cbInit;
    hr = dev->CreateBuffer(&bd, &sd, &q->cb);
    if (FAILED(hr)) Fail("CreateBuffer(CB) failed", hr);

    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    hr = dev->CreateRasterizerState(&rsd, &q->rs);
    if (FAILED(hr)) Fail("CreateRasterizerState failed", hr);
}

// Draw the quad into the currently bound render target with the given aspect.
static void DrawQuad(ID3D11DeviceContext *ctx, QuadScene *q, float aspect,
                     float angle) {
    float mvp[4][4];
    BuildMVP(aspect, angle, mvp);
    ctx->UpdateSubresource(q->cb, 0, NULL, mvp, 0, 0);

    UINT stride = sizeof(Vertex), offset = 0;
    ctx->RSSetState(q->rs);
    ctx->IASetInputLayout(q->layout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetVertexBuffers(0, 1, &q->vb, &stride, &offset);
    ctx->IASetIndexBuffer(q->ib, DXGI_FORMAT_R16_UINT, 0);
    ctx->VSSetShader(q->vs, NULL, 0);
    ctx->PSSetShader(q->ps, NULL, 0);
    ctx->VSSetConstantBuffers(0, 1, &q->cb);
    ctx->DrawIndexed(6, 0, 0);
}

// ---------------------------------------------------------------------------
// Argument parsing: -res WxH (window), -ires WxH (internal), -vsync N,
// -fullscreen.
// ---------------------------------------------------------------------------
static int ParseIntPair(const char *s, int *a, int *b) {
    int x = 0, y = 0;
    if (sscanf(s, "%dx%d", &x, &y) != 2)
        return 0;
    if (x <= 0 || y <= 0)
        return 0;
    *a = x;
    *b = y;
    return 1;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    // vsync defaults to 0 so the harness runs deterministically to completion
    // even headless (Present(1,0) can block when the window is not visible).
    Dx11RendererConfig cfg = { 800, 600, 320, 240, 0, 0 };

    // Parse the command line (parse lpCmdLine manually; -mwindows hides argv).
    {
        char buf[512];
        strncpy(buf, lpCmdLine ? lpCmdLine : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, " \t");
        while (tok) {
            if (!strcmp(tok, "-res") && (tok = strtok(NULL, " \t"))) {
                int w, h;
                if (ParseIntPair(tok, &w, &h)) { cfg.windowW = w; cfg.windowH = h; }
            } else if (!strcmp(tok, "-ires") && (tok = strtok(NULL, " \t"))) {
                int w, h;
                if (ParseIntPair(tok, &w, &h)) { cfg.internalW = w; cfg.internalH = h; }
            } else if (!strcmp(tok, "-vsync") && (tok = strtok(NULL, " \t"))) {
                cfg.vsync = atoi(tok);
            } else if (!strcmp(tok, "-fullscreen")) {
                cfg.fullscreen = 1;
            }
            tok = strtok(NULL, " \t");
        }
    }

    Dx11RendererResult res;
    Dx11Renderer *r = Dx11Renderer_Create(&cfg, &res);
    if (!r) {
        char buf[128];
        sprintf(buf, "Dx11Renderer_Create failed (code %d)", (int)res);
        MessageBoxA(NULL, buf, "dx11_foundation", MB_OK | MB_ICONERROR);
        return 2;
    }

    ID3D11Device *dev = Dx11Renderer_GetDevice(r);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(r);

    QuadScene q = {};
    InitQuad(dev, &q);

    // Log the effective sizes.
    {
        FILE *f = fopen("dx11_foundation_info.txt", "w");
        if (f) {
            fprintf(f, "window=%dx%d internal=%dx%d\n",
                    Dx11Renderer_GetWindowWidth(r), Dx11Renderer_GetWindowHeight(r),
                    Dx11Renderer_GetInternalWidth(r), Dx11Renderer_GetInternalHeight(r));
            fclose(f);
        }
    }

    float angle = 0.0f;
    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    int frame = 0;
    int resized = 0;
    int quit = 0;
    while (!quit && frame < 60) {
        float aspect = (float)Dx11Renderer_GetWindowWidth(r) /
                       (float)Dx11Renderer_GetWindowHeight(r);

        if (Dx11Renderer_BeginFrame(r))
            break;

        // --- Backbuffer pass (proves window + swapchain + backbuffer RTV) ---
        Dx11Renderer_BindBackbuffer(r);
        DrawQuad(ctx, &q, aspect, angle);

        if (frame == 0) {
            // Capture BEFORE present (DISCARD).
            Dx11Renderer_CaptureToBMP(r, NULL, "dx11_foundation_back.bmp",
                                      "dx11_foundation_back.txt");
        }

        Dx11Renderer_Present(r);

        // --- Offscreen pass (proves offscreen RT at internal resolution) ---
        if (frame == 1) {
            Dx11Renderer_BindOffscreen(r, 0);
            float clear[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ID3D11RenderTargetView *offRTV = Dx11Renderer_GetOffscreenRTV(r, 0);
            ctx->ClearRenderTargetView(offRTV, clear);
            float offAspect = (float)Dx11Renderer_GetInternalWidth(r) /
                              (float)Dx11Renderer_GetInternalHeight(r);
            DrawQuad(ctx, &q, offAspect, 0.0f);
            Dx11Renderer_CaptureToBMP(r, Dx11Renderer_GetOffscreenTexture(r, 0),
                                      "dx11_foundation_offscreen.bmp",
                                      "dx11_foundation_offscreen.txt");

            // Re-bind and re-draw the backbuffer for the next Present.
            Dx11Renderer_BindBackbuffer(r);
            aspect = (float)Dx11Renderer_GetWindowWidth(r) /
                     (float)Dx11Renderer_GetWindowHeight(r);
            DrawQuad(ctx, &q, aspect, angle);
            Dx11Renderer_Present(r);
        }

        // --- Resize test (swapchain backbuffer recreated) ---
        if (frame == 30 && !resized) {
            resized = 1;
            Dx11RendererResult rr = Dx11Renderer_Resize(r, 640, 480);
            FILE *f = fopen("dx11_foundation_resize.txt", "w");
            if (f) {
                fprintf(f, "resize=%d window=%dx%d\n", (int)rr,
                        Dx11Renderer_GetWindowWidth(r), Dx11Renderer_GetWindowHeight(r));
                fclose(f);
            }
            Dx11Renderer_BeginFrame(r);
            Dx11Renderer_BindBackbuffer(r);
            aspect = (float)Dx11Renderer_GetWindowWidth(r) /
                     (float)Dx11Renderer_GetWindowHeight(r);
            DrawQuad(ctx, &q, aspect, angle);
            Dx11Renderer_CaptureToBMP(r, NULL, "dx11_foundation_resized.bmp",
                                      "dx11_foundation_resized.txt");
            Dx11Renderer_Present(r);
        }

        // Drive the spin angle from wall clock.
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart);
        last = now;
        angle += dt * 0.8f;

        ++frame;
    }

    Dx11Renderer_Destroy(r);
    return 0;
}
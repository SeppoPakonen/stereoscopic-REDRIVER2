// dx11_spike.cpp — T0.5 DX11 spike: bare window + one meshed quad.
//
// A self-contained Win32 + Direct3D 11 program that proves the DX11 stack
// works end-to-end on this machine/toolchain before the real renderer is
// built:
//   * Win32 window + DXGI swapchain
//   * D3D11 device/context + render-target view
//   * minimal VS/PS pair compiled at runtime with D3DCompile
//   * one quad (2 triangles, 4 colored vertices) in a VB/IB
//   * a constant buffer carrying the model-view-projection matrix
//   * clear -> draw -> present
//
// The projection matrix follows the model from T0.4 (right-handed, +Y up,
// GTE-camera conventions mapped to DX11 NDC). This file has NO dependency on
// the game; it is built as its own premake project `dx11_spike`.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <math.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Embedded HLSL (compiled at runtime with D3DCompile).
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

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static HWND      g_hwnd      = NULL;
static ID3D11Device*        g_device  = NULL;
static ID3D11DeviceContext* g_context = NULL;
static IDXGISwapChain*      g_swap    = NULL;
static ID3D11RenderTargetView* g_rtv = NULL;
static ID3D11VertexShader*  g_vs = NULL;
static ID3D11PixelShader*   g_ps = NULL;
static ID3D11InputLayout*   g_layout = NULL;
static ID3D11Buffer*        g_vb = NULL;
static ID3D11Buffer*        g_ib = NULL;
static ID3D11Buffer*        g_cb = NULL;

static int g_winW = 800;
static int g_winH = 600;

// ---------------------------------------------------------------------------
// Column-major 4x4 helpers (DX convention: v' = v * M).
// ---------------------------------------------------------------------------
static void MatIdentity(float m[4][4]) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            m[c][r] = (c == r) ? 1.0f : 0.0f;
}

// Right-handed perspective per T0.4: fovY from the GTE focal length, aspect
// from the internal resolution, NDC z in [near,far] mapped to [0,1] (RH).
static void MatPerspectiveRH(float fovY, float aspect, float zn, float zf, float m[4][4]) {
    MatIdentity(m);
    float f = 1.0f / tanf(fovY * 0.5f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = zf / (zn - zf);      // clip.z  = cam_z * this + w * (zn*zf/(zn-zf))
    m[2][3] = zn * zf / (zn - zf); // z-buffer translation term
    m[3][2] = -1.0f;               // clip.w  = -cam_z  (right-handed, -Z forward)
}

// Rotation about Y (used to spin the quad so the 3D transform is visible).
static void MatRotY(float a, float m[4][4]) {
    MatIdentity(m);
    float c = cosf(a), s = sinf(a);
    m[0][0] = c;  m[2][0] = -s;
    m[0][2] = s;  m[2][2] = c;
}

// dst = a * b (column-major, matches mul(row, M) row-vector convention).
static void MatMul(const float a[4][4], const float b[4][4], float dst[4][4]) {
    float t[4][4];
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) {
            t[c][r] = 0.0f;
            for (int k = 0; k < 4; ++k)
                t[c][r] += a[k][r] * b[c][k];
        }
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            dst[c][r] = t[c][r];
}

// ---------------------------------------------------------------------------
// Error reporting (no console under -mwindows; use a message box).
// ---------------------------------------------------------------------------
static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_spike: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

// ---------------------------------------------------------------------------
// Win32
// ---------------------------------------------------------------------------
// Resize the render-target view to the current swapchain back buffer.
static void ResizeRTV(HWND hwnd);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_SIZE: {
            // Keep the window size for the next resize (recreate swapchain).
            g_winW = LOWORD(lp);
            g_winH = HIWORD(lp);
            if (g_swap) {
                g_context->OMSetRenderTargets(0, NULL, NULL);
                if (g_rtv) { g_rtv->Release(); g_rtv = NULL; }
                g_swap->ResizeBuffers(0, (UINT)g_winW, (UINT)g_winH,
                                      DXGI_FORMAT_UNKNOWN, 0);
                ResizeRTV(hwnd);
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void CreateWindowDX(void) {
    HINSTANCE hinst = GetModuleHandleA(NULL);
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = "DX11SpikeWnd";
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);

    RECT rc = { 0, 0, g_winW, g_winH };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowA("DX11SpikeWnd", "REDRIVER2 DX11 spike (T0.5)",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           NULL, NULL, hinst, NULL);
    if (!g_hwnd)
        Fail("CreateWindow failed", (HRESULT)GetLastError());
}

// ---------------------------------------------------------------------------
// D3D11 init
// ---------------------------------------------------------------------------
static void InitD3D(void) {
    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE,
                                   NULL, createFlags, levels, 1,
                                   D3D11_SDK_VERSION, &g_device, &got,
                                   &g_context);
    if (FAILED(hr))
        Fail("D3D11CreateDevice failed", hr);

    // Swapchain via the DXGI factory.
    IDXGIDevice *dxgiDevice = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory *factory = NULL;
    hr = g_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    if (FAILED(hr)) Fail("QueryInterface IDXGIDevice failed", hr);
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) Fail("GetAdapter failed", hr);
    hr = adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory);
    if (FAILED(hr)) Fail("GetParent IDXGIFactory failed", hr);

    DXGI_SWAP_CHAIN_DESC sc = {};
    sc.BufferDesc.Width = g_winW;
    sc.BufferDesc.Height = g_winH;
    sc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.BufferDesc.RefreshRate.Numerator = 0;
    sc.BufferDesc.RefreshRate.Denominator = 1;
    sc.SampleDesc.Count = 1;
    sc.SampleDesc.Quality = 0;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.BufferCount = 2;
    sc.OutputWindow = g_hwnd;
    sc.Windowed = TRUE;
    sc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    hr = factory->CreateSwapChain(g_device, &sc, &g_swap);
    if (FAILED(hr)) Fail("CreateSwapChain failed", hr);

    dxgiDevice->Release();
    adapter->Release();
    factory->Release();

    ResizeRTV(g_hwnd);
}

static void ResizeRTV(HWND hwnd) {
    (void)hwnd;
    if (g_rtv) { g_rtv->Release(); g_rtv = NULL; }

    ID3D11Texture2D *back = NULL;
    HRESULT hr = g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back);
    if (FAILED(hr)) Fail("GetBuffer failed", hr);
    hr = g_device->CreateRenderTargetView(back, NULL, &g_rtv);
    back->Release();
    if (FAILED(hr)) Fail("CreateRenderTargetView failed", hr);

    D3D11_VIEWPORT vp = { 0, 0, (FLOAT)g_winW, (FLOAT)g_winH, 0.0f, 1.0f };
    g_context->RSSetViewports(1, &vp);
}

// ---------------------------------------------------------------------------
// Scene: one quad (2 triangles, 4 colored vertices).
// ---------------------------------------------------------------------------
struct Vertex { float x, y, z; float r, g, b, a; };

static void InitScene(void) {
    // Compile shaders.
    ID3DBlob *vsBlob = NULL, *psBlob = NULL, *err = NULL;
    HRESULT hr = D3DCompile(kVS, strlen(kVS), "vs", NULL, NULL, "main", "vs_4_0",
                            0, 0, &vsBlob, &err);
    if (FAILED(hr)) {
        if (err) MessageBoxA(NULL, (const char*)err->GetBufferPointer(), "VS compile", MB_OK);
        Fail("D3DCompile(VS) failed", hr);
    }
    hr = D3DCompile(kPS, strlen(kPS), "ps", NULL, NULL, "main", "ps_4_0",
                    0, 0, &psBlob, &err);
    if (FAILED(hr)) {
        if (err) MessageBoxA(NULL, (const char*)err->GetBufferPointer(), "PS compile", MB_OK);
        Fail("D3DCompile(PS) failed", hr);
    }

    hr = g_device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                      vsBlob->GetBufferSize(), NULL, &g_vs);
    if (FAILED(hr)) Fail("CreateVertexShader failed", hr);
    hr = g_device->CreatePixelShader(psBlob->GetBufferPointer(),
                                     psBlob->GetBufferSize(), NULL, &g_ps);
    if (FAILED(hr)) Fail("CreatePixelShader failed", hr);

    D3D11_INPUT_ELEMENT_DESC elems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = g_device->CreateInputLayout(elems, 2, vsBlob->GetBufferPointer(),
                                     vsBlob->GetBufferSize(), &g_layout);
    vsBlob->Release();
    psBlob->Release();
    // Non-fatal: the full-screen-triangle diagnostic shader (SV_VertexID) has
    // no IA inputs, so CreateInputLayout may legitimately fail.
    if (FAILED(hr)) g_layout = NULL;

    // Quad geometry (world space, centered ~origin, front at z=-2 in view).
    // Vertex colors make the quad visually distinct from the clear color.
    Vertex verts[4] = {
        { -0.75f, -0.75f, 0.0f, 1.0f, 0.2f, 0.2f, 1.0f }, // bottom-left  (red)
        {  0.75f, -0.75f, 0.0f, 0.2f, 1.0f, 0.2f, 1.0f }, // bottom-right (green)
        {  0.75f,  0.75f, 0.0f, 0.2f, 0.2f, 1.0f, 1.0f }, // top-right    (blue)
        { -0.75f,  0.75f, 0.0f, 1.0f, 1.0f, 0.2f, 1.0f }, // top-left     (yellow)
    };
    unsigned short idx[6] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC bd = {};
    D3D11_SUBRESOURCE_DATA sd = {};
    bd.ByteWidth = sizeof(verts);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    sd.pSysMem = verts;
    hr = g_device->CreateBuffer(&bd, &sd, &g_vb);
    if (FAILED(hr)) Fail("CreateBuffer(VB) failed", hr);

    bd.ByteWidth = sizeof(idx);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sd.pSysMem = idx;
    hr = g_device->CreateBuffer(&bd, &sd, &g_ib);
    if (FAILED(hr)) Fail("CreateBuffer(IB) failed", hr);

    // Constant buffer: a full 4x4 matrix (64 bytes = multiple of 16). Provide
    // initial data (identity) so creation is well-defined.
    float cbInit[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };
    bd.ByteWidth = 64;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sd.pSysMem = cbInit;
    hr = g_device->CreateBuffer(&bd, &sd, &g_cb);
    if (FAILED(hr)) Fail("CreateBuffer(CB) failed", hr);

    // Rasterizer: draw both faces so the spike quad is visible regardless of
    // winding. (The real renderer will configure proper backface culling.)
    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    ID3D11RasterizerState *rs = NULL;
    hr = g_device->CreateRasterizerState(&rsd, &rs);
    if (FAILED(hr)) Fail("CreateRasterizerState failed", hr);
    g_context->RSSetState(rs);
}

// Initialize the constant buffer with the T0.4 projection model.
static void UpdateMatrices(float angle) {
    float proj[4][4], rotY[4][4], mvp[4][4];
    float aspect = (float)g_winW / (float)g_winH;
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 100.0f, proj);
    MatRotY(angle, rotY);

    // Place the quad 2 units in front of a right-handed camera at the origin
    // looking down -Z (the T0.4 "option 1" convention). View = identity here.
    // Translate the quad to z=-2 by folding into the world matrix. In the
    // row-vector x column-major convention the z translation lives at [2][3].
    float world[4][4];
    MatIdentity(world);
    world[2][3] = -2.0f; // translation in z (row 3 of column 2)
    MatMul(rotY, world, rotY); // world = rotY * trans  (rotY now holds rot*trans)
    MatMul(rotY, proj, mvp);   // mvp = world * proj

    g_context->UpdateSubresource(g_cb, 0, NULL, mvp, 0, 0);
}

// ---------------------------------------------------------------------------
// Capture helper forward declaration (defined below Render).
// ---------------------------------------------------------------------------
static void SaveFirstFrame(const char *path);

// ---------------------------------------------------------------------------
// Render + present
// ---------------------------------------------------------------------------
static void Render(float angle) {
    float clear[4] = { 0.05f, 0.05f, 0.12f, 1.0f };
    g_context->ClearRenderTargetView(g_rtv, clear);

    UINT stride = sizeof(Vertex), offset = 0;
    g_context->IASetInputLayout(g_layout);
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);
    g_context->IASetIndexBuffer(g_ib, DXGI_FORMAT_R16_UINT, 0);
    g_context->VSSetShader(g_vs, NULL, 0);
    g_context->PSSetShader(g_ps, NULL, 0);
    g_context->VSSetConstantBuffers(0, 1, &g_cb);
    g_context->OMSetRenderTargets(1, &g_rtv, NULL);

    UpdateMatrices(angle);
    g_context->DrawIndexed(6, 0, 0);

    // Capture the just-cleared+drawn back buffer BEFORE present (after present
    // with DXGI_SWAP_EFFECT_DISCARD the back-buffer contents are undefined).
    static int saved = 0;
    if (!saved) {
        SaveFirstFrame("dx11_spike_frame.bmp");
        saved = 1;
    }

    g_swap->Present(1, 0);
}

// ---------------------------------------------------------------------------
// Back-buffer capture (verification aid): copy the presented frame to a BMP.
// Used once on the first rendered frame so the spike can be verified headless
// (the window itself may not be visible in some launch contexts).
// ---------------------------------------------------------------------------
static void SaveFirstFrame(const char *path) {
    FILE *dbg = fopen("dx11_spike_frame.txt", "w");
    if (!dbg) return;

    ID3D11Texture2D *back = NULL;
    if (FAILED(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back))) {
        fprintf(dbg, "GetBuffer failed\n"); fclose(dbg); return;
    }

    D3D11_TEXTURE2D_DESC bd;
    back->GetDesc(&bd);
    bd.BindFlags = 0;
    bd.MiscFlags = 0;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bd.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture2D *staging = NULL;
    if (FAILED(g_device->CreateTexture2D(&bd, NULL, &staging))) {
        fprintf(dbg, "CreateTexture2D failed\n"); fclose(dbg); back->Release(); return;
    }
    g_context->CopyResource(staging, back);

    D3D11_MAPPED_SUBRESOURCE map = {};
    if (FAILED(g_context->Map(staging, 0, D3D11_MAP_READ, 0, &map))) {
        fprintf(dbg, "Map failed\n"); fclose(dbg); staging->Release(); back->Release(); return;
    }

    // 24-bit BMP (bottom-up rows).
    int w = (int)bd.Width, h = (int)bd.Height;
    int rowSize = (w * 3 + 3) & ~3;
    int dataSize = rowSize * h;
    int fileSize = 54 + dataSize;
    unsigned char *bmp = (unsigned char*)malloc((size_t)fileSize);
    if (bmp) {
        memset(bmp, 0, 54);
        bmp[0] = 'B'; bmp[1] = 'M';
        memcpy(bmp + 2, &fileSize, 4);
        bmp[10] = 54;
        bmp[14] = 40;                     // BITMAPINFOHEADER
        memcpy(bmp + 18, &w, 4);
        memcpy(bmp + 22, &h, 4);
        bmp[26] = 1; bmp[28] = 24;        // planes, bpp
        unsigned char *dst = bmp + 54;
        const unsigned char *src = (const unsigned char*)map.pData;
        long brightPix = 0; int minX=w,minY=h,maxX=0,maxY=0; int chr0=255,chm0=0;
        for (int y = 0; y < h; ++y) {
            const unsigned char *row = src + (size_t)(h - 1 - y) * map.RowPitch;
            unsigned char *out = dst + (size_t)y * rowSize;
            for (int x = 0; x < w; ++x) {
                unsigned char b = row[x * 4 + 0];
                unsigned char g = row[x * 4 + 1];
                unsigned char r = row[x * 4 + 2];
                out[x * 3 + 0] = b;
                out[x * 3 + 1] = g;
                out[x * 3 + 2] = r;
                if (r > chm0) chm0 = r;
                if (r < chr0) chr0 = r;
                if (r > 60 || g > 60 || b > 60) { // bright (quad) pixel
                    ++brightPix;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }
        fprintf(dbg, "brightPix=%ld bbox=(%d,%d)-(%d,%d) Rrange[%d,%d]\n",
                brightPix, minX, minY, maxX, maxY, chr0, chm0);
        FILE *f = fopen(path, "wb");
        if (f) { fwrite(bmp, 1, (size_t)fileSize, f); fclose(f);
                 fprintf(dbg, "OK %dx%d\n", w, h); }
        else fprintf(dbg, "fopen(%s) failed\n", path);
        free(bmp);
    } else {
        fprintf(dbg, "malloc failed\n");
    }

    g_context->Unmap(staging, 0);
    staging->Release();
    back->Release();
    fclose(dbg);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    CreateWindowDX();
    InitD3D();
    InitScene();

    MSG msg = {};
    SetTimer(g_hwnd, 1, 16, NULL); // ~60 Hz
    float angle = 0.0f;
    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);

    while (msg.message != WM_QUIT) {
        if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float dt = (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart);
            last = now;
            angle += dt * 0.8f;
            Render(angle);
        }
    }

    if (g_cb) g_cb->Release();
    if (g_ib) g_ib->Release();
    if (g_vb) g_vb->Release();
    if (g_layout) g_layout->Release();
    if (g_ps) g_ps->Release();
    if (g_vs) g_vs->Release();
    if (g_rtv) g_rtv->Release();
    if (g_swap) g_swap->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    return 0;
}
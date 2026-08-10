// dx11_renderer.c — T1.1 DX11 renderer foundation implementation.
//
// Win32 + Direct3D 11 backend. Reuses the conventions proven in the T0.5
// spike (row-vector x column-major math, DISCARD swap effect, capture-before-
// present). Compiled as C++ by the premake `compileas "C++"` rule.

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#define DX11R_WNDCLASS "REDRIVER2_DX11Wnd"

struct Dx11Renderer {
    HWND hwnd;
    HINSTANCE hinst;

    ID3D11Device        *dev;
    ID3D11DeviceContext *ctx;
    IDXGISwapChain      *swap;

    ID3D11RenderTargetView *backRTV;
    ID3D11Texture2D        *backTex;
    ID3D11Texture2D        *depthTex;
    ID3D11DepthStencilView *dsv;

    // Internal-resolution offscreen RTs (a left/right pair for stereo).
    ID3D11Texture2D        *offTex[DX11R_OFFSCREEN_COUNT];
    ID3D11RenderTargetView *offRTV[DX11R_OFFSCREEN_COUNT];
    ID3D11ShaderResourceView *offSRV[DX11R_OFFSCREEN_COUNT];

    D3D11_VIEWPORT backVp;
    D3D11_VIEWPORT offVp;

    int winW, winH;
    int internalW, internalH;
    int vsync;
    int fullscreen;
    int quit;
};

// ---------------------------------------------------------------------------
// Error helper (no console under -mwindows; use a message box).
// ---------------------------------------------------------------------------
static void Dx11Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_renderer: fatal error", MB_OK | MB_ICONERROR);
}

// ---------------------------------------------------------------------------
// Win32 window
// ---------------------------------------------------------------------------
static LRESULT CALLBACK Dx11WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
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
            // Keep the window size for query; the swapchain is resized lazily
            // via Dx11Renderer_Resize (avoid re-entrant ResizeBuffers here).
            Dx11Renderer *r = (Dx11Renderer *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
            if (r) {
                r->winW = LOWORD(lp);
                r->winH = HIWORD(lp);
            }
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int CreateWindowDX(Dx11Renderer *r) {
    r->hinst = GetModuleHandleA(NULL);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = Dx11WndProc;
    wc.hInstance = r->hinst;
    wc.lpszClassName = DX11R_WNDCLASS;
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);

    RECT rc = { 0, 0, r->winW, r->winH };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    if (r->fullscreen)
        style = WS_POPUP | WS_VISIBLE;

    r->hwnd = CreateWindowA(DX11R_WNDCLASS, "REDRIVER2 DX11 (Phase 1)",
                            style, CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top,
                            NULL, NULL, r->hinst, NULL);
    if (!r->hwnd)
        return 0;

    // Stash the renderer pointer for the WM_SIZE handler.
    SetWindowLongPtrA(r->hwnd, GWLP_USERDATA, (LONG_PTR)r);

    if (r->fullscreen) {
        // Standard fullscreen: use the window client size as the swapchain size.
        RECT cr;
        GetClientRect(r->hwnd, &cr);
        r->winW = cr.right - cr.left;
        r->winH = cr.bottom - cr.top;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// D3D11 device + swapchain
// ---------------------------------------------------------------------------
static int CreateDeviceAndSwapchain(Dx11Renderer *r) {
    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE,
                                   NULL, createFlags, levels, 1,
                                   D3D11_SDK_VERSION, &r->dev, &got, &r->ctx);
    if (FAILED(hr)) {
        Dx11Fail("D3D11CreateDevice failed", hr);
        return 0;
    }

    IDXGIDevice *dxgiDevice = NULL;
    IDXGIAdapter *adapter = NULL;
    IDXGIFactory *factory = NULL;
    hr = r->dev->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgiDevice);
    if (FAILED(hr)) { Dx11Fail("QueryInterface IDXGIDevice failed", hr); return 0; }
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) { Dx11Fail("GetAdapter failed", hr); return 0; }
    hr = adapter->GetParent(__uuidof(IDXGIFactory), (void **)&factory);
    if (FAILED(hr)) { Dx11Fail("GetParent IDXGIFactory failed", hr); return 0; }

    DXGI_SWAP_CHAIN_DESC sc = {};
    sc.BufferDesc.Width = (UINT)r->winW;
    sc.BufferDesc.Height = (UINT)r->winH;
    sc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.BufferDesc.RefreshRate.Numerator = 0;
    sc.BufferDesc.RefreshRate.Denominator = 1;
    sc.SampleDesc.Count = 1;
    sc.SampleDesc.Quality = 0;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.BufferCount = 2;
    sc.OutputWindow = r->hwnd;
    sc.Windowed = !r->fullscreen;
    sc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    hr = factory->CreateSwapChain(r->dev, &sc, &r->swap);
    dxgiDevice->Release();
    adapter->Release();
    factory->Release();
    if (FAILED(hr)) {
        Dx11Fail("CreateSwapChain failed", hr);
        return 0;
    }
    return 1;
}

// Recreate the backbuffer RTV + depth/stencil + viewport for the current
// swapchain back buffer.
static int CreateBackTargets(Dx11Renderer *r) {
    if (r->backRTV) { r->backRTV->Release(); r->backRTV = NULL; }
    if (r->backTex) { r->backTex->Release(); r->backTex = NULL; }
    if (r->dsv)     { r->dsv->Release(); r->dsv = NULL; }
    if (r->depthTex){ r->depthTex->Release(); r->depthTex = NULL; }

    HRESULT hr = r->swap->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    (void **)&r->backTex);
    if (FAILED(hr)) { Dx11Fail("GetBuffer failed", hr); return 0; }
    hr = r->dev->CreateRenderTargetView(r->backTex, NULL, &r->backRTV);
    if (FAILED(hr)) { Dx11Fail("CreateRenderTargetView(back) failed", hr); return 0; }

    // Depth/stencil texture + view at the backbuffer size.
    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width = (UINT)r->winW;
    dd.Height = (UINT)r->winH;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = r->dev->CreateTexture2D(&dd, NULL, &r->depthTex);
    if (FAILED(hr)) { Dx11Fail("CreateTexture2D(depth) failed", hr); return 0; }
    hr = r->dev->CreateDepthStencilView(r->depthTex, NULL, &r->dsv);
    if (FAILED(hr)) { Dx11Fail("CreateDepthStencilView failed", hr); return 0; }

    r->backVp = { 0, 0, (FLOAT)r->winW, (FLOAT)r->winH, 0.0f, 1.0f };
    return 1;
}

// Create the internal-resolution offscreen RTs (texture + RTV + SRV).
static int CreateOffscreenTargets(Dx11Renderer *r) {
    for (int i = 0; i < DX11R_OFFSCREEN_COUNT; ++i) {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = (UINT)r->internalW;
        td.Height = (UINT)r->internalH;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = r->dev->CreateTexture2D(&td, NULL, &r->offTex[i]);
        if (FAILED(hr)) { Dx11Fail("CreateTexture2D(offscreen) failed", hr); return 0; }
        hr = r->dev->CreateRenderTargetView(r->offTex[i], NULL, &r->offRTV[i]);
        if (FAILED(hr)) { Dx11Fail("CreateRenderTargetView(offscreen) failed", hr); return 0; }
        hr = r->dev->CreateShaderResourceView(r->offTex[i], NULL, &r->offSRV[i]);
        if (FAILED(hr)) { Dx11Fail("CreateShaderResourceView(offscreen) failed", hr); return 0; }
    }

    r->offVp = { 0, 0, (FLOAT)r->internalW, (FLOAT)r->internalH, 0.0f, 1.0f };
    return 1;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
Dx11Renderer *Dx11Renderer_Create(const Dx11RendererConfig *cfg,
                                  Dx11RendererResult *outResult) {
    if (outResult) *outResult = DX11R_ERR_WINDOW;

    Dx11Renderer *r = (Dx11Renderer *)calloc(1, sizeof(Dx11Renderer));
    if (!r)
        return NULL;

    r->winW = cfg->windowW > 0 ? cfg->windowW : 800;
    r->winH = cfg->windowH > 0 ? cfg->windowH : 600;
    r->internalW = cfg->internalW > 0 ? cfg->internalW : 320;
    r->internalH = cfg->internalH > 0 ? cfg->internalH : 240;
    r->vsync = cfg->vsync;
    r->fullscreen = cfg->fullscreen;

    if (!CreateWindowDX(r))                         goto fail_window;
    if (!CreateDeviceAndSwapchain(r))               goto fail_device;
    if (!CreateBackTargets(r))                      goto fail_back;
    if (!CreateOffscreenTargets(r))                 goto fail_offscreen;

    if (outResult) *outResult = DX11R_OK;
    return r;

fail_offscreen: Dx11Renderer_Destroy(r); if (outResult) *outResult = DX11R_ERR_OFFSCREEN; return NULL;
fail_back:      Dx11Renderer_Destroy(r); if (outResult) *outResult = DX11R_ERR_BACKBUFFER_RTV; return NULL;
fail_device:    Dx11Renderer_Destroy(r); if (outResult) *outResult = DX11R_ERR_DEVICE; return NULL;
fail_window:    Dx11Renderer_Destroy(r); if (outResult) *outResult = DX11R_ERR_WINDOW; return NULL;
}

void Dx11Renderer_Destroy(Dx11Renderer *r) {
    if (!r)
        return;
    for (int i = 0; i < DX11R_OFFSCREEN_COUNT; ++i) {
        if (r->offSRV[i]) r->offSRV[i]->Release();
        if (r->offRTV[i]) r->offRTV[i]->Release();
        if (r->offTex[i]) r->offTex[i]->Release();
    }
    if (r->dsv) r->dsv->Release();
    if (r->depthTex) r->depthTex->Release();
    if (r->backRTV) r->backRTV->Release();
    if (r->backTex) r->backTex->Release();
    if (r->swap) r->swap->Release();
    if (r->ctx) r->ctx->Release();
    if (r->dev) r->dev->Release();
    if (r->hwnd) DestroyWindow(r->hwnd);
    UnregisterClassA(DX11R_WNDCLASS, r->hinst);
    free(r);
}

int Dx11Renderer_GetWindowWidth(const Dx11Renderer *r)     { return r->winW; }
int Dx11Renderer_GetWindowHeight(const Dx11Renderer *r)    { return r->winH; }
int Dx11Renderer_GetInternalWidth(const Dx11Renderer *r)   { return r->internalW; }
int Dx11Renderer_GetInternalHeight(const Dx11Renderer *r)  { return r->internalH; }

int Dx11Renderer_PollMessages(Dx11Renderer *r) {
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            r->quit = 1;
            return 1;
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}

int Dx11Renderer_BeginFrame(Dx11Renderer *r) {
    if (Dx11Renderer_PollMessages(r))
        return 1;

    float clear[4] = { 0.05f, 0.05f, 0.12f, 1.0f };
    r->ctx->ClearRenderTargetView(r->backRTV, clear);
    r->ctx->ClearDepthStencilView(r->dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    return 0;
}

void Dx11Renderer_Present(Dx11Renderer *r) {
    r->swap->Present(r->vsync ? 1u : 0u, 0);
}

static void ApplyViewport(ID3D11DeviceContext *ctx, const D3D11_VIEWPORT *vp) {
    ctx->RSSetViewports(1, vp);
}

void Dx11Renderer_BindBackbuffer(Dx11Renderer *r) {
    ID3D11RenderTargetView *rtv = r->backRTV;
    r->ctx->OMSetRenderTargets(1, &rtv, r->dsv);
    ApplyViewport(r->ctx, &r->backVp);
}

void Dx11Renderer_BindOffscreen(Dx11Renderer *r, int index) {
    if (index < 0 || index >= DX11R_OFFSCREEN_COUNT)
        return;
    ID3D11RenderTargetView *rtv = r->offRTV[index];
    r->ctx->OMSetRenderTargets(1, &rtv, r->dsv);
    ApplyViewport(r->ctx, &r->offVp);
}

ID3D11ShaderResourceView *Dx11Renderer_GetOffscreenSRV(Dx11Renderer *r, int index) {
    if (index < 0 || index >= DX11R_OFFSCREEN_COUNT)
        return NULL;
    return r->offSRV[index];
}

ID3D11Texture2D *Dx11Renderer_GetOffscreenTexture(Dx11Renderer *r, int index) {
    if (index < 0 || index >= DX11R_OFFSCREEN_COUNT)
        return NULL;
    return r->offTex[index];
}

// Copy a render target to a staging texture, map it, and write a 24-bit BMP
// plus a one-line pixel statistics file. Technique from the T0.5 spike.
void Dx11Renderer_CaptureToBMP(Dx11Renderer *r, ID3D11Texture2D *src,
                               const char *bmpPath, const char *statsPath) {
    FILE *dbg = statsPath ? fopen(statsPath, "w") : NULL;
    if (dbg) fprintf(dbg, "capture start\n");

    // NULL src means "capture the swapchain backbuffer" (used for the
    // backbuffer, which the caller does not own).
    ID3D11Texture2D *back = NULL;
    if (!src) {
        if (FAILED(r->swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back))) {
            if (dbg) { fprintf(dbg, "GetBuffer failed\n"); fclose(dbg); }
            return;
        }
        src = back;
    }

    if (!src) {
        if (dbg) { fprintf(dbg, "no source texture\n"); fclose(dbg); }
        return;
    }

    D3D11_TEXTURE2D_DESC bd;
    src->GetDesc(&bd);
    bd.BindFlags = 0;
    bd.MiscFlags = 0;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    bd.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture2D *staging = NULL;
    if (FAILED(r->dev->CreateTexture2D(&bd, NULL, &staging))) {
        if (dbg) { fprintf(dbg, "CreateTexture2D failed\n"); fclose(dbg); }
        return;
    }
    r->ctx->CopyResource(staging, src);

    D3D11_MAPPED_SUBRESOURCE map = {};
    if (FAILED(r->ctx->Map(staging, 0, D3D11_MAP_READ, 0, &map))) {
        if (dbg) { fprintf(dbg, "Map failed\n"); fclose(dbg); }
        staging->Release();
        return;
    }

    int w = (int)bd.Width, h = (int)bd.Height;
    int rowSize = (w * 3 + 3) & ~3;
    int dataSize = rowSize * h;
    int fileSize = 54 + dataSize;
    unsigned char *bmp = (unsigned char *)malloc((size_t)fileSize);
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
        const unsigned char *srcp = (const unsigned char *)map.pData;
        long brightPix = 0; int minX = w, minY = h, maxX = 0, maxY = 0;
        int chr0 = 255, chm0 = 0;
        for (int y = 0; y < h; ++y) {
            const unsigned char *row = srcp + (size_t)(h - 1 - y) * map.RowPitch;
            unsigned char *out = dst + (size_t)y * rowSize;
            for (int x = 0; x < w; ++x) {
                unsigned char b = row[x * 4 + 0];
                unsigned char g = row[x * 4 + 1];
                unsigned char r8 = row[x * 4 + 2];
                out[x * 3 + 0] = b;
                out[x * 3 + 1] = g;
                out[x * 3 + 2] = r8;
                if (r8 > chm0) chm0 = r8;
                if (r8 < chr0) chr0 = r8;
                if (r8 > 60 || g > 60 || b > 60) {
                    ++brightPix;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }
        if (dbg) {
            fprintf(dbg, "brightPix=%ld bbox=(%d,%d)-(%d,%d) Rrange[%d,%d]\n",
                    brightPix, minX, minY, maxX, maxY, chr0, chm0);
        }
        FILE *f = fopen(bmpPath, "wb");
        if (f) {
            fwrite(bmp, 1, (size_t)fileSize, f);
            fclose(f);
            if (dbg) fprintf(dbg, "OK %dx%d -> %s\n", w, h, bmpPath);
        } else if (dbg) {
            fprintf(dbg, "fopen(%s) failed\n", bmpPath);
        }
        free(bmp);
    } else if (dbg) {
        fprintf(dbg, "malloc failed\n");
    }

    r->ctx->Unmap(staging, 0);
    staging->Release();
    if (back) back->Release();
    if (dbg) fclose(dbg);
}

Dx11RendererResult Dx11Renderer_Resize(Dx11Renderer *r, int w, int h) {
    if (w <= 0 || h <= 0)
        return DX11R_ERR_BACKBUFFER_RTV;
    r->winW = w;
    r->winH = h;

    // Unbind and release ALL back-buffer references before resize; otherwise
    // ResizeBuffers fails with DXGI_ERROR_INVALID_CALL (the old buffer is still
    // referenced by backTex). CreateBackTargets re-fetches the new buffer.
    r->ctx->OMSetRenderTargets(0, NULL, NULL);
    if (r->backRTV) { r->backRTV->Release(); r->backRTV = NULL; }
    if (r->backTex) { r->backTex->Release(); r->backTex = NULL; }

    HRESULT hr = r->swap->ResizeBuffers(0, (UINT)w, (UINT)h,
                                        DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        Dx11Fail("ResizeBuffers failed", hr);
        return DX11R_ERR_BACKBUFFER_RTV;
    }
    if (!CreateBackTargets(r))
        return DX11R_ERR_BACKBUFFER_RTV;
    return DX11R_OK;
}

ID3D11Device        *Dx11Renderer_GetDevice(Dx11Renderer *r)         { return r->dev; }
ID3D11DeviceContext *Dx11Renderer_GetContext(Dx11Renderer *r)        { return r->ctx; }
IDXGISwapChain      *Dx11Renderer_GetSwapChain(Dx11Renderer *r)      { return r->swap; }
ID3D11RenderTargetView *Dx11Renderer_GetBackbufferRTV(Dx11Renderer *r) { return r->backRTV; }
ID3D11RenderTargetView *Dx11Renderer_GetOffscreenRTV(Dx11Renderer *r, int index) {
    if (index < 0 || index >= DX11R_OFFSCREEN_COUNT)
        return NULL;
    return r->offRTV[index];
}
ID3D11DepthStencilView *Dx11Renderer_GetDSV(Dx11Renderer *r)          { return r->dsv; }
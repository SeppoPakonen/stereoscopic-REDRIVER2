// dx11_renderer.h — T1.1 DX11 renderer foundation.
//
// A reusable device/context/swaphain/window + render-target module for the
// `dx11` backend. It is deliberately standalone (no game dependency) so it can
// be driven both by the standalone foundation harness (dx11_foundation.cpp)
// and, from Phase 1 on, by the game's draw-command renderer.
//
// Scope (T1.1):
//   * ID3D11Device / ID3D11DeviceContext (feature level 11_0)
//   * native Win32 window (CreateWindowEx) + IDXGISwapChain (no SDL window)
//   * backbuffer RTV + depth/stencil + viewport
//   * offscreen RTV/SRV pair at the internal resolution (T0.4), reusable for
//     per-eye rendering (T2.1)
//   * begin/end-frame (clear + present), resize, and -res/-ires handling
//
// Notes on conventions (from the T0.5 spike):
//   * row-vector x column-major math (v' = v*M); a world z-translation lives
//     at M[2][3], and the projection's w=-z term at M[3][2].
//   * with DXGI_SWAP_EFFECT_DISCARD the back-buffer is undefined after
//     Present, so any capture must happen before Present.

#ifndef DX11_RENDERER_H
#define DX11_RENDERER_H

#include <d3d11.h>
#include <dxgi.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to a renderer instance.
typedef struct Dx11Renderer Dx11Renderer;

// Configuration.
typedef struct {
    int windowW;      // native window size (default 800x600)
    int windowH;
    int internalW;    // internal-resolution offscreen RTs (T0.4, default 320x240)
    int internalH;
    int fullscreen;
    int vsync;        // present interval (0 = uncapped, 1 = vsync)
} Dx11RendererConfig;

// Result codes.
typedef enum {
    DX11R_OK = 0,
    DX11R_ERR_WINDOW,
    DX11R_ERR_DEVICE,
    DX11R_ERR_SWAPCHAIN,
    DX11R_ERR_BACKBUFFER_RTV,
    DX11R_ERR_DEPTH,
    DX11R_ERR_OFFSCREEN,
} Dx11RendererResult;

// Number of offscreen render targets (a left/right pair for stereo).
#define DX11R_OFFSCREEN_COUNT 2

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
// Creates the native window, device, swapchain, backbuffer RTV, depth/stencil
// and the offscreen RTs. Returns NULL on failure; *outResult carries the code.
Dx11Renderer *Dx11Renderer_Create(const Dx11RendererConfig *cfg,
                                  Dx11RendererResult *outResult);

void Dx11Renderer_Destroy(Dx11Renderer *r);

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
int Dx11Renderer_GetWindowWidth(const Dx11Renderer *r);
int Dx11Renderer_GetWindowHeight(const Dx11Renderer *r);
int Dx11Renderer_GetInternalWidth(const Dx11Renderer *r);
int Dx11Renderer_GetInternalHeight(const Dx11Renderer *r);

// ---------------------------------------------------------------------------
// Per-frame
// ---------------------------------------------------------------------------
// Begins a frame: pumps the message queue and clears the backbuffer RTV and
// the depth/stencil. Returns nonzero if the message loop received WM_QUIT.
// The caller should bind a render target (BindBackbuffer / BindOffscreen)
// before issuing draws.
int Dx11Renderer_BeginFrame(Dx11Renderer *r);

// Presents the swapchain (vsync interval from config).
void Dx11Renderer_Present(Dx11Renderer *r);

// ---------------------------------------------------------------------------
// Render-target selection
// ---------------------------------------------------------------------------
// Bind the swapchain backbuffer + its depth/stencil as the render target.
void Dx11Renderer_BindBackbuffer(Dx11Renderer *r);

// Bind one of the internal-resolution offscreen RTs (index 0..COUNT-1) as the
// target. Depth is shared. Use for per-eye / offscreen passes.
void Dx11Renderer_BindOffscreen(Dx11Renderer *r, int index);

// The offscreen texture as a shader resource, for sampling/compositing in
// later phases. Returns NULL if index is out of range.
ID3D11ShaderResourceView *Dx11Renderer_GetOffscreenSRV(Dx11Renderer *r, int index);

// The offscreen texture itself (for reads/capture). NULL if index out of range.
ID3D11Texture2D *Dx11Renderer_GetOffscreenTexture(Dx11Renderer *r, int index);

// ---------------------------------------------------------------------------
// Verification aid
// ---------------------------------------------------------------------------
// Copies `src` (a backbuffer or offscreen texture owned by this renderer) to a
// staging resource and writes a 24-bit BMP plus a one-line pixel-statistics
// file. Read-only, does not disturb the render targets. Used headless to prove
// a frame rendered correctly (DISCARD: call before Present for the backbuffer).
// `statsPath` may be NULL to skip the stats file.
void Dx11Renderer_CaptureToBMP(Dx11Renderer *r, ID3D11Texture2D *src,
                               const char *bmpPath, const char *statsPath);

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------
// Recreates the swapchain backbuffer (and its RTV/DSV) for a new window size.
// The offscreen internal-resolution RTs are unaffected. Present afterwards.
Dx11RendererResult Dx11Renderer_Resize(Dx11Renderer *r, int w, int h);

// Pumps the message queue (used by BeginFrame). Returns nonzero on WM_QUIT.
int Dx11Renderer_PollMessages(Dx11Renderer *r);

// ---------------------------------------------------------------------------
// Raw accessors (used by the harness / later renderer phases to issue draws)
// ---------------------------------------------------------------------------
ID3D11Device        *Dx11Renderer_GetDevice(Dx11Renderer *r);
ID3D11DeviceContext *Dx11Renderer_GetContext(Dx11Renderer *r);
IDXGISwapChain      *Dx11Renderer_GetSwapChain(Dx11Renderer *r);
ID3D11RenderTargetView *Dx11Renderer_GetBackbufferRTV(Dx11Renderer *r);
ID3D11RenderTargetView *Dx11Renderer_GetOffscreenRTV(Dx11Renderer *r, int index);
ID3D11DepthStencilView *Dx11Renderer_GetDSV(Dx11Renderer *r);

#ifdef __cplusplus
}
#endif

#endif // DX11_RENDERER_H
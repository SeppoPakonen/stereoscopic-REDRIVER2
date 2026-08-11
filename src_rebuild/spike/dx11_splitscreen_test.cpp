// dx11_splitscreen_test.cpp — T3.2 split-screen composite harness.
//
// Verifies the 2-player x 2-eye split-composite layout (2 players x 2 eyes =
// 4 images) maps four distinct eye images into the four backbuffer quadrants,
// for horizontal/vertical player split x SBS/TB eye layout, with no
// cross-player or cross-eye contamination, and that swap flips the eye
// assignment within each player.
//
// The four eye images are distinct solid colors:
//   P1-L = red, P1-R = green, P2-L = blue, P2-R = yellow.
// Probes:
//   H_SBS/H_TB/V_SBS/V_TB — quadrant mapping for each split x layout;
//   SWAP — swap flips the left/top eye within each player.

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_composite.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_splitscreen_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

// Create a solid-color 2x2 R8G8B8A8 SRV.
static ID3D11ShaderResourceView *CreateSolidSRV(ID3D11Device *dev,
                                                unsigned char r, unsigned char g, unsigned char b) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 2; td.Height = 2; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    unsigned char px[16] = { r,g,b,255, r,g,b,255, r,g,b,255, r,g,b,255 };
    D3D11_SUBRESOURCE_DATA sd = { px, 8, 0 };
    ID3D11Texture2D *tex = NULL;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return NULL;
    ID3D11ShaderResourceView *srv = NULL;
    if (FAILED(dev->CreateShaderResourceView(tex, NULL, &srv))) { tex->Release(); return NULL; }
    tex->Release();
    return srv;
}

// BMP probe (bottom-up, 24-bit BGR, rowSize padded to 4).
static int ProbeBMP(const char *path, int x, int y, int *r, int *g, int *b) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    unsigned char hdr[54];
    if (fread(hdr, 1, 54, f) != 54) { fclose(f); return 1; }
    int w = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
    int h = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
    if (x < 0 || x >= w || y < 0 || y >= h) { fclose(f); return 1; }
    int rowSize = (w * 3 + 3) & ~3;
    long off = 54 + (long)(h - 1 - y) * rowSize + (long)x * 3;
    if (fseek(f, off, SEEK_SET) != 0) { fclose(f); return 1; }
    unsigned char p[3];
    if (fread(p, 1, 3, f) != 3) { fclose(f); return 1; }
    *b = p[0]; *g = p[1]; *r = p[2];
    fclose(f);
    return 0;
}

// Expected colours of the four quadrants.
struct Quad { int redA, greA, bluA, redB, greB, bluB; };
static int IsRed(int r, int g, int b) { return r > 180 && g < 40 && b < 40; }
static int IsGreen(int r, int g, int b) { return g > 150 && r < 60 && b < 60; }
static int IsBlue(int r, int g, int b) { return b > 180 && r < 40 && g < 40; }
static int IsYellow(int r, int g, int b) { return r > 180 && g > 150 && b < 60; }

// Probe one quadrant; returns 1 if it matches the expected colour flag fn.
typedef int (*ColorFn)(int, int, int);

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    Dx11RendererConfig rcfg = { 800, 600, 320, 240, 0, 0 };
    (void)lpCmdLine;

    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_splitscreen_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11CompositeResult cr;
    Dx11Composite *comp = Dx11Composite_Create(dev, ctx, &cr);
    if (!comp) { MessageBoxA(NULL, "Dx11Composite_Create failed", "dx11_splitscreen_test", MB_OK | MB_ICONERROR); return 2; }

    ID3D11ShaderResourceView *p1L = CreateSolidSRV(dev, 220, 0, 0);   // red
    ID3D11ShaderResourceView *p1R = CreateSolidSRV(dev, 0, 200, 0);   // green
    ID3D11ShaderResourceView *p2L = CreateSolidSRV(dev, 0, 0, 220);   // blue
    ID3D11ShaderResourceView *p2R = CreateSolidSRV(dev, 220, 200, 0); // yellow
    if (!p1L || !p1R || !p2L || !p2R) { MessageBoxA(NULL, "CreateSolidSRV failed", "dx11_splitscreen_test", MB_OK | MB_ICONERROR); return 2; }

    int W = rcfg.windowW, H = rcfg.windowH;
    int fails = 0;
    FILE *resf = fopen("dx11_splitscreen_result.txt", "w");
    if (!resf) { MessageBoxA(NULL, "cannot open result file", "dx11_splitscreen_test", MB_OK); return 2; }

    // Render a split-composite into the backbuffer and capture it.
    auto ComposeSplit = [&](Dx11CompositeSplit split, Dx11CompositeEyeLayout layout,
                            int swap, const char *bmp) {
        if (Dx11Renderer_BeginFrame(ren) != 0) return;
        Dx11Renderer_BindBackbuffer(ren);
        Dx11Composite_SplitComposite(comp, ctx, split, layout, swap, p1L, p1R, p2L, p2R, W, H);
        Dx11Renderer_CaptureToBMP(ren, NULL, bmp, NULL);
        Dx11Renderer_Present(ren);
    };
    ComposeSplit(DX11C_SPLIT_H, DX11C_EYE_SBS, 0, "splitscreen_h_sbs.bmp");
    ComposeSplit(DX11C_SPLIT_H, DX11C_EYE_TB, 0, "splitscreen_h_tb.bmp");
    ComposeSplit(DX11C_SPLIT_V, DX11C_EYE_SBS, 0, "splitscreen_v_sbs.bmp");
    ComposeSplit(DX11C_SPLIT_V, DX11C_EYE_TB, 0, "splitscreen_v_tb.bmp");
    ComposeSplit(DX11C_SPLIT_H, DX11C_EYE_SBS, 1, "splitscreen_swap.bmp");

    // Probe a quadrant; append PASS/FAIL against the expected colour fn.
    auto Probe = [&](const char *bmp, int x, int y, ColorFn fn, const char *tag) {
        int r = 0, g = 0, b = 0;
        if (ProbeBMP(bmp, x, y, &r, &g, &b) == 0) {
            int ok = fn(r, g, b);
            fprintf(resf, "%s(%d,%d)=(%d,%d,%d) %s\n", tag, x, y, r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "%s probe unavailable\n", tag); }
    };

    // H split, SBS: P1 left half (L/R), P2 right half (L/R).
    fprintf(resf, "H_SBS:\n");
    Probe("splitscreen_h_sbs.bmp", W / 8, H / 2, IsRed,   "  P1L");
    Probe("splitscreen_h_sbs.bmp", 3 * W / 8, H / 2, IsGreen, "  P1R");
    Probe("splitscreen_h_sbs.bmp", 5 * W / 8, H / 2, IsBlue,  "  P2L");
    Probe("splitscreen_h_sbs.bmp", 7 * W / 8, H / 2, IsYellow," P2R");

    // H split, TB: each player's eyes top/bottom within their half.
    fprintf(resf, "H_TB:\n");
    Probe("splitscreen_h_tb.bmp", W / 4, H / 4, IsRed,    "  P1L");
    Probe("splitscreen_h_tb.bmp", W / 4, 3 * H / 4, IsGreen, "  P1R");
    Probe("splitscreen_h_tb.bmp", 3 * W / 4, H / 4, IsBlue,  "  P2L");
    Probe("splitscreen_h_tb.bmp", 3 * W / 4, 3 * H / 4, IsYellow, "P2R");

    // V split, SBS: P1 top half (L/R), P2 bottom half (L/R).
    fprintf(resf, "V_SBS:\n");
    Probe("splitscreen_v_sbs.bmp", W / 4, H / 4, IsRed,    "  P1L");
    Probe("splitscreen_v_sbs.bmp", 3 * W / 4, H / 4, IsGreen, "  P1R");
    Probe("splitscreen_v_sbs.bmp", W / 4, 3 * H / 4, IsBlue,  "  P2L");
    Probe("splitscreen_v_sbs.bmp", 3 * W / 4, 3 * H / 4, IsYellow, "P2R");

    // V split, TB: each player's eyes top/bottom within their half.
    fprintf(resf, "V_TB:\n");
    Probe("splitscreen_v_tb.bmp", W / 2, H / 8, IsRed,    "  P1L");
    Probe("splitscreen_v_tb.bmp", W / 2, 3 * H / 8, IsGreen, "  P1R");
    Probe("splitscreen_v_tb.bmp", W / 2, 5 * H / 8, IsBlue,  "  P2L");
    Probe("splitscreen_v_tb.bmp", W / 2, 7 * H / 8, IsYellow, "P2R");

    // SWAP (H_SBS, swap=1): P1 left = green, P1 right = red.
    fprintf(resf, "SWAP:\n");
    Probe("splitscreen_swap.bmp", W / 8, H / 2, IsGreen, "  P1L");
    Probe("splitscreen_swap.bmp", 3 * W / 8, H / 2, IsRed, "  P1R");

    fprintf(resf, "TOTAL_FAILS=%d SPLITSCREEN=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    p1R->Release(); p1L->Release(); p2R->Release(); p2L->Release();
    Dx11Composite_Destroy(comp);
    Dx11Renderer_Destroy(ren);
    return 0;
}
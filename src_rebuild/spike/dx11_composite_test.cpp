// dx11_composite_test.cpp — T2.3 SBS/TB composite harness.
//
// Renders a distinct colour quad per eye into the two offscreen RTs (red eye0 /
// blue eye1), then composites them into the backbuffer for each (mode, swap)
// and captures a BMP, probing that the composite lays the eyes out exactly as
// the legacy StereoCompositor_Composite:
//   * SBS — left half = eye0, right half = eye1;
//   * TB  — top half  = eye0, bottom half = eye1;
//   * SWAP — swap flips which eye fills the left/top half;
//   * MONO — pass-through of eye0 across the full screen.

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_resources.h"
#include "dx11_textures.h"
#include "dx11_shaders.h"
#include "dx11_drawcmdexec.h"
#include "dx11_composite.h"

#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Matrix helpers (row-vector x column-major). Camera at origin, no view.
// ---------------------------------------------------------------------------
static void MatIdentity(float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    for (int i = 0; i < 4; ++i) m[i][i] = 1.0f;
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

// Push a full-eye quad (covers the whole internal viewport at z=-3) and return
// its arena ranges.
struct Range { int vo, vc, io, ic; };
static struct Range AddQuad(Dx11Res *res) {
    Dx11ResVertex v[4];
    v[0] = { -2.5f,  2.0f, -3, 1, 1, 1, 1, 0, 0 };
    v[1] = {  2.5f,  2.0f, -3, 1, 1, 1, 1, 1, 0 };
    v[2] = {  2.5f, -2.0f, -3, 1, 1, 1, 1, 1, 1 };
    v[3] = { -2.5f, -2.0f, -3, 1, 1, 1, 1, 0, 1 };
    struct Range rg;
    int v0i = Dx11Res_PushVertex(res, &v[0]);
    int v1i = Dx11Res_PushVertex(res, &v[1]);
    int v2i = Dx11Res_PushVertex(res, &v[2]);
    int v3i = Dx11Res_PushVertex(res, &v[3]);
    rg.vo = v0i; rg.vc = 4;
    unsigned short ind[6] = { (unsigned short)v0i, (unsigned short)v1i, (unsigned short)v2i,
                              (unsigned short)v0i, (unsigned short)v2i, (unsigned short)v3i };
    rg.io = Dx11Res_IndexCount(res);
    for (int k = 0; k < 6; ++k) Dx11Res_PushIndex(res, ind[k]);
    rg.ic = 6;
    return rg;
}

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_composite_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

static int ParsePair(const char *s, int *a, int *b) {
    return (sscanf(s, "%dx%d", a, b) == 2 && *a > 0 && *b > 0) ? 1 : 0;
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
    unsigned char px[3];
    if (fread(px, 1, 3, f) != 3) { fclose(f); return 1; }
    *b = px[0]; *g = px[1]; *r = px[2];
    fclose(f);
    return 0;
}

// Render a full-eye flat-colour quad into offscreen RT `e` and return via the
// caller's res/cmds (both must have BeginFrame'd). BeginFrame clears only the
// backbuffer, so each offscreen is cleared first.
static void RenderEye(Dx11Renderer *ren, Dx11DrawCmds *cmds, Dx11Res *res,
                      ID3D11DeviceContext *ctx,
                      const float vp[4][4], const float ident[4][4],
                      int e, float r, float g, float b) {
    Dx11Renderer_BindOffscreen(ren, e);
    float base[4] = { 0, 0, 0, 1 };
    ctx->ClearRenderTargetView(Dx11Renderer_GetOffscreenRTV(ren, e), base);
    ctx->ClearDepthStencilView(Dx11Renderer_GetDSV(ren), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    Dx11DrawCmds_BeginFrame(cmds);
    Dx11Res_BeginFrame(res);
    Dx11DrawCmds_SetViewProj(cmds, vp);
    struct Range R = AddQuad(res);
    Dx11DrawCmdItem it = {};
    it.vertexOffset = R.vo; it.vertexCount = R.vc;
    it.indexOffset = R.io; it.indexCount = R.ic;
    it.bboxMin[0] = -2.5f; it.bboxMax[0] = 2.5f;
    it.bboxMin[1] = -2.0f; it.bboxMax[1] = 2.0f;
    it.bboxMin[2] = -3; it.bboxMax[2] = -3;
    memcpy(it.world, ident, 16 * sizeof(float)); // ident is a param: sizeof is a pointer
    it.texture = -1;
    it.flatColor[0] = r; it.flatColor[1] = g; it.flatColor[2] = b; it.flatColor[3] = 1.0f;
    it.blend = DX11SH_BLEND_NONE; it.shade = DX11SH_COLOR_FLAT; it.translucent = 0;
    Dx11DrawCmds_Submit(cmds, &it);
    Dx11DrawCmds_Execute(cmds, ctx);
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
                int w, hh; if (ParsePair(tok, &w, &hh)) { rcfg.windowW = w; rcfg.windowH = hh; }
            }
            else if (!strcmp(tok, "-ires") && (tok = strtok(NULL, " \t"))) {
                int w, hh; if (ParsePair(tok, &w, &hh)) { rcfg.internalW = w; rcfg.internalH = hh; }
            }
            tok = strtok(NULL, " \t");
        }
    }

    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_composite_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ResResult rsres;
    Dx11Res *res = Dx11Res_Create(dev, ctx, NULL, &rsres);
    if (!res) { MessageBoxA(NULL, "Dx11Res_Create failed", "dx11_composite_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11TexResult tr;
    Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
    if (!tex) { MessageBoxA(NULL, "Dx11Tex_Create failed", "dx11_composite_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11ShadersResult sr;
    Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    if (!sh) { MessageBoxA(NULL, "Dx11Shaders_Create failed", "dx11_composite_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11DrawCmdsResult cr;
    Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, res, tex, sh, NULL, &cr);
    if (!cmds) { MessageBoxA(NULL, "Dx11DrawCmds_Create failed", "dx11_composite_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11CompositeResult cp;
    Dx11Composite *comp = Dx11Composite_Create(dev, ctx, &cp);
    if (!comp) { MessageBoxA(NULL, "Dx11Composite_Create failed", "dx11_composite_test", MB_OK | MB_ICONERROR); return 2; }

    float vp[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f,
                     (float)rcfg.internalW / (float)rcfg.internalH, 0.1f, 100.0f, vp);
    float ident[4][4]; MatIdentity(ident);

    // Render eye0 (red) into RT0 and eye1 (blue) into RT1.
    float red[3]  = { 220.0f / 255.0f, 0, 0 };   // eye0
    float blue[3] = { 0, 0, 220.0f / 255.0f };   // eye1
    for (int e = 0; e < 2; ++e) {
        if (Dx11Renderer_BeginFrame(ren) != 0) break;
        float *col = (e == 0) ? red : blue;
        RenderEye(ren, cmds, res, ctx, (const float (*)[4])vp, (const float (*)[4])ident, e,
                  col[0], col[1], col[2]);
    }

    ID3D11ShaderResourceView *eye0 = Dx11Renderer_GetOffscreenSRV(ren, 0);
    ID3D11ShaderResourceView *eye1 = Dx11Renderer_GetOffscreenSRV(ren, 1);

    int W = rcfg.windowW, H = rcfg.windowH;
    int fails = 0;
    FILE *resf = fopen("dx11_composite_result.txt", "w");
    if (!resf) { MessageBoxA(NULL, "cannot open result file", "dx11_composite_test", MB_OK); return 2; }

    // Do a composite pass into the backbuffer and capture it.
    auto Compose = [&](Dx11CompositeMode mode, int swap, const char *bmp) {
        if (Dx11Renderer_BeginFrame(ren) != 0) return;
        Dx11Renderer_BindBackbuffer(ren);
        Dx11Composite_Composite(comp, ctx, mode, swap, eye0, eye1, W, H);
        Dx11Renderer_CaptureToBMP(ren, NULL, bmp, NULL);
        Dx11Renderer_Present(ren);
    };
    Compose(DX11C_MODE_SBS, 0, "composite_sbs.bmp");
    Compose(DX11C_MODE_TB, 0,  "composite_tb.bmp");
    Compose(DX11C_MODE_SBS, 1, "composite_sbs_swap.bmp");
    Compose(DX11C_MODE_TB, 1,  "composite_tb_swap.bmp");
    Compose(DX11C_MODE_MONO, 0, "composite_mono.bmp");

    int r = 0, g = 0, b = 0;
    auto IsRed = [](int r, int g, int b) { return r > 180 && g < 40 && b < 40; };
    auto IsBlue = [](int r, int g, int b) { return b > 180 && r < 40 && g < 40; };

    // SBS (swap=0): left half = red (eye0), right half = blue (eye1).
    if (ProbeBMP("composite_sbs.bmp", W / 4, H / 2, &r, &g, &b) == 0) {
        int ok = IsRed(r, g, b);
        fprintf(resf, "SBS_L left-half(%d,%d)=(%d,%d,%d) red %s\n", W / 4, H / 2, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "SBS_L probe unavailable\n"); }
    if (ProbeBMP("composite_sbs.bmp", 3 * W / 4, H / 2, &r, &g, &b) == 0) {
        int ok = IsBlue(r, g, b);
        fprintf(resf, "SBS_R right-half(%d,%d)=(%d,%d,%d) blue %s\n", 3 * W / 4, H / 2, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "SBS_R probe unavailable\n"); }

    // TB (swap=0): top half = red (eye0), bottom half = blue (eye1).
    if (ProbeBMP("composite_tb.bmp", W / 2, H / 4, &r, &g, &b) == 0) {
        int ok = IsRed(r, g, b);
        fprintf(resf, "TB_T top-half(%d,%d)=(%d,%d,%d) red %s\n", W / 2, H / 4, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "TB_T probe unavailable\n"); }
    if (ProbeBMP("composite_tb.bmp", W / 2, 3 * H / 4, &r, &g, &b) == 0) {
        int ok = IsBlue(r, g, b);
        fprintf(resf, "TB_B bottom-half(%d,%d)=(%d,%d,%d) blue %s\n", W / 2, 3 * H / 4, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "TB_B probe unavailable\n"); }

    // SWAP SBS (swap=1): left = blue, right = red.
    if (ProbeBMP("composite_sbs_swap.bmp", W / 4, H / 2, &r, &g, &b) == 0) {
        int ok = IsBlue(r, g, b);
        fprintf(resf, "SWAP SBS left = blue (%d,%d,%d) %s\n", r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "SWAP SBS left probe unavailable\n"); }
    if (ProbeBMP("composite_sbs_swap.bmp", 3 * W / 4, H / 2, &r, &g, &b) == 0) {
        int ok = IsRed(r, g, b);
        fprintf(resf, "SWAP SBS right = red (%d,%d,%d) %s\n", r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "SWAP SBS right probe unavailable\n"); }

    // SWAP TB (swap=1): top = blue, bottom = red.
    if (ProbeBMP("composite_tb_swap.bmp", W / 2, H / 4, &r, &g, &b) == 0) {
        int ok = IsBlue(r, g, b);
        fprintf(resf, "SWAP TB top = blue (%d,%d,%d) %s\n", r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "SWAP TB top probe unavailable\n"); }
    if (ProbeBMP("composite_tb_swap.bmp", W / 2, 3 * H / 4, &r, &g, &b) == 0) {
        int ok = IsRed(r, g, b);
        fprintf(resf, "SWAP TB bottom = red (%d,%d,%d) %s\n", r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "SWAP TB bottom probe unavailable\n"); }

    // MONO: full-screen pass-through of eye0 = red.
    if (ProbeBMP("composite_mono.bmp", W / 2, H / 2, &r, &g, &b) == 0) {
        int ok = IsRed(r, g, b);
        fprintf(resf, "MONO full(%d,%d)=(%d,%d,%d) red (eye0 pass-through) %s\n", W / 2, H / 2, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "MONO probe unavailable\n"); }

    fprintf(resf, "TOTAL_FAILS=%d COMPOSITE=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    Dx11Composite_Destroy(comp);
    Dx11DrawCmds_Destroy(cmds);
    Dx11Shaders_Destroy(sh);
    Dx11Tex_Destroy(tex);
    Dx11Res_Destroy(res);
    Dx11Renderer_Destroy(ren);
    return 0;
}
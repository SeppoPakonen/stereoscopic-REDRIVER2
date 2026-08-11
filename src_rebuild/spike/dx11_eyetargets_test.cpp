// dx11_eyetargets_test.cpp — T2.1 per-eye render-target harness.
//
// Proves the DX11 per-eye offscreen RT pair (T1.1) renders each eye into its
// OWN render target at a configurable internal resolution, with independent
// contents and no cross-eye reuse, while the non-stereo path still renders to
// the backbuffer:
//   * EYE0 — a red quad rendered into offscreen RT 0 (left) -> left BMP is red;
//   * EYE1 — a blue quad rendered into offscreen RT 1 (right) -> right BMP is
//            blue (independent of eye 0);
//   * INDEP — the two eye captures differ at the same pixel (no shared texture);
//   * IRES  — the captured dims equal the configured internal resolution
//            (configurable, not 320x240-locked);
//   * BACK  — a green quad on the backbuffer (non-stereo path) is separate.

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_resources.h"
#include "dx11_textures.h"
#include "dx11_shaders.h"
#include "dx11_drawcmdexec.h"

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
static struct Range AddQuad(Dx11Res *res, float r, float g, float b) {
    Dx11ResVertex v[4];
    // x -2.5..2.5, y -2..2 spans the 60-FOV frustum at z=-3 for any 4:3 aspect.
    v[0] = { -2.5f,  2.0f, -3, 1, 1, 1, 1, 0, 0 };
    v[1] = {  2.5f,  2.0f, -3, 1, 1, 1, 1, 1, 0 };
    v[2] = {  2.5f, -2.0f, -3, 1, 1, 1, 1, 1, 1 };
    v[3] = { -2.5f, -2.0f, -3, 1, 1, 1, 1, 0, 1 };
    struct Range rg;
    int v0i = Dx11Res_PushVertex(res, &v[0]);
    int v1i = Dx11Res_PushVertex(res, &v[1]);
    int v2i = Dx11Res_PushVertex(res, &v[2]);
    int v3i = Dx11Res_PushVertex(res, &v[3]);
    (void)r; (void)g; (void)b;   // color via the command flatColor
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
    MessageBoxA(NULL, buf, "dx11_eyetargets_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

static int ParsePair(const char *s, int *a, int *b) {
    return (sscanf(s, "%dx%d", a, b) == 2 && *a > 0 && *b > 0) ? 1 : 0;
}

// BMP probe (bottom-up, 24-bit BGR, rowSize padded to 4). Also returns the dims
// via *outW/*outH when non-NULL.
static int ProbeBMP(const char *path, int x, int y, int *r, int *g, int *b,
                    int *outW, int *outH) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    unsigned char hdr[54];
    if (fread(hdr, 1, 54, f) != 54) { fclose(f); return 1; }
    int w = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
    int h = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
    if (outW) *outW = w;
    if (outH) *outH = h;
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
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_eyetargets_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ResResult rsres;
    Dx11Res *res = Dx11Res_Create(dev, ctx, NULL, &rsres);
    if (!res) { MessageBoxA(NULL, "Dx11Res_Create failed", "dx11_eyetargets_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11TexResult tr;
    Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
    if (!tex) { MessageBoxA(NULL, "Dx11Tex_Create failed", "dx11_eyetargets_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11ShadersResult sr;
    Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    if (!sh) { MessageBoxA(NULL, "Dx11Shaders_Create failed", "dx11_eyetargets_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11DrawCmdsResult cr;
    Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, res, tex, sh, NULL, &cr);
    if (!cmds) { MessageBoxA(NULL, "Dx11DrawCmds_Create failed", "dx11_eyetargets_test", MB_OK | MB_ICONERROR); return 2; }

    float vp[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f,
                     (float)rcfg.internalW / (float)rcfg.internalH, 0.1f, 100.0f, vp);
    float ident[4][4]; MatIdentity(ident);

    // Helper: render a full-eye flat-colour quad into a bound RT and capture it.
    // (BeginFrame clears only the backbuffer, so each offscreen is cleared first.)
    int fails = 0;
    FILE *resf = fopen("dx11_eyetargets_result.txt", "w");
    if (!resf) { MessageBoxA(NULL, "cannot open result file", "dx11_eyetargets_test", MB_OK); return 2; }

    struct { float base[4]; float color[3]; const char *bmp; const char *name; } eye[2] = {
        { { 0.15f, 0.0f, 0.0f, 1.0f }, { 220.0f/255.0f, 0, 0 }, "eyetargets_left.bmp",  "EYE0 left"  },
        { { 0.0f, 0.0f, 0.15f, 1.0f }, { 0, 0, 220.0f/255.0f }, "eyetargets_right.bmp", "EYE1 right" },
    };

    for (int e = 0; e < 2; ++e) {
        if (Dx11Renderer_BeginFrame(ren) != 0) break;
        Dx11Renderer_BindOffscreen(ren, e);
        ctx->ClearRenderTargetView(Dx11Renderer_GetOffscreenRTV(ren, e), eye[e].base);
        ctx->ClearDepthStencilView(Dx11Renderer_GetDSV(ren), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        Dx11DrawCmds_BeginFrame(cmds);
        Dx11Res_BeginFrame(res);
        Dx11DrawCmds_SetViewProj(cmds, (const float (*)[4])vp);

        struct Range R = AddQuad(res, 0, 0, 0);
        Dx11DrawCmdItem it = {};
        it.vertexOffset = R.vo; it.vertexCount = R.vc;
        it.indexOffset = R.io; it.indexCount = R.ic;
        it.bboxMin[0] = -2.5f; it.bboxMax[0] = 2.5f;
        it.bboxMin[1] = -2.0f; it.bboxMax[1] = 2.0f;
        it.bboxMin[2] = -3; it.bboxMax[2] = -3;
        memcpy(it.world, ident, sizeof(ident));
        it.texture = -1;
        it.flatColor[0] = eye[e].color[0]; it.flatColor[1] = eye[e].color[1];
        it.flatColor[2] = eye[e].color[2]; it.flatColor[3] = 1.0f;
        it.blend = DX11SH_BLEND_NONE; it.shade = DX11SH_COLOR_FLAT; it.translucent = 0;
        Dx11DrawCmds_Submit(cmds, &it);

        Dx11DrawCmds_Execute(cmds, ctx);
        Dx11Renderer_CaptureToBMP(ren, Dx11Renderer_GetOffscreenTexture(ren, e),
                                  eye[e].bmp, NULL);
    }

    // Non-stereo path: render a green quad to the backbuffer.
    if (Dx11Renderer_BeginFrame(ren) == 0) {
        Dx11Renderer_BindBackbuffer(ren);
        Dx11DrawCmds_BeginFrame(cmds);
        Dx11Res_BeginFrame(res);
        Dx11DrawCmds_SetViewProj(cmds, (const float (*)[4])vp);
        struct Range R = AddQuad(res, 0, 0, 0);
        Dx11DrawCmdItem it = {};
        it.vertexOffset = R.vo; it.vertexCount = R.vc;
        it.indexOffset = R.io; it.indexCount = R.ic;
        it.bboxMin[0] = -2.5f; it.bboxMax[0] = 2.5f;
        it.bboxMin[1] = -2.0f; it.bboxMax[1] = 2.0f;
        it.bboxMin[2] = -3; it.bboxMax[2] = -3;
        memcpy(it.world, ident, sizeof(ident));
        it.texture = -1;
        it.flatColor[0] = 0; it.flatColor[1] = 220.0f/255.0f; it.flatColor[2] = 0; it.flatColor[3] = 1.0f;
        it.blend = DX11SH_BLEND_NONE; it.shade = DX11SH_COLOR_FLAT; it.translucent = 0;
        Dx11DrawCmds_Submit(cmds, &it);
        Dx11DrawCmds_Execute(cmds, ctx);
        Dx11Renderer_CaptureToBMP(ren, NULL, "eyetargets_back.bmp", NULL);
        Dx11Renderer_Present(ren);
    }

    int W = rcfg.internalW, H = rcfg.internalH;
    int cx = W / 2, cy = H / 2;
    int r = 0, g = 0, b = 0, w = 0, h = 0;

    // EYE0: left BMP centre = red.
    if (ProbeBMP("eyetargets_left.bmp", cx, cy, &r, &g, &b, &w, &h) == 0) {
        int ok = (r > 180 && g < 40 && b < 40);
        fprintf(resf, "EYE0 left(%d,%d)=(%d,%d,%d) red %s\n", cx, cy, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "EYE0 probe unavailable\n"); }

    // EYE0: left BMP dims = internal res.
    int okIres = (w == W && h == H);
    fprintf(resf, "IRES left bmp %dx%d expect %dx%d %s\n", w, h, W, H, okIres ? "PASS" : "FAIL");
    if (!okIres) ++fails;

    // EYE1: right BMP centre = blue.
    if (ProbeBMP("eyetargets_right.bmp", cx, cy, &r, &g, &b, &w, &h) == 0) {
        int ok = (b > 180 && r < 40 && g < 40);
        fprintf(resf, "EYE1 right(%d,%d)=(%d,%d,%d) blue %s\n", cx, cy, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "EYE1 probe unavailable\n"); }

    // INDEP: left and right differ at the same pixel (no cross-eye reuse).
    int lr = 0, lg = 0, lb = 0, rrt = 0, gp = 0, bp = 0;
    if (ProbeBMP("eyetargets_left.bmp", cx, cy, &lr, &lg, &lb, NULL, NULL) == 0 &&
        ProbeBMP("eyetargets_right.bmp", cx, cy, &rrt, &gp, &bp, NULL, NULL) == 0) {
        int ok = (lr != rrt || lg != gp || lb != bp);
        fprintf(resf, "INDEP left(%d,%d,%d) vs right(%d,%d,%d) differ %s\n",
                lr, lg, lb, rrt, gp, bp, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "INDEP probe unavailable\n"); }

    // BACK: backbuffer capture centre = green (non-stereo path separate).
    if (ProbeBMP("eyetargets_back.bmp", rcfg.windowW / 2, rcfg.windowH / 2, &r, &g, &b, NULL, NULL) == 0) {
        int ok = (g > 180 && r < 40 && b < 40);
        fprintf(resf, "BACK backbuffer(%d,%d)=(%d,%d,%d) green %s\n",
                rcfg.windowW / 2, rcfg.windowH / 2, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "BACK probe unavailable\n"); }

    fprintf(resf, "TOTAL_FAILS=%d EYETARGETS=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    Dx11DrawCmds_Destroy(cmds);
    Dx11Shaders_Destroy(sh);
    Dx11Tex_Destroy(tex);
    Dx11Res_Destroy(res);
    Dx11Renderer_Destroy(ren);
    return 0;
}
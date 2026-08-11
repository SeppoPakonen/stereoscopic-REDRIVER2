// dx11_ires_test.cpp — T3.3 higher-internal-resolution harness.
//
// Verifies the DX11 renderer's internal-resolution rendering is
// projection-correct at every resolution (the legacy PSX 320x240 lock broke the
// projection at non-native res — perspective centre landed top-left). For each
// internal resolution in {320x240, 640x480, 1280x720} it renders a symmetric
// pair of vertical marker bars (red at world x=-a, blue at x=+a, z=-d) into the
// offscreen RT (identity camera) and verifies:
//   * IRES — the captured offscreen dims equal the configured resolution;
//   * PROJ — each bar's rendered centre column matches the analytic projection
//            col(x) = ((f/aspect)*x/(1-d) + 1)/2 * iW (projection stays correct);
//   * SYMM — the rendered left/right bar centres are symmetric about iW/2 (no
//            top-left perspective-centre shift at higher res).

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
// Matrix helpers (row-vector x column-major).
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

struct Range { int vo, vc, io, ic; };
static struct Range AddQuadXY(Dx11Res *res, float x0, float x1, float y0, float y1,
                              float z) {
    Dx11ResVertex v[4];
    v[0] = { x0, y1, z, 1, 1, 1, 1, 0, 0 };
    v[1] = { x1, y1, z, 1, 1, 1, 1, 1, 0 };
    v[2] = { x1, y0, z, 1, 1, 1, 1, 1, 1 };
    v[3] = { x0, y0, z, 1, 1, 1, 1, 0, 1 };
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

static void SubmitBar(Dx11DrawCmds *cmds, Dx11Res *res, const float ident[4][4],
                      float cx, float r, float g, float b) {
    struct Range R = AddQuadXY(res, cx - 0.06f, cx + 0.06f, -0.5f, 0.5f, -3.0f);
    Dx11DrawCmdItem it = {};
    it.vertexOffset = R.vo; it.vertexCount = R.vc;
    it.indexOffset = R.io; it.indexCount = R.ic;
    it.bboxMin[0] = cx - 0.06f; it.bboxMax[0] = cx + 0.06f;
    it.bboxMin[1] = -0.5f; it.bboxMax[1] = 0.5f;
    it.bboxMin[2] = -3; it.bboxMax[2] = -3;
    memcpy(it.world, ident, 16 * sizeof(float));
    it.texture = -1;
    it.flatColor[0] = r; it.flatColor[1] = g; it.flatColor[2] = b; it.flatColor[3] = 1.0f;
    it.blend = DX11SH_BLEND_NONE; it.shade = DX11SH_COLOR_FLAT; it.translucent = 0;
    Dx11DrawCmds_Submit(cmds, &it);
}

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_ires_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

// BMP probe (bottom-up, 24-bit BGR, rowSize padded to 4). Returns dims via out.
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
    unsigned char p[3];
    if (fread(p, 1, 3, f) != 3) { fclose(f); return 1; }
    *b = p[0]; *g = p[1]; *r = p[2];
    fclose(f);
    return 0;
}

typedef int (*ColorFn)(int, int, int);
static int IsRed(int r, int g, int b) { return r > 180 && g < 40 && b < 40; }
static int IsBlue(int r, int g, int b) { return b > 180 && r < 40 && g < 40; }

// Centre column of the first horizontal run matching `fn` in row `y`; -1 if none.
static int ScanRunCentre(const char *path, int y, int w, ColorFn fn) {
    int first = -1, last = -1;
    for (int x = 0; x < w; ++x) {
        int r = 0, g = 0, b = 0;
        if (ProbeBMP(path, x, y, &r, &g, &b, NULL, NULL) == 0 && fn(r, g, b)) {
            if (first < 0) first = x;
            last = x;
        }
    }
    if (first < 0) return -1;
    return (first + last) / 2;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    static const int res[][2] = { {320, 240}, {640, 480}, {1280, 720} };
    const int n = 3;
    int fails = 0;
    FILE *resf = fopen("dx11_ires_result.txt", "w");
    if (!resf) { MessageBoxA(NULL, "cannot open result file", "dx11_ires_test", MB_OK); return 2; }

    // Create all renderers upfront (destroying one mid-loop posts WM_QUIT to
    // the thread queue, which the next window's BeginFrame would pick up).
    Dx11Renderer *ren[n] = { NULL, NULL, NULL };
    for (int ri = 0; ri < n; ++ri) {
        Dx11RendererConfig rcfg = { 1280, 720, res[ri][0], res[ri][1], 0, 0 };
        Dx11RendererResult rr;
        ren[ri] = Dx11Renderer_Create(&rcfg, &rr);
        if (!ren[ri]) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_ires_test", MB_OK | MB_ICONERROR); return 2; }
    }

    for (int ri = 0; ri < n; ++ri) {
        int W = res[ri][0], H = res[ri][1];
        Dx11Renderer *r = ren[ri];
        ID3D11Device *dev = Dx11Renderer_GetDevice(r);
        ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(r);

        Dx11ResResult rsres;
        Dx11Res *rsc = Dx11Res_Create(dev, ctx, NULL, &rsres);
        Dx11TexResult tr; Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
        Dx11ShadersResult sr; Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
        Dx11DrawCmdsResult cr; Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, rsc, tex, sh, NULL, &cr);
        if (!rsc || !tex || !sh || !cmds) { MessageBoxA(NULL, "module create failed", "dx11_ires_test", MB_OK | MB_ICONERROR); return 2; }

        float proj[4][4];
        MatPerspectiveRH(60.0f * 3.14159265f / 180.0f, (float)W / (float)H, 0.1f, 100.0f, proj);
        float ident[4][4]; MatIdentity(ident);

        if (Dx11Renderer_BeginFrame(r) != 0) break;
        Dx11Renderer_BindOffscreen(r, 0);
        float base[4] = { 0, 0, 0, 1 };
        ctx->ClearRenderTargetView(Dx11Renderer_GetOffscreenRTV(r, 0), base);
        ctx->ClearDepthStencilView(Dx11Renderer_GetDSV(r), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        Dx11DrawCmds_BeginFrame(cmds);
        Dx11Res_BeginFrame(rsc);
        Dx11DrawCmds_SetViewProj(cmds, proj);
        SubmitBar(cmds, rsc, ident, -1.0f, 220.0f / 255.0f, 0, 0);   // left red
        SubmitBar(cmds, rsc, ident, +1.0f, 0, 0, 220.0f / 255.0f);   // right blue
        Dx11DrawCmds_Execute(cmds, ctx);

        char bmp[64]; sprintf(bmp, "ires_%dx%d.bmp", W, H);
        Dx11Renderer_CaptureToBMP(r, Dx11Renderer_GetOffscreenTexture(r, 0), bmp, NULL);

        // IRES: captured dims == configured res.
        int cw = 0, ch = 0;
        int r2 = 0, g2 = 0, b2 = 0;
        ProbeBMP(bmp, 0, 0, &r2, &g2, &b2, &cw, &ch);
        int okIres = (cw == W && ch == H);
        fprintf(resf, "IRES %dx%d captured %dx%d %s\n", W, H, cw, ch, okIres ? "PASS" : "FAIL");
        if (!okIres) ++fails;

        // Analytic expected bar-centre columns (projection from the harness).
        float f = 1.0f / tanf(30.0f * 3.14159265f / 180.0f);
        float aspect = (float)W / (float)H;
        float ndcLeft = (f / aspect) * (-1.0f) / 4.0f;   // 1-d = 4
        float ndcRight = (f / aspect) * (+1.0f) / 4.0f;
        int exL = (int)((ndcLeft + 1.0f) * 0.5f * W + 0.5f);
        int exR = (int)((ndcRight + 1.0f) * 0.5f * W + 0.5f);
        int cy = H / 2;

        // PROJ: rendered bar centres match the analytic projection.
        int gotL = ScanRunCentre(bmp, cy, W, IsRed);
        int gotR = ScanRunCentre(bmp, cy, W, IsBlue);
        int okProj = (abs(gotL - exL) <= 2 && abs(gotR - exR) <= 2);
        fprintf(resf, "PROJ %dx%d left=%d(exp%d) right=%d(exp%d) %s\n",
                W, H, gotL, exL, gotR, exR, okProj ? "PASS" : "FAIL");
        if (!okProj) ++fails;

        // SYMM: rendered centres symmetric about iW/2 (no top-left shift).
        int okSym = (gotL >= 0 && gotR >= 0 && abs((W / 2 - gotL) - (gotR - W / 2)) <= 2);
        fprintf(resf, "SYMM %dx%d left=%d right=%d symmetric %s\n", W, H, gotL, gotR, okSym ? "PASS" : "FAIL");
        if (!okSym) ++fails;

        Dx11DrawCmds_Destroy(cmds);
        Dx11Shaders_Destroy(sh);
        Dx11Tex_Destroy(tex);
        Dx11Res_Destroy(rsc);
    }

    for (int ri = 0; ri < n; ++ri) if (ren[ri]) Dx11Renderer_Destroy(ren[ri]);

    fprintf(resf, "TOTAL_FAILS=%d IRES=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);
    return 0;
}
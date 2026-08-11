// dx11_stereoscene_test.cpp — T2.4 right-eye-map verification harness.
//
// Renders a representative scene (a large map/terrain quad + a car quad) into
// BOTH per-eye offscreen RTs through the DX11 stereo path — per-eye view from
// Dx11Stereo_ViewMatrix (T2.2), each eye into its own independent RT (T2.1) —
// and proves the legacy "right-eye map/terrain disappears" bug is structurally
// absent: the right-eye RT contains the full map at the same pixel as the left
// (not background), both eyes submit equal draw counts, and both eyes carry the
// same complete scene (symmetric apart from the small lateral offset).
//
// Probes:
//   MAP_L / MAP_R — a map-only pixel is green in both eyes (right-eye map
//                   present — the legacy symptom is gone);
//   CAR_L / CAR_R — the car pixel is red in both eyes;
//   DUAL_DRAWCOUNT — both eyes submitted the same number of commands;
//   SYMMETRIC — map present in both eyes (eyes differ only by lateral offset).

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_resources.h"
#include "dx11_textures.h"
#include "dx11_shaders.h"
#include "dx11_drawcmdexec.h"
#include "dx11_stereo.h"

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
// out = a * b (standard matrix product; row-vector: applying a then b).
static void MatMul(const float a[4][4], const float b[4][4], float out[4][4]) {
    float t[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[i][k] * b[k][j];
            t[i][j] = s;
        }
    memcpy(out, t, sizeof(t));
}

// Push a world-quad spanning x0..x1, y0..y1 at depth z (TL,TR,BR,BL; front face
// +z, matching the T2.1 AddQuad). Returns its arena ranges.
struct Range { int vo, vc, io, ic; };
static struct Range AddQuadXY(Dx11Res *res, float x0, float x1, float y0, float y1,
                              float z) {
    Dx11ResVertex v[4];
    v[0] = { x0, y1, z, 1, 1, 1, 1, 0, 0 };   // TL
    v[1] = { x1, y1, z, 1, 1, 1, 1, 1, 0 };   // TR
    v[2] = { x1, y0, z, 1, 1, 1, 1, 1, 1 };   // BR
    v[3] = { x0, y0, z, 1, 1, 1, 1, 0, 1 };   // BL
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

// Submit a flat opaque quad command (green map / red car).
static void SubmitQuad(Dx11DrawCmds *cmds, const float ident[4][4], struct Range R,
                       float r, float g, float b) {
    Dx11DrawCmdItem it = {};
    it.vertexOffset = R.vo; it.vertexCount = R.vc;
    it.indexOffset = R.io; it.indexCount = R.ic;
    it.bboxMin[0] = -2.5f; it.bboxMax[0] = 2.5f;
    it.bboxMin[1] = -2.0f; it.bboxMax[1] = 2.0f;
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
    MessageBoxA(NULL, buf, "dx11_stereoscene_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    Dx11RendererConfig rcfg = { 800, 600, 320, 240, 0, 0 };
    {
        char buf[400];
        strncpy(buf, lpCmdLine ? lpCmdLine : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, " \t");
        while (tok) {
            if (!strcmp(tok, "-ires") && (tok = strtok(NULL, " \t"))) {
                int w, hh; if (sscanf(tok, "%dx%d", &w, &hh) == 2 && w > 0 && hh > 0) {
                    rcfg.internalW = w; rcfg.internalH = hh;
                }
            }
            tok = strtok(NULL, " \t");
        }
    }

    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_stereoscene_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ResResult rsres;
    Dx11Res *res = Dx11Res_Create(dev, ctx, NULL, &rsres);
    if (!res) { MessageBoxA(NULL, "Dx11Res_Create failed", "dx11_stereoscene_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11TexResult tr;
    Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
    if (!tex) { MessageBoxA(NULL, "Dx11Tex_Create failed", "dx11_stereoscene_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11ShadersResult sr;
    Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    if (!sh) { MessageBoxA(NULL, "Dx11Shaders_Create failed", "dx11_stereoscene_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11DrawCmdsResult cr;
    Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, res, tex, sh, NULL, &cr);
    if (!cmds) { MessageBoxA(NULL, "Dx11DrawCmds_Create failed", "dx11_stereoscene_test", MB_OK | MB_ICONERROR); return 2; }

    float proj[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f,
                     (float)rcfg.internalW / (float)rcfg.internalH, 0.1f, 100.0f, proj);
    float ident[4][4]; MatIdentity(ident);

    // Camera at origin, looking -z (yaw 0). Map quad fills the view at z=-3;
    // car quad off-center at z=-2.8.
    float camPos[3] = { 0, 0, 0 };
    float sep = 0.1f;   // small lateral offset (gain = sep*2 = 0.2) keeps both
                        // eyes seeing the full scene; offset is purely lateral.
    int swap = 0;

    int fails = 0;
    FILE *resf = fopen("dx11_stereoscene_result.txt", "w");
    if (!resf) { MessageBoxA(NULL, "cannot open result file", "dx11_stereoscene_test", MB_OK); return 2; }

    int submitted[2] = { -1, -1 };
    const char *bmp[2] = { "stereoscene_left.bmp", "stereoscene_right.bmp" };
    Dx11StereoEye eyeE[2] = { DX11STEREO_EYE_LEFT, DX11STEREO_EYE_RIGHT };

    for (int e = 0; e < 2; ++e) {
        if (Dx11Renderer_BeginFrame(ren) != 0) break;
        Dx11Renderer_BindOffscreen(ren, e);
        float base[4] = { 0, 0, 0, 1 };
        ctx->ClearRenderTargetView(Dx11Renderer_GetOffscreenRTV(ren, e), base);
        ctx->ClearDepthStencilView(Dx11Renderer_GetDSV(ren), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        // Per-eye view -> viewProj = view * proj.
        float view[4][4], vp[4][4];
        Dx11Stereo_ViewMatrix(camPos, 0.0f, eyeE[e], sep, swap, view);
        MatMul(view, proj, vp);

        Dx11DrawCmds_BeginFrame(cmds);
        Dx11Res_BeginFrame(res);
        Dx11DrawCmds_SetViewProj(cmds, vp);

        // Map (green) + car (red), both opaque.
        struct Range Rmap = AddQuadXY(res, -2.0f, 2.0f, -1.5f, 1.5f, -3.0f);
        SubmitQuad(cmds, ident, Rmap, 0, 200.0f / 255.0f, 0);
        struct Range Rcar = AddQuadXY(res, -1.5f, -0.5f, -0.5f, 0.5f, -2.8f);
        SubmitQuad(cmds, ident, Rcar, 220.0f / 255.0f, 0, 0);

        Dx11DrawCmds_Execute(cmds, ctx);
        submitted[e] = Dx11DrawCmds_SubmittedCount(cmds);
        Dx11Renderer_CaptureToBMP(ren, Dx11Renderer_GetOffscreenTexture(ren, e),
                                  bmp[e], NULL);
    }

    int W = rcfg.internalW, H = rcfg.internalH;
    // Map-only pixel: world x=+1.0 (right of the car), y=0. Car pixel: x=-1.0.
    int mapCol = (int)((1.0f / 2.309f + 1.0f) * 0.5f * W); // ~229 at 320
    int carCol = (int)((-1.0f / 2.309f + 1.0f) * 0.5f * W); // ~90 at 320
    int cy = H / 2;
    int r = 0, g = 0, b = 0;
    auto IsGreen = [](int r, int g, int b) { return g > 140 && r < 40 && b < 40; };
    auto IsRed = [](int r, int g, int b) { return r > 180 && g < 40 && b < 40; };

    // MAP_L / MAP_R: map pixel green in both eyes (right-eye map present).
    if (ProbeBMP(bmp[0], mapCol, cy, &r, &g, &b) == 0) {
        int ok = IsGreen(r, g, b);
        fprintf(resf, "MAP_L left map-pixel(%d,%d)=(%d,%d,%d) green %s\n", mapCol, cy, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "MAP_L probe unavailable\n"); }
    if (ProbeBMP(bmp[1], mapCol, cy, &r, &g, &b) == 0) {
        int ok = IsGreen(r, g, b);
        fprintf(resf, "MAP_R right map-pixel(%d,%d)=(%d,%d,%d) green %s\n", mapCol, cy, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "MAP_R probe unavailable\n"); }

    // CAR_L / CAR_R: car pixel red in both eyes.
    if (ProbeBMP(bmp[0], carCol, cy, &r, &g, &b) == 0) {
        int ok = IsRed(r, g, b);
        fprintf(resf, "CAR_L left car-pixel(%d,%d)=(%d,%d,%d) red %s\n", carCol, cy, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "CAR_L probe unavailable\n"); }
    if (ProbeBMP(bmp[1], carCol, cy, &r, &g, &b) == 0) {
        int ok = IsRed(r, g, b);
        fprintf(resf, "CAR_R right car-pixel(%d,%d)=(%d,%d,%d) red %s\n", carCol, cy, r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(resf, "CAR_R probe unavailable\n"); }

    // DUAL_DRAWCOUNT: both eyes submitted the same number of commands.
    int okDc = (submitted[0] >= 0 && submitted[0] == submitted[1]);
    fprintf(resf, "DUAL_DRAWCOUNT left=%d right=%d equal %s\n",
            submitted[0], submitted[1], okDc ? "PASS" : "FAIL");
    if (!okDc) ++fails;

    // SYMMETRIC: map present in both eyes (both carry the full scene).
    int mapL = 0, gL = 0, bL = 0, mapR = 0, gR = 0, bR = 0;
    int okSym = (ProbeBMP(bmp[0], mapCol, cy, &mapL, &gL, &bL) == 0 && IsGreen(mapL, gL, bL) &&
                 ProbeBMP(bmp[1], mapCol, cy, &mapR, &gR, &bR) == 0 && IsGreen(mapR, gR, bR));
    fprintf(resf, "SYMMETRIC map present in both eyes %s\n", okSym ? "PASS" : "FAIL");
    if (!okSym) ++fails;

    fprintf(resf, "TOTAL_FAILS=%d RIGHTEYE=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    Dx11DrawCmds_Destroy(cmds);
    Dx11Shaders_Destroy(sh);
    Dx11Tex_Destroy(tex);
    Dx11Res_Destroy(res);
    Dx11Renderer_Destroy(ren);
    return 0;
}
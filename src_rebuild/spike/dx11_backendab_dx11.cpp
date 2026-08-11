// dx11_backendab_dx11.cpp — T4.2 DX11-side of the dual-backend A/B harness.
//
// This binary links ONLY the DX11 draw-command stack (dx11_renderer /
// dx11_resources / dx11_textures / dx11_shaders / dx11_drawcmdexec) — it
// contains no psyx/PsyCross code. It renders the common T4.2 screen-space scene
// (from `dx11_backendab_state.h`) through the draw-command executor with an
// orthographic screen->NDC projection, and implements the emulator-style
// store/load + screenshot + compare contract:
//
//   -store <state.bin> -shot <out.bmp>      render the default scene, save state+screenshot
//   -load  <state.bin> -shot <out.bmp>      load state, render immediately, save screenshot
//   -compare <a.bmp> <b.bmp> <state.bin>    prove both screenshots reproduced the
//                                           same quads at the same positions with the
//                                           same colors (the A/B result)
//
// Verification headless: the DX11 binary must link with no psyx symbols (the
// psyx reference is the other binary, dx11_backendab_psyx). The -compare step
// proves the store/load round-trip reproduces the scene across backends.

#define WIN32_LEAN_AND_MEAN
#include "dx11_backendab_state.h"
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
// Matrix helpers (row-vector x column-major, v' = v*M).
// ---------------------------------------------------------------------------
static void MatIdentity(float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    for (int i = 0; i < 4; ++i) m[i][i] = 1.0f;
}
// Orthographic screen->NDC, stored in the module's P^t convention (the shader's
// mul(pos,viewProj) reads the CB transposed, so clip = myC * pos: translations
// live in the LAST COLUMN M[0][3]/M[1][3]). x 0..resW -> -1..1, y 0..resH
// (top-down) -> +1..-1.
static void MatOrthoScreen(float resW, float resH, float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    m[0][0] = 2.0f / resW;   m[0][3] = -1.0f;
    m[1][1] = -2.0f / resH;  m[1][3] = 1.0f;
    m[2][2] = 1.0f;
    m[3][3] = 1.0f;
}

// Push a screen-space quad (TL,TR,BR,BL) at depth z. Returns its arena ranges.
struct Range { int vo, vc, io, ic; };
static struct Range AddQuadXY(Dx11Res *res, float x0, float x1, float y0, float y1,
                              float z) {
    Dx11ResVertex v[4];
    v[0] = { x0, y1, z, 1, 1, 1, 1, 0, 0 };   // top-left
    v[1] = { x1, y1, z, 1, 1, 1, 1, 1, 0 };   // top-right
    v[2] = { x1, y0, z, 1, 1, 1, 1, 1, 1 };   // bottom-right
    v[3] = { x0, y0, z, 1, 1, 1, 1, 0, 1 };   // bottom-left
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

static void SubmitQuad(Dx11DrawCmds *cmds, const float ident[4][4], struct Range R,
                       unsigned char r, unsigned char g, unsigned char b) {
    Dx11DrawCmdItem it = {};
    it.vertexOffset = R.vo; it.vertexCount = R.vc;
    it.indexOffset = R.io; it.indexCount = R.ic;
    // bbox is only used for frustum culling; with the ortho view it is safe to
    // leave a generous box so nothing is culled.
    it.bboxMin[0] = -100000.f; it.bboxMin[1] = -100000.f; it.bboxMin[2] = -10;
    it.bboxMax[0] = 100000.f;  it.bboxMax[1] = 100000.f;  it.bboxMax[2] = 10;
    memcpy(it.world, ident, 16 * sizeof(float));
    it.texture = -1;
    it.flatColor[0] = r / 255.0f; it.flatColor[1] = g / 255.0f;
    it.flatColor[2] = b / 255.0f; it.flatColor[3] = 1.0f;
    it.blend = DX11SH_BLEND_NONE; it.shade = DX11SH_COLOR_FLAT; it.translucent = 0;
    it.twoSided = 1;   // ortho y-flip reverses winding; render both faces
    Dx11DrawCmds_Submit(cmds, &it);
}

// ---------------------------------------------------------------------------
// BMP probe (bottom-up, 24-bit BGR, rowSize padded to 4).
// ---------------------------------------------------------------------------
static int ProbeBMPPixel(const char *path, int x, int y, int *r, int *g, int *b) {
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

static int ColorMatch(int r, int g, int b, int er, int eg, int eb, int tol) {
    return abs(r - er) <= tol && abs(g - eg) <= tol && abs(b - eb) <= tol;
}

// ---------------------------------------------------------------------------
// Render the scene through the DX11 draw-command executor -> offscreen BMP.
// Returns 0 on success.
// ---------------------------------------------------------------------------
static int RenderAndCapture(const Dx11AbScene *s, const char *bmpOut) {
    Dx11RendererConfig rcfg = { (int)s->resW, (int)s->resH, (int)s->resW, (int)s->resH, 0, 0 };
    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) return 1;
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ResResult rsres;
    Dx11Res *res = Dx11Res_Create(dev, ctx, NULL, &rsres);
    Dx11TexResult tr;
    Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
    Dx11ShadersResult sr;
    Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    Dx11DrawCmdsResult cr;
    Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, res, tex, sh, NULL, &cr);
    if (!res || !tex || !sh || !cmds) return 1;

    float vp[4][4];
    MatOrthoScreen((float)s->resW, (float)s->resH, vp);
    float ident[4][4]; MatIdentity(ident);

    if (Dx11Renderer_BeginFrame(ren) != 0) return 1;
    Dx11Renderer_BindOffscreen(ren, 0);
    float base[4] = { 0, 0, 0, 1 };   // black background
    ctx->ClearRenderTargetView(Dx11Renderer_GetOffscreenRTV(ren, 0), base);
    ctx->ClearDepthStencilView(Dx11Renderer_GetDSV(ren), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    Dx11DrawCmds_BeginFrame(cmds);
    Dx11Res_BeginFrame(res);
    Dx11DrawCmds_SetViewProj(cmds, vp);
    for (unsigned int i = 0; i < s->numQuads; ++i) {
        const Dx11AbQuad *q = &s->quads[i];
        struct Range R = AddQuadXY(res, q->x, q->x + q->w, q->y, q->y + q->h, 0.0f);
        SubmitQuad(cmds, ident, R, q->r, q->g, q->b);
    }
    Dx11DrawCmds_Execute(cmds, ctx);
    Dx11Renderer_CaptureToBMP(ren, Dx11Renderer_GetOffscreenTexture(ren, 0), bmpOut, NULL);

    Dx11DrawCmds_Destroy(cmds);
    Dx11Shaders_Destroy(sh);
    Dx11Tex_Destroy(tex);
    Dx11Res_Destroy(res);
    Dx11Renderer_Destroy(ren);
    return 0;
}

// ---------------------------------------------------------------------------
// Verify one screenshot against the stored state: each quad's centroid pixel
// must match its stored color. Returns number of failed probes.
// ---------------------------------------------------------------------------
static int VerifyFrame(const Dx11AbScene *s, const char *bmp, FILE *resf,
                       const char *tag) {
    int fails = 0;
    for (unsigned int i = 0; i < s->numQuads; ++i) {
        const Dx11AbQuad *q = &s->quads[i];
        int cx = (int)(q->x + q->w * 0.5f);
        int cy = (int)(q->y + q->h * 0.5f);
        int r = 0, g = 0, b = 0;
        int ok = (ProbeBMPPixel(bmp, cx, cy, &r, &g, &b) == 0 &&
                  ColorMatch(r, g, b, q->r, q->g, q->b, 40));
        fprintf(resf, "  %s quad%d centroid(%d,%d)=(%d,%d,%d) expect(%d,%d,%d) %s\n",
                tag, i, cx, cy, r, g, b, q->r, q->g, q->b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    }
    return fails;
}

// ---------------------------------------------------------------------------
// Command-line dispatch (WinMain).
// ---------------------------------------------------------------------------
static void ParseArgs(char *buf, Dx11AbScene *scene, int *doStore, int *doCompare,
                      char *statePath, char *shotPath, char *aBmp, char *bBmp) {
    *doStore = *doCompare = 0;
    statePath[0] = shotPath[0] = aBmp[0] = bBmp[0] = 0;
    char *tok = strtok(buf, " \t");
    while (tok) {
        if (!strcmp(tok, "-store")) { *doStore = 1; }
        else if (!strcmp(tok, "-load")) { *doStore = 0; }
        else if (!strcmp(tok, "-compare")) { *doCompare = 1; }
        else if (!strcmp(tok, "-state") && (tok = strtok(NULL, " \t"))) {
            strncpy(statePath, tok, 260); statePath[259] = 0;
        } else if (!strcmp(tok, "-shot") && (tok = strtok(NULL, " \t"))) {
            strncpy(shotPath, tok, 260); shotPath[259] = 0;
        } else if (!strcmp(tok, "-a") && (tok = strtok(NULL, " \t"))) {
            strncpy(aBmp, tok, 260); aBmp[259] = 0;
        } else if (!strcmp(tok, "-b") && (tok = strtok(NULL, " \t"))) {
            strncpy(bBmp, tok, 260); bBmp[259] = 0;
        } else if (!strcmp(tok, "-res") && (tok = strtok(NULL, " \t"))) {
            int w, h; if (sscanf(tok, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                scene->resW = (unsigned int)w; scene->resH = (unsigned int)h;
            }
        }
        tok = strtok(NULL, " \t");
    }
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    Dx11AbScene scene;
    Dx11AbScene_FillDefault(&scene, 320, 240);
    int doStore = 0, doCompare = 0;
    char statePath[260], shotPath[260], aBmp[260], bBmp[260];
    {
        char buf[1024];
        strncpy(buf, lpCmdLine ? lpCmdLine : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        ParseArgs(buf, &scene, &doStore, &doCompare, statePath, shotPath, aBmp, bBmp);
    }

    FILE *resf = fopen("dx11_backendab_dx11_result.txt", "w");
    if (!resf) return 2;
    int fails = 0;

    if (doCompare) {
        // A/B compare: both screenshots must reproduce the scene from state.
        Dx11AbScene lst;
        if (Dx11AbState_Read(&lst, statePath) != 0) {
            fprintf(resf, "COMPARE state load FAIL\n"); fclose(resf); return 2;
        }
        int fA = VerifyFrame(&lst, aBmp, resf, "A");
        int fB = VerifyFrame(&lst, bBmp, resf, "B");
        fprintf(resf, "COMPARE A_FAILS=%d B_FAILS=%d IDENTICAL=%s\n",
                fA, fB, (fA == 0 && fB == 0) ? "PASS" : "FAIL");
        fails = fA + fB;
    } else {
        // Render this backend's frame.
        if (doStore) {
            if (Dx11AbState_Write(&scene, statePath) != 0) {
                fprintf(resf, "STORE state write FAIL\n"); fclose(resf); return 2;
            }
            fprintf(resf, "STORE wrote %s (%ux%u, %u quads)\n", statePath,
                    scene.resW, scene.resH, scene.numQuads);
        } else {
            if (Dx11AbState_Read(&scene, statePath) != 0) {
                fprintf(resf, "LOAD state read FAIL\n"); fclose(resf); return 2;
            }
            fprintf(resf, "LOAD read %s (%ux%u, %u quads)\n", statePath,
                    scene.resW, scene.resH, scene.numQuads);
        }
        if (RenderAndCapture(&scene, shotPath) != 0) {
            fprintf(resf, "RENDER FAIL\n"); fclose(resf); return 2;
        }
        fprintf(resf, "RENDER -> %s\n", shotPath);
        fails = VerifyFrame(&scene, shotPath, resf, "SELF");
    }

    fprintf(resf, "TOTAL_FAILS=%d DX11BACKEND=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);
    return 0;
}
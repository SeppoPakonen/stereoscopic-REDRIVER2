// dx11_nonstereo_test.cpp — T4.3 non-stereo regression harness.
//
// Verifies the DX11 renderer's MONO path — the path the game uses when stereo
// is disabled (render the base view-projection DIRECTLY into the swapchain
// backbuffer, no per-eye RT, no stereo composite) — renders a representative
// base-game scene correctly, is a FULL-FRAME direct render (not a composite
// SBS/TB half-frame), and is PIXEL-IDENTICAL whether or not a stereo
// configuration is present. This is the regression guarantee that the stereo
// work (T2.1–T3.3) does not leak into / corrupt normal (mono) play.
//
// Scene: the T4.2 common world-state default (dx11_backendab_state.h) — a red
// left bar, a green centre square, a blue right bar — the same scene the T4.2
// psyx reference backend renders identically (COMPARE IDENTICAL=PASS). With
// -psyxref <psyx.bmp> the harness also probes the psyx reference BMP against
// the mono frame to prove base-game parity.
//
// Probes:
//   MONO_QUAD0..2 — each quad's centroid pixel == its stored color (the base
//                   scene renders correctly through the mono path);
//   FULL_FRAME    — the left/right bars span the full height and the centre
//                   square is present (a full-frame direct render, not a
//                   composite SBS/TB half-frame) — the mono path bypasses the
//                   stereo composite;
//   STATE_INDEP   — the mono frame is pixel-identical whether a stereo config
//                   is absent or present (mode/sep/conv/swap) — stereo state
//                   never corrupts normal play;
//   PSYX_PARITY   — the quad centroids match the T4.2 psyx reference BMP.

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

// A stereo configuration, mirroring the game's gStereoMode/gStereoSeparation/
// gStereoConvergence/gStereoSwapEyes. The mono path deliberately ignores it —
// proving the mono render is independent of stereo state (STATE_INDEP).
typedef struct {
    int   mode;   // 0 = disabled, 3 = SBS, ...
    float sep;    // stereo separation
    float conv;   // convergence distance
    int   swap;   // swap eyes
} StereoConfig;

// ---------------------------------------------------------------------------
// Matrix helpers (row-vector x column-major, v' = v*M).
// ---------------------------------------------------------------------------
static void MatIdentity(float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    for (int i = 0; i < 4; ++i) m[i][i] = 1.0f;
}
// Orthographic screen->NDC, in the module's P^t convention (the shader's
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

// Pixel-by-pixel comparison of two BMPs. Returns the number of differing pixels,
// or -1 if either file can't be read or the dims differ.
static int CompareBMPs(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return -1; }
    unsigned char ha[54], hb[54];
    if (fread(ha, 1, 54, fa) != 54 || fread(hb, 1, 54, fb) != 54) {
        fclose(fa); fclose(fb); return -1;
    }
    int w = ha[18] | (ha[19] << 8) | (ha[20] << 16) | (ha[21] << 24);
    int h = ha[22] | (ha[23] << 8) | (ha[24] << 16) | (ha[25] << 24);
    int wb = hb[18] | (hb[19] << 8) | (hb[20] << 16) | (hb[21] << 24);
    int hb2 = hb[22] | (hb[23] << 8) | (hb[24] << 16) | (hb[25] << 24);
    if (w != wb || h != hb2) { fclose(fa); fclose(fb); return -1; }
    int rowSize = (w * 3 + 3) & ~3;
    int diff = 0;
    if (fseek(fa, 54, SEEK_SET) != 0 || fseek(fb, 54, SEEK_SET) != 0) {
        fclose(fa); fclose(fb); return -1;
    }
    for (int row = 0; row < h; ++row) {
        for (int i = 0; i < rowSize; ++i) {
            int ca = fgetc(fa), cb = fgetc(fb);
            if (ca == EOF || cb == EOF) { fclose(fa); fclose(fb); return -1; }
            if (ca != cb) ++diff;
        }
    }
    fclose(fa); fclose(fb);
    return diff;
}

// ---------------------------------------------------------------------------
// Mono renderer: one renderer instance reused across frames (the mono path uses
// a single renderer, frame after frame). Setup once, then RenderMonoFrame per
// frame. Each frame renders the base view-projection DIRECTLY into the swapchain
// backbuffer (no per-eye RT, no stereo composite) and captures it to a BMP.
// ---------------------------------------------------------------------------
typedef struct {
    Dx11Renderer *ren;
    ID3D11DeviceContext *ctx;
    Dx11Res *res;
    Dx11Tex *tex;
    Dx11Shaders *sh;
    Dx11DrawCmds *cmds;
    float vp[4][4];
    float ident[4][4];
} MonoRenderer;

static void MonoRenderer_Destroy(MonoRenderer *m);

static MonoRenderer *MonoRenderer_Create(const Dx11AbScene *s) {
    Dx11RendererConfig rcfg = { (int)s->resW, (int)s->resH, (int)s->resW, (int)s->resH, 0, 0 };
    Dx11RendererResult rr;
    MonoRenderer *m = (MonoRenderer *)calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!m->ren) { free(m); return NULL; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(m->ren);
    m->ctx = Dx11Renderer_GetContext(m->ren);
    Dx11ResResult rsres;   m->res = Dx11Res_Create(dev, m->ctx, NULL, &rsres);
    Dx11TexResult tr;      m->tex = Dx11Tex_Create(dev, m->ctx, NULL, &tr);
    Dx11ShadersResult sr;  m->sh = Dx11Shaders_Create(dev, m->ctx, &sr);
    Dx11DrawCmdsResult cr; m->cmds = Dx11DrawCmds_Create(dev, m->ctx, m->res, m->tex, m->sh, NULL, &cr);
    if (!m->res || !m->tex || !m->sh || !m->cmds) { MonoRenderer_Destroy(m); return NULL; }
    MatOrthoScreen((float)s->resW, (float)s->resH, m->vp);
    MatIdentity(m->ident);
    return m;
}

static void MonoRenderer_Destroy(MonoRenderer *m) {
    if (!m) return;
    if (m->cmds) Dx11DrawCmds_Destroy(m->cmds);
    if (m->sh)   Dx11Shaders_Destroy(m->sh);
    if (m->tex)  Dx11Tex_Destroy(m->tex);
    if (m->res)  Dx11Res_Destroy(m->res);
    if (m->ren)  Dx11Renderer_Destroy(m->ren);
    free(m);
}

// Render one mono frame into the backbuffer and capture it to `bmpOut`.
// `stereo` is deliberately ignored: the mono path is independent of stereo
// state (STATE_INDEP probes this). Returns 0 on success.
static int MonoRenderer_RenderFrame(MonoRenderer *m, const Dx11AbScene *s,
                                    const StereoConfig *stereo, const char *bmpOut) {
    (void)stereo;   // the mono path ignores stereo state by design
    if (Dx11Renderer_BeginFrame(m->ren) != 0) return 1;
    // MONO: bind the swapchain backbuffer directly — no offscreen RT, no
    // composite pass onto the backbuffer.
    Dx11Renderer_BindBackbuffer(m->ren);
    float base[4] = { 0, 0, 0, 1 };   // black background
    m->ctx->ClearRenderTargetView(Dx11Renderer_GetBackbufferRTV(m->ren), base);
    m->ctx->ClearDepthStencilView(Dx11Renderer_GetDSV(m->ren), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    Dx11DrawCmds_BeginFrame(m->cmds);
    Dx11Res_BeginFrame(m->res);
    Dx11DrawCmds_SetViewProj(m->cmds, m->vp);
    for (unsigned int i = 0; i < s->numQuads; ++i) {
        const Dx11AbQuad *q = &s->quads[i];
        struct Range R = AddQuadXY(m->res, q->x, q->x + q->w, q->y, q->y + q->h, 0.0f);
        SubmitQuad(m->cmds, m->ident, R, q->r, q->g, q->b);
    }
    Dx11DrawCmds_Execute(m->cmds, m->ctx);
    // DISCARD swapchain: capture the backbuffer BEFORE Present (NULL src).
    Dx11Renderer_CaptureToBMP(m->ren, NULL, bmpOut, NULL);
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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    Dx11AbScene scene;
    Dx11AbScene_FillDefault(&scene, 320, 240);
    char psyxRef[260] = { 0 };
    {
        char buf[1024];
        strncpy(buf, lpCmdLine ? lpCmdLine : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, " \t");
        while (tok) {
            if (!strcmp(tok, "-res") && (tok = strtok(NULL, " \t"))) {
                int w, h; if (sscanf(tok, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                    scene.resW = (unsigned int)w; scene.resH = (unsigned int)h;
                }
            } else if (!strcmp(tok, "-psyxref") && (tok = strtok(NULL, " \t"))) {
                strncpy(psyxRef, tok, 259); psyxRef[259] = 0;
            }
            tok = strtok(NULL, " \t");
        }
    }

    FILE *resf = fopen("dx11_nonstereo_result.txt", "w");
    if (!resf) return 2;
    int fails = 0;

    const char *bmpDisabled = "dx11_nonstereo_disabled.bmp";
    const char *bmpStereo   = "dx11_nonstereo_stereoconfig.bmp";

    // Stereo config that MUST NOT affect the mono render.
    StereoConfig sc;
    sc.mode = 3; sc.sep = 0.1f; sc.conv = 10.0f; sc.swap = 1;

    MonoRenderer *m = MonoRenderer_Create(&scene);
    if (!m) {
        fprintf(resf, "RENDER setup FAIL\n"); fclose(resf); return 2;
    }

    // Render the same scene twice through ONE mono renderer: once with stereo
    // absent, once with a full stereo config present. The mono path ignores the
    // config, so the two frames must be pixel-identical.
    int r1 = MonoRenderer_RenderFrame(m, &scene, NULL, bmpDisabled);
    int r2 = MonoRenderer_RenderFrame(m, &scene, &sc, bmpStereo);
    if (r1 != 0 || r2 != 0) {
        fprintf(resf, "RENDER frame disabled=%d stereoconfig=%d FAIL\n", r1, r2);
        MonoRenderer_Destroy(m); fclose(resf); return 2;
    }

    // MONO_QUAD0..2: the base scene renders correctly (mono self-check).
    fails += VerifyFrame(&scene, bmpDisabled, resf, "MONO");

    // FULL_FRAME: the left/right bars span the full height and the centre
    // square is present — a full-frame direct render, not a composite SBS/TB
    // half-frame (a stereo composite would split the frame in half).
    int W = (int)scene.resW, H = (int)scene.resH;
    int cy = H / 2;
    int r = 0, g = 0, b = 0;
    int okL = (ProbeBMPPixel(bmpDisabled, (int)(W * 0.10f), cy, &r, &g, &b) == 0 &&
               ColorMatch(r, g, b, scene.quads[0].r, scene.quads[0].g, scene.quads[0].b, 40));
    int okR = (ProbeBMPPixel(bmpDisabled, (int)(W * 0.90f), cy, &r, &g, &b) == 0 &&
               ColorMatch(r, g, b, scene.quads[2].r, scene.quads[2].g, scene.quads[2].b, 40));
    int okC = (ProbeBMPPixel(bmpDisabled, (int)(W * 0.50f), cy, &r, &g, &b) == 0 &&
               ColorMatch(r, g, b, scene.quads[1].r, scene.quads[1].g, scene.quads[1].b, 40));
    int okFull = okL && okR && okC;
    fprintf(resf, "FULL_FRAME leftbar(%.0f)=%s rightbar(%.0f)=%s centersq(%.0f)=%s %s\n",
            W * 0.10f, okL ? "ok" : "FAIL", W * 0.90f, okR ? "ok" : "FAIL",
            W * 0.50f, okC ? "ok" : "FAIL", okFull ? "PASS" : "FAIL");
    if (!okFull) ++fails;

    // STATE_INDEP: the mono frame is pixel-identical with vs without a stereo
    // config — stereo state never corrupts normal play.
    int diffPx = CompareBMPs(bmpDisabled, bmpStereo);
    int okState = (diffPx == 0);
    fprintf(resf, "STATE_INDEP differing_pixels=%d identical %s\n",
            diffPx, okState ? "PASS" : "FAIL");
    if (!okState) ++fails;

    // PSYX_PARITY: the mono frame's quad centroids match the T4.2 psyx
    // reference backend (same scene, same colors, proven identical in T4.2).
    if (psyxRef[0]) {
        int fP = VerifyFrame(&scene, psyxRef, resf, "PSYX");
        int okPar = (fP == 0);
        fprintf(resf, "PSYX %u/%u centroids match psyx.bmp %s\n",
                scene.numQuads - (unsigned)fP, scene.numQuads, okPar ? "PASS" : "FAIL");
        if (!okPar) ++fails;
    } else {
        fprintf(resf, "PSYX skipped (no -psyxref; run the T4.2 psyx backend -store first)\n");
    }

    fprintf(resf, "TOTAL_FAILS=%d NONSTEREO=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    MonoRenderer_Destroy(m);
    return 0;
}
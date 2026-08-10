// dx11_drawcmdexec_test.cpp — T1.5 draw-command executor harness.
//
// Feeds a representative command list to the executor and verifies the
// renderer-side logic headless:
//   * culling  — an off-screen command is frustum-culled (CulledCount, and the
//                region where it would be is background);
//   * batching — two same-material/same-world/index-contiguous opaque quads
//                draw as ONE DrawIndexed (DrawCallCount < submitted);
//   * sorting  — two overlapping translucent quads draw back-to-front by
//                sortKey, so the nearer one is on top (probed);
//   * emitting — the in-view opaque quad + the batched quads render.
//
// World layout (camera at origin, looking -z, 60 FOV perspective):
//   main    : opaque blue  quad, x -0.8..0.8, y -0.8..0.8,  z -3
//   batch A : opaque green quad, x -1.6..-1.2, y -0.8..0.8, z -3 (world identity)
//   batch B : opaque green quad, x -1.2..-0.8, y -0.8..0.8, z -3 (world identity)
//   off     : opaque magenta quad, x 5..7        (far outside frustum -> culled)
//   T1      : translucent red quad,  x -0.4..0.4, y -0.4..0.4, sortKey 10 (far)
//   T2      : translucent teal quad, x -0.4..0.4, y -0.4..0.4, sortKey 5  (near)
// All world = identity (vertices already in world space at z=-3).

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

// ---------------------------------------------------------------------------
// Arena geometry: push a quad (local coords, world identity) and return ranges.
// ---------------------------------------------------------------------------
struct Range { int vo, vc, io, ic; };

static struct Range AddQuad(Dx11Res *res, float xl, float yt, float xr, float yb,
                            float z, float r, float g, float b) {
    Dx11ResVertex v[4];
    v[0] = { xl, yt, z, 1, 1, 1, 1, 0, 0 };
    v[1] = { xr, yt, z, 1, 1, 1, 1, 1, 0 };
    v[2] = { xr, yb, z, 1, 1, 1, 1, 1, 1 };
    v[3] = { xl, yb, z, 1, 1, 1, 1, 0, 1 };
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
    (void)r; (void)g; (void)b;
    return rg;
}

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_drawcmdexec_test: fatal error", MB_OK | MB_ICONERROR);
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
            tok = strtok(NULL, " \t");
        }
    }

    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_drawcmdexec_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ResResult rsres;
    Dx11Res *res = Dx11Res_Create(dev, ctx, NULL, &rsres);
    if (!res) { MessageBoxA(NULL, "Dx11Res_Create failed", "dx11_drawcmdexec_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11TexResult tr;
    Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
    if (!tex) { MessageBoxA(NULL, "Dx11Tex_Create failed", "dx11_drawcmdexec_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11ShadersResult sr;
    Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    if (!sh) { MessageBoxA(NULL, "Dx11Shaders_Create failed", "dx11_drawcmdexec_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11DrawCmdsResult cr;
    Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, res, tex, sh, NULL, &cr);
    if (!cmds) { MessageBoxA(NULL, "Dx11DrawCmds_Create failed", "dx11_drawcmdexec_test", MB_OK | MB_ICONERROR); return 2; }

    // Point sampler (needed by the shaders; executor binds via resources).
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState *samp = NULL;
    if (FAILED(dev->CreateSamplerState(&sd, &samp))) Fail("CreateSamplerState", 0);

    float vp[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f,
                     (float)rcfg.windowW / (float)rcfg.windowH, 0.1f, 100.0f, vp);
    float ident[4][4]; MatIdentity(ident);

    // ----------------------------------------------------------------------
    // Build the command list (geometry into the arena, then submit commands).
    // ----------------------------------------------------------------------
    Dx11DrawCmds_BeginFrame(cmds);
    Dx11Res_BeginFrame(res);
    Dx11DrawCmds_SetViewProj(cmds, (const float (*)[4])vp);

    // main: opaque blue quad (center).
    struct Range Rmain = AddQuad(res, -0.8f, 0.8f, 0.8f, -0.8f, -3, 0, 0, 200.0f / 255.0f);
    // batch A / B: opaque green quads, same world identity, index-contiguous.
    struct Range RA = AddQuad(res, -1.6f, 0.8f, -1.2f, -0.8f, -3, 0, 200.0f / 255.0f, 0);
    struct Range RB = AddQuad(res, -1.2f, 0.8f, -0.8f, -0.8f, -3, 0, 200.0f / 255.0f, 0);
    // off: opaque magenta quad, far outside the frustum (culled).
    struct Range Roff = AddQuad(res, 5.0f, 0.5f, 7.0f, -0.5f, -3, 200.0f / 255.0f, 0, 200.0f / 255.0f);
    // T1 (far, sortKey 10) / T2 (near, sortKey 5): translucent, overlap at center.
    // z=-2.5 (closer than main's -3) so the depth test passes unambiguously.
    struct Range RT1 = AddQuad(res, -0.4f, 0.4f, 0.4f, -0.4f, -2.5f, 200.0f / 255.0f, 0, 0);
    struct Range RT2 = AddQuad(res, -0.4f, 0.4f, 0.4f, -0.4f, -2.5f, 0, 200.0f / 255.0f, 200.0f / 255.0f);

    Dx11DrawCmdItem it = {};
    it.vertexOffset = Rmain.vo; it.vertexCount = Rmain.vc;
    it.indexOffset = Rmain.io; it.indexCount = Rmain.ic;
    it.bboxMin[0] = -0.8f; it.bboxMax[0] = 0.8f; it.bboxMin[1] = -0.8f; it.bboxMax[1] = 0.8f;
    it.bboxMin[2] = -3; it.bboxMax[2] = -3;
    memcpy(it.world, ident, sizeof(ident));
    it.texture = -1; it.flatColor[0] = 0; it.flatColor[1] = 0; it.flatColor[2] = 200.0f / 255.0f; it.flatColor[3] = 1;
    it.blend = DX11SH_BLEND_NONE; it.shade = DX11SH_COLOR_FLAT; it.translucent = 0; it.twoSided = 0;
    Dx11DrawCmds_Submit(cmds, &it);

    it.vertexOffset = RA.vo; it.vertexCount = RA.vc; it.indexOffset = RA.io; it.indexCount = RA.ic;
    it.bboxMin[0] = -1.6f; it.bboxMax[0] = -1.2f; it.bboxMin[1] = -0.8f; it.bboxMax[1] = 0.8f;
    it.flatColor[0] = 0; it.flatColor[1] = 200.0f / 255.0f; it.flatColor[2] = 0;
    Dx11DrawCmds_Submit(cmds, &it);

    it.vertexOffset = RB.vo; it.vertexCount = RB.vc; it.indexOffset = RB.io; it.indexCount = RB.ic;
    it.bboxMin[0] = -1.2f; it.bboxMax[0] = -0.8f;
    Dx11DrawCmds_Submit(cmds, &it);

    it.vertexOffset = Roff.vo; it.indexOffset = Roff.io; it.indexCount = Roff.ic;
    it.bboxMin[0] = 5.0f; it.bboxMax[0] = 7.0f; it.bboxMin[1] = -0.5f; it.bboxMax[1] = 0.5f;
    it.flatColor[2] = 200.0f / 255.0f;
    Dx11DrawCmds_Submit(cmds, &it);

    it.vertexOffset = RT1.vo; it.indexOffset = RT1.io; it.indexCount = RT1.ic;
    it.bboxMin[0] = -0.4f; it.bboxMax[0] = 0.4f; it.bboxMin[1] = -0.4f; it.bboxMax[1] = 0.4f;
    it.bboxMin[2] = -2.5f; it.bboxMax[2] = -2.5f;
    it.flatColor[0] = 200.0f / 255.0f; it.flatColor[1] = 0; it.flatColor[2] = 0; it.flatColor[3] = 0.5f;
    it.blend = DX11SH_BLEND_AVERAGE; it.translucent = 1; it.sortKey = 10; it.twoSided = 0;
    Dx11DrawCmds_Submit(cmds, &it);

    it.vertexOffset = RT2.vo; it.indexOffset = RT2.io; it.indexCount = RT2.ic;
    it.flatColor[0] = 0; it.flatColor[1] = 200.0f / 255.0f; it.flatColor[2] = 200.0f / 255.0f; it.flatColor[3] = 0.5f;
    it.sortKey = 5;
    Dx11DrawCmds_Submit(cmds, &it);

    // ----------------------------------------------------------------------
    // Execute + capture.
    // ----------------------------------------------------------------------
    if (Dx11Renderer_BeginFrame(ren) == 0) {
        Dx11Renderer_BindBackbuffer(ren);
        ctx->PSSetSamplers(0, 1, &samp);
        int drawCalls = Dx11DrawCmds_Execute(cmds, ctx);
        Dx11Renderer_CaptureToBMP(ren, NULL, "dx11_drawcmdexec_frame.bmp",
                                  "dx11_drawcmdexec_frame.txt");
        Dx11Renderer_Present(ren);

        FILE *resf = fopen("dx11_drawcmdexec_result.txt", "w");
        if (!resf) { MessageBoxA(NULL, "cannot open result file", "dx11_drawcmdexec_test", MB_OK); return 2; }
        int r = 0, g = 0, b = 0, fails = 0;
        const char *bmp = "dx11_drawcmdexec_frame.bmp";

        int culled = Dx11DrawCmds_CulledCount(cmds);
        int submitted = Dx11DrawCmds_SubmittedCount(cmds);
        fprintf(resf, "submitted=%d culled=%d drawCalls=%d\n", submitted, culled, drawCalls);
        int okCull = (culled == 1);
        fprintf(resf, "CULL off-screen command removed (culled=%d expect 1) %s\n", culled, okCull ? "PASS" : "FAIL");
        if (!okCull) ++fails;
        int okBatch = (drawCalls == 4); // main + (A,B batched) + T1 + T2
        fprintf(resf, "BATCH same-material pair -> 1 call (drawCalls=%d expect 4) %s\n", drawCalls, okBatch ? "PASS" : "FAIL");
        if (!okBatch) ++fails;

        // Center: T2 (teal, near) on top of T1 (red, far) over main blue.
        // Opaque blue -> T1 red (0.5) -> T2 teal (0.5) = (50,100,150).
        if (ProbeBMP(bmp, 400, 300, &r, &g, &b) == 0) {
            int ok = (r < g && b > 120);
            fprintf(resf, "SORT center(400,300)=(%d,%d,%d) expect(50,100,150) %s\n", r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "SORT probe unavailable\n"); }

        // Main quad (above the translucent quads) = blue.
        if (ProbeBMP(bmp, 350, 210, &r, &g, &b) == 0) {
            int ok = (b > 180 && r < 40);
            fprintf(resf, "EMIT main(350,210)=(%d,%d,%d) blue %s\n", r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "EMIT main probe unavailable\n"); }

        // Batch quads (left side) = green.
        if (ProbeBMP(bmp, 220, 300, &r, &g, &b) == 0) {
            int ok = (g > 180 && r < 40 && b < 40);
            fprintf(resf, "EMIT batchA(220,300)=(%d,%d,%d) green %s\n", r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "EMIT batchA probe unavailable\n"); }
        if (ProbeBMP(bmp, 270, 300, &r, &g, &b) == 0) {
            int ok = (g > 180 && r < 40 && b < 40);
            fprintf(resf, "EMIT batchB(270,300)=(%d,%d,%d) green %s\n", r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "EMIT batchB probe unavailable\n"); }

        fprintf(resf, "TOTAL_FAILS=%d EXEC=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
        fclose(resf);
    }

    samp->Release();
    Dx11DrawCmds_Destroy(cmds);
    Dx11Shaders_Destroy(sh);
    Dx11Tex_Destroy(tex);
    Dx11Res_Destroy(res);
    Dx11Renderer_Destroy(ren);
    return 0;
}
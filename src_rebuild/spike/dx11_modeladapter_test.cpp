// dx11_modeladapter_test.cpp — T1.6 MODEL -> arena adapter harness.
//
// Feeds raw mesh data (mirroring the game's resolved MODEL polys) through the
// adapter into the executor and verifies the whole conversion headless:
//   * TEXTURE  — a textured flat quad shows the baked 4-bit palette color
//                (adapter normalized the tpage-local texel UVs by region size);
//   * GOURAUD  — a gouraud quad shows the per-vertex gradient (left green,
//                right blue) via the white-substitute texture;
//   * BATCH    — two same-texture/same-state contiguous quads batch into one
//                DrawIndexed (drawCalls < submitted);
//   * SORT     — two overlapping translucent quads draw back-to-front by
//                sortKey, so the nearer one is on top (probed blend color);
//   * WORLD    — a quad placed by a world-translation matrix renders at the
//                translated screen position;
//   * CULL     — an off-screen poly is frustum-culled;
//   * BLEND    — the translucent AVERAGE mix is correct.
//
// World layout (camera at origin looking -z, 60 FOV perspective; all main-mesh
// vertices are already in view space at z=-30, world = identity; coordinates
// are raw-model integer units, converted to float verbatim by the adapter):
//   texA  : textured red quad, x -8..8,   y -8..8,  z -30 (opaque)
//   texB  : same texture,       x  8..16, y -8..8,  z -30 (batches w/ texA)
//   gou   : gouraud quad,       x -16..-8, y -8..8, z -30 (green->blue)
//   world : textured red quad, local (0..8, 0..8, 0) placed by world
//           translate(10,8,-30) -> world x 10..18, y 8..16, z -30
//   t1    : translucent blue quad, x -4..4, y -4..4, z -25, sortKey 10
//   t2    : translucent teal quad, x -4..4, y -4..4, z -25, sortKey 5
//   off   : textured red quad, x 50..70 (outside frustum -> culled)

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_resources.h"
#include "dx11_textures.h"
#include "dx11_shaders.h"
#include "dx11_drawcmdexec.h"
#include "dx11_modeladapter.h"

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
static void MatTranslation(float tx, float ty, float tz, float m[4][4]) {
    MatIdentity(m);
    m[0][3] = tx; m[1][3] = ty; m[2][3] = tz;   // row-vector, column-major
}

// Transform a point by a matrix the way the VS mul reads it (transposed).
static void XformPoint(const float m[4][4], const float p[4], float o[4]) {
    for (int j = 0; j < 4; ++j)
        o[j] = p[0] * m[j][0] + p[1] * m[j][1] + p[2] * m[j][2] + p[3] * m[j][3];
}
// World-space point -> window pixel (for BMP probes).
static void ProjectToScreen(const float vp[4][4], float x, float y, float z,
                            int W, int H, int *sx, int *sy) {
    float p[4] = { x, y, z, 1 }, o[4];
    XformPoint(vp, p, o);
    float w = o[3];
    float nx = w != 0 ? o[0] / w : 0, ny = w != 0 ? o[1] / w : 0;
    *sx = (int)((nx * 0.5f + 0.5f) * W);
    *sy = (int)((1.0f - (ny * 0.5f + 0.5f)) * H);
}

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_modeladapter_test: fatal error", MB_OK | MB_ICONERROR);
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

// ---------------------------------------------------------------------------
// VRAM: a 4-bit paletted texture at tpage 0 (tpX=0, tpY=0), texel region
// (0,0,8,8), all texels = palette index 1 = red. CLUT at (0,8) -> clut 512.
// ---------------------------------------------------------------------------
static void BuildVRAM(unsigned short *vram) {
    // CLUT row (16 entries) at x=0, y=8: [0]=white, [1]=red, ...
    const unsigned short pal[4] = { 0x7FFF, 0x001F, 0x03E0, 0x7C00 };
    for (int i = 0; i < 4; ++i) vram[8 * DX11TEX_VRAM_W + i] = pal[i];
    // 4-bit texel region (0,0)-(8,8), all index 1 (red).
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            int ax = x, ay = y;
            int idx = 1;
            int wordCol = ax / 4;
            int byteShift = ((ax & 2) >> 1) * 8;
            int nibShift = (ax & 1) ? 4 : 0;
            vram[ay * DX11TEX_VRAM_W + wordCol] |=
                (unsigned short)(idx << (nibShift + byteShift));
        }
}

// Texture-resolve hook: (set 0, id 0) -> the red 4-bit region; anything else
// (set 1+) -> untextured (white substitute).
static int TexResolve(void *user, unsigned char set, unsigned char id,
                      Dx11ModelTexture *out) {
    (void)user;
    if (set == 0 && id == 0) {
        out->tpage = 0;
        out->clut = 512;        // getClut(0, 8)
        out->tex_x = 0; out->tex_y = 0;
        out->width = 8; out->height = 8;
        return 0;
    }
    return 1;
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
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_modeladapter_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ResResult rsres;
    Dx11Res *res = Dx11Res_Create(dev, ctx, NULL, &rsres);
    if (!res) { MessageBoxA(NULL, "Dx11Res_Create failed", "dx11_modeladapter_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11TexResult tr;
    Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
    if (!tex) { MessageBoxA(NULL, "Dx11Tex_Create failed", "dx11_modeladapter_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11ShadersResult sr;
    Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    if (!sh) { MessageBoxA(NULL, "Dx11Shaders_Create failed", "dx11_modeladapter_test", MB_OK | MB_ICONERROR); return 2; }
    Dx11DrawCmdsResult cr;
    Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, res, tex, sh, NULL, &cr);
    if (!cmds) { MessageBoxA(NULL, "Dx11DrawCmds_Create failed", "dx11_modeladapter_test", MB_OK | MB_ICONERROR); return 2; }

    // Fill the VRAM staging with the red texture.
    unsigned short vram[DX11TEX_VRAM_W * DX11TEX_VRAM_H] = { 0 };
    BuildVRAM(vram);
    Dx11Tex_CopyVRAM(tex, vram, 0, 0, DX11TEX_VRAM_W, DX11TEX_VRAM_H, 0, 0);

    float vp[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f,
                     (float)rcfg.windowW / (float)rcfg.windowH, 0.1f, 100.0f, vp);
    float ident[4][4]; MatIdentity(ident);
    // World quad: local (0..8, 0..8, 0) placed at world x[10,18] y[8,16] z=-30.
    float worldT[4][4]; MatTranslation(10.0f, 8.0f, -30.0f, worldT);

    // ----------------------------------------------------------------------
    // Raw mesh 1 (world = identity): texA, texB, gou, t1, t2, off.
    // ----------------------------------------------------------------------
    Dx11ModelVertex verts[28];
    // texA (0..3)
    verts[0] = { -8,  8, -30, 0,0,0 }; verts[1] = {  8,  8, -30, 0,0,0 };
    verts[2] = {  8, -8, -30, 0,0,0 }; verts[3] = { -8, -8, -30, 0,0,0 };
    // texB (4..7)
    verts[4] = {  8,  8, -30, 0,0,0 }; verts[5] = { 16,  8, -30, 0,0,0 };
    verts[6] = { 16, -8, -30, 0,0,0 }; verts[7] = {  8, -8, -30, 0,0,0 };
    // gou (8..11): left green, right blue
    verts[8]  = { -16,  8, -30, 0, 200, 0 }; verts[9]  = {  -8,  8, -30, 0, 0, 200 };
    verts[10] = {  -8, -8, -30, 0, 0, 200 }; verts[11] = { -16, -8, -30, 0, 200, 0 };
    // world (12..15): local TL,TR,BR,BL = (0,8),(8,8),(8,0),(0,0) so it faces +z
    verts[12] = { 0,  8, 0, 0,0,0 }; verts[13] = { 8,  8, 0, 0,0,0 };
    verts[14] = { 8,  0, 0, 0,0,0 }; verts[15] = { 0,  0, 0, 0,0,0 };
    // t1 (16..19) / t2 (20..23)
    verts[16] = { -4,  4, -25, 0,0,0 }; verts[17] = {  4,  4, -25, 0,0,0 };
    verts[18] = {  4, -4, -25, 0,0,0 }; verts[19] = { -4, -4, -25, 0,0,0 };
    verts[20] = { -4,  4, -25, 0,0,0 }; verts[21] = {  4,  4, -25, 0,0,0 };
    verts[22] = {  4, -4, -25, 0,0,0 }; verts[23] = { -4, -4, -25, 0,0,0 };
    // off (24..27)
    verts[24] = { 50,  5, -30, 0,0,0 }; verts[25] = { 70,  5, -30, 0,0,0 };
    verts[26] = { 70, -5, -30, 0,0,0 }; verts[27] = { 50, -5, -30, 0,0,0 };

    // UVs: full 8x8 texel region -> normalized 0..1.
    // Vertex indices follow the game's quad convention: vi0,vi1,vi3,vi2 are the
    // corners in order (TL,TR,BR,BL); the adapter splits (vi0,vi1,vi3)+(vi0,vi3,vi2).
    Dx11ModelPoly polys[6];
    Dx11ModelPoly *p = &polys[0];
    // texA
    *p = (Dx11ModelPoly){ 0,1,3,2, 0,0, 8,0, 8,8, 0,8, 255,255,255,
                          DX11SH_COLOR_FLAT, DX11SH_BLEND_NONE, 0,0, 0,0, 0 }; ++p;
    // texB (same material, contiguous -> batch)
    *p = (Dx11ModelPoly){ 4,5,7,6, 0,0, 8,0, 8,8, 0,8, 255,255,255,
                          DX11SH_COLOR_FLAT, DX11SH_BLEND_NONE, 0,0, 0,0, 0 }; ++p;
    // gou: no texture (set 1), gouraud per-vertex colors
    *p = (Dx11ModelPoly){ 8,9,11,10, 0,0, 8,0, 8,8, 0,8, 0,0,0,
                          DX11SH_COLOR_GOURAUD, DX11SH_BLEND_NONE, 0,0, 1,0, 0 }; ++p;
    // t1: translucent blue, far (sortKey 10)
    *p = (Dx11ModelPoly){ 16,17,19,18, 0,0, 8,0, 8,8, 0,8, 200,0,0,
                          DX11SH_COLOR_FLAT, DX11SH_BLEND_AVERAGE, 0,0, 1,0, 10 }; ++p;
    // t2: translucent teal, near (sortKey 5)
    *p = (Dx11ModelPoly){ 20,21,23,22, 0,0, 8,0, 8,8, 0,8, 0,200,200,
                          DX11SH_COLOR_FLAT, DX11SH_BLEND_AVERAGE, 0,0, 1,0, 5 }; ++p;
    // off: off-screen (culled)
    *p = (Dx11ModelPoly){ 24,25,27,26, 0,0, 8,0, 8,8, 0,8, 255,255,255,
                          DX11SH_COLOR_FLAT, DX11SH_BLEND_NONE, 0,0, 0,0, 0 };

    Dx11ModelMesh mesh1 = { verts, 28, polys, 6 };

    // Raw mesh 2 (world = worldT): the world-translated quad.
    Dx11ModelPoly wpoly = { 12,13,15,14, 0,0, 8,0, 8,8, 0,8, 255,255,255,
                            DX11SH_COLOR_FLAT, DX11SH_BLEND_NONE, 0,0, 0,0, 0 };
    Dx11ModelMesh mesh2 = { verts, 28, &wpoly, 1 };

    // ----------------------------------------------------------------------
    // Submit + execute + capture.
    // ----------------------------------------------------------------------
    if (Dx11Renderer_BeginFrame(ren) == 0) {
        Dx11Renderer_BindBackbuffer(ren);
        Dx11DrawCmds_BeginFrame(cmds);
        Dx11Res_BeginFrame(res);
        Dx11DrawCmds_SetViewProj(cmds, (const float (*)[4])vp);

        int n1 = 0, n2 = 0;
        if (Dx11ModelAdapter_Submit(res, tex, cmds, &mesh1, (const float (*)[4])ident,
                                    NULL, TexResolve, &n1) != 0)
            Fail("Dx11ModelAdapter_Submit(mesh1)", 0);
        if (Dx11ModelAdapter_Submit(res, tex, cmds, &mesh2, (const float (*)[4])worldT,
                                    NULL, TexResolve, &n2) != 0)
            Fail("Dx11ModelAdapter_Submit(mesh2)", 0);

        int drawCalls = Dx11DrawCmds_Execute(cmds, ctx);
        Dx11Renderer_CaptureToBMP(ren, NULL, "dx11_modeladapter_frame.bmp",
                                  "dx11_modeladapter_frame.txt");
        Dx11Renderer_Present(ren);

        FILE *resf = fopen("dx11_modeladapter_result.txt", "w");
        if (!resf) { MessageBoxA(NULL, "cannot open result file", "dx11_modeladapter_test", MB_OK); return 2; }
        int r = 0, g = 0, b = 0, fails = 0;
        const char *bmp = "dx11_modeladapter_frame.bmp";
        int W = rcfg.windowW, H = rcfg.windowH;

        int culled = Dx11DrawCmds_CulledCount(cmds);
        int submitted = n1 + n2;
        fprintf(resf, "submitted=%d culled=%d drawCalls=%d\n", submitted, culled, drawCalls);

        // CULL: the off-screen poly (x 5..7) is removed.
        int okCull = (culled == 1);
        fprintf(resf, "CULL off-screen poly removed (culled=%d expect 1) %s\n", culled, okCull ? "PASS" : "FAIL");
        if (!okCull) ++fails;

        // BATCH: texA+texB (same material, contiguous) -> 1 call.
        // 7 submitted, 1 culled -> 6 drawn; texA+texB batch = 5 calls.
        int okBatch = (drawCalls == 5);
        fprintf(resf, "BATCH same-material pair -> 1 call (drawCalls=%d expect 5) %s\n", drawCalls, okBatch ? "PASS" : "FAIL");
        if (!okBatch) ++fails;

        // TEXTURE: texA (0,6,-30) -> red (baked palette index 1), above the
        // translucent t1/t2 footprint.
        int sx, sy;
        ProjectToScreen(vp, 0.0f, 6.0f, -30.0f, W, H, &sx, &sy);
        if (ProbeBMP(bmp, sx, sy, &r, &g, &b) == 0) {
            int ok = (r > 180 && g < 40 && b < 40);
            fprintf(resf, "TEXTURE texA(%d,%d)=(%d,%d,%d) red %s\n", sx, sy, r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "TEXTURE texA probe unavailable\n"); }

        // TEXTURE: texB (12,0,-30) -> red (batched pair still renders).
        ProjectToScreen(vp, 12.0f, 0.0f, -30.0f, W, H, &sx, &sy);
        if (ProbeBMP(bmp, sx, sy, &r, &g, &b) == 0) {
            int ok = (r > 180 && g < 40 && b < 40);
            fprintf(resf, "TEXTURE texB(%d,%d)=(%d,%d,%d) red %s\n", sx, sy, r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "TEXTURE texB probe unavailable\n"); }

        // GOURAUD: left (-14,0,-30) green, right (-9,0,-30) blue.
        ProjectToScreen(vp, -14.0f, 0.0f, -30.0f, W, H, &sx, &sy);
        if (ProbeBMP(bmp, sx, sy, &r, &g, &b) == 0) {
            int ok = (g > 120 && r < 60 && b < 90);   // green-dominant (interp adds some blue)
            fprintf(resf, "GOURAUD left(%d,%d)=(%d,%d,%d) green %s\n", sx, sy, r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "GOURAUD left probe unavailable\n"); }
        ProjectToScreen(vp, -9.0f, 0.0f, -30.0f, W, H, &sx, &sy);
        if (ProbeBMP(bmp, sx, sy, &r, &g, &b) == 0) {
            int ok = (b > 150 && r < 40 && g < 40);
            fprintf(resf, "GOURAUD right(%d,%d)=(%d,%d,%d) blue %s\n", sx, sy, r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "GOURAUD right probe unavailable\n"); }

        // WORLD: (14,12,-30) -> red (world-translated textured quad).
        ProjectToScreen(vp, 14.0f, 12.0f, -30.0f, W, H, &sx, &sy);
        if (ProbeBMP(bmp, sx, sy, &r, &g, &b) == 0) {
            int ok = (r > 180 && g < 40 && b < 40);
            fprintf(resf, "WORLD world(%d,%d)=(%d,%d,%d) red %s\n", sx, sy, r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "WORLD probe unavailable\n"); }

        // SORT + BLEND: center (0,0,-25) -> t2(near teal) must be on top of
        // t1(far blue) over texA(textured red 248,0,0). Correct B2F order:
        //   after t1 (a=.5 over red) = (224,0,0); after t2 (a=.5) = (112,100,100).
        // Wrong order (t2 first) would give g=b~50. So teal-dominant proves order.
        ProjectToScreen(vp, 0.0f, 0.0f, -25.0f, W, H, &sx, &sy);
        if (ProbeBMP(bmp, sx, sy, &r, &g, &b) == 0) {
            int ok = (g > 80 && b > 80);
            fprintf(resf, "SORT center(%d,%d)=(%d,%d,%d) expect(112,100,100) teal-on-top %s\n", sx, sy, r, g, b, ok ? "PASS" : "FAIL");
            if (!ok) ++fails;
        } else { ++fails; fprintf(resf, "SORT probe unavailable\n"); }

        fprintf(resf, "TOTAL_FAILS=%d ADAPTER=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
        fclose(resf);
    }

    Dx11DrawCmds_Destroy(cmds);
    Dx11Shaders_Destroy(sh);
    Dx11Tex_Destroy(tex);
    Dx11Res_Destroy(res);
    Dx11Renderer_Destroy(ren);
    return 0;
}
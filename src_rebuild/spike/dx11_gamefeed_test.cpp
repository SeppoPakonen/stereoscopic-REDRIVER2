// dx11_gamefeed_test.cpp — T5.1 in-game renderer integration harness.
//
// Drives the real DrawCommand -> Dx11ModelAdapter -> executor -> per-eye ->
// composite path with a synthetic map (large green textured-quad MODEL) + car
// (small red quad MODEL) DrawCommand scene, and verifies the composited output:
// the map is present in both eye halves (no "right-eye map disappears"), the car
// is present in both, and the MONO path renders the base scene.
//
// Probes:
//   MAP_L / MAP_R — a map-only pixel is green in eye0 (left) and eye1 (right);
//   CAR_L / CAR_R — the car pixel is red in both eye halves;
//   MONO — the map fills the frame in the MONO composite (base scene).

#define WIN32_LEAN_AND_MEAN
#include "dx11_gamefeed.h"
#include "mdl.h"
#include "libgte.h"

#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Perspective RH projection (row-vector, column-major) — same as T2.4.
// ---------------------------------------------------------------------------
static void MatPerspectiveRH(float fovY, float aspect, float zn, float zf, float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fovY * 0.5f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = zf / (zn - zf);
    m[2][3] = zn * zf / (zn - zf);
    m[3][2] = -1.0f;
}

// ---------------------------------------------------------------------------
// A relocatable MODEL buffer holding one flat-textured-quad (PL_POLYFT4).
// ---------------------------------------------------------------------------
typedef struct {
    MODEL mdl;
    SVECTOR verts[4];
    PL_POLYFT4 poly;
} QuadModel;

// Build a quad model at the given world-local corners (TL,TR,BR,BL order).
static QuadModel MakeQuadModel(const short v[4][3]) {
    QuadModel m;
    memset(&m, 0, sizeof(m));
    m.mdl.vertices = (int)((unsigned char *)m.verts - (unsigned char *)&m);   // offset to verts
    m.mdl.poly_block = (int)((unsigned char *)&m.poly - (unsigned char *)&m); // offset to poly
    m.mdl.num_vertices = 4;
    m.mdl.num_polys = 1;
    for (int i = 0; i < 4; ++i) {
        m.verts[i].vx = v[i][0];
        m.verts[i].vy = v[i][1];
        m.verts[i].vz = v[i][2];
        m.verts[i].pad = 0;
    }
    // Flat textured quad, poly type 11 (PL_POLYFT4, 16 bytes). v0,v1,v2,v3 use
    // the game's winding: v0=TL, v1=TR, v3=BR, v2=BL (the renderer reads
    // v0,v1,v3 as the triangle and v2 as the 4th corner). verts[2]=BR, verts[3]=BL.
    m.poly.id = 11;
    m.poly.texture_set = 0;
    m.poly.texture_id = 0;
    m.poly.th = 0;
    m.poly.v0 = 0; m.poly.v1 = 1; m.poly.v2 = 3; m.poly.v3 = 2;
    m.poly.uv0.u = 0; m.poly.uv0.v = 0;
    m.poly.uv1.u = 0; m.poly.uv1.v = 0;
    m.poly.uv2.u = 0; m.poly.uv2.v = 0;
    m.poly.uv3.u = 0; m.poly.uv3.v = 0;
    return m;
}

// Identity + translation MATRIX (int16 rot /4096, long trans).
static void MatSetTrans(MATRIX *m, long x, long y, long z) {
    memset(m, 0, sizeof(*m));
    m->m[0][0] = 4096; m->m[1][1] = 4096; m->m[2][2] = 4096;
    m->t[0] = x; m->t[1] = y; m->t[2] = z;
}

// Texture resolve: always untextured (white substitute + flatColor).
static int TexResolve_Untextured(void *user, unsigned char set, unsigned char id,
                                 Dx11ModelTexture *out) {
    (void)user; (void)set; (void)id; (void)out;
    return 1;   // nonzero => untextured
}

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

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Dx11RendererConfig rcfg = { 640, 240, 320, 240, 0, 0 };   // window 2x for SBS
    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_gamefeed", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ResResult rsres;   Dx11Res *res = Dx11Res_Create(dev, ctx, NULL, &rsres);
    Dx11TexResult tr;      Dx11Tex *tex = Dx11Tex_Create(dev, ctx, NULL, &tr);
    Dx11ShadersResult sr;  Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    Dx11DrawCmdsResult cr; Dx11DrawCmds *cmds = Dx11DrawCmds_Create(dev, ctx, res, tex, sh, NULL, &cr);
    Dx11CompositeResult cpr; Dx11Composite *comp = Dx11Composite_Create(dev, ctx, &cpr);
    if (!res || !tex || !sh || !cmds || !comp) { MessageBoxA(NULL, "setup failed", "dx11_gamefeed", MB_OK | MB_ICONERROR); return 2; }

    // Synthetic scene (near-scale, int16 ints; the proven T2.4 camera): a large
    // map quad (green) at z=-3 and a smaller car quad (red) at z=-2, x=-1.
    const short mapV[4][3] = { { -2, -1, -3 }, { 2, -1, -3 },
                               { 2, 1, -3 }, { -2, 1, -3 } };
    const short carV[4][3] = { { -1, -1, -2 }, { 0, -1, -2 },
                               { 0, 1, -2 }, { -1, 1, -2 } };
    QuadModel map = MakeQuadModel(mapV);
    QuadModel car = MakeQuadModel(carV);

    DrawCommand cmdsList[2];
    memset(cmdsList, 0, sizeof(cmdsList));
    cmdsList[0].mesh = &map.mdl;
    MatSetTrans(&cmdsList[0].world, 0, 0, 0);
    cmdsList[0].flags = DRAWCMD_OPAQUE | DRAWCMD_TWOSIDED;
    cmdsList[1].mesh = &car.mdl;
    MatSetTrans(&cmdsList[1].world, 0, 0, 0);
    cmdsList[1].flags = DRAWCMD_OPAQUE | DRAWCMD_TWOSIDED;

    unsigned char colors[2][3] = { { 0, 200, 0 }, { 220, 0, 0 } };   // map green, car red

    float proj[4][4];
    MatPerspectiveRH(60.0f * 3.14159265f / 180.0f, 320.0f / 240.0f, 1.0f, 100.0f, proj);
    float camPos[3] = { 0, 0, 0 };

    FILE *resf = fopen("dx11_gamefeed_result.txt", "w");
    if (!resf) return 2;
    int fails = 0;

    // SBS composite: left half = eye0, right half = eye1.
    Dx11GameFeed_RenderFrame(ren, res, tex, sh, cmds, comp, proj,
                             cmdsList, 2, colors, camPos, 0.0f, 0.1f, 0,
                             DX11C_MODE_SBS, NULL, TexResolve_Untextured,
                             "dx11_gamefeed.bmp");

    int iw = 320, cy = 120;
    // NDC x = m[0][0] * (x / -z), m[0][0] = (1/tan(30deg)) / aspect = 1.299.
    // Map-only probe at world x=+1, z=-3; car centre at x=-0.5, z=-2.
    int mapCol = (int)((1.299f * (1.0f / 3.0f) + 1.0f) * 0.5f * iw);      // ~229
    int carCol = (int)((1.299f * (-0.5f / 2.0f) + 1.0f) * 0.5f * iw);     // ~108
    int r = 0, g = 0, b = 0;
    auto IsGreen = [](int r, int g, int b) { return g > 140 && r < 40 && b < 40; };
    auto IsRed = [](int r, int g, int b) { return r > 180 && g < 40 && b < 40; };

    // MAP_L / MAP_R: map present in both eyes (right-eye map not dropped).
    int okMapL = (ProbeBMPPixel("dx11_gamefeed.bmp", mapCol, cy, &r, &g, &b) == 0 && IsGreen(r, g, b));
    int okMapR = (ProbeBMPPixel("dx11_gamefeed.bmp", mapCol + 320, cy, &r, &g, &b) == 0 && IsGreen(r, g, b));
    fprintf(resf, "MAP_L left map(%d,%d)=%s MAP_R right map(%d,%d)=%s %s\n",
            mapCol, cy, okMapL ? "green" : "FAIL", mapCol + 320, cy, okMapR ? "green" : "FAIL",
            (okMapL && okMapR) ? "PASS" : "FAIL");
    if (!okMapL) ++fails;
    if (!okMapR) ++fails;

    // CAR_L / CAR_R: car present in both eyes.
    int okCarL = (ProbeBMPPixel("dx11_gamefeed.bmp", carCol, cy, &r, &g, &b) == 0 && IsRed(r, g, b));
    int okCarR = (ProbeBMPPixel("dx11_gamefeed.bmp", carCol + 320, cy, &r, &g, &b) == 0 && IsRed(r, g, b));
    fprintf(resf, "CAR_L left car(%d,%d)=%s CAR_R right car(%d,%d)=%s %s\n",
            carCol, cy, okCarL ? "red" : "FAIL", carCol + 320, cy, okCarR ? "red" : "FAIL",
            (okCarL && okCarR) ? "PASS" : "FAIL");
    if (!okCarL) ++fails;
    if (!okCarR) ++fails;

    // MONO: eye0 pass-through — the base scene renders (map fills the frame).
    // MONO stretches eye0 (320) across the 640 window, so scale probe x by 2.
    Dx11GameFeed_RenderFrame(ren, res, tex, sh, cmds, comp, proj,
                             cmdsList, 2, colors, camPos, 0.0f, 0.1f, 0,
                             DX11C_MODE_MONO, NULL, TexResolve_Untextured,
                             "dx11_gamefeed_mono.bmp");
    int okMonoMap = (ProbeBMPPixel("dx11_gamefeed_mono.bmp", 2 * mapCol, 120, &r, &g, &b) == 0 && IsGreen(r, g, b));
    int okMonoCar = (ProbeBMPPixel("dx11_gamefeed_mono.bmp", 2 * carCol, 120, &r, &g, &b) == 0 && IsRed(r, g, b));
    fprintf(resf, "MONO map(%d,120)=%s car(%d,120)=%s %s\n",
            2 * mapCol, okMonoMap ? "green" : "FAIL", 2 * carCol, okMonoCar ? "red" : "FAIL",
            (okMonoMap && okMonoCar) ? "PASS" : "FAIL");
    if (!okMonoMap) ++fails;
    if (!okMonoCar) ++fails;

    fprintf(resf, "TOTAL_FAILS=%d GAMEFEED=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    Dx11Composite_Destroy(comp);
    Dx11DrawCmds_Destroy(cmds);
    Dx11Shaders_Destroy(sh);
    Dx11Tex_Destroy(tex);
    Dx11Res_Destroy(res);
    Dx11Renderer_Destroy(ren);
    return 0;
}
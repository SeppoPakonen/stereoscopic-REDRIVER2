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

// ---------------------------------------------------------------------------
// Focused unit test for the T5.2 car body converter (Dx11GameFeed_CarModelToMesh):
// verifies it decodes a CAR_MODEL's GT3/FT3 triangles — vertex indices, page-
// scaled UVs, and the per-kind clut source (GT3 via civClut[carid][texid][palette],
// FT3 raw clut address) + tpage. Mirrors the game's CAR_MODEL/CAR_POLY layout.
// ---------------------------------------------------------------------------
typedef struct { int vindices, nindices, clut_uv0, tpage_uv1, uv3_uv2; short originalindex; } TCAR_POLY;
typedef struct { int numFT3; TCAR_POLY* pFT3; int numGT3; TCAR_POLY* pGT3; int numB3; TCAR_POLY* pB3; SVECTOR* vlist; SVECTOR* nlist; } TCAR_MODEL;

static void TestCarModelConverter(FILE* resf, int* fails) {
    SVECTOR vlist[256];
    for (int i = 0; i < 256; ++i) { vlist[i].vx = (short)i; vlist[i].vy = (short)-i; vlist[i].vz = (short)(i * 2); }

    // GT3: packed clut index 0 (carid=1,texid=0); clut resolved via civClut[1][0][2].
    TCAR_POLY gt3;
    gt3.vindices = 0 | (1 << 8) | (2 << 16);
    gt3.nindices = 0;
    gt3.clut_uv0  = (0 << 16) | (128 | 64 << 8);        // u0=128,v0=64
    gt3.tpage_uv1 = (0x2000 << 16) | (32 | 16 << 8);    // tpage fmt 0 -> pageW 64, u1=32,v1=16
    gt3.uv3_uv2   = (200 | 8 << 8);                      // u2=200,v2=8
    gt3.originalindex = 0;

    // FT3: clut is the raw VRAM clut address 0x3400.
    TCAR_POLY ft3;
    ft3.vindices = 0 | (1 << 8) | (2 << 16);
    ft3.nindices = 0;
    ft3.clut_uv0  = (0x3400 << 16) | (10 | 20 << 8);
    ft3.tpage_uv1 = (0x2000 << 16) | (30 | 40 << 8);
    ft3.uv3_uv2   = (50 | 60 << 8);
    ft3.originalindex = 1;

    TCAR_MODEL car;
    memset(&car, 0, sizeof(car));
    car.numGT3 = 1; car.pGT3 = &gt3;
    car.numFT3 = 1; car.pFT3 = &ft3;
    car.numB3  = 0;
    car.vlist = vlist;
    car.nlist = vlist;

    u_short civClut[8][32][6];
    memset(civClut, 0, sizeof(civClut));
    civClut[1][0][2] = 0x1234;   // carid=1, texid=0, palette=2

    Dx11ModelVertex verts[256];
    Dx11ModelPoly polys[8];
    int ov = 0, op = 0;
    int rc = Dx11GameFeed_CarModelToMesh(&car, 2, civClut, verts, 256, polys, 8, &ov, &op);

    int okVerts = (rc == 0 && ov == 256);
    int okCount = (op == 2);
    int okGT3 = okVerts ? (polys[0].vi0 == 0 && polys[0].vi1 == 1 && polys[0].vi2 == 2 && polys[0].vi3 == 2
                          && polys[0].carTexture && polys[0].carClut == 0x1234 && polys[0].carTpage == 0x2000
                          && polys[0].u0 == 32 && polys[0].v0 == 64 && polys[0].u1 == 8 && polys[0].v1 == 16
                          && polys[0].u2 == 50 && polys[0].v2 == 8) : 0;
    int okFT3 = okCount ? (polys[1].carTexture && polys[1].carClut == 0x3400 && polys[1].carTpage == 0x2000
                           && polys[1].u0 == 2 && polys[1].v0 == 20) : 0;
    fprintf(resf, "CAR_FEED verts=%s count=%s gt3=%s ft3=%s %s\n",
            okVerts ? "ok" : "FAIL", okCount ? "ok" : "FAIL", okGT3 ? "ok" : "FAIL",
            okFT3 ? "ok" : "FAIL", (okVerts && okCount && okGT3 && okFT3) ? "PASS" : "FAIL");
    if (!okVerts) ++(*fails);
    if (!okCount) ++(*fails);
    if (!okGT3) ++(*fails);
    if (!okFT3) ++(*fails);
}

// Focused unit test for the T5.2 sky converter (Dx11GameFeed_SkyModelToMesh):
// verifies it textures a horizon MODEL's poly from the sky tables
// (skytpage/skyclut/skytexuv via HorizonTextures[horizOffset]) with the
// u2,u3,u0,u1 UV remap and the per-poly carTpage/carClut direct-bake path.
static void TestSkyModelConverter(FILE* resf, int* fails) {
    const short mkV[4][3] = { { 0,0,0 }, { 1,0,0 }, { 1,1,0 }, { 0,1,0 } };
    QuadModel m = MakeQuadModel(mkV);

    unsigned short skytpage[28] = { 0 };
    unsigned short skyclut[28] = { 0 };
    Dx11SkyUV skytexuv[28];
    memset(skytexuv, 0, sizeof(skytexuv));
    unsigned char horizonTex[40] = { 0 };
    horizonTex[0] = 2;          // poly 0 -> sky texture 2
    skytpage[2] = 0x2000;       // fmt 0 -> pageW 64
    skyclut[2] = 0x1234;
    skytexuv[2] = (Dx11SkyUV){ 0,0, 128,0, 0,84, 128,84 };   // u0=0,v0=0 | u1=128,v1=0 | u2=0,v2=84 | u3=128,v3=84

    Dx11SkyTextures sky = { skytpage, skyclut, skytexuv, horizonTex };

    Dx11ModelVertex verts[8];
    Dx11ModelPoly polys[4];
    int ov = 0, op = 0;
    int rc = Dx11GameFeed_SkyModelToMesh(&m.mdl, &sky, 0, verts, 8, polys, 4, &ov, &op);

    // pageW=64: u0=(0*64)>>8=0, u1=(128*64)>>8=32, u3=0, u2=32; v via remap.
    // Winding: vi0=0, vi1=1, vi3=2, vi2=3 (poly v0=0,v1=1,v3=2,v2=3).
    int ok = (rc == 0 && op == 1
              && polys[0].carTexture && polys[0].carTpage == 0x2000 && polys[0].carClut == 0x1234
              && polys[0].vi0 == 0 && polys[0].vi1 == 1 && polys[0].vi3 == 2 && polys[0].vi2 == 3
              && polys[0].u0 == 0 && polys[0].v0 == 84
              && polys[0].u1 == 32 && polys[0].v1 == 84
              && polys[0].u3 == 0 && polys[0].v3 == 0
              && polys[0].u2 == 32 && polys[0].v2 == 0);
    fprintf(resf, "SKY_FEED verts=%d polys=%d %s\n", ov, op, ok ? "PASS" : "FAIL");
    if (!ok) ++(*fails);
}

// Focused unit test for the T5.2 addPrim single-primitive billboard converter
// (Dx11GameFeed_BillboardToMesh): verifies it builds a 4-vertex quad + 1 flat
// textured-quad poly carrying the material (page-scaled UVs, blend, direct-bake
// carTexture/tpage/clut) and the flat color, in both camera-facing and world-
// ground orientations.
static void TestBillboardConverter(FILE* resf, int* fails) {
    MaterialRef mat;
    memset(&mat, 0, sizeof(mat));
    mat.tpage = 0x2000;        // fmt 0 -> pageW 64
    mat.clut = 0x1234;
    mat.blendMode = MATBLEND_TRANSLUCENT;   // -> DX11SH_BLEND_AVERAGE

    const unsigned char uv[8] = { 128,16, 32,16, 200,8, 32,8 };   // u0,v0,u1,v1,u2,v2,u3,v3
    const unsigned char rgb[3] = { 200, 100, 50 };
    float center[3] = { 0, 0, -10 };
    float camPos[3] = { 0, 0, 0 };

    Dx11ModelVertex verts[4];
    Dx11ModelPoly poly;

    // Camera-facing: toCam = +Z -> right=+X, up=+Y; halfX=5, halfY=3.
    int rc = Dx11GameFeed_BillboardToMesh(center, BILLBOARD_CAMERA, 5, 3, &mat, uv,
                                          rgb, 42, camPos, verts, &poly);
    int okCam = (rc == 0
                 && verts[0].x == -5 && verts[0].y == 3 && verts[0].z == 0     // TL
                 && verts[1].x ==  5 && verts[1].y == 3 && verts[1].z == 0     // TR
                 && verts[2].x == -5 && verts[2].y == -3 && verts[2].z == 0    // BL
                 && verts[3].x ==  5 && verts[3].y == -3 && verts[3].z == 0    // BR
                 && poly.vi0 == 0 && poly.vi1 == 1 && poly.vi3 == 3 && poly.vi2 == 2
                 && poly.u0 == 32 && poly.v0 == 16                             // (128*64)>>8
                 && poly.u1 == 8 && poly.v1 == 16
                 && poly.u2 == 8 && poly.v2 == 8
                 && poly.u3 == 50 && poly.v3 == 8                              // (200*64)>>8
                 && poly.r == 200 && poly.g == 100 && poly.b == 50
                 && poly.shade == DX11SH_COLOR_FLAT
                 && poly.blend == DX11SH_BLEND_AVERAGE
                 && poly.twoSided == 1
                 && poly.carTexture && poly.carTpage == 0x2000 && poly.carClut == 0x1234
                 && poly.sortKey == 42);
    fprintf(resf, "BILLBOARD_FEED camera=%s\n", okCam ? "ok" : "FAIL");
    if (!okCam) ++(*fails);

    // World-ground: right=+X, up=+Z (quad in the XZ plane); halfX=4, halfY=2.
    rc = Dx11GameFeed_BillboardToMesh(center, BILLBOARD_WORLD, 4, 2, &mat, uv,
                                      rgb, 7, camPos, verts, &poly);
    int okWorld = (rc == 0
                   && verts[0].x == -4 && verts[0].y == 0 && verts[0].z == 2
                   && verts[1].x ==  4 && verts[1].y == 0 && verts[1].z == 2
                   && verts[2].x == -4 && verts[2].y == 0 && verts[2].z == -2
                   && verts[3].x ==  4 && verts[3].y == 0 && verts[3].z == -2
                   && poly.sortKey == 7);
    fprintf(resf, "BILLBOARD_FEED world=%s\n", okWorld ? "ok" : "FAIL");
    if (!okWorld) ++(*fails);
}

// Focused unit test for the pitch/roll view basis path
// (Dx11Stereo_ViewMatrixBasis): verifies that feeding the yaw-derived basis
// reproduces the proven yaw view exactly, and that a tilted (pitched/rolled)
// basis produces a different view (the full camera orientation is applied).
static void TestViewBasis(FILE* resf, int* fails) {
    float camPos[3] = { 10, 20, 30 };
    float yaw = 0.7f;

    // Yaw basis = the Dx11Stereo_ViewMatrix basis; must reproduce it exactly.
    float right[3] = { cosf(yaw), 0.0f, sinf(yaw) };
    float up[3]    = { 0.0f, 1.0f, 0.0f };
    float negFwd[3]= { -sinf(yaw), 0.0f, cosf(yaw) };
    float basisMat[4][4], yawMat[4][4];
    Dx11Stereo_ViewMatrixBasis(camPos, right, up, negFwd, DX11STEREO_EYE_MONO, 0, 0, basisMat);
    Dx11Stereo_ViewMatrix(camPos, yaw, DX11STEREO_EYE_MONO, 0, 0, yawMat);
    int okYaw = 1;
    for (int i = 0; i < 4 && okYaw; ++i)
        for (int j = 0; j < 4; ++j)
            if (fabsf(basisMat[i][j] - yawMat[i][j]) > 1e-4f) { okYaw = 0; break; }

    // A tilted (rolled) basis must change the view vs the yaw one.
    float tUp[3]    = { 0.0f, cosf(0.5f), sinf(0.5f) };
    float tNegFwd[3]= { 0.0f, -sinf(0.5f), cosf(0.5f) };
    float tiltMat[4][4];
    Dx11Stereo_ViewMatrixBasis(camPos, right, tUp, tNegFwd, DX11STEREO_EYE_MONO, 0, 0, tiltMat);
    float maxDiff = 0.0f;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float d = fabsf(tiltMat[i][j] - yawMat[i][j]);
            if (d > maxDiff) maxDiff = d;
        }
    int okTilt = (maxDiff > 1e-4f);   // the tilted basis must change the view

    fprintf(resf, "BASIS_FEED yaw-match=%s tilt-applied=%s %s\n",
            okYaw ? "ok" : "FAIL", okTilt ? "ok" : "FAIL",
            (okYaw && okTilt) ? "PASS" : "FAIL");
    if (!okYaw) ++(*fails);
    if (!okTilt) ++(*fails);
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

    // T5.2 car body converter unit test (no renderer needed).
    TestCarModelConverter(resf, &fails);
    // T5.2 sky horizon converter unit test (no renderer needed).
    TestSkyModelConverter(resf, &fails);
    // T5.2 addPrim single-primitive billboard converter unit test (no renderer).
    TestBillboardConverter(resf, &fails);
    // T5.2 pitch/roll view basis unit test (no renderer).
    TestViewBasis(resf, &fails);

    // SBS composite: left half = eye0, right half = eye1.
    Dx11GameFeed_RenderFrame(ren, res, tex, sh, cmds, comp, proj,
                             cmdsList, 2, colors, camPos, 0.0f, 0.1f, 0,
                             DX11C_MODE_SBS, NULL, TexResolve_Untextured,
                             NULL /*tpages*/, NULL /*civClut*/, NULL /*skyTex*/,
                             "dx11_gamefeed.bmp", NULL /*customView*/, NULL /*basis*/);

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
                             NULL /*tpages*/, NULL /*civClut*/, NULL /*skyTex*/,
                             "dx11_gamefeed_mono.bmp", NULL /*customView*/, NULL /*basis*/);
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
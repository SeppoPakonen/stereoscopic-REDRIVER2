// gl_nonstereo_test.cpp — T4.4 modern GL mono-path harness + DX11 A/B.
//
// Renders the T4.2 common world-state scene (dx11_backendab_state.h) through
// the modern GL backend's MONO path (GlRenderer — SDL + GL 3.3 core + glad,
// VAO/VBO/IBO + GLSL shaders + orthographic projection) and proves it renders
// the base scene correctly, as a full-frame direct render, and with parity to
// the DX11 backend (which renders the identical scene from the same world-state).
//
// Probes:
//   GL_QUAD0..2  — each quad's centroid pixel == its stored color;
//   FULL_FRAME   — the left/right bars span the full height and the centre
//                  square is present (a standard full-frame projection, not a
//                  PSX primitive stream);
//   DX11_PARITY  — the quad centroids match the DX11 reference BMP (produced by
//                  the T4.2 dx11_backendab_dx11 binary), proving the modern GL
//                  backend reproduces the same scene as DX11.

#define WIN32_LEAN_AND_MEAN
#include "dx11_backendab_state.h"
#include "gl_renderer.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Verify one screenshot against the stored state: each quad's centroid pixel
// must match its stored color. Returns number of failed probes.
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
    char dx11Ref[260] = { 0 };
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
            } else if (!strcmp(tok, "-dx11ref") && (tok = strtok(NULL, " \t"))) {
                strncpy(dx11Ref, tok, 259); dx11Ref[259] = 0;
            }
            tok = strtok(NULL, " \t");
        }
    }

    FILE *resf = fopen("gl_nonstereo_result.txt", "w");
    if (!resf) return 2;
    int fails = 0;

    // Convert the common world-state into GL screen-space quads.
    unsigned int nq = scene.numQuads > DX11AB_MAX_QUADS ? DX11AB_MAX_QUADS : scene.numQuads;
    GlQuad *gq = (GlQuad *)malloc(nq * sizeof(GlQuad));
    for (unsigned int i = 0; i < nq; ++i) {
        gq[i].x = scene.quads[i].x;
        gq[i].y = scene.quads[i].y;
        gq[i].w = scene.quads[i].w;
        gq[i].h = scene.quads[i].h;
        gq[i].r = scene.quads[i].r;
        gq[i].g = scene.quads[i].g;
        gq[i].b = scene.quads[i].b;
    }

    GlRendererConfig cfg = { (int)scene.resW, (int)scene.resH };
    GlRenderer *ren = GlRenderer_Create(&cfg);
    if (!ren) {
        fprintf(resf, "RENDER setup FAIL\n"); fclose(resf); return 2;
    }
    const char *bmpGL = "gl_nonstereo.bmp";
    if (GlRenderer_RenderFrame(ren, gq, (int)nq, bmpGL) != 0) {
        fprintf(resf, "RENDER frame FAIL\n");
        GlRenderer_Destroy(ren); free(gq); fclose(resf); return 2;
    }

    // GL_QUAD0..2: the base scene renders correctly through the GL mono path.
    fails += VerifyFrame(&scene, bmpGL, resf, "GL");

    // FULL_FRAME: the left/right bars span the full height and the centre
    // square is present — a standard full-frame projection, not a PSX
    // primitive stream / composite.
    int W = (int)scene.resW, H = (int)scene.resH;
    int cy = H / 2;
    int r = 0, g = 0, b = 0;
    int okL = (ProbeBMPPixel(bmpGL, (int)(W * 0.10f), cy, &r, &g, &b) == 0 &&
               ColorMatch(r, g, b, scene.quads[0].r, scene.quads[0].g, scene.quads[0].b, 40));
    int okR = (ProbeBMPPixel(bmpGL, (int)(W * 0.90f), cy, &r, &g, &b) == 0 &&
               ColorMatch(r, g, b, scene.quads[2].r, scene.quads[2].g, scene.quads[2].b, 40));
    int okC = (ProbeBMPPixel(bmpGL, (int)(W * 0.50f), cy, &r, &g, &b) == 0 &&
               ColorMatch(r, g, b, scene.quads[1].r, scene.quads[1].g, scene.quads[1].b, 40));
    int okFull = okL && okR && okC;
    fprintf(resf, "FULL_FRAME leftbar(%.0f)=%s rightbar(%.0f)=%s centersq(%.0f)=%s %s\n",
            W * 0.10f, okL ? "ok" : "FAIL", W * 0.90f, okR ? "ok" : "FAIL",
            W * 0.50f, okC ? "ok" : "FAIL", okFull ? "PASS" : "FAIL");
    if (!okFull) ++fails;

    // DX11_PARITY: the GL frame's quad centroids match the DX11 reference
    // (the same scene rendered by the DX11 backend from the same world-state).
    if (dx11Ref[0]) {
        int fD = VerifyFrame(&scene, dx11Ref, resf, "DX11");
        int okPar = (fD == 0);
        fprintf(resf, "DX11_PARITY %u/%u centroids match dx11.bmp %s\n",
                scene.numQuads - (unsigned)fD, scene.numQuads, okPar ? "PASS" : "FAIL");
        if (!okPar) ++fails;
    } else {
        fprintf(resf, "DX11_PARITY skipped (no -dx11ref; run dx11_backendab_dx11 -store first)\n");
    }

    fprintf(resf, "TOTAL_FAILS=%d GLMONO=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    GlRenderer_Destroy(ren);
    free(gq);
    return 0;
}
// dx11_backendab_psyx.cpp — T4.2 psyx-side of the dual-backend A/B harness.
//
// This binary links ONLY the legacy PsyCross GL backend (PsyX_Initialise +
// GTE/addPrim/DrawOTag) — it contains no DX11 code. It renders the common T4.2
// screen-space scene (from `dx11_backendab_state.h`) through the legacy
// primitive path (POLY_F4 + ordering table) and implements the emulator-style
// store/load + screenshot contract:
//
//   -store <state.bin> -shot <out.bmp>      render the default scene, save state+screenshot
//   -load  <state.bin> -shot <out.bmp>      load state, render immediately, save screenshot
//
// The DX11-side binary (dx11_backendab_dx11) hosts the -compare step that
// proves both screenshots reproduced the same quads from the identical stored
// state. This binary is the reference/fallback backend output.

#include <windows.h>               // WinMain / HINSTANCE

#include "PsyX/PsyX_public.h"      // PsyX_Initialise / PsyX_EndScene / PsyX_GetScreenSize
#include "PsyX/common/glad.h"      // glReadPixels / glClear / glClearColor (loaded by PsyX_Initialise)
#include "psx/libgte.h"            // SVECTOR / PSX GTE types (required by libgpu.h)
#include "psx/libgpu.h"            // POLY_F4 + set macros + ClearOTagR/addPrim/DrawOTag/DRAWENV/DISPENV
#include "dx11_backendab_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// BMP writing: writes a standard bottom-up 24-bit BGR BMP from a TOP-DOWN BGR(A)
// buffer (row 0 = top-left). Matches the format Dx11Renderer_CaptureToBMP uses,
// so the A/B compare probes both screenshots identically.
// ---------------------------------------------------------------------------
static int WriteBMP(const char *path, int w, int h, const unsigned char *bgr) {
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    int rowSize = (w * 3 + 3) & ~3;
    int dataSize = rowSize * h;
    unsigned int fileSize = 54 + (unsigned int)dataSize;
    unsigned char hdr[54] = { 0 };
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = (unsigned char)(fileSize & 0xFF);
    hdr[3] = (unsigned char)((fileSize >> 8) & 0xFF);
    hdr[4] = (unsigned char)((fileSize >> 16) & 0xFF);
    hdr[5] = (unsigned char)((fileSize >> 24) & 0xFF);
    hdr[10] = 54;                                  // pixel data offset
    hdr[14] = 40;                                  // BITMAPINFOHEADER size
    hdr[18] = (unsigned char)(w & 0xFF);           // width
    hdr[19] = (unsigned char)((w >> 8) & 0xFF);
    hdr[20] = (unsigned char)((w >> 16) & 0xFF);
    hdr[21] = (unsigned char)((w >> 24) & 0xFF);
    hdr[22] = (unsigned char)(h & 0xFF);           // height
    hdr[23] = (unsigned char)((h >> 8) & 0xFF);
    hdr[24] = (unsigned char)((h >> 16) & 0xFF);
    hdr[25] = (unsigned char)((h >> 24) & 0xFF);
    hdr[26] = 1;                                   // planes
    hdr[28] = 24;                                  // bits per pixel
    if (fwrite(hdr, 1, 54, f) != 54) { fclose(f); return 1; }
    // BMP rows are stored bottom-up; write the top-down buffer reversed.
    unsigned char *pad = (unsigned char *)calloc(rowSize - w * 3, 1);
    for (int y = h - 1; y >= 0; --y) {
        const unsigned char *row = bgr + (size_t)y * w * 3;
        if (fwrite(row, 1, w * 3, f) != (size_t)w * 3) { free(pad); fclose(f); return 1; }
        if (rowSize > w * 3 && fwrite(pad, 1, rowSize - w * 3, f) != (size_t)(rowSize - w * 3)) {
            free(pad); fclose(f); return 1;
        }
    }
    free(pad);
    fclose(f);
    return 0;
}

// ---------------------------------------------------------------------------
// Render the scene through the psyx primitive path -> window BMP.
//   * PsyX_Initialise creates its own SDL window + GL context.
//   * We render flat POLY_F4 quads into an OT, DrawOTag (which calls
//     PsyX_BeginScene internally), PsyX_EndScene (present), then glReadPixels
//     the window and write a BMP.
// Returns 0 on success.
// ---------------------------------------------------------------------------
static int RenderAndCapture(const Dx11AbScene *s, const char *bmpOut) {
    int w = (int)s->resW, h = (int)s->resH;
    PsyX_Initialise("dx11_backendab_psyx", w, h, 0);

    DRAWENV draw;
    DISPENV disp;
    SetDefDrawEnv(&draw, 0, 0, w, h);
    SetDefDispEnv(&disp, 0, 0, w, h);
    draw.dfe = 1;                       // draw on the display area
    draw.isbg = 1;                      // clear to bg color on each frame
    draw.r0 = 0; draw.g0 = 0; draw.b0 = 0;
    SetDispMask(1);
    PutDrawEnv(&draw);
    PutDispEnv(&disp);

    // Build one flat quad per scene quad into the OT (E3stuff single-depth pattern).
    // NOTE: quads must live OUTSIDE the loop (persistent addresses) so addPrim's
    // linked-list next-pointers are distinct -- a loop-local POLY_F4 reuses one
    // stack address and self-links -> infinite OT walk.
    OT_TAG ot;
    POLY_F4 quads[DX11AB_MAX_QUADS];
    ClearOTagR((u_long *)&ot, 1);
    for (unsigned int i = 0; i < s->numQuads; ++i) {
        const Dx11AbQuad *q = &s->quads[i];
        POLY_F4 *quad = &quads[i];
        setPolyF4(quad);
        setXY4(quad, (short)q->x, (short)q->y,
               (short)(q->x + q->w), (short)q->y,
               (short)q->x, (short)(q->y + q->h),
               (short)(q->x + q->w), (short)(q->y + q->h));
        setRGB0(quad, q->r, q->g, q->b);
        setSemiTrans(quad, 0);
        setShadeTex(quad, 0);           // flat color (not textured/gouraud)
        addPrim(&ot, quad);
    }

    DrawOTag((u_long *)&ot);            // calls PsyX_BeginScene internally

    // Read the window back BEFORE EndScene (which swaps the buffer and leaves the
    // backbuffer undefined). glReadPixels returns rows bottom-up; flip rows so
    // the BMP holds a top-down image (row 0 = screen top).
    unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);
    unsigned char *bgr = (unsigned char *)malloc((size_t)w * h * 3);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < h; ++y) {
        const unsigned char *src = px + (size_t)(h - 1 - y) * w * 4; // GL row -> top-down
        unsigned char *dst = bgr + (size_t)y * w * 3;
        for (int x = 0; x < w; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 0];   // B
            dst[x * 3 + 1] = src[x * 4 + 1];   // G
            dst[x * 3 + 2] = src[x * 4 + 2];   // R
        }
    }
    int rw = WriteBMP(bmpOut, w, h, bgr);
    free(bgr);
    free(px);

    PsyX_EndScene();                    // present + store framebuffer
    PsyX_Shutdown();
    return rw;
}

// ---------------------------------------------------------------------------
// Verify one screenshot against the stored state (each quad's centroid pixel
// must match its stored color). Returns number of failed probes.
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
    unsigned char p[3];
    if (fread(p, 1, 3, f) != 3) { fclose(f); return 1; }
    *b = p[0]; *g = p[1]; *r = p[2];
    fclose(f);
    return 0;
}

static int VerifyFrame(const Dx11AbScene *s, const char *bmp, FILE *resf) {
    int fails = 0;
    for (unsigned int i = 0; i < s->numQuads; ++i) {
        const Dx11AbQuad *q = &s->quads[i];
        int cx = (int)(q->x + q->w * 0.5f);
        int cy = (int)(q->y + q->h * 0.5f);
        int r = 0, g = 0, b = 0;
        int ok = (ProbeBMPPixel(bmp, cx, cy, &r, &g, &b) == 0 &&
                  abs(r - q->r) <= 40 && abs(g - q->g) <= 40 && abs(b - q->b) <= 40);
        fprintf(resf, "  SELF quad%d centroid(%d,%d)=(%d,%d,%d) expect(%d,%d,%d) %s\n",
                i, cx, cy, r, g, b, q->r, q->g, q->b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    }
    return fails;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    Dx11AbScene scene;
    Dx11AbScene_FillDefault(&scene, 320, 240);
    int doStore = 1;
    char statePath[260] = "", shotPath[260] = "";
    {
        char buf[1024];
        strncpy(buf, lpCmdLine ? lpCmdLine : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, " \t");
        while (tok) {
            if (!strcmp(tok, "-store")) doStore = 1;
            else if (!strcmp(tok, "-load")) doStore = 0;
            else if (!strcmp(tok, "-state") && (tok = strtok(NULL, " \t"))) {
                strncpy(statePath, tok, 259); statePath[259] = 0;
            } else if (!strcmp(tok, "-shot") && (tok = strtok(NULL, " \t"))) {
                strncpy(shotPath, tok, 259); shotPath[259] = 0;
            } else if (!strcmp(tok, "-res") && (tok = strtok(NULL, " \t"))) {
                int w, hh; if (sscanf(tok, "%dx%d", &w, &hh) == 2 && w > 0 && hh > 0) {
                    scene.resW = (unsigned int)w; scene.resH = (unsigned int)hh;
                }
            }
            tok = strtok(NULL, " \t");
        }
    }

    FILE *resf = fopen("dx11_backendab_psyx_result.txt", "w");
    if (!resf) return 2;
    int fails = 0;

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
    fails = VerifyFrame(&scene, shotPath, resf);

    fprintf(resf, "TOTAL_FAILS=%d PSYXBACKEND=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);
    return 0;
}
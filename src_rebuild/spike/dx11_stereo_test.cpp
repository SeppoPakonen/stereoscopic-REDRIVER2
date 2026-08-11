// dx11_stereo_test.cpp — T2.2 per-eye projection harness.
//
// Verifies headless the game-agnostic stereo camera math:
//   * EYE_OFF  — at yaw 0 the left offset is (-sep*2,0,0), right (+sep*2,0,0),
//                mono 0; at a non-zero yaw it follows right=(cos,0,sin)*gain.
//   * STABLE   — the left/right view matrices differ ONLY in the row-0
//                translation (the lateral axis), by exactly 4*separation; the
//                rotation part and the other translations are identical.
//   * SWAP     — swapping eyes flips the offset sign.
//   * SEP      — doubling the separation doubles the offset (proportional).
//   * CONV     — the convergence shear shifts the projection horizontally and
//                scales with the shift amount.

#include "dx11_stereo.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int Close(float a, float b, float tol) {
    float d = a - b;
    return d < tol && d > -tol;
}

static void MatPerspectiveRH(float fovY, float aspect, float zn, float zf, float m[4][4]) {
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fovY * 0.5f);
    m[0][0] = f / aspect;
    m[1][1] = f;
    m[2][2] = zf / (zn - zf);
    m[2][3] = zn * zf / (zn - zf);
    m[3][2] = -1.0f;
    m[3][3] = 1.0f;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    FILE *resf = fopen("dx11_stereo_result.txt", "w");
    if (!resf) return 2;
    int fails = 0;
    static const float tol = 1e-4f;

    // ------------------------------------------------------------------
    // EYE_OFF — yaw 0, sep 1.
    // ------------------------------------------------------------------
    float off[3];
    Dx11Stereo_EyeOffset(0.0f, DX11STEREO_EYE_MONO, 1.0f, 0, off);
    int okMono = (off[0] == 0 && off[1] == 0 && off[2] == 0);
    fprintf(resf, "EYE_OFF mono=(%.2f,%.2f,%.2f) zero %s\n", off[0], off[1], off[2], okMono ? "PASS" : "FAIL");
    if (!okMono) ++fails;

    Dx11Stereo_EyeOffset(0.0f, DX11STEREO_EYE_LEFT, 1.0f, 0, off);
    int okLeft = (Close(off[0], -2.0f, tol) && off[1] == 0 && off[2] == 0);
    fprintf(resf, "EYE_OFF left=(%.2f,%.2f,%.2f) expect(-2,0,0) %s\n", off[0], off[1], off[2], okLeft ? "PASS" : "FAIL");
    if (!okLeft) ++fails;

    Dx11Stereo_EyeOffset(0.0f, DX11STEREO_EYE_RIGHT, 1.0f, 0, off);
    int okRight = (Close(off[0], 2.0f, tol) && off[1] == 0 && off[2] == 0);
    fprintf(resf, "EYE_OFF right=(%.2f,%.2f,%.2f) expect(2,0,0) %s\n", off[0], off[1], off[2], okRight ? "PASS" : "FAIL");
    if (!okRight) ++fails;

    // EYE_OFF at a non-zero yaw -> offset follows right=(cos,0,sin)*gain.
    float yaw = 0.4f;
    Dx11Stereo_EyeOffset(yaw, DX11STEREO_EYE_LEFT, 1.0f, 0, off);
    float ex = -cosf(yaw) * 2.0f, ez = -sinf(yaw) * 2.0f;
    int okYaw = (Close(off[0], ex, tol) && Close(off[2], ez, tol) && off[1] == 0);
    fprintf(resf, "EYE_OFF yaw=0.4 left=(%.3f,%.3f) expect=(%.3f,%.3f) %s\n",
            off[0], off[2], ex, ez, okYaw ? "PASS" : "FAIL");
    if (!okYaw) ++fails;

    // ------------------------------------------------------------------
    // STABLE — left/right view matrices differ only in row-0 translation.
    // ------------------------------------------------------------------
    float cam[3] = { 100.0f, 50.0f, -200.0f };
    float mL[4][4], mR[4][4];
    Dx11Stereo_ViewMatrix(cam, yaw, DX11STEREO_EYE_LEFT,  1.0f, 0, mL);
    Dx11Stereo_ViewMatrix(cam, yaw, DX11STEREO_EYE_RIGHT, 1.0f, 0, mR);

    int rotSame = 1;
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (!Close(mL[r][c], mR[r][c], tol)) rotSame = 0;
    fprintf(resf, "STABLE rotation part identical %s\n", rotSame ? "PASS" : "FAIL");
    if (!rotSame) ++fails;

    // row-0 translation differs by exactly -4*sep (right minus left).
    float drow0 = mR[0][3] - mL[0][3];
    int okT0 = Close(drow0, -4.0f, tol);
    fprintf(resf, "STABLE row0 trans diff=%.3f expect -4.0 %s\n", drow0, okT0 ? "PASS" : "FAIL");
    if (!okT0) ++fails;

    // row-1 and row-2 translations identical (offset is purely lateral).
    int okT12 = Close(mL[1][3], mR[1][3], tol) && Close(mL[2][3], mR[2][3], tol);
    fprintf(resf, "STABLE row1/row2 trans identical %s\n", okT12 ? "PASS" : "FAIL");
    if (!okT12) ++fails;

    // ------------------------------------------------------------------
    // SWAP — flipping the eye swap sign flips the offset.
    // ------------------------------------------------------------------
    float offNorm[3], offSwap[3];
    Dx11Stereo_EyeOffset(0.0f, DX11STEREO_EYE_LEFT, 1.0f, 0, offNorm);
    Dx11Stereo_EyeOffset(0.0f, DX11STEREO_EYE_LEFT, 1.0f, 1, offSwap);
    int okSwap = Close(offSwap[0], -offNorm[0], tol) && Close(offSwap[2], -offNorm[2], tol);
    fprintf(resf, "SWAP left offset flips (%.2f -> %.2f) %s\n", offNorm[0], offSwap[0], okSwap ? "PASS" : "FAIL");
    if (!okSwap) ++fails;

    // ------------------------------------------------------------------
    // SEP — doubling separation doubles the offset.
    // ------------------------------------------------------------------
    float offS1[3], offS2[3];
    Dx11Stereo_EyeOffset(0.0f, DX11STEREO_EYE_LEFT, 1.0f, 0, offS1);
    Dx11Stereo_EyeOffset(0.0f, DX11STEREO_EYE_LEFT, 2.0f, 0, offS2);
    int okSep = Close(offS2[0], 2.0f * offS1[0], tol);
    fprintf(resf, "SEP offset(sep=2)=%.2f == 2*offset(sep=1)=%.2f %s\n",
            offS2[0], 2.0f * offS1[0], okSep ? "PASS" : "FAIL");
    if (!okSep) ++fails;

    // ------------------------------------------------------------------
    // CONV — convergence shear shifts the projection horizontally.
    // ------------------------------------------------------------------
    float proj0[4][4], proj1[4][4], proj2[4][4];
    MatPerspectiveRH(60.0f * (float)M_PI / 180.0f, 4.0f / 3.0f, 0.1f, 100.0f, proj0);
    memcpy(proj1, proj0, sizeof(proj0));
    memcpy(proj2, proj0, sizeof(proj0));
    Dx11Stereo_ApplyConvergence(proj1, 0.1f);
    Dx11Stereo_ApplyConvergence(proj2, 0.2f);

    // proj1 differs from proj0 only in row 0, by 0.1*row3.
    int convOK = 1;
    for (int i = 0; i < 4; ++i) {
        if (!Close(proj1[0][i], proj0[0][i] + 0.1f * proj0[3][i], tol)) convOK = 0;
        for (int r = 1; r < 4; ++r)
            if (!Close(proj1[r][i], proj0[r][i], tol)) convOK = 0;
    }
    fprintf(resf, "CONV shear row0 += shift*row3 %s\n", convOK ? "PASS" : "FAIL");
    if (!convOK) ++fails;

    // proportional: shift 0.2 -> 2x the change of 0.1 (row0 col2, e.g.).
    float c1 = proj1[0][2] - proj0[0][2];
    float c2 = proj2[0][2] - proj0[0][2];
    int okProp = Close(c2, 2.0f * c1, tol);
    fprintf(resf, "CONV proportional (0.2 = %.4f, 0.1 = %.4f) %s\n", c2, c1, okProp ? "PASS" : "FAIL");
    if (!okProp) ++fails;

    fprintf(resf, "TOTAL_FAILS=%d STEREO=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);
    return 0;
}
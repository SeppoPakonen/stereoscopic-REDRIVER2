// dx11_stereo.c — T2.2 per-eye projection implementation.
//
// See dx11_stereo.h. The eye offset reuses the legacy math verbatim; the view
// matrix is built from the yaw basis with the offset folded into the camera
// position; convergence is a horizontal projection shear.

#include "dx11_stereo.h"

#include <math.h>
#include <string.h>

void Dx11Stereo_EyeOffset(float yawRad, Dx11StereoEye eye, float separation,
                          int swap, float out[3])
{
    out[0] = out[1] = out[2] = 0.0f;
    if (eye == DX11STEREO_EYE_MONO)
        return;

    if (swap)
        eye = (eye == DX11STEREO_EYE_LEFT) ? DX11STEREO_EYE_RIGHT : DX11STEREO_EYE_LEFT;

    float cx = cosf(yawRad), sz = sinf(yawRad);
    float gain = separation * 2.0f;                 // world units (sep*2)
    float sign = (eye == DX11STEREO_EYE_LEFT) ? -1.0f : 1.0f;

    out[0] = sign * cx * gain;
    out[1] = 0.0f;                                  // lateral (horizontal) only
    out[2] = sign * sz * gain;
}

void Dx11Stereo_ViewMatrix(const float camPos[3], float yawRad, Dx11StereoEye eye,
                           float separation, int swap, float mat[4][4])
{
    memset(mat, 0, 16 * sizeof(float));

    float off[3];
    Dx11Stereo_EyeOffset(yawRad, eye, separation, swap, off);
    float eyePos[3] = { camPos[0] + off[0], camPos[1] + off[1], camPos[2] + off[2] };

    // Camera basis from yaw: right = (cos,0,sin), up = (0,1,0),
    // forward = (sin,0,-cos); view z axis = -forward.
    float right[3] = { cosf(yawRad), 0.0f, sinf(yawRad) };
    float up[3]    = { 0.0f, 1.0f, 0.0f };
    float negFwd[3]= { -sinf(yawRad), 0.0f, cosf(yawRad) };

    // Output V^t (transpose of the standard column-vector view matrix V), so
    // that viewProj = view * proj composes to (P*V)^t under the DX11 pipeline
    // convention (C++ row-major storage, HLSL mul reads the matrix transposed,
    // effective = mat*p; proj is stored as P^t). Columns hold the basis:
    //   col0 = right, col1 = up, col2 = -forward,
    // and the translation -R*eyePos lives in the last ROW.
    mat[0][0] = right[0]; mat[0][1] = up[0];    mat[0][2] = negFwd[0];
    mat[1][0] = right[1]; mat[1][1] = up[1];    mat[1][2] = negFwd[1];
    mat[2][0] = right[2]; mat[2][1] = up[2];    mat[2][2] = negFwd[2];
    mat[3][0] = -(right[0]*eyePos[0] + right[1]*eyePos[1] + right[2]*eyePos[2]);
    mat[3][1] = -(up[0]*eyePos[0] + up[1]*eyePos[1] + up[2]*eyePos[2]);
    mat[3][2] = -(negFwd[0]*eyePos[0] + negFwd[1]*eyePos[1] + negFwd[2]*eyePos[2]);
    mat[3][3] = 1.0f;
}

void Dx11Stereo_ApplyConvergence(float mat[4][4], float shift)
{
    for (int i = 0; i < 4; ++i)
        mat[0][i] += shift * mat[3][i];
}
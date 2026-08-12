// dx11_stereo.h — T2.2 per-eye projection (yaw-derived lateral offset).
//
// Game-agnostic stereo camera math for the DX11 backend. It reuses, verbatim,
// the per-eye lateral-offset logic from the game's StereoCamera_ApplyToRender:
//   right = (cos yaw, 0, sin yaw)          (camera yaw, radians)
//   gain  = separation * 2.0f              (world units)
//   left  -> -right * gain,  right -> +right * gain,  swap negates
// and builds a world->view matrix per eye (eye offset folded into the camera
// position), plus a projection convergence shear (the legacy renderer-side
// gStereoConvergence handling). Matrix convention: row-vector x column-major,
// result = m * p (m[j][i] = row j, col i).

#ifndef DX11_STEREO_H
#define DX11_STEREO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DX11STEREO_EYE_MONO = 0,
    DX11STEREO_EYE_LEFT,
    DX11STEREO_EYE_RIGHT,
} Dx11StereoEye;

// ---------------------------------------------------------------------------
// Eye offset
// ---------------------------------------------------------------------------
// Fills `out[3]` with the world-space lateral offset for an eye, replicating
// StereoCamera_ApplyToRender: right = (cos yaw, 0, sin yaw), gain = sep*2,
// left -> -right*gain, right -> +right*gain; `swap` flips the sign; MONO -> 0.
void Dx11Stereo_EyeOffset(float yawRad, Dx11StereoEye eye, float separation,
                          int swap, float out[3]);

// ---------------------------------------------------------------------------
// Per-eye world->view matrix
// ---------------------------------------------------------------------------
// Builds the world->view matrix for an eye. The camera looks along
// forward = (sin yaw, 0, -cos yaw), up = +Y, right = (cos yaw, 0, sin yaw);
// the eye's world position is camPos + EyeOffset. Returns V^t (the transpose
// of the standard column-vector view matrix V): columns hold the right / up /
// -forward basis and the translation -R*eyePos lives in the last row. This is
// the storage the DX11 pipeline needs so that viewProj = view * proj composes
// to (P*V)^t under the row-vector/column-major convention (proj is stored as
// P^t). `mat` is a 4x4 in row-major storage.
void Dx11Stereo_ViewMatrix(const float camPos[3], float yawRad, Dx11StereoEye eye,
                           float separation, int swap, float mat[4][4]);

// Builds the world->view matrix for an eye from an explicit orthonormal camera
// basis (right / up / -forward as world axes) instead of deriving it from yaw.
// This lets the game feed the full camera orientation (yaw + pitch + roll) via
// its own inv_camera_matrix rows, matching the GTE view the plot functions use.
// The per-eye world position is camPos + off where off is the lateral offset
// ALONG `right` (gain = sep*2, left -> -right*gain, right -> +right*gain,
// swap negates); MONO / sep=0 -> no offset. Output is the same V^t storage as
// Dx11Stereo_ViewMatrix. The basis is NOT normalized here (the caller passes
// unit vectors).
void Dx11Stereo_ViewMatrixBasis(const float camPos[3],
                                const float right[3], const float up[3],
                                const float negFwd[3], Dx11StereoEye eye,
                                float separation, int swap, float mat[4][4]);

// ---------------------------------------------------------------------------
// Convergence (projection shear)
// ---------------------------------------------------------------------------
// Shears a perspective projection in place by a horizontal screen offset
// (convergence): for each column i, mat[0][i] += shift * mat[3][i], which adds
// `shift * clip.w` to clip.x (a horizontal NDC offset). `shift` is in NDC
// units (e.g. -0.1 .. 0.1). This mirrors the legacy renderer-side convergence.
void Dx11Stereo_ApplyConvergence(float mat[4][4], float shift);

#ifdef __cplusplus
}
#endif

#endif // DX11_STEREO_H
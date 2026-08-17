# 004 — Test Camera Setup

## Goal
Set up a simple test camera for rendering the test cube (bypass normal game camera/level system).

## Scope
- Set camera position (e.g., (0, 0, -500) looking at origin)
- Set camera orientation (identity rotation, looking down +Z axis)
- Bypass normal game camera system (InitCamera, Set_Inv_CameraMatrix, etc.)
- Use fixed camera for test rendering

## Camera Parameters
- Position: (0, 0, -500) — 500 units behind origin, looking at cube at (0,0,0)
- Orientation: identity rotation (looking down +Z axis in game coordinates)
- FOV: use default FrAng (or set to 60° vertical FOV)

## Implementation
In `RenderGame2()` (main.c), when renderer is soft (or dx11 for test):
```c
// Bypass normal camera setup for test mode.
if (gTestMode) {
    // Set camera position.
    camera_position.vx = 0;
    camera_position.vy = 0;
    camera_position.vz = -500;

    // Set inv_camera_matrix to identity (looking down +Z).
    memset(&inv_camera_matrix, 0, sizeof(MATRIX));
    inv_camera_matrix.m[0][0] = 4096; // identity (1.0 in fixed-point)
    inv_camera_matrix.m[1][1] = 4096;
    inv_camera_matrix.m[2][2] = 4096;

    // Set camera vector (not used for test, but set to zero).
    memset(&camera_vector, 0, sizeof(VECTOR));
}
```

## Acceptance Criteria
- [ ] Camera positioned at (0, 0, -500)
- [ ] Camera looking at origin (cube at (0,0,0) is centered in view)
- [ ] Bypass normal camera system (no InitCamera, Set_Inv_CameraMatrix calls in test mode)
- [ ] Visual test: cube appears centered in both psyx and soft renderer windows

## Notes
- Test mode is enabled by setting `gTestMode = 1` (global flag, set via command-line arg `-test` or `-renderer soft`).
- Normal game camera system is bypassed only in test mode. Normal gameplay uses InitCamera, Set_Inv_CameraMatrix, etc.
- Camera position (0, 0, -500) is in game units. Cube is at (0,0,0) with size 100 units (from 002-model-builder scale=100). So cube is 500 units away, appears small in view.
- For soft renderer test, camera parameters are also passed to SoftGame_RenderFrame() (already implemented in 005-cube-render-integration).

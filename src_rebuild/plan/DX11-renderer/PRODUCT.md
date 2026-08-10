# Product — DX11 Renderer (multi-renderer)

This folder is a plan for a new DirectX 11 renderer. No DX11 code exists yet;
PRODUCT.md records the existing reusable components the effort should build on
(and avoid re-writing).

## Reusable existing components

### Stereo camera
- **`StereoCamera_ApplyToRender(eye)`** — `Game/render/stereo.c`. Applies the
  per-eye lateral offset. The offset axis is computed from the camera yaw:
  `right = (cos θ, 0, sin θ)`, left eye `-right`, right eye `+right`. Reuse this
  math for the DX11 per-eye projection.
- **`StereoCamera_Update(eye)`** — `Game/render/stereo.c`. Selects the eye only
  (no offset; the offset is applied in `ApplyToRender` after `InitCamera`).
- **`gStereoMode`, `gStereoSeparation`, `gStereoConvergence`, `gStereoSwapEyes`,
  `gCurrentStereoEye`** — `Game/render/stereo.h`. Stereo state.

### Iteration / test harness
- **`StereoLog_Open/Write/Close`** + `gStereoIterLogEnabled` — `Game/render/stereo.c/.h`.
  Opt-in file log (`-iterlog`) for headless verification.
- **Command-line flags** — `Game/C/main.c`: `-mission`, `-playercar`, `-stereo`,
  `-stereosep`, `-stereoconv`, `-swap`, `-stereodebug`, `-iterlog`, `-exitafter`.
- **Build scripts** — `build_debug_mingw32.sh`, `build_release_mingw32.sh`
  (mingw32 32-bit; release uses the Release_dev config).

### Per-eye offscreen (existing GL path to be replaced)
- **`GR_SetOffscreenState`** + `g_offscreenEye` + `g_offscreenRTTexture/2` —
  `PsyCross/src/render/PsyX_render.cpp`. Offscreen render targets (320x240).
- **`GR_ResetOffscreenSize()`** — resets the shared offscreen-size tracker so
  each eye resizes its own texture.
- **`StereoCompositor_Composite(mode)`** — `Game/render/stereo_compositor.c`.
  Blit-based SBS/TB composite (left/right, top/bottom) from the two eye textures.

### Game rendering (to be consumed by the DX11 backend)
- **`RenderGame2(view)`** — `Game/C/main.c`. Queues the scene into the OT.
- **OT / primitive buffer** — `Game/C/system.c` (`MPBuff[2][2]`, `_OT1/_OT2`,
  `_primTab1/_primTab2`, `ClearOTagR`, `addPrim`).
- **Camera** — `Game/C/camera.c` (`InitCamera`, `PlaceCameraFollowCar`,
  `ModifyCamera`), `camera_angle`, `camera_position`, `camera_matrix`.
- **`DrawOTag`** — `PsyCross/src/psx/LIBGPU.C` (the rasterization to replace).

## Design notes for the new renderer
- Keep the game's queueing logic; replace only the rasterization layer.
- The DX11 backend should consume the OT/primitive stream directly and render
  per-eye into configurable-resolution render targets.
- Reuse the yaw-derived lateral eye offset for per-eye projection.
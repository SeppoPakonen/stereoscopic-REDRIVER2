# Handoff — DX11 Renderer (multi-renderer)

## Current status (as of the end of the stereo session)

The stereoscopic SBS rendering WORKS end-to-end, and the game runs at full
speed. The remaining issue (right-eye map disappearing) motivated the decision
to move to a new renderer.

## What is done / working

- **SBS stereo rendering** at 320x240 offscreen: left eye -> offscreen texture,
  right eye -> offscreen texture2, blit composite to left/right screen halves.
- **Per-eye OT buffers** (two separate OT + primitive tables per frame).
- **Eye offset is lateral and stable**: computed from the camera yaw
  `right = (cos θ, 0, sin θ)`, left eye -right, right eye +right. The camera is
  confirmed stable between the two eyes (same angle, positions differ only by
  the 2-unit lateral offset).
- **Game runs at correct speed**: render-loop debug prints removed (gated behind
  `gStereoDebugLog`; `gStereoDebugLog` is no longer loaded from the profile —
  only `-stereodebug` sets it). Iteration log is opt-in via `-iterlog`.
- **Release build** works: `build_release_mingw32.sh` builds the Release_dev
  config (NDEBUG + DEBUG_OPTIONS). `platform/Windows/Resource.rc` was converted
  to UTF-8 so windres works.
- **Command-line flags** for headless iteration: `-mission N`, `-playercar N`,
  `-stereo <mode>`, `-stereosep`, `-stereoconv`, `-swap`, `-stereodebug`,
  `-iterlog`, `-exitafter <sec>`.
- **Iteration harness**: mingw32 32-bit debug + release builds, gdb, and the
  `stereo_iter.log` (opt-in) for headless verification.

## Known issues / what is NOT solved

- **Right eye's map/terrain disappears** (sometimes all polys except the player
  car + ground decals, sometimes only far ones). Per-eye prim counts are EQUAL
  and the camera is correct, so the bug is downstream in the **offscreen
  rasterization (`DrawOTag`) or the composite blit** for the right eye's
  texture. NOT a queueing/culling/camera issue.
- **Higher-res offscreen** breaks the PSX projection (perspective centre
  top-left, skybox wrong). Not solved without rescaling the projection.
- **Color modes** (anaglyph, interlaced, polarized, checkerboard) are not
  implemented (the shader compositor was disabled — it crashed the GL driver).
- **Split-screen** (2 players x 2 eyes = 4 images) is not implemented.
- The camera can drift to the side over time (user reported it may be
  controller-related, unresolved).

## Advice for the DX11 renderer effort

- Design the renderer to render the scene **directly per-eye** with proper
  framebuffers/projections, so the 320x240 lock and the deferred-OT FBO
  fragility disappear.
- Make the internal resolution configurable (not tied to PSX 320x240) so SBS
  can be sharp.
- Keep the game's SIMULATION/queueing logic; only replace the rasterization
  layer (`DrawOTag` + PsyX renderer) with a DX11 backend that understands the
  OT/primitive stream.
- Reuse the existing camera -> eye-offset math (yaw-derived lateral axis).
- Preserve the headless iteration harness (mingw32 build + gdb + opt-in log).

## Build / run commands

```
cd src_rebuild
bash build_release_mingw32.sh          # release (Release_dev) build
bash build_debug_mingw32.sh            # debug build
# run (in the installed data dir):
REDRIVER2_rel.exe -nointro -nofmv -mission 50 -playercar 0 -stereo 3 -iterlog -exitafter 15
```
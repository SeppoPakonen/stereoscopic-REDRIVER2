# Context — DX11 Renderer (multi-renderer)

## Why a new renderer

The stereoscopic (SBS) work on REDRIVER2 (Driver 2) reached a point where the
existing OpenGL/PsyX pipeline is the wrong foundation. The stereo rendering
works (two eye images side by side at 320x240), but the per-eye rendering is
fighting the deferred OT (ordering-table) renderer and the PSX-fixed 320x240
projection:

- **Offscreen is locked to 320x240 (PSX native res).** The PSX projection maps
  the scene into the 320x240 space, so rendering the offscreen at the window
  size (1280x720) breaks the 3D projection (perspective centre lands top-left,
  skybox/`Projection3D` renders wrong). Higher-res requires rescaling the
  projection/viewport, which is surgery on the PSX math.
- **The right eye's map/terrain disappears** (sometimes all polygons except the
  player car + ground decals, sometimes only far ones). Per-eye prim counts are
  EQUAL and the camera is confirmed correct/stable, so the bug is downstream in
  the **offscreen rasterization (`DrawOTag`) or the composite blit** for the
  right eye's texture. This is hard to fix because rendering is split between
  the game's OT queueing (RenderGame2) and PsyX's DrawOTag rasterization.
- **The deferred OT renderer fights per-eye work.** `RenderGame2()` only QUEUES
  primitives into `current->ot`; rasterization happens later in `DrawOTag`.
  FBOs must be bound during `DrawOTag`, not `RenderGame2`. This makes per-eye
  offscreen rendering fragile.

A modern renderer (DirectX 11) that renders the scene directly per-eye with
proper framebuffers/projections would solve all of this cleanly and also enable
the higher-res, color-mode (anaglyph/interlaced/etc.) and split-screen features
that are impractical in the current pipeline.

## Current architecture (what we have)

- **Game code** (`src_rebuild/Game/`): PS1-style deferred renderer. `RenderGame2`
  queues primitives into an OT (ordering table) + primitive buffer per frame.
  `DrawOTag` (in PsyCross) rasterizes them.
- **PsyX / PsyCross** (`src_rebuild/PsyCross/`): the OpenGL renderer underneath.
  `DrawOTag` -> `ParsePrimitivesLinkedList` -> `DrawSplit` -> GPU. Offscreen
  render targets via `GR_SetOffscreenState` (single offscreen texture + a second
  one added for stereo).
- **Stereo** (`src_rebuild/Game/render/stereo.c`, `stereo_compositor.c`):
  per-eye OT buffers, per-eye offscreen textures, blit composite.
- **Camera**: PSX camera matrix (`inv_camera_matrix`/`camera_matrix`) from
  `camera_angle`. Eye offset computed from the camera yaw (lateral axis).

## Key constraints discovered

- Offscreen must stay 320x240 unless the projection is rescaled.
- Two eyes share `g_PreviousOffscreen` in PsyCross (fixed with `GR_ResetOffscreenSize`).
- `camera_matrix` column 0 is the view (forward) axis and is distorted by the
  aspect matrix — the eye offset must use the yaw-derived lateral axis.
- The skybox uses `Projection3D`, which is only set up for the window, not the
  offscreen.
- The game's bake/assemble pipeline (jpeg, PsyCross, REDRIVER2) builds with
  mingw32 32-bit (debug + Release_dev) via `build_debug_mingw32.sh` and
  `build_release_mingw32.sh`.
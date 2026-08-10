# Tasks — DX11 Renderer (multi-renderer)

Goal: replace the OpenGL/PsyX rasterization layer with a DirectX 11 backend that
renders the game's OT/primitive stream **directly per-eye**, enabling correct
per-eye stereo, higher internal resolution, color modes, and split-screen.

## Phase 0 — Architecture & spike

- [ ] **T0.1** Map the game's primitive/OT stream: what `RenderGame2` queues into
      `current->ot` / `primtab`, and the exact set of PSX primitive types (sprite,
      poly, line, tile, etc.) PsyX's `DrawOTag` rasterizes.
- [ ] **T0.2** Decide the integration boundary: a DX11 backend that converts the
      OT/primitive stream into DX11 draw calls, replacing `DrawOTag` + PsyX GPU.
- [ ] **T0.3** Spike: a bare DX11 window that renders a single test primitive from
      one OT, proving the pipeline before the full converter.
- [ ] **T0.4** Define the internal-resolution model (configurable, not tied to
      320x240) and how the projection matrix is built per viewport.

## Phase 1 — Core DX11 renderer

- [ ] **T1.1** DX11 device/context/swapchain setup (replacing SDL2-GL window init).
- [ ] **T1.2** Vertex/texture/constant-buffer management.
- [ ] **T1.3** ranslate the PSX VRAM/tpage/clut texture model to DX11 textures.
- [ ] **T1.4** Render the map/terrain and static geometry from the OT stream.
- [ ] **T1.5** Preserve render state (depth, blending, alpha, draw modes).
- [ ] **T1.6** Match the current visual output (side-by-side at whatever internal
      resolution) so behavior is verified against the GL version.

## Phase 2 — Stereo natively

- [ ] **T2.1** Render each eye to its own DX11 render target at the configured
      internal resolution (no 320x240 lock).
- [ ] **T2.2** Per-eye projection using the yaw-derived lateral offset (reuse the
      existing `StereoCamera_ApplyToRender` math).
- [ ] **T2.3** Composite SBS/TB in the renderer (left/right halves, top/bottom).
- [ ] **T2.4** Verify the right-eye map issue is gone (it was downstream in the GL
      offscreen rasterization).

## Phase 3 — Color modes & split-screen

- [ ] **T3.1** Shader-based composite: anaglyph (simple + full-color), interlaced,
      polarized, checkerboard.
- [ ] **T3.2** Split-screen: 2 players x 2 eyes = 4 images, user-selectable
      h/v split.
- [ ] **T3.3** Higher internal resolution option (config).

## Phase 4 — Integration & cleanup

- [ ] **T4.1** Keep the mingw32 build + headless iteration harness working.
- [ ] **T4.2** Remove/disable the old GL/PsyX rasterization path when the DX11
      backend is complete.
- [ ] **T4.3** Regression-test the game non-stereo path (the DX11 renderer must
      also render the base game correctly).

## Out of scope (for now)

- Full PGXP-quality 3D reconstruction / texture perspective correction.
- Post-processing / anti-aliasing beyond what the modes need.
- Platform targets beyond Windows (DirectX 11).
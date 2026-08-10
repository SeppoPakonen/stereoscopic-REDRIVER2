# Tasks — renderer rewrite (multi-renderer, DX11 default)

Goal: replace the PSX primitive renderer with a **standard DirectX 11 renderer
stack** fed by a high-level **draw-command list**, so per-eye stereo, higher
internal resolution, color modes and split-screen become clean features — while
the legacy PsyX/PsyCross GL renderer stays selectable via `-renderer psyx`.

Renderer selection: `-renderer dx11` (DEFAULT) / `-renderer psyx` (legacy) /
(planned) `-renderer gl`. The DX11 path never uses the OT/primitive stream.

> **Task files:** each task below has its own `T<phase>.<n>-<title>.md` file
> (linked) that must be written/updated **before** the task is implemented.
> Each task file declares its Phase and Milestone in the frontmatter. Template:
> `_task-template.md`.
>
> Milestones: **M0** = Phase 0, **M1** = Phase 1, **M2** = Phase 2,
> **M3** = Phase 3, **M4** = Phase 4.

## Phase 0 — Architecture & spike (M0)

- [x] **T0.1** Renderer registry + `-renderer` flag — [T0.1](T0.1-renderer-registry-and-flag.md)
- [x] **T0.2** Draw-command list model — [T0.2](T0.2-draw-command-list-model.md)
- [x] **T0.3** Audit plot functions — [T0.3](T0.3-audit-plot-functions.md)
- [x] **T0.4** Internal resolution & projection model — [T0.4](T0.4-internal-resolution-and-projection.md)
- [x] **T0.5** DX11 spike: bare window + one meshed quad — [T0.5](T0.5-dx11-spike-bare-window.md)

## Phase 1 — Core DX11 renderer (M1)

- [x] **T1.1** DX11 device/context/swapchain + render targets — [T1.1](T1.1-dx11-device-context-swapchain.md)
- [ ] **T1.2** Resource management (VB/IB, CBs, SRVs, arena) — [T1.2](T1.2-resource-management.md)
- [ ] **T1.3** Texture system: VRAM/tpage/clut → DX11 SRVs — [T1.3](T1.3-texture-system.md)
- [ ] **T1.4** Shaders + render state — [T1.4](T1.4-shaders-and-render-state.md)
- [ ] **T1.5** Draw-command execution (culling, sort, batch) — [T1.5](T1.5-draw-command-execution.md)
- [ ] **T1.6** Render map/terrain + match visual output — [T1.6](T1.6-render-map-and-match-output.md)
- [ ] **T1.7** DX11 platform & input (DirectX window + DirectInput8) — [T1.7](T1.7-dx11-platform-input-directinput.md)
- [ ] **T1.8** DX11 audio (XAudio2) — [T1.8](T1.8-dx11-audio-xaudio2.md)

## Phase 2 — Stereo natively (M2)

- [ ] **T2.1** Per-eye render targets — [T2.1](T2.1-per-eye-render-targets.md)
- [ ] **T2.2** Per-eye projection (yaw-derived lateral offset) — [T2.2](T2.2-per-eye-projection.md)
- [ ] **T2.3** SBS/TB composite in the renderer — [T2.3](T2.3-sbs-tb-composite.md)
- [ ] **T2.4** Verify right-eye map issue is gone — [T2.4](T2.4-verify-right-eye-issue-gone.md)

## Phase 3 — Color modes & split-screen (M3)

- [ ] **T3.1** Shader-based composite color modes — [T3.1](T3.1-shader-composite-color-modes.md)
- [ ] **T3.2** Split-screen — [T3.2](T3.2-split-screen.md)
- [ ] **T3.3** Higher internal resolution option — [T3.3](T3.3-higher-internal-resolution-option.md)

## Phase 4 — Integration & cleanup (M4)

- [ ] **T4.1** mingw32 build + headless harness with `-renderer` — [T4.1](T4.1-build-and-headless-harness.md)
- [ ] **T4.2** Keep legacy `psyx` path selectable — [T4.2](T4.2-keep-psyx-selectable.md)
- [ ] **T4.3** Non-stereo regression — [T4.3](T4.3-non-stereo-regression.md)
- [ ] **T4.4** (stretch) Modern GL backend mirroring DX11 — [T4.4](T4.4-modern-gl-backend.md)

## Out of scope (for now)

- Full PGXP-quality 3D reconstruction / texture perspective correction.
- Post-processing / anti-aliasing beyond what the modes need.
- Removing the legacy psyx path (it stays as a selectable backend).
- Platform targets beyond Windows (DirectX 11).
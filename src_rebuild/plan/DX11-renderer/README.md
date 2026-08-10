# DX11 Renderer — Multi-Renderer Plan

Plan for replacing the OpenGL/PsyX rasterization layer with a DirectX 11 backend
so the game can render per-eye correctly, at higher internal resolution, with
color modes and split-screen.

## Documents

- **[CONTEXT.md](CONTEXT.md)** — why a new renderer; the current OpenGL/PsyX
  architecture and the constraints/issues that motivated it.
- **[HANDOFF.md](HANDOFF.md)** — current status, what is done/working, known
  issues, and advice for the DX11 effort.
- **[TASKS.md](TASKS.md)** — phased task breakdown (architecture spike, core
  renderer, native stereo, color modes, integration).
- **[PRODUCT.md](PRODUCT.md)** — existing reusable components (stereo camera,
  iteration harness, per-eye offscreen, game queueing) to build on.

## TL;DR

The stereo SBS rendering works, but per-eye rendering is fighting the PSX-fixed
320x240 projection and the deferred OT renderer. A modern DX11 renderer that
draws the scene directly per-eye with proper framebuffers/projections solves
the remaining issues (right-eye map disappearing, low internal resolution,
color modes, split-screen) cleanly.
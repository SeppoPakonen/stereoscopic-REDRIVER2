# Renderer rewrite — standard DX11 stack + selectable legacy backends

Plan for replacing the PSX primitive renderer with a **standard DirectX 11
renderer stack**, fed by a high-level **draw-command list** instead of the
PSX ordering-table (OT) / primitive stream. Renderers stay selectable at
runtime via a command-line flag; DX11 is the default.

## Documents

- **[CONTEXT.md](CONTEXT.md)** — why the PSX primitive stream is cut; the
  multi-renderer architecture (renderer abstraction, draw-command list feed).
- **[HANDOFF.md](HANDOFF.md)** — current status, what stays / what is cut, and
  advice for the DX11 effort.
- **[TASKS.md](TASKS.md)** — phased task breakdown (renderer abstraction,
  standard DX11 stack, native stereo, color modes, integration). Each task has
  its own `T<phase>.<n>-<title>.md` task file (see below).
- **[PRODUCT.md](PRODUCT.md)** — existing reusable components (simulation state,
  MODEL meshes, camera, textures, stereo math, iteration harness) and the
  renderer registry.

## Task files

Each task in `TASKS.md` has its own task file `T<phase>.<n>-<title>.md`, written
**before** the task is implemented. Every task file:
- declares its **Phase** and **Milestone** (M0–M4) in the frontmatter, and
- follows `_task-template.md` (Goal / Context / Steps / Acceptance criteria /
  References).

Work a task by flipping its frontmatter `status` to `in_progress`, ticking
steps, and marking it `done` when its acceptance criteria pass.

## Architecture (what this plan is)

```
STATE_GameLoop()  --simulation-->  render path
                                     |
        +----------------------------+----------------------------+
        |  -renderer psyx (legacy)   |  -renderer dx11 (DEFAULT)  |
        |  GTE -> OT -> DrawOTag     |  plot fns build world-space |
        |  -> PsyCross/GL            |  DRAW-COMMAND LIST -> DX11  |
        +----------------------------+----------------------------+
```

- **DX11 backend (default, `-renderer dx11`):** a standard DX11 stack
  (device/swapchain, VB/IB, SRVs/textures, shaders, constant buffers, render
  targets) on a **native Windows/DirectX platform** — Win32 + DXGI window,
  DirectInput8 input, XAudio2 audio (SDL2 is not used on this path). It
  consumes a **draw-command list** — per-frame high-level draw
  commands (`mesh + world transform + material + sort key`) produced by the
  game's plot functions. The OT/primitive stream is **never** used by this
  path. The renderer owns projection, frustum culling, sorting, batching, RT
  selection and compositing — so per-eye stereo, higher internal resolution,
  color modes and split-screen become clean features.
- **PsyX/PsyCross GL backend (selectable, `-renderer psyx`):** the existing
  legacy path (GTE transform -> OT -> `DrawOTag` -> PsyCross GL) kept intact as
  a compatibility/fallback reference. This is the only place the PSX primitive
  stream survives.
- **(future) OpenGL backend (`-renderer gl`):** a modern OpenGL stack that
  mirrors the DX11 architecture (draw-command list, same renderer-owned
  features), so GL is not tied to the PSX primitive model.

A **renderer abstraction / registry** selects the active backend at startup
from `-renderer <name>`.

## TL;DR

Stereo SBS works today only through the fragile PSX 320x240 offscreen path. A
standard DX11 renderer that draws the scene directly from a world-space
draw-command list removes the OT/primitive stream, the 320x240 projection lock
and the deferred-OT FBO fragility — and enables sharp per-eye stereo, color
modes and split-screen. The legacy PsyX renderer stays selectable while the
DX11 renderer becomes the default.
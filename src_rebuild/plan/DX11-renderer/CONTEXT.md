# Context — renderer rewrite (multi-renderer, DX11 default)

## Why the PSX primitive renderer is cut

The stereoscopic (SBS) work on REDRIVER2 (Driver 2) reached the point where the
existing PSX/PsyX pipeline is the wrong foundation. The stereo rendering works
(two eye images side by side at 320x240), but per-eye rendering fights the
deferred OT renderer and the PSX-fixed 320x240 projection.

The owner's decision for the rewrite is **maximal cut of the original renderer**:

- **Do not leave anything of the OT/primitive stream** for the primary renderer.
  The DX11 renderer does **not** consume PSX primitives.
- **Build a standard DX11 renderer stack** (device/swapchain, VB/IB, textures,
  shaders, constant buffers, render targets, compositing).
- **Change the rest (other than the renderer) to serve that standard stack** —
  the game's plot functions feed the renderer a high-level **draw-command list**
  (world-space `mesh + transform + material + sort key`), not PSX primitives.
- **Keep renderers selectable** via `-renderer <name>`: `dx11` is the default;
  `psyx` (the legacy PsyX/PsyCross GL path) stays selectable; a modern `gl`
  backend that mirrors the DX11 architecture is planned later.

## The load-bearing fact: the OT stream is screen-space, not world-space

`RenderModel` / `DrawCar` / `DrawTILES` / `DrawSprites` (and every `*_plot*`
in `draw.c`, `tile.c`, `cars.c`) currently:

1. fetch the world-space `MODEL` mesh + placement transform + camera,
2. run the **GTE transform** (`gte_rtpt`) to **screen-space 320x240** coords,
3. backface-cull and depth-sort via `addPrim(current->ot + (Z >> 1), prim)`.

So the OT streams **GTE-projected 2D polygons with UVs and tpage/clut** — the
Z is baked into the OT index, and `DrawOTag`/`ParsePrimitive` only rasterizes
these already-projected quads. A renderer built on the OT stream inherits the
PSX 320x240 projection and can never produce true arbitrary-resolution 3D.

**Consequence:** to cut the renderer and get a real DX11 3D pipeline, the
capture point must move **before** the GTE transform — at the plot-function
level, where world transforms, `MODEL` meshes, camera and textures are still
available. That is the source of the draw-command list.

## Multi-renderer architecture

A **renderer abstraction / registry** selects the active backend at startup:

```
State_GameLoop()  --StepGame() (simulation unchanged)-->  DrawGame()
                                                            |
                            +-------------------------------+-------------------------------+
                            |  -renderer psyx (legacy)      |  -renderer dx11 (DEFAULT)      |
                            |  plot fns GTE+addPrim -> OT   |  plot fns build draw-cmd list |
                            |  DrawOTag -> PsyCross/GL      |  -> DX11 renderer stack        |
                            +-------------------------------+-------------------------------+
```

- **Simulation** (car physics, AI, world state, camera update) is unchanged and
  backend-agnostic. It produces `CAR_DATA`, `modelpointers[]`, cell objects,
  `camera_*`, `texture_pages[]`/`texture_cluts[][]`.
- **Draw-command list (DX11 feed):** a per-frame list of high-level draw
  commands (mesh ref + world transform + material/state + sort key). The DX11
  renderer owns projection, frustum culling, sorting, batching, RT selection and
  compositing. The game may still do coarse **potential-visibility** culling
  (it owns the PVS/cell structure); the renderer does the rest.
- **PsyX backend:** the existing OT/primitive path is preserved behind
  `-renderer psyx` as the compatibility/fallback renderer. This is the **only**
  place the PSX primitive stream survives.

## Platform / window / input / audio (DX11 = native DirectX)

The DX11 backend is a **native Windows/DirectX stack**, not a SDL2-embedded one.
SDL2 today backs the PsyX/GL layer (window + GL context + input) and stays as
the option for the GL backends (`psyx`, and the later `gl`). The DX11 backend
replaces the SDL2 surface with native DirectX:

- **Window / swapchain** — Win32 (`CreateWindowEx`) + **DXGI swapchain**
  (`IDXGISwapChain`), replacing `SDL_CreateWindow` + `SDL_GL_SwapWindow`.
  (The GL path keeps its SDL2 window.)
- **Input** — **DirectInput8** (`IDirectInput8`) for keyboard, mouse and
  gamepad, replacing `SDL_PollEvent` / `SDL_GetKeyboardState` /
  `SDL_GameController*` and the `SDL_SCANCODE_*`/`SDL_CONTROLLER_*` mappings in
  `PsyX_main.cpp`.
- **Audio** — **XAudio2** for the DX11 path, replacing the OpenAL backend
  (`PsyCross/src/audio/PsyX_SPUAL.cpp`, `utils/audio_source/`). OpenAL stays
  for the GL path. (Audio is currently OpenAL-only, not SDL2 — swapping it is
  only about the DX11 "pure DirectX" goal.)
- **Timing / misc** — replace `SDL_GetTicks`/`SDL_ShowSimpleMessageBox` where
  the DX11 path needs them (Win32 `QueryPerformanceCounter` /
  `MessageBox`), keeping SDL for the GL path and for shared helpers.

A shared platform abstraction (implemented per backend: SDL2 for GL,
DirectX/DirectInput/XAudio2 for DX11) lets the simulation and renderer code
stay backend-agnostic.

## Key constraints carried over from the stereo work

- Offscreen is locked to 320x240 **only** because the PSX GTE projection is
  baked in. The DX11 renderer builds its own projection matrix, so internal
  resolution becomes configurable.
- Two eyes share `g_PreviousOffscreen` in PsyCross (fixed with
  `GR_ResetOffscreenSize`) — this is a psyx-only concern that disappears in DX11.
- `camera_matrix` column 0 is the view (forward) axis and is distorted by the
  aspect matrix — the eye offset must use the yaw-derived lateral axis
  (`right = (cos θ, 0, sin θ)`), reused verbatim for the DX11 per-eye projection.
- The build is mingw32 32-bit (debug + Release_dev) via
  `build_debug_mingw32.sh` / `build_release_mingw32.sh`, plus an MSVC solution
  at `src_rebuild/build/REDRIVER2.sln`.
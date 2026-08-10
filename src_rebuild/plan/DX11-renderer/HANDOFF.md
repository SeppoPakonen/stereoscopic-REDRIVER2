# Handoff — renderer rewrite (multi-renderer, DX11 default)

## Current status (as of the end of the stereo session)

Stereoscopic SBS rendering WORKS end-to-end through the legacy PsyX/GL path,
and the game runs at full speed. The remaining issue (right-eye map
disappearing) is downstream in the GL offscreen rasterization/composite and
motivated the move to a standard DX11 renderer.

## What is done / working (reusable foundation)

- **Simulation** (`State_GameLoop` -> `StepGame`) is backend-agnostic and
  unchanged: car physics, AI, world state, camera update. It produces
  `CAR_DATA`, `MODEL* modelpointers[]`, cell objects, `camera_*`, `texture_*`.
- **Scene data sources** (the inputs to the draw-command list):
  - `MODEL` meshes (world-space vertices + `POLYFT4/GT4/...` with UVs + color)
    in `Game/engine/mdl.h`, loaded via `models.c` (`ProcessMDSLump`).
  - Terrain/map placement: `CELL_OBJECT`/`PACKED_CELL_OBJECT` + `yang` yaw,
    PVS tables, `map.c`.
  - Cars: `CAR_DATA` (`ap.model`, `hd.where`, `hd.drawCarMat`, culling `oBox`).
  - Camera: `camera_position`, `camera_angle`, `camera_matrix`,
    `inv_camera_matrix` (`camera.c`, `draw.c`).
  - Textures: `texture_pages[128]`, `texture_cluts[128][32]` decode
    engine texture-set/id -> PSX tpage/clut (`texture.c`, `tset.h`).
- **Stereo camera math** (reuse verbatim for DX11 per-eye projection):
  `StereoCamera_ApplyToRender(eye)` (`Game/render/stereo.c`) — yaw-derived
  lateral offset `right=(cos θ,0,sin θ)`, left `-right`, right `+right`.
- **Iteration / test harness**: `StereoLog_*` + `-iterlog`,
  CLI flags (`-mission`, `-playercar`, `-stereo`, `-stereosep`, `-stereoconv`,
  `-swap`, `-stereodebug`, `-iterlog`, `-exitafter`), mingw32 builds
  (`build_*_mingw32.sh` + gdb).

## What is CUT for the DX11 renderer (the primary path)

The DX11 renderer does **not** use any of the following. They remain reachable
only through the legacy `-renderer psyx` backend:

- **RenderGame2 bottom half** — the GTE transform (`gte_rtpt`/`gte_stsxy3`),
  `addPrim`, OT depth assignment (`Z >> 1`), `ClearOTagR`.
- **OT / primitive buffer** — `MPBuff[2][2]`, `_OT1/_OT2`, `_primTab1/_primTab2`,
  `struct DB`, `current->ot`/`primptr` (`system.c/.h`).
- **PSX primitive structs** — `POLY_F3/F4/FT4`, `POLY_G3/G4/GT4`,
  `LINE_*`, `TILE/SPRT*`, `OT_TAG`/`P_TAG` (`PsyCross/include/psx/libgpu.h`).
- **Rasterization** — `DrawOTag`, `DrawPrim`, `ParsePrimitivesLinkedList`,
  `ParsePrimitive`, `Process*`, `AddSplit`, `DrawSplit`, `DrawAllSplits`
  (`PsyCross/src/psx/LIBGPU.C`, `PsyCross/src/gpu/PsyX_GPU.cpp`).
- **PsyX GL backend** — `GR_*` + `glDrawArrays` + the single-VRAM GL texture
  (`PsyCross/src/render/PsyX_render.cpp`, `glad.c`).

## Renderer selection (new)

- `-renderer dx11` — **default**. Standard DX11 stack, consumes the
  draw-command list. No OT/primitive stream.
- `-renderer psyx` — legacy PsyX/PsyCross GL (the code above). Kept selectable
  as a compatibility/fallback reference.
- (planned) `-renderer gl` — modern OpenGL mirroring the DX11 architecture.

## SDL2 usage today (what the DX11 path replaces with native DirectX)

SDL2 is embedded in the PsyX/GL layer and stays as the option for the GL
backends. The DX11 backend replaces this SDL2 surface with native Windows
DirectX (window = Win32 + DXGI, input = DirectInput8, audio = XAudio2):

- **Window / GL context** — `PsyX_render.cpp`: `SDL_CreateWindow`,
  `SDL_GL_*` (context, attributes), `SDL_GL_SwapWindow`, display mode.
- **Input** — `PsyX_main.cpp`: `SDL_PollEvent`/`SDL_Event`, keyboard
  (`SDL_GetKeyboardState`, `SDL_SCANCODE_*` mapping), gamepad
  (`SDL_GameController*`, `SDL_CONTROLLER_*` mapping); `PsyX_Pad_*`.
- **Threads / mutex** — `PsyX_main.cpp`: `SDL_Thread`, `SDL_mutex`
  (intr thread).
- **Timing** — `SDL_GetTicks` (main.c, state.c, stereo.c), `SDL_Delay`
  (commented out).
- **Message boxes** — `SDL_ShowSimpleMessageBox` (main.c, mission.c, system.c).
- **Audio** — NOT SDL2. Already OpenAL (`PsyCross/src/audio/PsyX_SPUAL.cpp`,
  `utils/audio_source/`). DX11 swaps this to XAudio2; OpenAL stays for GL.

## Known issues (context for the rewrite)

- **Right eye's map/terrain disappears** in the psyx path (downstream in
  DrawOTag rasterization or the composite blit). Expected to be inherently
  gone in DX11, which renders each eye directly into its own RT.
- **Higher-res offscreen** breaks the PSX projection (psyx-only; DX11 builds
  its own projection).
- **Color modes** (anaglyph/interlaced/polarized/checkerboard) not implemented
  (the shader compositor was disabled — it crashed the GL driver).
- **Split-screen** (2 players x 2 eyes = 4 images) not implemented.
- Camera can drift to the side over time (user reported; possibly
  controller-related, unresolved).

## Advice for the DX11 effort

- Capture scene data **before** the GTE transform — at the plot-function level
  (`RenderModel`, `DrawCar`, `DrawTILES`, `DrawSprites`), which is where world
  transforms + `MODEL` meshes + camera + textures are still available.
- Feed the renderer a **draw-command list** (mesh + transform + material +
  sort key); let the renderer own culling, sorting, batching, RTs, compositing.
- Make internal resolution configurable (not tied to 320x240).
- Reuse the yaw-derived per-eye offset math.
- Keep the legacy psyx path compilable/selectable so it remains a reference
  and the game stays playable during the transition.
- Preserve the mingw32 build + headless iteration harness.

## Build / run commands

```
cd src_rebuild
bash build_release_mingw32.sh          # release (Release_dev) build
bash build_debug_mingw32.sh            # debug build
# run (in the installed data dir):
REDRIVER2_rel.exe -nointro -nofmv -mission 50 -playercar 0 -renderer dx11 -stereo 3 -iterlog -exitafter 15
REDRIVER2_rel.exe ... -renderer psyx   # legacy path
```
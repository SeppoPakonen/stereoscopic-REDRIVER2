# Product — renderer rewrite (multi-renderer, DX11 default)

No DX11 code exists yet. This records the **existing reusable components the
effort builds on** (and must not re-write), the **planned renderer registry**,
and the **draw-command list** contract that the DX11 backend consumes.

## Renderer registry (planned)

- `-renderer dx11` — **default**, standard DX11 stack, consumes the draw-command
  list. No OT/primitive stream.
- `-renderer psyx` — legacy PsyX/PsyCross GL (OT/primitive path), kept selectable.
- `-renderer gl` — modern OpenGL mirroring the DX11 architecture (registry wired
  in T4.5; the mono slice renders the base scene, stereo/composite parity is a
  follow-up).

A startup-time selector (in `main.c`, alongside the existing `-stereo`/`-mission`
flags) picks the backend; the game's render path branches on it. All three
(`dx11`/`psyx`/`gl`) resolve via `Renderer_FromName` and dispatch in `DrawGame`
(T4.5): the dx11 branch probes `Dx11Renderer_Available()`, the gl branch probes
`GlRenderer_Available()`, and psyx runs the legacy path.

## Draw-command list contract (DX11 feed)

Per-frame high-level draw commands produced by the game's plot functions and
consumed by the DX11 renderer. Defined and implemented in
`Game/render/drawcmd.h` (`DrawCommand`, `MaterialRef`) + `drawcmd.c`
(per-frame growable arena).

```
struct DrawCommand {
    struct MODEL *mesh;   // world-space mesh source (NULL = single-prim via material)
    MATRIX world;         // placement transform (rotation + translation)
    MaterialRef material; // tpage/clut + blend/state (per-poly texture read from mesh)
    int sortKey;          // depth / transparency order
    unsigned char flags;  // DRAWCMD_OPAQUE/TRANSLUCENT, FLAT/GOURAUD, TWOSIDED, NODEPTH
    short palette;        // per-instance override (-1 = none)
    short subdiv;         // detail/subdivision hint (-1 = none)
};
```

Buffer API: `DrawCmd_BeginFrame()` (reset per frame), `DrawCmd_Submit(cmd)`
(returns stable index), `DrawCmd_Count()`, `DrawCmd_At(index)`.

**Ownership split (agreed):** the game keeps coarse **potential-visibility**
culling (PVS/cell structure), animation and LOD/subdiv selection; the renderer
owns projection, frustum culling, depth sorting (opaque front-to-back /
transparent back-to-front via `sortKey`), batching by `material`/`flags`, and
RT/composite selection. Because `MODEL` polys each carry their own
`texture_set`/`texture_id`, per-poly texture is resolved from the mesh; the
`material` supplies blend/state + a fallback texture for single-prims
(sprites/tiles/overlays) where `mesh == NULL`.

## Reusable existing components (source of the draw-command inputs)

### Scene data
- **`MODEL` meshes** — `Game/engine/mdl.h`: world-space vertices + engine polys
  (`POLYFT4/GT4/PL_POLYFT4`) with vertex indices, UVs, RGB. Loaded by
  `models.c` (`ProcessMDSLump`, `ProcessCarModelLump`) into
  `MODEL* modelpointers[]`.
- **Terrain/map placement** — `map.c`: `cells_across/down`, `current_cell_x/z`,
  PVS tables, `GetFirstPackedCop`/`GetNextPackedCop` iterate `CELL_OBJECT`
  (`engine/cell.h`) with `yang` yaw.
- **Cars** — `CAR_DATA` (`ap.model`, `ap.palette`, `hd.where`, `hd.drawCarMat`,
  `hd.oBox`) in `cars.c`/`car.h`.
- **Camera** — `camera.c`: `camera_position`, `camera_angle`, `camera_matrix`,
  `inv_camera_matrix` (`draw.c`), `InitCamera`/`PlaceCameraFollowCar`/
  `PlaceCameraInCar`/`ModifyCamera`.
- **Textures** — `texture.c`: `texture_pages[128]`, `texture_cluts[128][32]`
  map engine texture-set/id -> PSX tpage/clut; `tset.h` `TEXINF`/`TP`
  (`LoadTPageFromTIMs`, `ProcessTEXLump`/`ProcessClutLump`).

### Stereo camera (reuse verbatim)
- **`StereoCamera_ApplyToRender(eye)`** — `Game/render/stereo.c`. Per-eye
  lateral offset from camera yaw: `right=(cos θ,0,sin θ)`, left `-right`,
  right `+right`. Reuse for the DX11 per-eye projection.
- **`StereoCamera_Update(eye)`** — `Game/render/stereo.c`. Selects the eye.
- **`gStereoMode`, `gStereoSeparation`, `gStereoConvergence`, `gStereoSwapEyes`,
  `gCurrentStereoEye`** — `Game/render/stereo.h`.

### Iteration / test harness
- **`StereoLog_Open/Write/Close`** + `gStereoIterLogEnabled` — `Game/render/stereo.c/.h`
  (opt-in `-iterlog`).
- **CLI flags** — `Game/C/main.c`: `-mission`, `-playercar`, `-stereo`,
  `-stereosep`, `-stereoconv`, `-swap`, `-stereodebug`, `-iterlog`,
  `-exitafter` (add `-renderer` here).
- **Build scripts** — `build_debug_mingw32.sh`, `build_release_mingw32.sh`
  (mingw32 32-bit; release = Release_dev config), plus MSVC
  `build/REDRIVER2.sln`.

## Existing legacy path (kept only behind `-renderer psyx`)

- **Plot functions** — `draw.c` (`RenderModel`, `PlotModelSubdivNxN`,
  `PlotBuildingModel`, `DrawSprites`, `DrawSkyDome`), `tile.c` (`TileNxN`,
  `drawMesh`, `DrawTILES`), `cars.c` (`DrawCar`, `DrawCarObject`,
  `plotNewCarModel`, `DrawWheels`). These do GTE + `addPrim` today; under dx11
  they build draw commands instead.
- **OT/primitive system** — `system.c/.h` (`MPBuff[2][2]`, `_OT1/_OT2`,
  `_primTab1/_primTab2`, `struct DB`, `ClearOTagR`, `addPrim`).
- **Rasterization** — `PsyCross/src/psx/LIBGPU.C` (`DrawOTag`, `DrawPrim`,
  `LoadImage`), `PsyCross/src/gpu/PsyX_GPU.cpp` (`ParsePrimitivesLinkedList`,
  `ParsePrimitive`, `Process*`, `AddSplit`, `DrawSplit`, `DrawAllSplits`),
  `PsyCross/src/render/PsyX_render.cpp` (`GR_*` GL backend, single-VRAM GL
  texture, offscreen RTs, `GR_DrawTriangles`).

## Design notes for the new renderer
- Feed the DX11 renderer the world-space draw-command list; never the OT stream.
- The DX11 renderer builds its own projection (internal resolution configurable,
  not 320x240-locked) and per-eye projection using the yaw-derived offset.
- Keep the game's simulation and coarse PVS culling; the renderer owns the rest.

## Built DX11 modules (reusable — do not re-write)

Standalone, game-agnostic DX11 modules in `spike/`, each verified headless via
its own harness (mingw32 32-bit, `premake5 gmake2 --cc=gcc` +
`mingw32-make <proj> config=release_x86`). Adopted into the game draw-command
renderer in T1.5.

- **`dx11_renderer.{h,c}`** (T1.1, **T4.1**) — `Dx11Renderer`: device/context
  (11_0), native Win32 window + DXGI swapchain, backbuffer RTV + D24 depth +
  viewport, offscreen RT pair at internal resolution, begin-frame/present,
  resize, backbuffer capture to BMP. Also `Dx11Renderer_Available()` (T4.1) — a
  lightweight D3D11CreateDevice+WARP probe used by the game's `DrawGame`
  `-renderer dx11` dispatch. Harness: `dx11_foundation.cpp`.
- **`dx11_resources.{h,c}`** (T1.2) — `Dx11Res`: per-frame vertex arena +
  dynamic VB/IB, per-draw world-CB pool (64-byte DEFAULT CB/slot, plain
  `VSSetConstantBuffers`) + view/proj CB, default point sampler + `BindSRV`,
  bounded-growth stats. Harness: `dx11_resources_test.cpp`.
- **`dx11_textures.{h,c}`** (T1.3) — `Dx11Tex`: 1024x512 u16 VRAM staging +
  `CopyVRAM`, CPU decode-to-RGBA (tpage X/Y, `GET_TPAGE_FORMAT`, `GET_CLUT_X/Y`,
  4/8-bit nibble/byte extraction, CLUT lookup, RGB555→RGBA), per-region
  R8G8B8A8 texture+SRV cache (`Bake`/`GetSRV`), white 1x1 substitute.
  Design: **per-texture CPU bake**, not a VRAM atlas. Harness:
  `dx11_textures_test.cpp`.
- **`dx11_shaders.{h,c}`** (T1.4) — `Dx11Shaders`: universal VS (world→view→proj
  via b0/b1) + flat/gouraud textured PS (flat color from a per-draw `b2` CB;
  untextured = white substitute), input layout matching `Dx11ResVertex`, the 5
  PSX blend states (`NONE/AVERAGE/ADD/SUBTRACT/ADD_QUATER`), opaque/translucent
  depth-stencil states, cull/two-sided rasterizer states. Harness:
  `dx11_shaders_test.cpp`. Also **fixed a T1.1 bug**: `CaptureToBMP` was writing
  the BMP with R and B swapped (the render was always correct).
- **`dx11_drawcmdexec.{h,c}`** (T1.5) — `Dx11DrawCmds`: the draw-command
  **executor** — per-frame command list (geometry already in the T1.2 arena as
  vertex/index ranges + material + world + sort key + flags + local bbox),
  per-command world CBs, frustum culling (bbox corners through world·viewProj,
  replicating the shader's transposed `mul`), sorting (opaque by
  material/state front-to-back, translucent back-to-front by sortKey, opaque
  first), material/world/index-contiguous **batching**, and `DrawIndexed`
  emission on top of `dx11_resources` + `dx11_textures` + `dx11_shaders`.
  Harness: `dx11_drawcmdexec_test.cpp`.
- **`dx11_modeladapter.{h,c}`** (T1.6) — `Dx11ModelAdapter`: the game-side
  **MODEL → arena adapter** — converts a raw, game-agnostic mesh (int16
  `Dx11ModelVertex` + `Dx11ModelPoly` mirroring the game's resolved `MODEL`
  data: per-poly vertex indices `vi0..vi3`, tpage-local texel UVs,
  `texture_set`/`texture_id`, flat/gouraud color, blend/state) into the
  `dx11_resources` arena + executor commands: int16→float verts (world applied
  via the command CB), game-winding quad split `(vi0,vi1,vi3)+(vi0,vi3,vi2)`,
  **UV texel→normalized** by the baked region size, texture bake via a
  `Dx11ModelTexResolve` hook (`set,id` → tpage/clut/region), local bbox, and
  flat/gouraud + translucent (PSX 50% `stp` alpha) state. Vertices duplicated
  per poly so same-material consecutive polys stay index-contiguous → the T1.5
  executor batches them. Harness: `dx11_modeladapter_test.cpp` (texture /
  gouraud / batch / sort / world / cull / blend probes, all PASS).
- **`dx11_input.{h,c}`** (T1.7) — `Dx11Input`: the DX11 backend's native input
  layer — game-agnostic DirectInput8 (`CoInitializeEx` + `DirectInput8Create`),
  keyboard/mouse/joystick devices (`SetDataFormat`/`SetCooperativeLevel`/
  `Acquire`, `DISCL_NONEXCLUSIVE | DISCL_BACKGROUND`), `Poll` into a
  `Dx11InputState { keys[256]; mouse x/y/z + buttons; joyButtons[32] +
  joyAxis[6] + joyConnected }` (lost devices re-acquired + zeroed), and
  `Dx11Input_MapKey` mapping DIK scancodes to logical keys mirroring the game's
  default PSX pad controls (square=X, circle=V, triangle=Z, cross=C, L1=LSHIFT,
  L2=LCTRL, L3=LBRACKET, R1=RSHIFT, R2=RCTRL, R3=RBRACKET, select=SPACE,
  start=RETURN, dpad=arrows). SDL2 input stays for the GL backends. Links
  `dinput8` + `dxguid` + `ole32`. Harness: `dx11_input_test.cpp` (init/poll/map
  probes, all PASS; the attached gamepad was enumerated).
- **`dx11_audio.{h,c}`** (T1.8) — `Dx11Audio`: the DX11 backend's native audio
  layer — game-agnostic XAudio2 (`CoInitializeEx` + `XAudio2Create` +
  `CreateMasteringVoice`, stereo 44100), a source voice per sound with a PCM
  `WAVEFORMATEX` format, raw 16-bit PCM submission (`SubmitSourceBuffer`,
  loopable), `Play`/`Stop`, `SetVolume` (0..1) and `SetPan` (−1..+1 via the
  2-channel output matrix). A missing audio output device is tolerated
  (`HasOutput()`). OpenAL (`PsyX_SPUAL.cpp`) stays for the GL backends. Links
  `xaudio2_8` + `ole32`. Harness: `dx11_audio_test.cpp` (engine/voice/play/
  volpan probes, all PASS — 0.5 s sine buffer plays and drains).
- **`dx11_eyetargets_test.cpp`** (T2.1) — **per-eye render-target verification
  harness**: renders a distinct quad per eye into the two offscreen RTs
  (`DX11R_OFFSCREEN_COUNT = 2`, `BindOffscreen(index)` with an
  internal-resolution viewport, from T1.1) and asserts the RTs are independent
  at a configurable internal resolution (`-ires`, not 320x240-locked) while the
  backbuffer path stays separate. Probes: `EYE0`/`EYE1` (left red / right blue),
  `INDEP` (no cross-eye reuse), `IRES` (captured dims == configured res; PASS at
  320x240 and 480x360), `BACK` (green). The `GR_ResetOffscreenSize` workaround
  from the shared-GL path is not needed here.
- **`dx11_stereo.{h,c}`** (T2.2) — `Dx11Stereo`: game-agnostic **per-eye
  projection** math. `Dx11Stereo_EyeOffset` reuses the legacy
  `StereoCamera_ApplyToRender` math verbatim (right = `(cos θ,0,sin θ)`, gain =
  `sep*2`, left → `-right·gain`, right → `+right·gain`, swap negates);
  `Dx11Stereo_ViewMatrix` builds the per-eye world→view matrix (eye offset
  folded into the camera position) so the two eyes differ only by the lateral
  offset — returned as the **transposed view `V^t`** (basis in columns,
  translation in the last row) so `viewProj = view * proj` composes to
  `(P*V)^t` under the DX11 pipeline convention (proj stored as `P^t`; fixed
  during T2.4, which caught the original `V`-form not composing and ~3x parallax);
  `Dx11Stereo_ApplyConvergence` shears the projection horizontally (the legacy
  renderer-side `gStereoConvergence`). Harness: `dx11_stereo_test.cpp`
  (offset/stable/swap/separation/convergence probes, all PASS). Pure math, no
  extra links.
- **`dx11_composite.{h,c}`** (T2.3, **extended T3.1**) — `Dx11Composite`:
  game-agnostic **stereo composite pass** that samples the two per-eye offscreen
  SRVs (T2.1) into the backbuffer, replacing the legacy
  `StereoCompositor_Composite` GL blit. A fullscreen triangle (no vertex buffer,
  generated via `SV_VertexID`) + a pixel shader driven by a `b0` params CB
  (`mode`, `swap`) + a point sampler (nearest, matching the legacy `GL_NEAREST`).
  Modes: `DX11C_MODE_SBS` (eye0 left / eye1 right), `DX11C_MODE_TB` (eye0 top /
  eye1 bottom), `DX11C_MODE_MONO` (eye0 pass-through), plus the T3.1 color modes
  ported verbatim from the legacy shader sources — `ANAGLYPH` (red-cyan),
  `ANAGLYPH_FULLCOLOR` (luminance blend), `INTERLACED` (odd rows = eye0),
  `POLARIZED` (complementary scanlines), `CHECKERBOARD` (`mod(x+y,2)` interleave);
  `swap` flips the channel/eye assignment. A **4-SRV split pass**
  (`Dx11Composite_SplitComposite`, T3.2) maps 2 players x 2 eyes into the
  backbuffer quadrants (horizontal/vertical player split × SBS/TB eye layout,
  with swap). The per-pixel pass is the foundation for Phase 3's color
  modes/split-screen. Harness: `dx11_composite_test.cpp` (sbs/tb/swap/mono +
  anaglyph/interlaced/polarized/checkerboard probes, all PASS at 800x600 and
  1280x720). Links `d3d11`+`dxgi`+`d3dcompiler`+`user32`+`gdi32`.
- **`dx11_splitscreen_test.cpp`** (T3.2) — **split-screen verification harness**:
  creates four distinct solid-color eye images (P1-L red, P1-R green, P2-L
  blue, P2-R yellow) and verifies `Dx11Composite_SplitComposite` maps them into
  the four backbuffer quadrants for every split×layout (H_SBS / H_TB / V_SBS /
  V_TB) and that swap flips the eye assignment. Probes: all PASS.
- **`dx11_ires_test.cpp`** (T3.3) — **higher-internal-resolution harness**: renders
  a symmetric marker-pair probe scene into the offscreen RT at internal
  resolutions 320x240 / 640x480 / 1280x720 and proves the projection stays
  correct at every res (the perspective centre stays centred — unlike the PSX
  320x240 lock, which broke the projection at non-native res) and the captured
  dims match the configured res. Probes: `IRES`/`PROJ`/`SYMM` — all PASS.
- **`dx11_rendererselect_test.cpp`** (T4.1) — **renderer-selection A/B harness**:
  verifies the `-renderer dx11|psyx` selection (`Renderer_FromName` /
  `Renderer_IsDX11` / `Renderer_IsPsyX` from `Game/render/renderer.h`) resolves
  both backends and that `Dx11Renderer_Available()` reports the DX11 stack
  usable. Used as the headless two-backend A/B slice when the full game cannot
  reach the gameplay loop. Probes: `SELECT`/`FLAG`/`AVAIL` — all PASS.

**Game build integration (T4.1):** the 8 core DX11 modules (`dx11_renderer`,
`dx11_resources`, `dx11_textures`, `dx11_shaders`, `dx11_drawcmdexec`,
`dx11_modeladapter`, `dx11_stereo`, `dx11_composite`) now compile into the
`REDRIVER2` game binary (premake Windows filter: `spike/*` files +
`d3d11`/`dxgi`/`d3dcompiler`/`user32`/`gdi32` links), and `DrawGame` genuinely
dispatches on `-renderer dx11` (calls `Dx11Renderer_Available()`) vs
`-renderer psyx`.
- **`dx11_stereoscene_test.cpp`** (T2.4) — **right-eye-map verification harness**:
  renders a representative scene (a large map/terrain quad + a car quad) into
  BOTH per-eye RTs through the stereo path (`Dx11Stereo_ViewMatrix` per-eye view
  + T2.1 independent eye RTs) and proves the legacy "right-eye map disappears"
  bug is structurally absent: the right-eye RT has the full map (pixel green,
  not background), both eyes submit equal draw counts, and both carry the same
  objects (symmetric apart from the lateral offset). Caught and fixed the
  `ViewMatrix` `V`-vs-`V^t` convention bug (see `dx11_stereo`). Probes:
  `MAP_L`/`MAP_R`, `CAR_L`/`CAR_R`, `DUAL_DRAWCOUNT`, `SYMMETRIC` — all PASS.
- **`dx11_backendab_state.h`** (T4.2) — the common **store/load world-state**
  format shared by the two backend A/B binaries: a screen resolution + a list of
  flat screen-space quads (`Dx11AbScene`), written by `-store` and read by
  `-load`. This is the emulator-style state snapshot that lets one backend save
  [state + screenshot] in one render and the other reproduce it immediately
  (no engine iterations).
- **`dx11_backendab_psyx.cpp`** (T4.2) — **psyx-only backend binary**: links
  ONLY the PsyCross GL primitive path (`PsyX_Initialise` + POLY_F4 +
  `addPrim`/`DrawOTag`) — no DX11 code. It renders the common scene via the
  ordering table and captures the window to a BMP (`-store`/`-load`). This is
  the **reference/fallback** backend output. Follows the game's `E3stuff.c`
  render-loop model (single-depth OT, `SetDispMask`, env put, read the window
  **before** `PsyX_EndScene`).
- **`dx11_backendab_dx11.cpp`** (T4.2) — **DX11-only backend binary**: links
  ONLY the DX11 draw-command stack — no psyx/PsyCross sources. Renders the same
  scene with an orthographic screen→NDC projection (stored in the `P^t`
  convention: translations in `M[0][3]`/`M[1][3]`, `twoSided` for the y-flip
  winding) and captures to a BMP. Also hosts the `-compare` step (game-agnostic
  BMP+state reader) that proves both binaries reproduced the same scene from the
  identical stored state. Probes: `SELF` per quad + `COMPARE IDENTICAL` — all
  PASS.

**psyx = reference/fallback backend (T4.2):** the legacy PsyX/PsyCross GL path
(`-renderer psyx`) is documented and verified as the reference/fallback. The two
T4.2 binaries are built **separately** (`dx11_backendab_dx11` imports only
`d3d11`/`D3DCOMPILER_47` — no SDL2/GL; `dx11_backendab_psyx` imports only
`SDL2` — no d3d11), proving the DX11 binary contains no psyx code and vice
versa, and the `-store`/`-load` A/B shows both reproduce the identical scene
from the same world-state.

**DX11 mono path verified (T4.3):** the DX11 renderer's **mono path** — the path
the game uses when stereo is disabled (render the base view-projection DIRECTLY
into the swapchain backbuffer — no per-eye RT, no stereo composite) — is
verified as a correct, full-frame, stereo-state-independent render.
- **`dx11_nonstereo_test.cpp`** (T4.3) — **non-stereo regression harness**: renders
  the T4.2 common world-state scene through the mono path (one `MonoRenderer`
  reused across frames, `BindBackbuffer` + `CaptureToBMP(NULL)` for the
  backbuffer), and proves: `MONO_QUAD0..2` (each quad's centroid == its stored
  color — the base scene renders correctly), `FULL_FRAME` (left/right bars span
  the full height, centre square present — a full-frame direct render, not a
  composite SBS/TB half-frame), `STATE_INDEP` (the mono frame is pixel-identical
  whether a stereo config is present or absent — `differing_pixels=0`, stereo
  work does not regress normal play), and `PSYX_PARITY` (3/3 quad centroids match
  the T4.2 psyx reference BMP). All PASS. This is the regression gate proving the
  stereo work (T2.1–T3.3) lives entirely in the per-eye/composite path and never
  leaks into the mono path.

**Modern GL backend — mono + per-eye stereo verified (T4.4–T4.6):** a **modern
OpenGL backend** (`-renderer gl`, wired into the registry + `DrawGame` dispatch
in T4.5) that mirrors the DX11 architecture — a standard renderer stack, NOT
the PSX primitive/OT model. (The legacy `-renderer psyx` is the
PSX-primitive GL path; the T4.4 GL path is the modern GL mirror of the DX11
stack.)
- **`gl_renderer.{h,c}`** (T4.4, extended T4.6) — the **modern GL renderer
  module**: SDL window + OpenGL 3.3 core-profile context, glad loader
  (`gladLoadGL`), a VAO + interleaved pos/color VBO + indexed EBO, a GLSL 150
  core VS/FS pair, an orthographic screen→NDC projection (column-major, y-flip),
  a per-frame render of screen-space flat quads to a target (the default
  framebuffer, or a per-eye offscreen FBO), a `glReadPixels` → BMP capture
  (before swap), and a fullscreen-triangle **SBS/TB/MONO stereo composite**
  (T4.6, mirroring the DX11 T2.3 composite). Links `opengl32` + `SDL2`; portable
  by construction (SDL + glad are the same pieces the PsyCross GL path uses).
- **`gl_nonstereo_test.cpp`** (T4.4) — **GL mono-path harness + DX11 A/B**: renders
  the T4.2 common world-state scene through the GL mono path and proves
  `GL_QUAD0..2` (each quad's centroid == its stored color), `FULL_FRAME`
  (a standard full-frame projection), and `DX11_PARITY` (3/3 quad centroids match
  the DX11 backend reference BMP — the GL backend reproduces the identical scene).
  All PASS.
- **`gl_stereo_test.cpp`** (T4.6) — **GL per-eye stereo + composite harness**:
  renders a distinct scene per eye into the two per-eye offscreen FBOs (eye0 red,
  eye1 blue) and composites them SBS/TB/MONO into the default framebuffer,
  proving `SBS` (left=eye0, right=eye1), `TB` (top=eye0, bottom=eye1), `SWAP`
  (swap flips), and `MONO` (eye0 pass-through) — the GL stereo parity with the
  DX11 per-eye RT + composite. All PASS. (Anaglyph/interlaced/polarized/
  checkerboard color modes + split-screen are further GL follow-ups.)

Each task's `T1.n-*.md` file documents the module's API, verification outputs
and the bugs found/fixed.
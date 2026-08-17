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

**In-game renderer integration (T5.1):** the renderer half of
"DrawGame → draw-command → per-eye → composite" — the module DrawGame's
`-renderer dx11` branch calls to render the game's real `DrawCommand` feed.
- **`dx11_gamefeed.{h,c}`** (T5.1) — a game-aware DX11 renderer integration:
  `Dx11GameFeed_ModelToMesh` converts a `MODEL`'s flat-textured-quad polys
  (`PL_POLYFT4`, poly type `id & 31` ∈ {11,21,23}, stride `PolySizes[type]`) into
  the `Dx11ModelAdapter`'s raw mesh; `Dx11GameFeed_RenderFrame` consumes a real
  `DrawCommand[]` list and drives the full pipeline — per-command
  `ModelToMesh` + `Dx11ModelAdapter_Submit` → executor → `Dx11Stereo_ViewMatrix`
  per-eye → eye RTs → SBS/TB/MONO composite → backbuffer BMP. `MATRIX` world →
  float row-vector conversion (P^t convention). The plot-function feed (real
  game geometry into the `DrawCommand` list) is the remaining integration half.
- **`dx11_gamefeed_test.cpp`** (T5.1) — harness: builds a synthetic map (green)
  + car (red) as real `MODEL`/`DrawCommand` objects and verifies
  `MAP_L`/`MAP_R` (map present in both eyes), `CAR_L`/`CAR_R`, and `MONO`
  (base scene) — TOTAL_FAILS=0 GAMEFEED=PASS. Proves the game's draw-command
  feed renders end-to-end through DX11 per-eye → composite.

**Terrain/tile feed (T5.2, core slice):** the first **feed (producer) half** of
the in-game path — real terrain/tile geometry into the `DrawCommand` arena.
- **`PlotFeed_SubmitModel`** (draw.c, declared in draw.h) — builds + submits one
  world-space `DrawCommand` for a selected LOD `MODEL`: `world` = world rotation
  (`matrixtable[yang]` / `matrix`) + world position (`pos`), `material` fallback
  from the model's first poly (`texture_pages`/`texture_cluts`), `sortKey` =
  `z>>1` (mirrors the OT bucket), `flags` = opaque/two-sided/flat (+ translucent
  when `PLOT_TRANSPARENT`). **World-space convention:** the renderer builds its
  own per-eye view from the camera, so the feed passes the world transform, NOT
  the camera-space `CompoundMatrix`/GTE.
- **`RenderModel`** (draw.c) and **`DrawTILES`** (tile.c) — under
  `Renderer_IsDX11()`, submit a command per model/tile (after LOD selection) in
  **parallel with the legacy GTE draw** (non-breaking: the dx11-default game
  keeps rendering until DrawGame consumes the feed).
- **`RenderGame2`** (main.c) — `DrawCmd_BeginFrame()` under `Renderer_IsDX11()`
  bounds the per-frame arena.
- Verified by build-link + inspection (the game doesn't reach `DrawGame`
  headless); the end-to-end renderer path is T5.1-proven. The consumer half
  (DrawGame's dx11 branch calls `Dx11GameFeed_RenderFrame`) is done — see the
  next section.

**DrawGame dx11 consumer (T5.2, A/B):** the **consumer half** of the in-game
path — `DrawGame`'s `-renderer dx11` branch now renders the arena to a
**companion DX11 window** (`Dx11GameFeed_RenderFrame` → per-eye → MONO composite)
**in parallel** with the legacy SDL/PsyX path, so the two can be compared on
screen (per the owner's A/B choice). `dx11_gamefeed.{h,c}` is now compiled into
the REDRIVER2 build (premake Windows filter); `drawcmd.c` gained `DrawCmd_Data()`
(contiguous arena accessor); `Dx11GameFeed_RenderFrame`'s `CaptureToBMP` is
guarded by `bmpOut` so the game passes NULL (no per-frame BMP). The `main.c`
consumer (`Dx11GameDisplay` + `Dx11Game_EnsureDisplay` + `Dx11Game_RenderFrame`)
lazily creates a cached `Dx11Renderer` (own window) + the full system, converts
the game camera (`camera_position` → camPos, `inv_camera_matrix` rows → the full
yaw+pitch+roll camera basis) and the projection (from `FrAng`), and calls
`Dx11GameFeed_RenderFrame` (MONO, `sep=0`).

**Pitch/roll camera (full `inv_camera_matrix` basis):** the companion window's
view now follows the game camera's full orientation, not just yaw.
`Dx11Stereo_ViewMatrixBasis` (dx11_stereo.c) builds the per-eye world→view from
an explicit orthonormal basis (right / up / −forward), applying the stereo
lateral offset along `right`; `Dx11GameFeed_RenderFrame` gained a
`customViewBasis` param; `main.c` feeds the basis from `inv_camera_matrix`'s
rows (normalized per row to drop the game's pre-multiplied aspect horizontal
scale, which the DX11 projection already handles). The earlier `yawRad += π`
alignment is subsumed by using the actual rotation matrix. Verified by build-link
+ a headless `BASIS_FEED` unit test (the yaw basis reproduces the proven yaw view
exactly; a tilted basis changes the view) — `TOTAL_FAILS=0 GAMEFEED=PASS`.

**Feed texture baking (T5.2):** the consumer's companion window now renders the
terrain feed with **real PSX textures** instead of the white substitute.
`Dx11GameFeed_ModelToMesh`/`RenderFrame` gained a `tpages` param (the game's
`texture_pages[128]`) that scales each poly's UV X by the tpage page width
(64/128/256 texels by format `/256`) — matching the legacy GL shader's per-format
page mapping (4-bit ×0.25, 8-bit ×0.5, 16-bit ×1.0 in X, V 1:1). The game's
`Dx11Game_TexResolve(set,id)` (main.c) returns the **full page region**
`(0,0,pageW,256)` from `texture_pages[set]`/`texture_cluts[set][id]`; every frame
`Dx11Game_RenderFrame` refreshes the `Dx11Tex` VRAM staging from the game VRAM
(`GR_ReadVRAM` 1024×512 → `Dx11Tex_CopyVRAM`, so spooled textures are present at
first bake); the bake cache cap is raised to 512. Verified by build-link +
inspection + the dx11_gamefeed headless harness (GAMEFEED=PASS).

**In-game A/B verified (T5.2):** the real game was run (`-mission 50`, the
repo's `data/DRIVER2` is only a stub — a full install at
`J:\sblo\Pelit\PC\INSTALLED\Driver2` is required to reach `DrawGame`). Under
`-renderer dx11` the game opens **two** windows — the SDL/PsyX window (full
legacy scene) and the `REDRIVER2 DX11 (Phase 1)` companion window — and the
companion now renders the **terrain feed** (grey sky, olive-green ground, road
markers, a structure). `-renderer psyx` opens only the SDL window (legacy
unchanged). The headless harness still passes (`GAMEFEED=PASS`). Running the
game was the first exercise of the feed with real data and exposed three
in-game-only bugs, all fixed:
1. **Tile positions were packed, not nearCell-resolved** — `DrawMapPSX` now
   stores each tile's `nearCell`-resolved world position in a new
   `model_tile_pos[]` (parallel to `model_tile_ptrs[]`), and `DrawTILES`'s dx11
   branch submits it instead of the raw `ppco->pos` (which was ~260,000 units
   from the camera, beyond the far plane).
2. **Camera view direction inverted vs the game's GTE** — the game builds its
   view with `RotMatrixY(-yaw)`, so its forward is `+view z = (-sin,0,+cos)`;
   `yawRad = camera_angle.vy·(2π/4096) + π` aligns the DX11 yaw-derived forward
   while keeping the right-handed `front = -z` convention the projection expects.
3. **`MatWorldFromGte` stored the world translation in the wrong slot** — the
   vertex shader is row-vector (`mul(pos, world)`), so the translation must be in
   the **last row** (`world[3][0..2]`), but it was in the last column; the
   geometry never moved off the model's local origin. The headless harness did
   not catch this (its world matrices had zero translation, where both
   conventions agree). Fix: translation now written to the last row.

`Dx11GameFeed_RenderFrame` gained an optional `customView` parameter (NULL = the
yaw-based `Dx11Stereo_ViewMatrix`, prior behaviour; the game passes NULL, and
the game's own `inv_camera_matrix` is available for a pitch/roll-correct view).
Full detail: `T5.2-ab-verify-in-game.md`.

**Car feed (T5.2):** `DrawCar`'s **body** and **wheels** now submit world-space
`DrawCommand`s under `-renderer dx11` (legacy GTE kept in parallel). The body is
a game **`CAR_MODEL`** (dr2types.h) — triangulated `CAR_POLY`s (GT3 body / FT3
bottom / B3 bottom) over a shared **dented** `vlist` (`gTempCarVertDump[cp->id]`),
textured from the game's **`civ_clut`** table (NOT `texture_pages`/`texture_cluts`).
This is the first non-flat-quad poly kind the feed handles:
- **`DrawCommand.carModel`** (drawcmd.h) — a `struct CAR_MODEL *`; when set
  (`mesh == NULL`) the command is a car body; the existing `palette` field
  carries the car's `civ_clut` color variant.
- **`PlotFeed_SubmitCarModel`** (draw.c, declared in draw.h) — builds + submits
  the body command (world = `cp->hd.drawCarMat` + cog'd `where.t`, `sortKey =
  z>>1`). `DrawCar` captures the world position (with cog) before the GTE camera
  transform and submits in each LOD branch (`NewCarModel`/`NewLowCarModel`).
- **`Dx11GameFeed_CarModelToMesh`** (dx11_gamefeed.c) — converts a `CAR_MODEL`
  into the adapter's raw mesh: decodes each `vindices` triple, the page-scaled
  packed UVs (`clut_uv0&0xffff`/`tpage_uv1&0xffff`/`uv3_uv2&0xffff`), and the
  per-kind clut — **GT3** = `civClut[carid][texid][palette]` (carid =
  `1+(idx/192)`, texid = `(idx%192)/6`), **FT3** = raw `clut_uv0>>16`, **B3** =
  untextured. Emits triangles (`vi3==vi2`), two-sided, flat.
- **`Dx11ModelPoly`** (dx11_modeladapter.h/.c) — new `carTexture`/`carTpage`/
  `carClut` fields; when `carTexture` is set the adapter bakes the full page
  region directly from `carTpage`/`carClut`, bypassing the terrain/model resolve
  (which only covers `texture_pages`/`texture_cluts`).
- **`Dx11GameFeed_RenderFrame`** gained a `civClut` param + a `carModel` branch.
  Wheels (`MODEL` POLYFT4) reuse the existing `PlotFeed_SubmitModel` path
  (world = `drawCarMat`(+steer) + `rot*sWheelPos + where.t`).

Verified by build-link + the headless harness, which gained a `CAR_FEED`
converter unit test (GT3 clut via civ_clut, FT3 raw clut, triangle indices,
page-scaled UVs): `TOTAL_FAILS=0 GAMEFEED=PASS`. A real in-game visual A/B vs
`-renderer psyx` is deferred to the user. Full detail: `T5.2-car-feed.md`.

**Sprite / sky / effects feed (T5.2, MODEL-based):** the **MODEL-based**
sprite/effect plot functions now submit world-space `DrawCommand`s under
`-renderer dx11` (legacy GTE kept in parallel), reusing `PlotFeed_SubmitModel`:
- **`DrawSprites`** (draw.c) — tree billboards. The billboard world rotation is
  **`face_camera_work`** (camera.h; the legacy path pre-composites it as
  `face_camera = inv_camera_matrix * face_camera_work` for the GTE). Sprite
  positions are **packed relative to the cell origin** (like tiles), so a new
  `sprite_pos[]` array (draw.h/draw.c, parallel to `spriteList`) is filled with
  the nearCell-resolved world position in `DrawMapPSX`; `DrawSprites`'s dx11
  branch submits `PlotFeed_SubmitModel(model, &face_camera_work,
  &sprite_pos[i], z, PLOT_TRANSPARENT)`.
- **`DrawThrownBombs`** (bomberman.c) — `PlotFeed_SubmitModel(gBombModel,
  &object_matrix, &bomb->position, z, 0)`.
- **`DrawSmashable_sprites`** (debris.c) — `PlotFeed_SubmitModel(model,
  &object_matrix, &pos, z, 0)`.

Camera depth `z` for the sort key is computed like `RenderModel` (dot
`inv_camera_matrix` row 2 · `(worldpos − camera)`). Verified by build-link +
inspection (renderer unchanged; the headless harness is unaffected). Full
detail: `T5.2-sprite-sky-effects.md`.

**Sky feed (T5.2):** `DrawSkyDome`'s 4 horizon MODELS submit world-space
`DrawCommand`s under `-renderer dx11` via a **dedicated sky texture path** — the
horizon MODEL's polys are textured **per-poly from the game's sky tables**, NOT
the model's `texture_set`/`texture_id`/UVs (which `PlotSkyPoly` overrides):
- **`DrawCommand.skyModel`** + **`horizOffset`** (drawcmd.h) mark a sky horizon
  command; `PlotFeed_SubmitSkyModel` (sky.c) builds it camera-anchored (world pos
  = `camera_position + (0, sky_y_offset[GameLevel], 0)`, identity rotation).
- **`Dx11SkyTextures`** (dx11_gamefeed.h) bundles the game's
  `skytpage`/`skyclut`/`skytexuv`/`HorizonTextures`; `main.c` passes `&skyTex`.
- **`Dx11GameFeed_SkyModelToMesh`** (dx11_gamefeed.c) converts each horizon poly:
  `skytexnum = HorizonTextures[horizOffset + polyIndex]`, sets the poly's
  `carTpage`/`carClut` (the direct-bake car path) to `skytpage`/`skyclut[skytexnum]`
  + the `skytexuv[skytexnum]` UVs (remapped `u2,u3,u0,u1`, matching `PlotSkyPoly`).

Verified by build-link + a headless `SKY_FEED` converter unit test (sky texture
via HorizonTextures + carTpage/carClut + UV remap): `TOTAL_FAILS=0 GAMEFEED=PASS`.
Full detail: `T5.2-sprite-sky-effects.md`.

**addPrim single-primitive effects feed (T5.2):** the five addPrim effects now
submit world-space `DrawCommand`s under `-renderer dx11` (legacy GTE kept in
parallel) via a new **single-primitive (`mesh == NULL` + `material`) billboard
path**:
- **`DrawCommand.billboard`** (drawcmd.h) — `billboard` (1 = single-primitive
  quad), `bbOrient` (`BILLBOARD_CAMERA` = camera-facing /
  `BILLBOARD_WORLD` = XZ ground plane), `bbSizeX`/`bbSizeY` (half-extents),
  `bbUV[8]` (page-relative texel UVs), `bbRGB[3]` (flat color). `world` carries
  the placement translation.
- **`PlotFeed_SubmitBillboard`** (draw.c, declared in draw.h) — builds + submits
  the command from a world position + the effect's `tpageid`/`clutid` (VRAM, no
  blend bits) + `MATBLEND_*` + UVs + color + OT-depth sort key.
- **`Dx11GameFeed_BillboardToMesh`** (dx11_gamefeed.c) — converts the command to
  a 4-vertex quad + 1 flat textured-quad poly: camera-facing basis from
  `camPos − center` (or the XZ ground basis for world), UV X scaled by page
  width, `carTpage`/`carClut` direct-bake (full page), and the `MATBLEND_*` →
  `DX11SH_BLEND_*` mapping (OPAQUE→NONE, TRANSLUCENT→AVERAGE, ADDITIVE→ADD).
  `RenderFrame` dispatches on `dc->billboard` and submits through the adapter
  (which already binds the per-command blend state).
- Effects wired (`#ifndef PSX` + `Renderer_IsDX11()`): **`DrawExplosion`**
  (job_fx.c, camera-facing smoke_texture billboard), **`DisplayDebris`**
  (debris.c, litter/debris texture), **`DisplaySmoke`** (debris.c, camera-facing
  smoke_texture), **`DrawRainDrops`** (debris.c, thin light_texture streak),
  **`DrawTyreTracks`** (shadow.c, world-ground gTyreTexture quad).

Verified by build-link + a headless `BILLBOARD_FEED` converter unit test
(camera-facing + world-ground quad geometry, page-scaled UVs, blend mapping,
carTexture/tpage/clut direct-bake, flat color): `TOTAL_FAILS=0 GAMEFEED=PASS`.
The **sprite-shadows** (`addSubdivSpriteShadow`) reuse the same billboard path:
`PlotFeed_SubmitSpriteShadow` (draw.c) submits each shadow as a ground
`BILLBOARD_WORLD` quad at the sprite's nearCell-resolved world position
(`sprite_pos[spriteIndex-1]`), sized from the 4 shadow-corner verts, textured
from the model poly (`texture_pages`/`texture_cluts` + page-relative UVs), dark
+ translucent — an approximation of the PSX shadowMatrix-projected subdivided
quad (the `m*m` subdivision is lost). Verified by build-link + inspection.
Deferred: the real in-game visual A/B vs `-renderer psyx` (user run). Full
detail: `T5.2-sprite-sky-effects.md`.

**Software renderer + `-testcube` stand-alone mono mode:** a software-renderer
debug slice + a self-contained cube-mode that bypass the whole
level/mission/simulation system, used to A/B the projection geometrically
(matching wireframe between the psyx main window and the soft window).
- **`spike/soft_renderer.{h,c}`** — `SoftRenderer`: a simple CPU rasterizer
  (640x480 window, Bresenham `DrawLine` + filled triangles) that consumes the
  same `DrawCommand[]` feed as the DX11 path. Reusable probes:
  `SoftRenderer_Create` / `SoftRenderer_Destroy`,
  `SoftRenderer_RenderDebugBox` (prints the 8 box corners at every pipeline
  stage + rasterizes), `SoftRenderer_RenderFeed` (render all feed commands,
  optional per-command transform log to a file), and
  `SoftRenderer_RenderNdcEdges` (draw a shared NDC edge table as a white-on-black
  wireframe — this is what makes the soft window pixel-match the psyx `LINE_F2`
  wireframe).
- **`RENDERER_SOFT = 3`** + `Renderer_FromName("soft")` / `Renderer_IsSoft()` /
  `Renderer_ToName`, and **`Renderer_IsFeedActive()`** =
  `RENDERER_DX11 || RENDERER_SOFT` (renderer.h) — the plot functions populate
  the `DrawCommand` feed whenever the feed is consumed, soft included (fixes the
  earlier bug where the soft window showed nothing before the camera/level init).
- **`-testcube`** (main.c arg parse ~line 2687) — out-of-band mode enabled by
  `gTestCubeMode` (main.c:178). Sets `GAME_TAKEADRIVE` +
  `SetState(STATE_GAMELAUNCH)`; `State_GameInit`'s gTestCubeMode branch
  (`gameinit=1; NoPlayerControl=1; NewLevel=0; SetupDrawBuffers(); SetDispMask(1);
  SetState(STATE_GAMELOOP);`) and `State_GameLoop` jump straight to
  `TestCubeRenderFrame()` with **no level/mission/simulation, no cars, no
  pedestrians, no music** (`InitMusic` skipped under `!gTestCubeMode`),
  `GetRandomChase()` skipped, stereo forced mono.
- **`TestCubeRenderFrame()`** (main.c) — the `-testcube` frame loop:
  `ClearOTagR(current->ot, OTSIZE)` + `current->primptr = current->primtab`
  (the `0x4` crash fix — the OT was uninitialized before), `DrawTestCube()`,
  `SwapDrawBuffers()`, `PsyX_EndScene()`, and under `RENDERER_SOFT`
  `SoftGame_RenderFrame()`.
- **Shared wireframe, one source of truth** — `TestCube_WireCompute(camDist,
  cubeScale)` (main.c) computes the 12 cube edges' NDC endpoints into
  `gTestEdgeNdc[12][4]` + `gTestEdgeVisible[12]` with an explicit simple row-vector
  perspective (fovV=60°, f=1/tan(fovV/2), camera at (0,0,-camDist) identity —
  deliberately NOT the game's GTE projection, so both renderers match exactly).
  `DrawTestCubePsyX` draws those NDC edges as white `LINE_F2` (scaled NDC→320x240
  offscreen, y flip) in the PsyX main window; `SoftGame_RenderFrame` →
  `SoftRenderer_RenderNdcEdges` draws the **same** table into the soft window.
- Runtime: `REDRIVER2_dev.exe -renderer soft -testcube` opens the PsyX/SDL window
  (white cube wireframe on black) + the `640x480` soft window (same wireframe).
  Verified by build-link + a clean run (no crash, no "command line arguments"
  popup, no stderr; loop runs end-to-end). The OBJ/MTL/PNG loaders +
  `ModelBuilder` and the test camera live in plan 001–005.

Each task's `T1.n-*.md` file documents the module's API, verification outputs
and the bugs found/fixed.
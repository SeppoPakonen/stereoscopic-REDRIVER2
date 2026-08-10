# Product — renderer rewrite (multi-renderer, DX11 default)

No DX11 code exists yet. This records the **existing reusable components the
effort builds on** (and must not re-write), the **planned renderer registry**,
and the **draw-command list** contract that the DX11 backend consumes.

## Renderer registry (planned)

- `-renderer dx11` — **default**, standard DX11 stack, consumes the draw-command
  list. No OT/primitive stream.
- `-renderer psyx` — legacy PsyX/PsyCross GL (OT/primitive path), kept selectable.
- (planned) `-renderer gl` — modern OpenGL mirroring the DX11 architecture.

A startup-time selector (in `main.c`, alongside the existing `-stereo`/`-mission`
flags) picks the backend; the game's render path branches on it.

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
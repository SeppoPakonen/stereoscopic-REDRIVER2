# Building the REDRIVER2 Launcher

## Overview

The Launcher is a separate U++ application located in `src/Launcher/` directory. It has been enhanced with stereoscopic rendering GUI controls as part of Phase 3.

## Prerequisites

1. **ai-upp Framework** - Required for building the launcher
   - Repository: https://github.com/OuluBSD/ai-upp
   - Default installation: `C:\Users\sblo\upp`
   - Build tool: `C:\Users\sblo\Dev\ai-upp\bin\build.exe`

2. **Visual Studio 2022** - Required for MSVS22x64 build method

## Building the Launcher

### Important: Architecture Matching

**Both Launcher.exe and REDRIVER2.exe must be the same architecture (32-bit or 64-bit) if placed in the same directory**, because they share the SDL2.dll runtime library. A 32-bit executable cannot load a 64-bit DLL and vice versa.

**Current setup**: REDRIVER2.exe is 32-bit, so Launcher must also be built as 32-bit.

### Build Commands

From the repository root (`I:\Dev\stereoscopic-REDRIVER2`):

#### Recommended: 32-bit Build (matches REDRIVER2.exe)

```bash
C:\Users\sblo\Dev\ai-upp\bin\build.exe --source-roots ".;C:\Users\sblo\upp" -m CLANG -r Launcher
```

Runtime dependency: `C:\Users\sblo\upp\bin\SDL2\lib\x86\SDL2.dll`

#### Alternative: 64-bit Build (requires 64-bit REDRIVER2.exe)

```bash
C:\Users\sblo\Dev\ai-upp\bin\build.exe --source-roots ".;C:\Users\sblo\upp" -m MSVS22x64 -r Launcher
```

Runtime dependency: `C:\Users\sblo\upp\bin\SDL2\lib\x64\SDL2.dll`

**Note**: Only use 64-bit build if REDRIVER2.exe is also rebuilt as 64-bit, and place them in separate directories or ensure only one SDL2.dll version is present.

**Parameters Explained:**
- `--source-roots ".;C:\Users\sblo\upp"` - Source paths for U++ packages (local + ai-upp installation)
- `-m MSVS22x64` or `-m CLANG` - Build method (Visual Studio 2022 64-bit or CLANG 32-bit)
- `-r` - Release mode (optimized build)
- `Launcher` - Package name to build

### Output

- Executable location: `I:\Dev\stereoscopic-REDRIVER2\bin\Launcher.exe`
- Size: ~3.0 MB
- Build time: ~5-6 seconds per version

## Launcher Features (Phase 3)

The launcher GUI includes stereoscopic rendering configuration:

### Graphics Section
- **Stereoscopic Mode** - Dropdown with 8 modes:
  1. Disabled (monoscopic)
  2. Anaglyph Simple (red-cyan)
  3. Anaglyph Full-Color (improved color)
  4. Side-by-Side (half-width per eye)
  5. Top-and-Bottom (half-height per eye)
  6. Interlaced Scanlines (alternating lines)
  7. Polarized (scanline encoding)
  8. Checkerboard (pixel-level interleaving)

- **Swap Eyes** - Toggle to reverse eye rendering order
- **Convergence Distance** - Slider (0.5-100.0)
- **Stereo Separation** - Slider (0.1-3.0)

### Settings Storage
- All stereo settings saved to `config.ini`
- Settings persist between sessions
- Format: `[render]` section with `stereoMode`, `stereoSwapEyes`, `stereoConvergence`, `stereoSeparation` keys

## Source Files

Modified files in `src/Launcher/`:
- `RED2Launcher.lay` - GUI layout with stereo controls
- `RED2Launcher.h` - ConfigWindow class updates
- `configwin.cpp` - Load/save handlers for stereo settings
- `config_ini_writer.h/cpp` - Config persistence layer
- `.gitignore` - Excludes `bin/` directory from version control

## Building ai-upp from Source

If `C:\Users\sblo\Dev\ai-upp\bin\build.exe` doesn't exist:

1. Clone ai-upp: `https://github.com/OuluBSD/ai-upp`
2. Navigate to bootstrap directory: `uppsrc/build/bootstrap*`
3. Follow build instructions in that directory
4. The resulting `build.exe` will be in `bin/` subdirectory

## Troubleshooting

### Build Error: "Unable to locate .upp file for Launcher"
- Ensure you're running the command from the repository root
- Verify `src/Launcher/Launcher.upp` exists
- Check that ai-upp is properly installed at `C:\Users\sblo\upp`

### Build Error: Package dependencies missing
- The build system automatically downloads/links U++ dependencies
- Ensure internet connectivity during first build
- Check `--source-roots` path includes both local and ai-upp directories

## Release Packaging

To create a release zip with both Launcher and game:

### 64-bit Release

```powershell
$releaseDir = "release_phase3_x64"
mkdir $releaseDir
cp "bin/Launcher.exe" $releaseDir/
cp "src_rebuild/bin/Release/REDRIVER2.exe" $releaseDir/
cp "C:\Users\sblo\upp\bin\SDL2\lib\x64\SDL2.dll" $releaseDir/
Compress-Archive -Path "$releaseDir/*" -DestinationPath "REDRIVER2_Phase3_Release_x64.zip"
```

### 32-bit Release

```powershell
# Rebuild with: C:\Users\sblo\Dev\ai-upp\bin\build.exe --source-roots ".;C:\Users\sblo\upp" -m CLANG -r Launcher
$releaseDir = "release_phase3_x86"
mkdir $releaseDir
cp "bin/Launcher.exe" $releaseDir/
cp "src_rebuild/bin/Release/REDRIVER2.exe" $releaseDir/
cp "C:\Users\sblo\upp\bin\SDL2\lib\x86\SDL2.dll" $releaseDir/
Compress-Archive -Path "$releaseDir/*" -DestinationPath "REDRIVER2_Phase3_Release_x86.zip"
```

Result files:
- `REDRIVER2_Phase3_Release_x64.zip` (~2.4 MB) - 64-bit build
- `REDRIVER2_Phase3_Release_x86.zip` (~2.4 MB) - 32-bit build

Each contains:
- `Launcher.exe` (3.0 MB) - GUI launcher with stereo settings
- `REDRIVER2.exe` (0.8 MB) - Game executable
- `SDL2.dll` (1.5 MB) - Runtime dependency

## Related Components

- **Game Executable**: `src_rebuild/bin/Release/REDRIVER2.exe`
  - Built using Visual Studio (see src_rebuild/build/REDRIVER2.vcxproj)
  - Contains Phase 1, 2, 3 stereo rendering implementation

- **Stereo Implementation**: `src_rebuild/Game/render/stereo.*`
  - Core stereo camera system
  - 8 stereo output modes
  - Shader-based composition
  - Convergence/separation controls

## Renderer Rewrite — Phase 4 (DX11 + modern GL)

The renderer rewrite (plan docs in `src_rebuild/plan/DX11-renderer/`) replaces the
PSX OT/primitive renderer with a **standard DX11 renderer stack** fed by a
world-space **draw-command list**, with the legacy PsyX GL path and a modern GL
backend kept selectable. Phase 4 (T4.1–T4.6) + the in-game renderer integration
slice (T5.1) are **complete**:

- **Multi-renderer selection**: `-renderer dx11` (default) / `-renderer psyx`
  (legacy) / `-renderer gl` (modern). All three resolve in `renderer.h`
  (`Renderer_FromName`/`IsDX11`/`IsPsyX`/`IsGL`) and genuinely dispatch in `DrawGame`
  (`main.c`), probing `Dx11Renderer_Available()` / `GlRenderer_Available()`.
- **Standalone DX11 modules** in `src_rebuild/spike/` (the `dx11_renderer`,
  `dx11_resources`, `dx11_textures`, `dx11_shaders`, `dx11_drawcmdexec`,
  `dx11_modeladapter`, `dx11_input`, `dx11_audio`, `dx11_stereo`,
  `dx11_composite` modules) — each verified headless via its own harness + BMP
  pixel probes.
- **Backend A/B / separation**: `dx11_backendab_{dx11,psyx}` prove the DX11 binary
  contains no psyx code and vice versa (objdump DLL imports), and reproduce the
  identical scene from a common stored world-state (`dx11_backendab_state.h`
  store/load A/B).
- **Non-stereo regression**: `dx11_nonstereo_test.cpp` proves the DX11 **mono path**
  (render straight to the backbuffer, no per-eye/composite) is correct, full-frame,
  and stereo-state-independent; parity with the psyx reference.
- **Modern GL backend**: `gl_renderer.{h,c}` (SDL + GL 3.3 core + glad,
  VAO/VBO/IBO + GLSL shaders + ortho projection — not the PSX primitive model) +
  `gl_nonstereo_test.cpp` A/B vs DX11 (identical output) + `gl_stereo_test.cpp`
  (per-eye FBOs + SBS/TB/MONO composite, T4.6). Wired into the game build +
  registry in T4.5.
- **In-game renderer integration**: `dx11_gamefeed.{h,c}` (T5.1) — the renderer
  half of `DrawGame → draw-command → per-eye → composite`: consumes the game's
  real `DrawCommand[]` list, converts `MODEL` flat-quad polys
  (`PL_POLYFT4`, `id & 31` ∈ {11,21,23}) via `Dx11ModelAdapter`, and renders
  per-eye → composite. Verified headless with a synthetic map+car scene
  (`dx11_gamefeed_test.cpp`, GAMEFEED=PASS).
- **Terrain/tile feed (T5.2 core slice)**: the first **feed (producer) half** of
  the in-game path — `PlotFeed_SubmitModel` (draw.c) + `RenderModel` (draw.c) +
  `DrawTILES` (tile.c) submit world-space `DrawCommand`s under `-renderer dx11`
  (world rotation `matrixtable[yang]`/`matrix` + world `pos`; `sortKey = z>>1`,
  opaque/two-sided/flat flags), with `DrawCmd_BeginFrame()` in `RenderGame2`
  (main.c). Verified by build-link + inspection. Still to do: cars,
  sprites/sky/effects.
- **DrawGame dx11 consumer (T5.2, A/B)**: the **consumer half** — `DrawGame`'s
  `-renderer dx11` branch now renders the arena to a **companion DX11 window**
  (`Dx11GameFeed_RenderFrame` → per-eye → MONO composite) **in parallel** with
  the legacy SDL/PsyX path (the GTE draw stays on so the SDL window shows the
  full legacy scene). `dx11_gamefeed.{h,c}` is now in the REDRIVER2 build;
  `drawcmd.c` gained `DrawCmd_Data()`; the `main.c` consumer (`Dx11GameDisplay`,
  `Dx11Game_EnsureDisplay`, `Dx11Game_RenderFrame`) lazily creates a cached
  `Dx11Renderer` (own window) + system, converts the game camera/projection, and
  renders the feed (MONO). The terrain feed now bakes **real tpage/clut textures**
  from VRAM (`Dx11Game_TexResolve` → full-page regions; `Dx11GameFeed_ModelToMesh`
  `tpages` UV-X scaling by format; per-frame `GR_ReadVRAM` → `Dx11Tex_CopyVRAM`
  staging refresh). Verified by build-link + inspection; a running-game A/B
  requires a user run.

Build: `premake5 gmake2 --os=windows` + `mingw32-make <proj>
config=release_dev_x86` (32-bit mingw32; the game's known-good PsyCross path).
Runtime DLLs beside the exe: `libgcc_s_dw2-1.dll`, `libstdc++-6.dll`,
`libwinpthread-1.dll`, `SDL2.dll`, `libopenal-1.dll` (psyx/GL); `d3d11.dll`/
`D3DCOMPILER_47.dll` (DX11).

## Next Steps for Future Development

1. Finish the plot-function feed rewiring: cars (DrawCar chain), sprites/sky/
   effects. The terrain/tile feed (T5.2 core) + the DrawGame dx11 consumer (A/B
   companion window) + real feed texture baking are done; pitch/roll camera and
   a running-game A/B are the immediate follow-ups.
2. GL composite color modes (anaglyph / interlaced / polarized / checkerboard)
   + split-screen.
3. Advanced quality tuning / performance optimization for stereo rendering.
4. Comprehensive regression testing.
5. User documentation.

---

**Last Updated**: 2026-08-11
**Phase**: 4 (Integration & cleanup — renderer rewrite)
**Status**: Phase 4 done (T4.1–T4.6) + T5.1 renderer integration + T5.2
terrain/tile feed + DrawGame dx11 consumer (A/B); launcher + stereo GUI working,
DX11 + modern GL backends selectable, mono/per-eye/stereo-composite A/B-verified

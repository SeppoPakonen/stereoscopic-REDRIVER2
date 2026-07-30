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

### Build Commands

From the repository root (`I:\Dev\stereoscopic-REDRIVER2`):

#### 64-bit Build (MSVS22x64)

```bash
C:\Users\sblo\Dev\ai-upp\bin\build.exe --source-roots ".;C:\Users\sblo\upp" -m MSVS22x64 -r Launcher
```

Runtime dependency: `C:\Users\sblo\upp\bin\SDL2\lib\x64\SDL2.dll`

#### 32-bit Build (CLANG)

```bash
C:\Users\sblo\Dev\ai-upp\bin\build.exe --source-roots ".;C:\Users\sblo\upp" -m CLANG -r Launcher
```

Runtime dependency: `C:\Users\sblo\upp\bin\SDL2\lib\x86\SDL2.dll`

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

## Next Steps for Future Development

1. Implement advanced quality tuning (Task #13 continuation)
2. Performance optimization for stereo rendering
3. Extended mode support (polarized, checkerboard)
4. Comprehensive regression testing
5. User documentation

---

**Last Updated**: 2026-07-31
**Phase**: 3 (Advanced Features & Optimization)
**Status**: Launcher GUI complete, executable building working

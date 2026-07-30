# Phase 3 Task #12: Modified Files Reference

## Quick Reference for Code Review

This document provides exact file locations and line numbers for all changes made to implement the Polarized and Checkerboard stereo modes.

---

## 1. Game/render/stereo.h

**File Path**: `I:\Dev\stereoscopic-REDRIVER2\src_rebuild\Game\render\stereo.h`

### Change: Add new enum values to STEREO_MODE

**Location**: Lines 11-19
**Before**:
```c
typedef enum {
    STEREO_DISABLED = 0,
    STEREO_ANAGLYPH_SIMPLE = 1,
    STEREO_ANAGLYPH_FULLCOLOR = 2,
    STEREO_SIDEBYSIDE = 3,
    STEREO_TOPBOTTOM = 4,
    STEREO_INTERLACED = 5
} STEREO_MODE;
```

**After**:
```c
typedef enum {
    STEREO_DISABLED = 0,
    STEREO_ANAGLYPH_SIMPLE = 1,
    STEREO_ANAGLYPH_FULLCOLOR = 2,
    STEREO_SIDEBYSIDE = 3,
    STEREO_TOPBOTTOM = 4,
    STEREO_INTERLACED = 5,
    STEREO_POLARIZED = 6,
    STEREO_CHECKERBOARD = 7
} STEREO_MODE;
```

**Impact**: Adds two new stereo mode identifiers
**Lines Changed**: 2 (added)
**Backward Compatible**: YES

---

## 2. Game/render/stereo_compositor.h

**File Path**: `I:\Dev\stereoscopic-REDRIVER2\src_rebuild\Game\render\stereo_compositor.h`

### Change: Add shader pointers to STEREO_COMPOSITOR struct

**Location**: Lines 22-30
**Before**:
```c
    // Shader for anaglyph composition
    uintptr_t anaglyph_shader;
    uintptr_t anaglyph_fullcolor_shader;
    uintptr_t sidebyside_shader;
    uintptr_t topbottom_shader;
    uintptr_t interlaced_shader;
    // Vertex array for fullscreen quad
```

**After**:
```c
    // Shader for anaglyph composition
    uintptr_t anaglyph_shader;
    uintptr_t anaglyph_fullcolor_shader;
    uintptr_t sidebyside_shader;
    uintptr_t topbottom_shader;
    uintptr_t interlaced_shader;
    uintptr_t polarized_shader;
    uintptr_t checkerboard_shader;
    // Vertex array for fullscreen quad
```

**Impact**: Adds storage for two new shader programs
**Lines Changed**: 2 (added)
**Backward Compatible**: YES (struct extended, not modified)

---

## 3. Game/render/stereo_compositor.c

**File Path**: `I:\Dev\stereoscopic-REDRIVER2\src_rebuild\Game\render\stereo_compositor.c`

### Change 1: Add Polarized Shader Source Code

**Location**: Lines 111-133

```c
// Polarized stereoscopy shader
// Left image: even scanlines
// Right image: odd scanlines
// Each scanline is marked with polarization state (encoded in output)
static const char* g_polarized_shader_source =
    "#version 120\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "uniform vec2 screenSize;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    // Polarized stereoscopy:\n"
    "    // Even scanlines: left eye (horizontal polarization)\n"
    "    // Odd scanlines: right eye (vertical polarization)\n"
    "    // The display hardware uses polarized filters to separate the images\n"
    "    float scanline = mod(gl_FragCoord.y, 2.0);\n"
    "    vec4 color;\n"
    "    if (scanline > 0.5) {\n"
    "        // Odd scanline - right eye (vertical polarization)\n"
    "        color = texture2D(rightEyeTexture, v_texcoord);\n"
    "    } else {\n"
    "        // Even scanline - left eye (horizontal polarization)\n"
    "        color = texture2D(leftEyeTexture, v_texcoord);\n"
    "    }\n"
    "    // Output color directly - display hardware handles polarization\n"
    "    gl_FragColor = color;\n"
    "}\n";
```

**Impact**: Defines GLSL shader for polarized stereoscopy
**Lines Added**: 23
**Dependencies**: None (standard GLSL 1.2)

### Change 2: Add Checkerboard Shader Source Code

**Location**: Lines 138-164

```c
// Checkerboard pattern shader
// Interleave pixels in checkerboard pattern
// Left eye on black squares, right eye on white squares
static const char* g_checkerboard_shader_source =
    "#version 120\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "uniform vec2 screenSize;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    // Checkerboard pattern: pixel-level interlacing\n"
    "    // Creates a checkerboard by interleaving pixels from left and right eyes\n"
    "    // Sample the pixel coordinates\n"
    "    float pixelX = gl_FragCoord.x;\n"
    "    float pixelY = gl_FragCoord.y;\n"
    "    \n"
    "    // Determine if we're on a 'black' or 'white' square\n"
    "    float checker = mod(pixelX + pixelY, 2.0);\n"
    "    \n"
    "    vec4 color;\n"
    "    if (checker > 0.5) {\n"
    "        // White squares: right eye\n"
    "        color = texture2D(rightEyeTexture, v_texcoord);\n"
    "    } else {\n"
    "        // Black squares: left eye\n"
    "        color = texture2D(leftEyeTexture, v_texcoord);\n"
    "    }\n"
    "    \n"
    "    gl_FragColor = color;\n"
    "}\n";
```

**Impact**: Defines GLSL shader for checkerboard stereoscopy
**Lines Added**: 27
**Dependencies**: None (standard GLSL 1.2)

### Change 3: Add Shader Compilation Calls

**Location**: Lines 305-309 (within StereoCompositor_Init function)

**Before** (line 303-304):
```c
    // Compile interlaced scanline shader
    g_compositor.interlaced_shader = (uintptr_t)GR_Shader_Compile(g_interlaced_shader_source, 0);

    g_compositor_initialized = 1;
```

**After** (lines 302-311):
```c
    // Compile interlaced scanline shader
    g_compositor.interlaced_shader = (uintptr_t)GR_Shader_Compile(g_interlaced_shader_source, 0);

    // Compile polarized stereoscopy shader
    g_compositor.polarized_shader = (uintptr_t)GR_Shader_Compile(g_polarized_shader_source, 0);

    // Compile checkerboard pattern shader
    g_compositor.checkerboard_shader = (uintptr_t)GR_Shader_Compile(g_checkerboard_shader_source, 0);

    g_compositor_initialized = 1;
```

**Impact**: Compiles shaders when compositor initializes
**Lines Added**: 5
**Function Called**: GR_Shader_Compile (existing function)

### Change 4: Add Cases to Composite Function

**Location**: Lines 532-540 (within StereoCompositor_Composite function switch statement)

**Before** (lines 525-531):
```c
        case STEREO_INTERLACED:
            shader = g_compositor.interlaced_shader;
            break;
        default:
            if (gStereoDebugLog) {
                printf("StereoCompositor_Composite: Unknown mode %d\n", mode);
            }
            return;
```

**After** (lines 525-541):
```c
        case STEREO_INTERLACED:
            shader = g_compositor.interlaced_shader;
            break;
        case STEREO_POLARIZED:
            shader = g_compositor.polarized_shader;
            break;
        case STEREO_CHECKERBOARD:
            shader = g_compositor.checkerboard_shader;
            break;
        default:
            if (gStereoDebugLog) {
                printf("StereoCompositor_Composite: Unknown mode %d\n", mode);
            }
            return;
```

**Impact**: Routes correct shader based on selected stereo mode
**Lines Added**: 8
**Dependencies**: STEREO_POLARIZED and STEREO_CHECKERBOARD enum values

**Total Changes in stereo_compositor.c**: 63 lines added

---

## 4. Game/Frontend/FEscreens.inc

**File Path**: `I:\Dev\stereoscopic-REDRIVER2\src_rebuild\Game\Frontend\FEscreens.inc`

### Change: Update Screen 40 (Stereo Mode Selection)

**Location**: Lines 1275-1352

**Key Changes**:
1. Button count: `40, 7, 26,` → `40, 9, 26,`
2. Added "Polarized" button (lines 1327-1334)
3. Added "Checkerboard" button (lines 1335-1342)
4. Updated "Back" button position (lines 1343-1350)
5. Updated navigation indices for all buttons

**Button Details**:

**Existing Buttons (Updated Navigation)**:
| Button | Label | Y Position | Up Index | Down Index | Status |
|--------|-------|-----------|----------|------------|--------|
| 0 | Disabled | 160 | 8 | 2 | Navigation updated |
| 1 | Anaglyph Simple | 197 | 1 | 3 | Unchanged |
| 2 | Anaglyph Full-Color | 234 | 2 | 4 | Unchanged |
| 3 | Side-by-Side | 271 | 3 | 5 | Unchanged |
| 4 | Top-Bottom | 308 | 4 | 6 | Unchanged |
| 5 | Interlaced Scanlines | 345 | 5 | 7 | Unchanged |

**New Buttons**:
| Button | Label | Y Position | Up Index | Down Index | Added |
|--------|-------|-----------|----------|------------|-------|
| 6 | Polarized | 382 | 6 | 8 | NEW |
| 7 | Checkerboard | 419 | 7 | 9 | NEW |
| 8 | Back | 456 | 8 | 1 | NEW POSITION |

**Example for Button 6 (Polarized)**:
```c
{
    163, 382, 256, 36,
    0, 0, 6, 8,           // Navigation: up=6, down=8
    0,
    363, 382,
    512, 512,
    "Polarized"
}
```

**Impact**: Adds GUI options for two new stereo modes
**Lines Added**: ~45 (new buttons + navigation updates)
**Affected Buttons**: All 9 buttons updated for navigation consistency

---

## 5. Game/Frontend/FEmain.c

**File Path**: `I:\Dev\stereoscopic-REDRIVER2\src_rebuild\Game\Frontend\FEmain.c`

### Change: Update StereoModeScreen Handler

**Location**: Line 3861 (within StereoModeScreen function)

**Before**:
```c
        // Position cursor based on current stereo mode
        if (gStereoMode >= 0 && gStereoMode < 6)
            currSelIndex = gStereoMode;
```

**After**:
```c
        // Position cursor based on current stereo mode
        if (gStereoMode >= 0 && gStereoMode < 8)
            currSelIndex = gStereoMode;
```

**Impact**: Handler now supports 8 stereo modes (IDs 0-7)
**Lines Changed**: 1 (modified condition)
**Function**: StereoModeScreen (starting at line 3856)

---

## Summary of Changes by File

| File | Lines Added | Type | Difficulty |
|------|------------|------|------------|
| stereo.h | 2 | Enum extension | Trivial |
| stereo_compositor.h | 2 | Struct extension | Trivial |
| stereo_compositor.c | 63 | Shaders + implementation | Moderate |
| FEscreens.inc | 45 | GUI button + navigation | Moderate |
| FEmain.c | 1 | Handler update | Trivial |
| **TOTAL** | **113** | **Complete** | **Low Risk** |

---

## Verification Commands

### Check enum values
```bash
grep "STEREO_POLARIZED\|STEREO_CHECKERBOARD" Game/render/stereo.h
```

### Check shader declarations
```bash
grep "polarized_shader\|checkerboard_shader" Game/render/stereo_compositor.h
```

### Check shader implementations
```bash
grep "g_polarized_shader_source\|g_checkerboard_shader_source" Game/render/stereo_compositor.c
```

### Check GUI buttons
```bash
grep -A 2 "Polarized\|Checkerboard" Game/Frontend/FEscreens.inc
```

### Check handler support
```bash
grep "gStereoMode < 8" Game/Frontend/FEmain.c
```

---

## Testing Protocol

### Unit Tests
1. Verify enum values compile without error
2. Verify struct fields properly initialized
3. Verify shaders compile at runtime
4. Verify composite function selects correct shader

### Integration Tests
1. GUI displays both new modes
2. Mode selection updates gStereoMode
3. Mode persists in configuration
4. Mode loads correctly on startup

### Visual Tests
1. Polarized mode produces scanline output
2. Checkerboard mode produces checkerboard output
3. 3D effect visible on compatible hardware
4. No visual artifacts or glitches

### Performance Tests
1. Shader compilation time < 100ms
2. Composite time < 3ms per frame
3. Memory usage stable
4. No framerate degradation

---

## Rollback Procedure

If rollback is needed:

1. **Revert stereo.h**: Remove STEREO_POLARIZED and STEREO_CHECKERBOARD enum values
2. **Revert stereo_compositor.h**: Remove polarized_shader and checkerboard_shader fields
3. **Revert stereo_compositor.c**: 
   - Remove shader source code (lines 111-164)
   - Remove shader compilation calls (lines 305-309)
   - Remove switch cases (lines 532-540)
4. **Revert FEscreens.inc**: Restore Screen 40 to 7 buttons
5. **Revert FEmain.c**: Change condition back to `< 6`

All changes are isolated and can be cleanly removed.

---

## Code Review Checklist

- [ ] All enum values properly defined
- [ ] Struct fields properly declared
- [ ] Shader source code syntactically valid
- [ ] Shader compilation calls present and correct
- [ ] Composite switch cases complete
- [ ] GUI buttons added and positioned
- [ ] Navigation indices correct and consistent
- [ ] Handler validation updated
- [ ] No breaking changes to existing code
- [ ] Backward compatibility maintained
- [ ] Documentation complete

---

**End of Reference Document**

For questions about specific changes, refer to line numbers provided above and the actual file contents at the specified paths.

# Stereoscopic Rendering - Implementation Strategy

Based on rendering pipeline and launcher architecture exploration.

## Codebase Structure Overview

### Rendering Pipeline
```
Main Loop: DoStateLoop() [state.c]
  ↓
State_GameLoop [main.c:1543]
  ↓
DrawGame() [main.c:1607]
  ├─ RenderGame2(0) - Single player or P1 in multiplayer
  ├─ SwapDrawBuffers() → GR_SwapWindow()
  └─ (if multiplayer) RenderGame2(1) - P2

RenderGame2(view) [main.c:2216]
  ├─ InitCamera(&player[view]) - Updates camera_matrix
  ├─ SetupDrawMapPSX() - Setup rendering state
  ├─ DrawAllPedestrians()
  ├─ DrawAllTheCars(view)
  ├─ DrawSkyDome()
  ├─ [... other scene elements ...]
  └─ (Primitives sent to GPU via PsyX shaders)

Frame Presentation
  ↓
GR_SwapWindow() [PsyX_render.cpp:1709]
  ↓
SDL_GL_SwapWindow() → On-screen
```

### Settings System
```
Frontend: FEmain.c [~2700 lines]
  ├─ Menu screens: FEscreens.inc (39 screens)
  ├─ Options screen: Screen 13 (volume, difficulty, etc.)
  └─ Settings data in GAME_GLOBAL struct

Config Persistence: loadsave.c
  ├─ Save: SaveCurrentProfile() → ~/.driver2/config.dat
  ├─ Load: LoadCurrentProfile() → Read config.dat
  └─ Config struct: CONFIG_SAVE_HEADER [lines 31-49]

Optional INI config: config.ini
  └─ Parsed by utils/ini.c at startup
```

## Phase 1 Implementation Plan: Foundation + Simple Anaglyph

### Step 1: Extend Settings Infrastructure

**Files to modify:**
- `Game/C/loadsave.c` — Add stereo config fields
- `Game/C/camera.h/c` — Add stereo camera offset logic
- `Game/C/dr2types.h` — Add stereo settings struct

**1a. Add stereo settings to CONFIG_SAVE_HEADER**

In `loadsave.c:31-49`, extend `CONFIG_SAVE_HEADER` struct:
```c
typedef struct {
    // ... existing fields ...
    
    // Stereo settings (new)
    int stereo_mode;              // 0=disabled, 1=anaglyph_simple, 2=anaglyph_full, 3=sbs, 4=tb, 5=interlaced
    float stereo_separation;      // Eye separation distance (0.0-2.0)
    float stereo_convergence;     // Convergence distance (0.5-100.0)
    int stereo_swap_eyes;         // 0=normal, 1=swapped
    int stereo_debug_log;         // 0=off, 1=on
} CONFIG_SAVE_HEADER;
```

**1b. Add stereo helper functions to camera.c**

```c
void ApplyStereoEyeOffset(int eye)
{
    // eye: 0=left, 1=right, -1=mono
    // Apply ±stereo_separation/2 offset to camera on X axis
}

MATRIX* GetStereoViewMatrix(int eye)
{
    // Return view matrix with eye offset applied
}
```

**1c. Add global stereo settings**

In `dr2types.h` or new `stereo.h`:
```c
extern int gStereoMode;
extern float gStereoSeparation;
extern float gStereoConvergence;
extern int gStereoSwapEyes;
extern int gStereoDebugLog;
```

---

### Step 2: Implement Stereo Camera System

**New file:** `Game/render/stereo_camera.h/cpp`

```c
// Camera setup for stereo
typedef struct {
    VECTOR left_eye_pos;
    VECTOR right_eye_pos;
    MATRIX left_view_matrix;
    MATRIX right_view_matrix;
} STEREO_CAMERA;

STEREO_CAMERA* CreateStereoCamera(VECTOR camera_pos, SVECTOR camera_angle);
void UpdateStereoCameraFromPlayer(PLAYER *player);
```

**Integration with existing camera:**

Modify `camera.c:InitCamera()` to optionally apply eye offset:
```c
void InitCamera(PLAYER *lp)
{
    // ... existing camera setup code ...
    
    if (gStereoMode != STEREO_DISABLED) {
        ApplyStereoEyeOffset(current_eye); // current_eye will be set per render
    }
}
```

---

### Step 3: Implement Simple Anaglyph Rendering

**New file:** `Game/render/stereo_composite.h/cpp`

```c
// Render to texture for compositing
typedef struct {
    GLuint framebuffer;
    GLuint color_texture_left;
    GLuint color_texture_right;
    GLuint depth_renderbuffer;
    int width, height;
} STEREO_RENDERTARGET;

STEREO_RENDERTARGET* CreateStereoRenderTarget(int width, int height);
void RenderSceneToStereoTarget(STEREO_RENDERTARGET *target, int eye);
void CompositeStereoToScreen(STEREO_RENDERTARGET *target, int mode);
```

**Simple anaglyph algorithm:**
```c
void CompositeAnaglyph_Simple(GLuint left_texture, GLuint right_texture)
{
    // Render full-screen quad with shader:
    // output.rgb = left.r | (right.g | right.b)
    // This extracts:
    //   - Red channel from left eye → red
    //   - Green+Blue channels from right eye → cyan
}
```

**New shader:** `PsyX/src/shaders/anaglyph_simple.glsl`

```glsl
#version 120

uniform sampler2D left_eye;
uniform sampler2D right_eye;

void main() {
    vec2 uv = gl_TexCoord[0].st;
    vec3 left = texture2D(left_eye, uv).rgb;
    vec3 right = texture2D(right_eye, uv).rgb;
    
    // Anaglyph: red from left, cyan from right
    gl_FragColor = vec4(left.r, right.g, right.b, 1.0);
}
```

---

### Step 4: Implement Debug Logging Infrastructure

**New file:** `Game/render/stereo_debug.h/cpp`

```c
#define STEREO_LOG_ENABLED 1

#if STEREO_LOG_ENABLED
void StereoLog(const char *fmt, ...);
void StereoLogFrame(int frame_num);
void StereoLogRenderPhase(const char *phase, int eye);
#else
#define StereoLog(...)
#define StereoLogFrame(...)
#define StereoLogRenderPhase(...)
#endif
```

**Logging points in rendering pipeline:**

```c
// In DrawGame()
StereoLogFrame(frame_count);
if (gStereoMode != STEREO_DISABLED) {
    StereoLog("Stereo rendering: mode=%d, separation=%.2f", 
              gStereoMode, gStereoSeparation);
}

// In RenderGame2()
StereoLogRenderPhase("RenderGame2", current_eye);

// In InitCamera()
if (gStereoDebugLog) {
    StereoLog("Eye offset applied: eye=%d, offset=%.2f", 
              current_eye, gStereoSeparation * 0.5f);
}
```

---

### Step 5: Modify Main Rendering Loop

**File:** `Game/C/main.c` (DrawGame function)

**Change from:**
```c
void DrawGame(void) {
    if (NumPlayers == 1 || NoPlayerControl) {
        RenderGame2(0);
        SwapDrawBuffers();
    } else {
        RenderGame2(0);
        SwapDrawBuffers2(0);
        RenderGame2(1);
        SwapDrawBuffers2(1);
    }
}
```

**Change to:**
```c
void DrawGame(void) {
    if (gStereoMode == STEREO_DISABLED) {
        // Original monoscopic path
        if (NumPlayers == 1 || NoPlayerControl) {
            RenderGame2(0);
            SwapDrawBuffers();
        } else {
            RenderGame2(0);
            SwapDrawBuffers2(0);
            RenderGame2(1);
            SwapDrawBuffers2(1);
        }
    } else {
        // Stereo rendering path
        if (NumPlayers == 1 || NoPlayerControl) {
            StereoRenderGame2(0); // Renders left+right eyes, composites based on mode
            SwapDrawBuffers();
        } else {
            // TODO: Stereo split-screen (lower priority)
            RenderGame2(0);
            SwapDrawBuffers();
        }
    }
}

void StereoRenderGame2(int player)
{
    STEREO_RENDERTARGET *target = CreateStereoRenderTarget(width, height);
    
    // Render left eye
    BindRenderTarget(target, STEREO_LEFT);
    current_eye = STEREO_LEFT;
    RenderGame2(player);
    
    // Render right eye
    BindRenderTarget(target, STEREO_RIGHT);
    current_eye = STEREO_RIGHT;
    RenderGame2(player);
    
    // Composite to screen
    BindScreenRenderTarget();
    CompositeStereoToScreen(target, gStereoMode);
    
    DestroyRenderTarget(target);
}
```

---

### Step 6: Add Launcher GUI Stereo Options Screen

**File:** `Game/Frontend/FEscreens.inc`

Add new screen entry (e.g., Screen 999 for Stereo Options):
```c
FRONTEND_SCREEN frontend_stereo_options_screen = {
    // Screen layout definition
    buttons: [
        { text: "Stereo Mode",      action: BTN_STEREO_MODE_SELECT, ... },
        { text: "Separation",       action: BTN_STEREO_SEPARATION, ... },
        { text: "Convergence",      action: BTN_STEREO_CONVERGENCE, ... },
        { text: "Swap Eyes",        action: BTN_STEREO_SWAP, ... },
        { text: "Debug Log",        action: BTN_STEREO_DEBUG_LOG, ... },
        { text: "Back",             action: BTN_RETURN_SCREEN, ... },
    ]
};
```

**File:** `Game/Frontend/FEmain.c`

Add button handlers:
```c
case BTN_STEREO_MODE_SELECT:
    gStereoMode = (gStereoMode + 1) % NUM_STEREO_MODES;
    break;
case BTN_STEREO_SEPARATION:
    gStereoSeparation += slider_increment;
    // Clamp to 0.0-2.0
    break;
// ... etc
```

---

### Step 7: Wire Config Save/Load

**File:** `Game/C/loadsave.c`

Modify `SaveConfigData()` to include stereo fields:
```c
void SaveConfigData()
{
    // ... existing code ...
    config->stereo_mode = gStereoMode;
    config->stereo_separation = gStereoSeparation;
    config->stereo_convergence = gStereoConvergence;
    config->stereo_swap_eyes = gStereoSwapEyes;
    config->stereo_debug_log = gStereoDebugLog;
}
```

Modify `LoadConfigData()` to restore stereo fields:
```c
void LoadConfigData()
{
    // ... existing code ...
    gStereoMode = config->stereo_mode;
    gStereoSeparation = config->stereo_separation;
    // ... etc
}
```

---

## Integration Checklist

- [ ] Extend CONFIG_SAVE_HEADER with stereo fields
- [ ] Create stereo_camera.h/cpp with eye offset logic
- [ ] Create stereo_composite.h/cpp with anaglyph shader
- [ ] Create stereo_debug.h/cpp with logging
- [ ] Modify DrawGame() to call StereoRenderGame2() when stereo enabled
- [ ] Add launcher GUI stereo options screen
- [ ] Wire stereo settings to config save/load
- [ ] Test simple anaglyph rendering
- [ ] Test debug logging output
- [ ] Test settings persistence (save/load)

## Testing Checklist (Phase 1)

- [ ] Launch game with anaglyph disabled → renders normally
- [ ] Launch game with anaglyph enabled → sees 3D effect with red/cyan glasses
- [ ] Toggle stereo on/off during gameplay → no crashes
- [ ] Adjust separation slider → 3D effect intensity changes
- [ ] Swap eyes checkbox → left/right reversed
- [ ] Debug logging enabled → see rendering traces
- [ ] Save settings → settings persist across game restart
- [ ] Load from config.ini → settings loaded on startup

## Notes

- Render-to-texture for left/right eyes may require OpenGL context changes; verify compatible with PsyX renderer
- Anaglyph compositing can be done in shader (most efficient) or CPU (easier to debug)
- Multiplayer stereo is deferred (complex: 4x rendering cost, split-screen layout questions)
- All eye offset math should be adjustable without recompiling (via sliders)

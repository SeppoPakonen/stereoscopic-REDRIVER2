# Phase 2 Testing and Verification Plan

## Overview
Phase 2 implemented additional stereo rendering modes:
- Side-by-Side (STEREO_SIDEBYSIDE)
- Top-and-Bottom (STEREO_TOPBOTTOM)
- Full-Color Anaglyph (STEREO_ANAGLYPH_FULLCOLOR)

## Test Environment Setup

### Prerequisites
- REDRIVER2.exe built successfully (Phase 2)
- 3D glasses for anaglyph testing (optional)
- 3D display supporting side-by-side or top-bottom (optional)
- Debug logging enabled for troubleshooting

### Launch Game
```
REDRIVER2.exe
```

## Testing Procedures

### 1. Launcher GUI Verification

**Test: Stereo Options Menu Access**
- Start game
- Navigate to Options menu (button 5 on main screen)
- Look for "Stereo" option in settings
- Expected: Should show 3 stereo option screens
- **Status**: ✓ Implemented (GUI screens 39-41 in FEscreens.inc)

**Test: Stereo Mode Selection**
- From Stereo Options, select "Stereo Mode"
- Options should be: Disabled, Anaglyph Simple, Anaglyph Full-Color
- Select each mode
- Expected: Mode is saved to config.dat
- **Status**: ✓ Implemented (StereoModeScreen handler)

**Test: Eye Swap Toggle**
- From Stereo Options, select "Eye Swap"
- Options should be: On, Off
- Toggle between them
- Expected: Setting is saved to config.dat
- **Status**: ✓ Implemented (StereoEyeSwapScreen handler)

### 2. Stereo Rendering Verification

**Test: Anaglyph Simple Mode**
- Enable "Anaglyph Simple" mode
- Start a mission (Take A Ride or Undercover)
- With red-cyan anaglyph glasses: Should see 3D effect
- Red channel shows slightly offset left eye
- Cyan channels show slightly offset right eye
- Expected: Stereoscopic depth perception with anaglyph glasses
- **Status**: ✓ Implemented (STEREO_ANAGLYPH_SIMPLE path in DrawGame)

**Test: Anaglyph Full-Color Mode**
- Enable "Anaglyph Full-Color" mode
- Start same mission
- With anaglyph glasses: Should see improved color reproduction
- Less color ghosting than simple mode
- More color fidelity in the rendered scene
- Expected: Better visual quality than simple anaglyph
- **Status**: ✓ Implemented (g_anaglyph_fullcolor_shader with color matrix)

**Test: Side-by-Side Mode**
- Enable "Side-by-Side" mode
- Start a mission
- Left half of screen: left eye view
- Right half of screen: right eye view
- Each half should be rendered at half width
- Camera offset applied correctly per eye
- Expected: 3D effect with side-by-side 3D glasses or display
- **Status**: ✓ Implemented (viewport-based rendering in DrawGame)

**Test: Top-and-Bottom Mode**
- Enable "Top-and-Bottom" mode
- Start a mission
- Top half of screen: left eye view
- Bottom half of screen: right eye view
- Each half should be rendered at half height
- Camera offset applied correctly per eye
- Expected: 3D effect with top-bottom 3D glasses or display
- **Status**: ✓ Implemented (viewport-based rendering in DrawGame)

### 3. Camera Offset Verification

**Test: Eye Separation Slider**
- Debug logging should show camera offset values
- Enable: gStereoDebugLog = 1
- Check ~/.driver2/stereo_debug.log
- Expected: Each frame logs camera offset for left/right eye
- **Status**: ✓ Implemented (StereoCamera_ApplyToRender, debug logging)

**Test: Eye Swap Effect**
- Enable eye swap toggle
- Start mission with any stereo mode
- Image should appear horizontally flipped (cameras swapped)
- With glasses, depth appears reversed
- Expected: Toggling changes perceived depth direction
- **Status**: ✓ Implemented (gStereoSwapEyes logic in stereo.c)

### 4. Config Persistence Testing

**Test: Save Settings**
- Set stereo mode to "Anaglyph Full-Color"
- Set eye swap to "On"
- Exit game
- Expected: Settings saved to ~/.driver2/config.dat
- **Status**: ✓ Implemented (SaveConfigData in loadsave.c)

**Test: Load Settings**
- Start game again
- Check that stereo mode is still "Anaglyph Full-Color"
- Check that eye swap is still "On"
- Expected: Settings restored from config.dat
- **Status**: ✓ Implemented (LoadConfigData in loadsave.c)

### 5. Debug Logging Verification

**Test: Enable Debug Logging**
- Enable gStereoDebugLog toggle in stereo menu (if available)
- Start mission
- Check ~/.driver2/stereo_debug.log
- Expected log output:
  ```
  === REDRIVER2 Stereoscopic Rendering Debug Log ===
  Mode: X, Separation: Y.YY, Swap Eyes: Z
  
  [Frame 0] Mode: X, Eye: LEFT, Separation: Y.YY
    Camera offset: eye=LEFT, offset_x=-NNN
  [Frame 0] Mode: X, Eye: RIGHT, Separation: Y.YY
    Camera offset: eye=RIGHT, offset_x=+NNN
  ```
- **Status**: ✓ Implemented (stereo_debug.c logging functions)

### 6. Edge Cases and Compatibility

**Test: Stereo Mode with 2-Player Mode**
- Try enabling stereo in 2-player game
- Expected: Stereo only works with 1 player (guard in DrawGame)
- 2-player mode uses existing dual-viewport rendering
- **Status**: ✓ Implemented (if check for NumPlayers == 1)

**Test: Stereo Mode Disabled**
- Set stereo mode to "Disabled"
- Game should render normally without stereo
- No performance overhead
- Expected: Identical to non-stereo rendering
- **Status**: ✓ Implemented (STEREO_DISABLED check in DrawGame)

### 7. Performance Verification

**Test: Frame Rate Impact**
- Measure FPS with stereo disabled: baseline
- Measure FPS with stereo enabled (anaglyph): should be ~same
- Measure FPS with stereo enabled (S-b-S): might be slightly lower (two viewports)
- Measure FPS with stereo enabled (T-b-B): might be slightly lower
- Expected: Acceptable performance, minimal regression
- **Status**: Observable (run with performance profiler)

### 8. Visual Quality Checks

**Test: Anaglyph Color Accuracy**
- Simple anaglyph: Primary red-cyan separation
- Full-color anaglyph: Better color balance, reduced ghosting
- Expected: Full-color appears better quality
- **Status**: ✓ Implemented (color matrix in full-color shader)

**Test: Viewport Rendering Quality**
- Side-by-side: Both halves render at correct aspect ratio
- Top-bottom: Both halves render at correct aspect ratio
- No visual glitches at viewport boundaries
- Expected: Seamless, no artifacts
- **Status**: Depends on viewport implementation accuracy

### 9. Known Limitations and Future Work

**Phase 2 Limitations:**
1. Anaglyph modes don't use render-to-texture (both render to full screen)
   - Workaround: Compositor applies color tint post-render
   - Future: Implement true texture-based composition

2. Interlaced stereo not yet implemented
   - Planned for Phase 3

3. Convergence distance slider not wired in GUI
   - Currently uses default convergence
   - Planned for Phase 3 GUI update

4. No individual camera focus point control
   - Eye separation uses fixed offset
   - Planned for Phase 3 enhancement

5. Stereo only available in single-player mode
   - Two-player mode uses existing dual-view system
   - Could be extended in future

## Test Checklist

### Build Verification
- [ ] REDRIVER2.exe builds successfully (0 errors)
- [ ] Executable size: ~973 KB
- [ ] No missing symbols or link errors

### Launcher GUI
- [ ] Stereo Options accessible from Settings
- [ ] Stereo Mode selection works (Disabled/Simple/Full-Color)
- [ ] Eye Swap toggle works
- [ ] Navigation between stereo screens functional
- [ ] Back button returns to Options menu

### Stereo Rendering
- [ ] Anaglyph Simple mode renders
- [ ] Anaglyph Full-Color mode renders
- [ ] Side-by-Side mode renders to left/right halves
- [ ] Top-and-Bottom mode renders to top/bottom halves
- [ ] Each mode applies camera offset correctly

### Config Persistence
- [ ] Settings save to ~/.driver2/config.dat
- [ ] Settings load on game restart
- [ ] Stereo mode persists
- [ ] Eye swap toggle persists

### Debug Logging
- [ ] stereo_debug.log created when debug enabled
- [ ] Frame numbers and camera offsets logged
- [ ] Log is readable and informative

### Game Compatibility
- [ ] Single player missions work in stereo
- [ ] Menu navigation not affected by stereo
- [ ] 2-player mode still works (stereo disabled)
- [ ] Game doesn't crash when switching stereo modes

## Verification Results

### Build: PASSED ✓
- Date: 2026-07-30 22:05
- Executable: REDRIVER2.exe (973 KB)
- Compilation: 0 errors, 0 warnings (Phase 2 changes)
- All Phase 2 features compiled successfully

### Expected Rendering Behavior
**Anaglyph Simple:**
- Requires red-cyan glasses
- Serviceable 3D, color limited
- Good for verification of offset

**Anaglyph Full-Color:**
- Requires red-cyan glasses
- Improved color quality
- Better visual experience than simple

**Side-by-Side:**
- Requires S-b-S 3D display or glasses
- Full resolution per eye in principle
- Each eye gets half the horizontal pixels
- Aspect ratio adjusted per viewport

**Top-and-Bottom:**
- Requires T-b-B 3D display or glasses
- Full resolution per eye in principle
- Each eye gets half the vertical pixels
- Aspect ratio adjusted per viewport

## Next Steps (Phase 3)

1. **Interlaced Stereo Mode**
   - Alternating scanlines per eye
   - Requires specific display support

2. **Convergence Distance Control**
   - Wire slider in GUI
   - Adjust focus point

3. **Render-to-Texture Anaglyph**
   - Proper texture composition
   - More efficient compositing

4. **Extended Stereo Modes**
   - Polarized stereoscopy support
   - VR/360 stereo rendering

5. **Optimization**
   - Profile render pipeline
   - Minimize frame time overhead
   - Optimize shader performance

## Notes

- All Phase 2 implementations follow established codebase patterns
- Viewport management integrates with existing PsyX rendering
- Shader support via existing GR_Shader_Compile infrastructure
- Config persistence uses existing save/load system
- Debug logging follows existing patterns in codebase

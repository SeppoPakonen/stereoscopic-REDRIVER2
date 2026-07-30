# Phase 3 Task #10: Testing & Verification Guide

## Overview

This guide provides comprehensive testing procedures for the Render-to-Texture Anaglyph Composition implementation.

## Pre-Testing Checklist

- [ ] Code compiles without errors
- [ ] Visual Studio 2019 or later available
- [ ] GPU supports OpenGL 2.0+ with framebuffer objects
- [ ] Debug build or release build with symbols available
- [ ] REDRIVER2 game executable built successfully

## Build Instructions

### Using Visual Studio

```batch
cd build
msbuild REDRIVER2.sln /p:Configuration=Release /p:Platform=x64 /maxcpucount:4
```

### Using Premake

```batch
premake5 vs2019
cd build
msbuild REDRIVER2.sln /p:Configuration=Release /p:Platform=x64
```

Expected output: `REDRIVER2.exe` in `bin/x64/Release/`

## Test Categories

### 1. Compilation & Build Verification

**Objective**: Ensure code compiles without errors or warnings

**Steps**:
1. Build entire solution
2. Check for any errors (not just warnings)
3. Look for these specific files in output:
   - `stereo_compositor.obj`
   - `stereo_performance_monitor.obj`

**Expected Results**:
- Zero compilation errors
- No linker errors
- No missing symbol errors
- Executable generates successfully

**Verification Command**:
```batch
echo %ERRORLEVEL%  # Should be 0 if successful
```

---

### 2. Anaglyph Mode Functionality

**Objective**: Verify anaglyph stereo rendering works with RTT

**Test 2.1: Simple Red-Cyan Anaglyph**

1. Launch REDRIVER2
2. Enable stereo mode: Press `S` (or configure launcher for STEREO_ANAGLYPH_SIMPLE)
3. Put on red-cyan 3D glasses
4. Observe game world in 3D

**Expected Visual Results**:
- Both eyes' images visible and distinct
- Red-cyan color separation visible
- Depth effect apparent when wearing glasses
- No visible flicker or instability
- No ghosting artifacts (previous render visible)

**Performance Check**:
- Frame rate should be stable
- FPS should be similar to baseline (not 2x slower)

---

**Test 2.2: Full-Color Anaglyph**

1. Launch REDRIVER2
2. Enable stereo mode: STEREO_ANAGLYPH_FULLCOLOR
3. Observe game world

**Expected Visual Results**:
- Better color reproduction than simple anaglyph
- Reduced color ghosting
- Smoother color transitions
- More natural-looking colors

---

### 3. Performance Testing

**Objective**: Verify performance improvement from render-to-texture optimization

**Test 3.1: Frame Time Measurement**

**Setup**:
1. Enable `gStereoDebugLog = 1` for detailed output
2. Launch REDRIVER2 in a simple scene (e.g., parked car)
3. Let game run for 10+ seconds to warm up

**Measurement Points**:
- Enable frame time display (if available in launcher)
- Observe console output for timing information

**Expected Performance**:
```
Before RTT (double render):
- Frame time: ~30-35ms (RTX 2060, 1024x768)
- FPS: ~28-33 fps
- Two full scene renders

After RTT (optimized):
- Frame time: ~15-18ms (RTX 2060, 1024x768)
- FPS: ~56-67 fps
- One scene render + one composition pass
- Improvement: ~45-50%
```

**Note**: Exact numbers depend on:
- GPU model and capabilities
- Screen resolution
- Scene complexity (vehicles, pedestrians, traffic)
- Driver optimization

---

**Test 3.2: Complex Scene Performance**

**Setup**:
1. Load a busy scene with lots of traffic
2. Enable dynamic object rendering
3. Measure frame time during high-traffic situations

**Expected Results**:
- RTT should reduce overhead even in complex scenes
- Frame time should remain more stable
- GPU utilization should be more consistent

---

**Test 3.3: Fallback Mode Performance**

**Setup** (Advanced):
1. Force fallback by modifying code: `g_compositor.use_render_to_texture = 0`
2. Rebuild and test
3. Compare frame times

**Expected Results**:
- Fallback mode should be similar to previous implementation
- Fallback should preserve functionality if RTT unavailable
- No crashes or undefined behavior

---

### 4. Visual Quality Testing

**Objective**: Ensure no visual regressions or artifacts

**Test 4.1: Anaglyph Color Accuracy**

**Setup**:
1. Render a scene with distinct colors
2. Examine with 3D glasses

**Verification**:
- [ ] Red objects appear red in left eye, suppressed in right
- [ ] Cyan objects appear in right eye only
- [ ] Mixed colors render naturally
- [ ] No extreme color separation
- [ ] No noticeable banding or posterization

---

**Test 4.2: Ghosting and Artifacts**

**Verification**:
- [ ] No double vision or ghosting
- [ ] Clean edges between objects
- [ ] No flickering or temporal instability
- [ ] Smooth transitions across viewport
- [ ] No "trails" behind moving objects

---

**Test 4.3: Side-by-Side and Top-Bottom Modes**

**Test Setup**:
1. Switch to STEREO_SIDEBYSIDE mode
2. Verify left half shows left eye, right half shows right eye
3. Switch to STEREO_TOPBOTTOM mode
4. Verify top half shows left eye, bottom half shows right eye

**Expected Results**:
- Clear separation of eyes
- Proper viewport division
- No bleeding between eyes
- Correct texture composition

---

### 5. Debug Logging Verification

**Objective**: Verify compositor logging provides accurate information

**Test 5.1: Enable Debug Logging**

**Setup**:
1. Set `gStereoDebugLog = 1` in code or launcher
2. Launch REDRIVER2
3. Enable stereo rendering
4. Capture console output

**Expected Log Sequence** (for anaglyph mode):
```
StereoCompositor_Init: 1024x768
StereoCompositor: Created left texture 0x12345..., right texture 0x67890...
StereoCompositor: Created FBO 1 for texture 12345
StereoCompositor: Created FBO 2 for texture 67890
StereoCompositor: Created fullscreen quad VAO 3, VBO 4
StereoCompositor_Init complete. RTT enabled

DrawGame: anaglyph rendering with RTT
StereoCompositor_BeginEyeRender: eye=0 (RTT), FBO=1
StereoCompositor_EndEyeRender
StereoCompositor_BeginEyeRender: eye=1 (RTT), FBO=2
StereoCompositor_EndEyeRender
StereoCompositor_Composite: mode=1, RTT=yes
StereoCompositor: Rendered fullscreen quad
StereoCompositor_Composite complete
```

**Verification**:
- [ ] All initialization messages present
- [ ] FBO creation successful (non-zero handles)
- [ ] BeginEyeRender/EndEyeRender pairs balanced
- [ ] Composite called with correct mode
- [ ] No error messages or warnings

---

### 6. Mode Switching Test

**Objective**: Ensure stereo mode can be switched during gameplay

**Test 6.1: Runtime Mode Switching**

**Setup**:
1. Launch REDRIVER2 in anaglyph mode
2. Play for ~10 seconds
3. Switch to side-by-side mode (if launcher supports)
4. Play for ~10 seconds
5. Switch back to anaglyph

**Expected Behavior**:
- [ ] Mode switches without crashes
- [ ] Rendering continues smoothly
- [ ] No artifacts or corruption
- [ ] Compositor properly reinitializes
- [ ] Frame rate remains stable

---

### 7. Fallback Testing (Advanced)

**Objective**: Verify graceful degradation if RTT unavailable

**Test 7.1: Force Fallback Mode**

**Setup** (Requires code modification):
```c
// In StereoCompositor_Init, add:
g_compositor.use_render_to_texture = 0;  // Force fallback
```

1. Recompile
2. Launch REDRIVER2 with anaglyph mode
3. Observe behavior

**Expected Results**:
- [ ] Game continues to function
- [ ] Anaglyph stereo still works (with fallback double-render)
- [ ] Debug log shows "RTT disabled"
- [ ] Performance reverts to original (2x render time)
- [ ] No crashes or undefined behavior

---

### 8. Edge Case Testing

**Test 8.1: Rapid Mode Switching**

1. Switch stereo modes as fast as possible
2. Observe for crashes or memory issues

**Expected**: No hangs or crashes

---

**Test 8.2: Resolution Changes**

1. Change resolution while stereo is enabled
2. Verify compositor updates texture sizes

**Expected**: Smooth transition, proper re-initialization

---

**Test 8.3: Alt-Tab and Focus Loss**

1. Enable stereo rendering
2. Alt-Tab to another application
3. Return to REDRIVER2

**Expected**: Graphics state preserved, no corruption

---

**Test 8.4: Extreme Settings**

1. Set convergence to extreme values
2. Set separation to extreme values
3. Observe for visual artifacts

**Expected**: Visual degradation acceptable, no crashes

---

## Performance Measurement Protocol

### Baseline Measurement

1. Disable stereo (STEREO_DISABLED)
2. Run in identical scene for 60+ frames
3. Record average frame time: `baseline_fps`

### RTT Measurement

1. Enable STEREO_ANAGLYPH_SIMPLE
2. Run in identical scene for 60+ frames
3. Record average frame time: `rtt_fps`

### Calculation

```
Improvement = (1 - (baseline_fps / rtt_fps)) * 100%
Expected: ~45-50% (for anaglyph modes)
```

### Reporting Template

```
PERFORMANCE TEST RESULTS
========================
GPU: [Model, VRAM, Driver Version]
Resolution: [e.g., 1024x768]
Scene Complexity: [Simple/Medium/Complex]

Baseline Frame Time:        [ms]
RTT Frame Time:             [ms]
Improvement:                [%]
Baseline FPS:               [fps]
RTT FPS:                    [fps]

Render Left Eye:            [ms]
Render Right Eye:           [ms]
Composite Pass:             [ms]

Status: [PASS/FAIL]
Notes: [Any observations]
```

---

## Verification Checklist

### Functionality
- [ ] Anaglyph modes render with RTT
- [ ] Both eye images properly captured
- [ ] Composition shader applies correctly
- [ ] No visual ghosting or artifacts
- [ ] Mode switching works correctly
- [ ] Fallback works if RTT unavailable

### Performance
- [ ] Frame time improved by ~45-50%
- [ ] GPU utilization more efficient
- [ ] No memory leaks
- [ ] Stable frame rate (no stuttering)
- [ ] Composition overhead < 3ms

### Debugging
- [ ] Debug log shows proper initialization
- [ ] FBO creation succeeds
- [ ] Texture binding works correctly
- [ ] Shader uniforms set properly
- [ ] No OpenGL errors in debug output

### Compatibility
- [ ] Compiles on Visual Studio 2019+
- [ ] Works on various GPU architectures
- [ ] No platform-specific issues
- [ ] Gracefully falls back if needed

---

## Known Issues & Workarounds

### Issue: Black Screen with RTT

**Cause**: OpenGL framebuffer object not created successfully

**Workaround**:
1. Check GPU driver is up to date
2. Verify OpenGL 2.0+ support
3. Check for error messages in debug log
4. Force fallback mode for testing

### Issue: Performance Not Improved

**Cause**: RTT overhead not offset by frame savings

**Workaround**:
1. Measure with complex scene (many objects)
2. Verify RTT actually enabled (check debug log)
3. Profile GPU vs CPU time
4. Check for other bottlenecks

### Issue: Mode Switching Crashes

**Cause**: Incomplete compositor state reset

**Workaround**:
1. Restart game between mode switches (during testing)
2. Check debug output for errors
3. Verify texture binding cleared

---

## Regression Test Suite

Run these tests to ensure no regressions:

### Non-Stereo Tests
- [ ] Single player campaigns work
- [ ] Multiplayer split-screen works
- [ ] Menus function normally
- [ ] Audio plays correctly
- [ ] Input handling works

### Stereo-Specific Tests
- [ ] All 6 stereo modes available
- [ ] Stereo modes toggle on/off smoothly
- [ ] Stereo settings persist
- [ ] Debug logging toggles correctly
- [ ] Convergence adjustment works

### Performance Tests
- [ ] Non-stereo FPS unchanged
- [ ] Stereo FPS improved from baseline
- [ ] No memory leaks detected
- [ ] No frame rate stuttering
- [ ] Consistent timing frame to frame

---

## Sign-Off

When all tests pass:

1. [ ] Compilation successful
2. [ ] All visual tests pass
3. [ ] Performance goals met (>40% improvement)
4. [ ] No regressions in non-stereo modes
5. [ ] Debug logging functions correctly
6. [ ] Fallback behavior verified

**Tester**: ____________
**Date**: ____________
**GPU Used**: ____________
**Resolution**: ____________
**Notes**: ____________

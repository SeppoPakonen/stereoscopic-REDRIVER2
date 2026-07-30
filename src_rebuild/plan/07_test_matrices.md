# Phase 3 Task #15: Test Matrices & Checklists

## Quick Navigation

1. [Master Test Matrix](#master-test-matrix) - All tests at a glance
2. [Stereo Mode × Game Mode Matrix](#stereo-mode--game-mode-matrix) - Coverage table
3. [Manual Testing Checklists](#manual-testing-checklists) - Step-by-step guides
4. [Performance Baseline Template](#performance-baseline-template) - Performance tracking
5. [Regression Report Template](#regression-report-template) - Issue documentation

---

## Master Test Matrix

Complete test coverage overview. Mark each cell with:
- ✓ = Pass
- ✗ = Fail
- ⚠ = Known issue
- ⊘ = Not applicable

### Legend

| Mode | Abbreviation |
|------|--------------|
| DISABLED | DIS |
| ANAGLYPH_SIMPLE | ANA-S |
| ANAGLYPH_FULLCOLOR | ANA-F |
| SIDEBYSIDE | SIDE |
| TOPBOTTOM | TOPB |
| INTERLACED | INTL |
| POLARIZED | POLAR |
| CHECKERBOARD | CHECK |

---

## Stereo Mode × Game Mode Matrix

### Complete Coverage Matrix

```
╔════════════════════╦═════╦═════╦═════╦═════╦═════╦═════╦═════╦════════╗
║ Game Mode          ║ DIS ║ANA-S║ANA-F║SIDE ║TOPB ║INTL ║POLAR║CHECK   ║
╠════════════════════╬═════╬═════╬═════╬═════╬═════╬═════╬═════╬════════╣
║ Undercover Mission ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ Take A Ride        ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ Arcade - Getaway   ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ Arcade - Chase     ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ Arcade - Race      ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ Arcade - Survivor  ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ 2-Player Split     ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ Cutscenes          ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
║ Menus/UI           ║  ✓  ║     ║     ║     ║     ║     ║     ║        ║
╚════════════════════╩═════╩═════╩═════╩═════╩═════╩═════╩═════╩════════╝
```

---

## Manual Testing Checklists

### Regression Testing Checklist - Non-Stereo

**Test Session**: ____________  
**Date**: ____________  
**Tester**: ____________  
**Build**: ____________  

#### Mission Testing (Stereo DISABLED)

```
UNDERCOVER MISSION
═════════════════════════════════════════════════════════════════

Mission 1 (Tutorial):
☐ Game launches without errors
☐ Tutorial instructions display correctly
☐ Basic controls respond as expected
☐ Camera works smoothly
☐ Audio plays without issues
☐ Mission completes cleanly
☐ No visual glitches observed
☐ Frame rate stable (record: ___ FPS)

Mission 2-5 Story:
☐ Each mission starts correctly
☐ Difficulty increases appropriately
☐ Gameplay mechanics unchanged
☐ AI behaves as expected
☐ Checkpoints function properly
☐ Save/load works correctly
☐ No regressions vs previous version
☐ All story elements present

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

#### Take A Ride Checklist

```
TAKE A RIDE MODE
═════════════════════════════════════════════════════════════════

Free Driving (10 minutes):
☐ Game starts in Take A Ride mode
☐ Player vehicle spawns correctly
☐ Camera position appropriate
☐ Vehicle physics responsive
☐ Acceleration/braking smooth
☐ Steering responsive and balanced
☐ Traffic appears and flows naturally
☐ Pedestrians move realistically
☐ Collisions register correctly
☐ No physics glitches
☐ Frame rate stable (record: ___ FPS)

Emergency Calls:
☐ Emergency dispatch works
☐ Mission objectives clear
☐ Time limits accurate
☐ Objectives completable
☐ Reward system works

Garage Access:
☐ Garage accessible from menu
☐ Vehicle selection works
☐ Vehicle switching works
☐ Vehicle stats display correctly

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

#### Multiplayer 2-Player Checklist

```
2-PLAYER SPLIT-SCREEN MODE
═════════════════════════════════════════════════════════════════

Arcade Race:
☐ Two players can start race
☐ Both viewports render correctly
☐ Left player camera on left half (or similar)
☐ Right player camera on right half (or similar)
☐ Both vehicles controllable independently
☐ Race timing works for both
☐ Lap counting accurate
☐ Winner determined correctly
☐ Frame rate acceptable (record: ___ FPS)

Arcade Deathmatch:
☐ Both players spawn correctly
☐ Both can move and attack
☐ Scoring works for both
☐ Health display accurate
☐ Time limit works
☐ Winner determined correctly

Arcade Survival:
☐ Wave progression works
☐ Both players can attack enemies
☐ Enemy AI spawns correctly
☐ Score updates for both
☐ Wave transitions smooth

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

#### Cutscenes & Menus Checklist

```
CUTSCENES & CINEMATICS
═════════════════════════════════════════════════════════════════

Mission Intro:
☐ Cutscene plays without stuttering
☐ Audio sync correct
☐ Video quality intact
☐ Subtitle display (if applicable)
☐ Transition to gameplay smooth

Mission Outro:
☐ Outro plays correctly
☐ Stats/rewards display
☐ Next mission loads properly

In-Engine Cinematics:
☐ Camera movements smooth
☐ Character animations play
☐ Audio/video sync maintained
☐ Lighting effects intact

MENU NAVIGATION
═════════════════════════════════════════════════════════════════

Main Menu:
☐ Menu displays all options
☐ Options: New Game, Continue, Settings, Quit
☐ Navigation responsive
☐ Menu music plays
☐ No visual artifacts

Settings Menu:
☐ All setting categories accessible
☐ Graphics options present
☐ Audio options present
☐ Controls options present
☐ Stereo options accessible
☐ Settings apply correctly

HUD Display (During Gameplay):
☐ Minimap displays
☐ Health/armor shows
☐ Weapon info shows
☐ Objective text clear
☐ Time display accurate
☐ Speed display (if applicable)
☐ No HUD overlap issues

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

---

### Stereo Mode Testing Checklist

**Test Session**: ____________  
**Date**: ____________  
**Tester**: ____________  
**Build**: ____________  
**Stereo Mode**: ____________  

#### Game Launch & Main Menu

```
STEREO MODE: [Mode Name]
═════════════════════════════════════════════════════════════════

Game Launch:
☐ Game starts with selected mode enabled
☐ Stereo effect visible (if applicable)
☐ No crashes on mode activation
☐ No immediate artifacts

Main Menu in Stereo:
☐ Menu text readable
☐ Buttons/options distinguishable
☐ Menu background visible
☐ Navigation responsive
☐ No text overlap or distortion
☐ Appropriate 3D depth for UI

Stereo Mode Specific (ANAGLYPH):
☐ Red/Cyan colors appropriate
☐ No color bleeding between channels
☐ Ghosting minimized
☐ Effect visible without glasses (if simple mode)

Stereo Mode Specific (SIDEBYSIDE):
☐ Left image on left half
☐ Right image on right half
☐ No overlap between halves
☐ Suitable for cross-eyed viewing
☐ Suitable for parallel viewing

Stereo Mode Specific (TOPBOTTOM):
☐ Left image on top half
☐ Right image on bottom half
☐ No vertical offset issues
☐ Suitable for vertical displays

Stereo Mode Specific (INTERLACED):
☐ Scanline pattern visible (optional)
☐ Minimal flicker observed
☐ Alternating scanlines correct
☐ No horizontal banding

Stereo Mode Specific (POLARIZED):
☐ Full color information visible
☐ Full brightness per eye
☐ Appropriate for polarized glasses
☐ No crosstalk visible

Stereo Mode Specific (CHECKERBOARD):
☐ Checkerboard pattern visible
☐ Pixel-level interlacing correct
☐ No obvious artifacts
☐ Higher effective resolution than scanline

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

#### Stereo Gameplay Testing

```
STEREO MODE: [Mode Name]
═════════════════════════════════════════════════════════════════

Mission Gameplay (First 2 minutes):
☐ Stereo effect maintained throughout
☐ Camera movements smooth in stereo
☐ Road/environment has correct depth
☐ Vehicles positioned correctly in 3D space
☐ No ghosting or color artifacts
☐ No flickering (except interlaced expected)
☐ Frame rate acceptable (record: ___ FPS)
☐ Audio sync maintained

High-Speed Driving:
☐ Stereo stable at high speeds
☐ No separation/convergence artifacts
☐ No additional flickering
☐ Performance maintained
☐ Frame rate doesn't drop significantly

Dense Traffic Scene:
☐ Multiple vehicles render correctly in stereo
☐ No visual artifacts with crowds
☐ Depth perception maintained
☐ Performance acceptable (record: ___ FPS)

Free Cam / Looking Around:
☐ Camera panning smooth in stereo
☐ Head movement responds correctly
☐ Vertical camera smooth
☐ Convergence point appropriate

Pause Menu in Stereo:
☐ Pause menu displays
☐ Options readable
☐ Settings accessible
☐ Can change stereo settings while paused

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

#### Stereo Settings Control Testing

```
STEREO MODE: [Mode Name]
═════════════════════════════════════════════════════════════════

Separation Slider (if available):
☐ Slider accessible in menu
☐ Values range displayed (0.0 - 3.0 or similar)
☐ Slider responds to input
☐ Real-time preview works
☐ Value displays accurately
☐ Extreme values handled (0, max)
☐ Can adjust during gameplay (if paused)

Convergence Slider (if available):
☐ Slider accessible in menu
☐ Values range displayed (0.5 - 100.0 or similar)
☐ Slider responds to input
☐ Real-time preview works
☐ Value displays accurately
☐ Extreme values handled
☐ Auto convergence option available

Eye Swap Toggle:
☐ Setting accessible
☐ Toggle switches effect correctly
☐ Left/right images swap
☐ Setting persists after reload

Debug Options (if available):
☐ Debug log toggle available
☐ Logs generated when enabled
☐ No performance impact when disabled
☐ Log content useful for debugging

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

---

### Performance Testing Checklist

**Test Session**: ____________  
**Date**: ____________  
**Tester**: ____________  
**Build**: ____________  

#### Frame Time Baseline (Stereo DISABLED)

```
BASELINE PERFORMANCE (STEREO DISABLED)
═════════════════════════════════════════════════════════════════

Menu Idle (30 seconds):
☐ Average FPS: ________
☐ Min FPS: ________
☐ Max FPS: ________
☐ Frame time variance: ________
☐ GPU usage: ________%
☐ CPU usage: ________%
☐ RAM usage: ________ MB

Simple Mission (5 minutes):
☐ Average FPS: ________
☐ Min FPS: ________
☐ Max FPS: ________
☐ Frame time variance: ________
☐ GPU usage: ________%
☐ CPU usage: ________%
☐ RAM usage: ________ MB

Dense Traffic (5 minutes):
☐ Average FPS: ________
☐ Min FPS: ________
☐ Max FPS: ________
☐ Frame time variance: ________
☐ GPU usage: ________%
☐ CPU usage: ________%
☐ RAM usage: ________ MB

High-Speed Chase (5 minutes):
☐ Average FPS: ________
☐ Min FPS: ________
☐ Max FPS: ________
☐ Frame time variance: ________
☐ GPU usage: ________%
☐ CPU usage: ________%
☐ RAM usage: ________ MB

Notes:
[Any observations during baseline]
```

#### Frame Time Stereo Comparison

```
STEREO PERFORMANCE: [Mode Name]
═════════════════════════════════════════════════════════════════

Menu Idle (30 seconds):
☐ Average FPS: ________ (Overhead: ____%)
☐ Min FPS: ________ (Δ: ____%)
☐ Max FPS: ________ (Δ: ____%)
☐ Frame time variance: ________ (Δ: ____%)
☐ GPU usage: ________% (Δ: ____%)
☐ CPU usage: ________% (Δ: ____%)
☐ RAM usage: ________ MB (Δ: ____%)

Simple Mission (5 minutes):
☐ Average FPS: ________ (Overhead: ____%)
☐ Min FPS: ________ (Δ: ____%)
☐ Max FPS: ________ (Δ: ____%)
☐ Frame time variance: ________ (Δ: ____%)
☐ GPU usage: ________% (Δ: ____%)
☐ CPU usage: ________% (Δ: ____%)
☐ RAM usage: ________ MB (Δ: ____%)

Dense Traffic (5 minutes):
☐ Average FPS: ________ (Overhead: ____%)
☐ Min FPS: ________ (Δ: ____%)
☐ Max FPS: ________ (Δ: ____%)
☐ Frame time variance: ________ (Δ: ____%)
☐ GPU usage: ________% (Δ: ____%)
☐ CPU usage: ________% (Δ: ____%)
☐ RAM usage: ________ MB (Δ: ____%)

High-Speed Chase (5 minutes):
☐ Average FPS: ________ (Overhead: ____%)
☐ Min FPS: ________ (Δ: ____%)
☐ Max FPS: ________ (Δ: ____%)
☐ Frame time variance: ________ (Δ: ____%)
☐ GPU usage: ________% (Δ: ____%)
☐ CPU usage: ________% (Δ: ____%)
☐ RAM usage: ________ MB (Δ: ____%)

Performance Summary:
☐ Stereo overhead < 20%: YES / NO / BORDERLINE
☐ Frame time stability maintained: YES / NO
☐ No memory leaks detected: YES / NO
☐ GPU not bottlenecked: YES / NO

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

---

### Settings Persistence Checklist

**Test Session**: ____________  
**Date**: ____________  
**Tester**: ____________  

```
CONFIGURATION PERSISTENCE TEST
═════════════════════════════════════════════════════════════════

Test 1: Stereo Mode Persistence
────────────────────────────────
1. Launch game
2. Go to Settings → Stereo Options
3. Select "SIDEBYSIDE" mode
4. Exit game
5. Relaunch game
6. Check Settings → Stereo Options

✓ SIDEBYSIDE mode is selected: YES / NO
✓ No errors on reload: YES / NO
✓ Setting persisted correctly: YES / NO

Test 2: Separation Value Persistence
─────────────────────────────────────
1. Launch game
2. Go to Settings → Stereo Options
3. Set Separation to 1.5
4. Exit game
5. Relaunch game
6. Check Settings → Stereo Options

✓ Separation value is 1.5: YES / NO
✓ No errors on reload: YES / NO
✓ Setting persisted correctly: YES / NO

Test 3: Convergence Value Persistence
──────────────────────────────────────
1. Launch game
2. Go to Settings → Stereo Options
3. Set Convergence to 15.0
4. Exit game
5. Relaunch game
6. Check Settings → Stereo Options

✓ Convergence value is 15.0: YES / NO
✓ No errors on reload: YES / NO
✓ Setting persisted correctly: YES / NO

Test 4: Eye Swap Persistence
────────────────────────────
1. Launch game
2. Go to Settings → Stereo Options
3. Enable "Swap Eyes"
4. Exit game
5. Relaunch game
6. Check Settings → Stereo Options

✓ Eye Swap is enabled: YES / NO
✓ No errors on reload: YES / NO
✓ Setting persisted correctly: YES / NO

Test 5: Multiple Settings Together
───────────────────────────────────
1. Launch game
2. Go to Settings → Stereo Options
3. Set:
   - Mode: ANAGLYPH_FULLCOLOR
   - Separation: 0.8
   - Convergence: 10.0
   - Eye Swap: ON
4. Exit game
5. Relaunch game
6. Check all settings

✓ All settings preserved together: YES / NO
✓ No partial saves or conflicts: YES / NO
✓ Values match what was set: YES / NO

Test 6: Reset to Defaults
──────────────────────────
1. Launch game
2. Go to Settings → Stereo Options
3. Set custom values (as in Test 5)
4. Find "Reset to Defaults" option
5. Confirm reset
6. Check all settings

✓ Mode reset to DISABLED: YES / NO
✓ Separation reset to 1.0: YES / NO
✓ Convergence reset to auto: YES / NO
✓ Eye Swap reset to OFF: YES / NO

Test 7: Config File Validation
───────────────────────────────
1. Set stereo configuration
2. Exit game
3. Open save folder (usually user/.config or appdata)
4. Locate config file
5. Open with text editor

✓ Config file exists: YES / NO
✓ File is readable (valid format): YES / NO
✓ Stereo settings present in file: YES / NO
✓ Values match what was set: YES / NO

Test 8: Corrupted Config Handling
──────────────────────────────────
1. Exit game cleanly
2. Open config file in text editor
3. Introduce syntax error (e.g., remove closing bracket)
4. Save config file
5. Launch game

✓ Game launches without crash: YES / NO
✓ Game uses default settings: YES / NO
✓ No error messages in console: YES / NO
✓ Config file can be regenerated: YES / NO

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

---

### Edge Cases Testing Checklist

**Test Session**: ____________  
**Date**: ____________  
**Tester**: ____________  

```
EDGE CASES & ROBUSTNESS
═════════════════════════════════════════════════════════════════

Test 1: Extreme Separation Values
──────────────────────────────────
1. Launch game in stereo mode
2. Set Separation to minimum (0.0)
3. Play 1 minute, observe effect
4. Set Separation to maximum (3.0+)
5. Play 1 minute, observe effect
6. Set back to normal (1.0)

☐ No crash with min value: YES / NO
☐ Minimal 3D effect visible at min: YES / NO
☐ No crash with max value: YES / NO
☐ Strong 3D effect visible at max: YES / NO
☐ Return to normal works: YES / NO

Test 2: Extreme Convergence Values
───────────────────────────────────
1. Launch game in stereo mode
2. Set Convergence to minimum (0.5)
3. Play 1 minute, observe planes
4. Set Convergence to maximum (100.0+)
5. Play 1 minute, observe planes
6. Set back to normal

☐ No crash with min value: YES / NO
☐ Plane very close with min: YES / NO
☐ No crash with max value: YES / NO
☐ Plane very far with max: YES / NO
☐ Eye strain acceptable: YES / NO

Test 3: Rapid Parameter Switching
──────────────────────────────────
1. During gameplay, rapidly adjust separation slider
2. Drag back and forth 10 times in 10 seconds
3. Observe for artifacts
4. Then rapidly adjust convergence slider

☐ No crashes during rapid changes: YES / NO
☐ No visual glitches: YES / NO
☐ Smooth transitions visible: YES / NO
☐ Frame rate doesn't stutter: YES / NO

Test 4: Resolution Changes
───────────────────────────
1. Start game at 1920x1080 in stereo
2. Play 1 minute
3. Change resolution to 1280x720
4. Observe transition
5. Play 1 minute
6. Change resolution to 2560x1440
7. Observe transition

☐ No crash on resolution change: YES / NO
☐ Stereo effect updates correctly: YES / NO
☐ No distortion at new resolution: YES / NO
☐ Performance acceptable: YES / NO
☐ Can switch back without issues: YES / NO

Test 5: Fullscreen/Windowed Toggle
───────────────────────────────────
1. Start game in fullscreen stereo mode
2. Toggle to windowed
3. Observe stereo rendering
4. Toggle back to fullscreen
5. Observe stereo rendering

☐ Works in fullscreen: YES / NO
☐ Works in windowed: YES / NO
☐ Toggle transition smooth: YES / NO
☐ Stereo effect maintained: YES / NO

Test 6: Alt-Tab Handling
────────────────────────
1. Start game in stereo mode
2. Play for 30 seconds
3. Press Alt-Tab to switch away
4. Switch back to game
5. Observe rendering
6. Continue playing 1 minute

☐ Alt-Tab successful: YES / NO
☐ Game continues or pauses correctly: YES / NO
☐ Stereo effect maintained on return: YES / NO
☐ No rendering corruption: YES / NO
☐ No crashes: YES / NO

Test 7: Mode Switching During Gameplay
───────────────────────────────────────
1. Start game in ANAGLYPH_SIMPLE mode
2. Play mission for 1 minute
3. Pause game
4. Go to Settings → Stereo
5. Switch to SIDEBYSIDE mode
6. Resume game
7. Observe transition

☐ Mode switch accepted while paused: YES / NO
☐ New mode visible after resume: YES / NO
☐ No crashes during switch: YES / NO
☐ Gameplay continues smoothly: YES / NO

Test 8: Rapid Mode Cycling
───────────────────────────
1. During gameplay, cycle through all 8 modes rapidly
2. Change mode every 3-5 seconds
3. Complete 2 full cycles
4. Observe for crashes or artifacts

☐ All modes accessible: YES / NO
☐ No crashes during cycling: YES / NO
☐ No permanent visual artifacts: YES / NO
☐ Game remains responsive: YES / NO

Issues Found:
- [Issue 1]
- [Issue 2]

Notes:
[Additional observations]
```

---

## Performance Baseline Template

**Test Date**: __________  
**Build Hash**: __________  
**Tester**: __________  
**Hardware**:
- CPU: __________
- GPU: __________
- RAM: __________
- OS: __________

### Baseline Results (Stereo DISABLED)

| Scenario | Avg FPS | Min FPS | Max FPS | Frame Variance | GPU % | CPU % | RAM MB |
|----------|---------|---------|---------|-----------------|-------|-------|--------|
| Menu Idle | | | | | | | |
| Simple Mission | | | | | | | |
| Dense Traffic | | | | | | | |
| High-Speed Chase | | | | | | | |

### Stereo Overhead Measurements

| Mode | Overhead % | Status | Notes |
|------|-----------|--------|-------|
| ANAGLYPH_SIMPLE | | ☐ Pass ☐ Fail | |
| ANAGLYPH_FULLCOLOR | | ☐ Pass ☐ Fail | |
| SIDEBYSIDE | | ☐ Pass ☐ Fail | |
| TOPBOTTOM | | ☐ Pass ☐ Fail | |
| INTERLACED | | ☐ Pass ☐ Fail | |
| POLARIZED | | ☐ Pass ☐ Fail | |
| CHECKERBOARD | | ☐ Pass ☐ Fail | |

**Target**: All modes < 20% overhead

### Memory Leak Testing (30 min continuous play)

| Test Scenario | Start RAM | End RAM | Leak | Status |
|---------------|----------|--------|------|--------|
| Simple Mission | | | MB | ☐ Pass ☐ Fail |
| Dense Traffic | | | MB | ☐ Pass ☐ Fail |

**Target**: < 5 MB leak per 30 minutes

---

## Regression Report Template

**Report Date**: __________  
**Build Hash**: __________  
**Tester**: __________  
**Test Duration**: __________ minutes

### Executive Summary

**Total Tests Run**: __________  
**Passed**: __________ (__%)  
**Failed**: __________ (__%)  
**Blocked**: __________ (__%)  

**Critical Issues**: __________  
**High Priority**: __________  
**Medium Priority**: __________  
**Low Priority**: __________  

**Overall Status**: ☐ PASS ☐ FAIL ☐ CONDITIONAL PASS

### Test Categories Summary

| Category | Pass | Fail | Notes |
|----------|------|------|-------|
| Non-Stereo Regression | | | |
| Stereo Mode Compatibility | | | |
| Settings Persistence | | | |
| Input Handling | | | |
| Audio Sync | | | |
| Performance | | | |
| Edge Cases | | | |
| Cross-Platform | | | |

### Regressions Detected

#### Critical Regressions

| # | Test Case | Expected | Actual | Severity | Status |
|---|-----------|----------|--------|----------|--------|
| R-1 | | | | CRITICAL | ☐ Open ☐ Fixed |
| R-2 | | | | CRITICAL | ☐ Open ☐ Fixed |

#### High-Priority Regressions

| # | Test Case | Expected | Actual | Severity | Status |
|---|-----------|----------|--------|----------|--------|
| R-3 | | | | HIGH | ☐ Open ☐ Fixed |
| R-4 | | | | HIGH | ☐ Open ☐ Fixed |

#### Medium-Priority Regressions

| # | Test Case | Expected | Actual | Severity | Status |
|---|-----------|----------|--------|----------|--------|
| R-5 | | | | MEDIUM | ☐ Open ☐ Fixed |

### Performance Analysis

**Target Stereo Overhead**: < 20%

| Mode | Actual Overhead | Target | Status |
|------|-----------------|--------|--------|
| ANAGLYPH_SIMPLE | ___% | < 20% | ☐ Pass ☐ Fail |
| ANAGLYPH_FULLCOLOR | ___% | < 20% | ☐ Pass ☐ Fail |
| SIDEBYSIDE | ___% | < 20% | ☐ Pass ☐ Fail |
| TOPBOTTOM | ___% | < 20% | ☐ Pass ☐ Fail |
| INTERLACED | ___% | < 20% | ☐ Pass ☐ Fail |
| POLARIZED | ___% | < 20% | ☐ Pass ☐ Fail |
| CHECKERBOARD | ___% | < 20% | ☐ Pass ☐ Fail |

### Known Issues Found

| # | Issue | Severity | Mode(s) | Workaround | Fix Status |
|---|-------|----------|---------|-----------|-----------|
| KI-1 | | | | | ☐ Open ☐ In Progress ☐ Fixed |
| KI-2 | | | | | ☐ Open ☐ In Progress ☐ Fixed |

### Platform-Specific Findings

**Windows 10**:
- [Findings]

**Windows 11**:
- [Findings]

**Linux (if tested)**:
- [Findings]

### Recommendations

**Must Fix (Before Release)**:
1. [Issue]
2. [Issue]

**Should Fix (Before Release)**:
1. [Issue]
2. [Issue]

**Nice to Have (Future)**:
1. [Issue]
2. [Issue]

### Sign-Off

**Test Lead**: _____________________ Date: _________

**QA Manager**: _____________________ Date: _________

**Development Lead**: _____________________ Date: _________

---

## Test Execution Instructions

### Quick Test Session (30 minutes)

**Smoke Test - Critical Paths Only**:

1. Launch game in stereo DISABLED mode (5 min)
   - Load mission, play briefly, check no crashes
2. Launch game in ANAGLYPH_SIMPLE mode (5 min)
   - Check stereo effect visible, no immediate errors
3. Launch game in SIDEBYSIDE mode (5 min)
   - Check mode displays correctly
4. Launch game in TOPBOTTOM mode (5 min)
   - Check mode displays correctly
5. Launch game in INTERLACED mode (5 min)
   - Check mode displays correctly
6. Settings persistence check (5 min)
   - Set mode, exit, relaunch, verify persisted

**Report Results**: Pass/Fail + any critical issues

### Standard Test Session (2-3 hours)

Run these checklists in order:

1. Regression Testing Checklist (45 min)
   - Complete non-stereo baseline
   - Verify no regressions vs previous version

2. Choose 2-3 Stereo Modes to test:
   - Stereo Mode Testing Checklist (30 min per mode)
   - Complete gameplay tests for each mode

3. Settings Persistence Checklist (30 min)
   - Run all 8 tests

4. Performance Testing Checklist (30 min)
   - Collect baseline and stereo measurements

5. Edge Cases Checklist (30 min)
   - Run priority edge cases

**Report Results**: Complete test matrix + regression report

### Extended Test Session (Full Day)

1. **Morning (4 hours)**
   - Non-stereo regression (full suite)
   - All game modes in DISABLED mode
   - Verify baseline unchanged

2. **Lunch Break**

3. **Afternoon (4 hours)**
   - Test 4 stereo modes (2 hours)
   - Settings persistence (1 hour)
   - Edge cases (1 hour)

4. **Follow-up**
   - Remaining 3 stereo modes (next day)
   - Cross-platform testing (as resources permit)

**Report Results**: Comprehensive report + known issues list

---

## Notes for Testers

### General Guidelines

1. **Test Systematically**: Follow checklists in order, don't skip
2. **Document Everything**: Record pass/fail for each item
3. **Note Issues**: Write down issues as found, don't wait until end
4. **Capture Evidence**: Screenshot visual issues, save performance logs
5. **Test Thoroughly**: Spend time exploring, not just following checklist
6. **Communicate**: Report blocking issues immediately

### Performance Testing Tips

1. **Warm Up**: Run 1-2 minute to let system stabilize
2. **Exclude Outliers**: First few frames have variable performance
3. **Multiple Runs**: Run each test 2-3 times, average results
4. **Monitor Throughout**: Watch GPU/CPU usage during entire test
5. **Clean System**: Close background apps before testing
6. **Log Everything**: Save performance logs for comparison

### Visual Quality Assessment

1. **Ghosting**: Look for color shadows in anaglyph modes
2. **Flicker**: Scan for interlaced mode scanline flicker
3. **Separation**: Verify left/right images correctly separated
4. **Alignment**: Check for vertical/horizontal alignment issues
5. **Color Accuracy**: Verify color is not distorted
6. **Brightness**: Check that both eyes see appropriate brightness

### Common Issues to Watch For

- Color bleeding in anaglyph modes
- Interlaced mode flicker
- Missing stereo effect
- Performance drops under load
- Settings not persisting
- Controller/keyboard not responding in stereo menus
- Audio sync issues
- Crashes on mode switches
- Artifacts at screen edges

---


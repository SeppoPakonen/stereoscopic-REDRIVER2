# REDRIVER2 Stereoscopic Rendering - Troubleshooting Guide

**Version**: 1.0  
**Last Updated**: July 30, 2026  
**Status**: Production Ready

---

## Table of Contents

1. [Quick Diagnosis](#quick-diagnosis)
2. [Display Configuration Issues](#display-configuration-issues)
3. [Stereo Effect Problems](#stereo-effect-problems)
4. [Visual Artifacts](#visual-artifacts)
5. [Performance Issues](#performance-issues)
6. [Compatibility Issues](#compatibility-issues)
7. [Advanced Troubleshooting](#advanced-troubleshooting)
8. [Getting Help](#getting-help)

---

## Quick Diagnosis

### Initial Checklist

Before deep troubleshooting, verify these basics:

**Is Stereo Enabled?**
```
Game Menu → Settings → Graphics → Stereo Mode
Expected: NOT "Disabled"
```

**Is Mode Correct for Equipment?**
- Have red-cyan glasses? → Use Anaglyph modes
- Have 3D TV? → Use Side-by-Side or Polarized
- Have 3D Projector? → Use Interlaced
- Have special display? → Verify mode type

**Is Hardware Initialized?**
- 3D TV: Enable 3D mode on TV remote
- 3D Projector: Verify stereo output enabled
- Polarized display: Check polarization settings
- Glasses: Verify working and clean

**Performance Acceptable?**
- Press TAB to show FPS counter
- Should be 60 FPS (or your target)
- Drop > 20% = see Performance Issues section

---

## Display Configuration Issues

### "Stereo Mode Shows as Unsupported"

**Symptom**: Game says "Stereo mode not supported" or "No stereo hardware detected"

**Diagnosis**:
1. Check GPU supports your stereo mode
2. Verify display/hardware connected properly
3. Update GPU drivers

**Solution**:

#### For Anaglyph Modes
- Works on any display
- Should always be supported
- If not: GPU drivers corrupted
- **Fix**: Reinstall GPU drivers

**Steps**:
1. Go to NVIDIA/AMD website
2. Download latest drivers for your GPU
3. Uninstall current drivers (Device Manager → Uninstall)
4. Restart
5. Install new drivers
6. Restart game

#### For 3D TV Modes (Side-by-Side, Top-Bottom)
- Requires 3D-capable TV
- TV must support HDMI 3D
- Correct HDMI cable required

**Fix Checklist**:
```
☐ TV model supports 3D (check manual)
☐ Using HDMI cable (not DVI or VGA)
☐ HDMI version 1.4+ (supports 3D)
☐ HDMI port on TV supports 3D (usually port 1 or 2)
☐ TV 3D mode enabled in TV menu
☐ Game resolution matches TV's 3D support
  (typically 1920×1080 at 60Hz)
```

**Test**:
1. Enable 3D mode on TV (remote button, often "3D" or "Picture" menu)
2. Should see notice on TV screen
3. Launch game with Side-by-Side or Top-Bottom mode
4. Check status in game settings (should show "Supported")

#### For 3D Projector (Interlaced)
- Requires stereo-capable projector
- Active shutter or polarized glasses needed
- 120 Hz refresh rate minimum

**Fix**:
1. Verify projector supports stereo:
   - Check manual for "3D" or "stereo" support
   - Look for stereo lens (on projector or control box)
   - Verify active shutter support
2. Enable stereo on projector:
   - Use remote control → 3D mode
   - Set to "interlaced" or "scanline" mode
   - Verify refresh rate ≥ 120 Hz
3. Sync glasses:
   - Put on active shutter glasses
   - Verify they're synchronized with projector
   - Test by pressing sync button on glasses

#### For Polarized Display
- Requires polarized 3D display
- Most common in professional/cinema

**Fix**:
1. Verify display is polarized:
   - Check monitor specs for "polarized stereo"
   - Look for passive glasses (not electronic)
2. Proper display connection:
   - Use recommended cable (usually DVI-D or HDMI)
   - Avoid adapters (can lose polarization)
3. Display settings:
   - Set to 3D or stereo mode
   - Verify "side-by-side" or "polarized" option
   - Check refresh rate (60 Hz typical)

---

### "3D Mode Enabled But No 3D Effect"

**Symptom**: Game runs in stereo mode but image appears flat (2D)

**Causes**:
1. Glasses not working
2. Display not in 3D mode
3. Wrong stereo mode selected
4. Stereoscopic rendering disabled

**Diagnosis**:

**Step 1: Check Glasses**
```
Test 1: Look at 3D object with glasses off
  - If you see red/cyan or double image: glasses needed!
  
Test 2: Put on glasses
  - Should see single 3D image
  - If still see colors/blur: glasses may be faulty
```

**Step 2: Verify Display 3D Mode**
```
3D TV:
  - Look at TV status bar (usually bottom)
  - Should show "3D Activated" or similar
  - If not: press 3D button on remote

3D Projector:
  - Check projector status menu
  - Should show "3D Mode: ON" or "Stereo: Active"
  - If not: enable via remote control

Polarized Display:
  - Check monitor OSD (on-screen display)
  - Should show "3D: On" or "Stereo: Active"
```

**Step 3: Verify Game Settings**
```
Game Menu → Settings → Graphics → Stereo
  - Mode: NOT "Disabled"
  - Mode matches equipment:
    • Anaglyph? → Simple or Full Color
    • 3D TV? → Side-by-Side or Polarized
    • Projector? → Interlaced
    • Polarized? → Polarized mode
```

**Step 4: Check Convergence Setting**
```
If settings correct but still no 3D:
  - Set Convergence Distance to 10.0 (closer focus)
  - Look at nearby objects
  - Should see depth effect around close objects
  
  If still nothing:
    - Lower Eye Separation to 0.5 (reduced effect)
    - But should still see SOME depth
```

**Advanced Diagnosis**:

Check debug log:
```
1. Enable debug logging:
   Settings → Graphics → Stereo → Debug Logging: ON
   
2. Load a game and play for 30 seconds
   
3. Check log file:
   [Game Folder]/debug/stereo_debug.log
   
4. Look for errors:
   ERROR: Stereo not initialized
   ERROR: Compositor failed
   ERROR: Mode not supported
```

---

## Stereo Effect Problems

### "3D Effect Appears Inverted (Inside-Out)"

**Symptom**: Objects that should be "in front" appear "behind" and vice versa

**Cause**: Left/right eyes rendering to wrong image

**Solution**: Enable "Swap Eyes"

**Steps**:
1. Game Menu → Settings → Graphics → Stereo
2. Find "Swap Eyes" setting
3. Change from OFF to ON
4. Return to game
5. 3D effect should now appear correct

**Verification**:
- Look at player car
- Should appear in front of screen
- Distant objects should appear behind screen

---

### "3D Effect Too Subtle"

**Symptom**: Can barely see depth effect, very weak 3D

**Causes**:
1. Eye Separation too low
2. Convergence distance wrong
3. Display not in correct 3D mode
4. Glasses not working properly

**Solution**:

**Step 1: Increase Eye Separation**
```
Current value: Check in Settings
Typical range: 0.1 - 3.0 (default 1.0)

Try: Increase to 1.3 or 1.5
  - More exaggerated 3D effect
  - May cause eye strain (dial back if so)
```

**Step 2: Check Convergence**
```
Set to Auto (0) or specific value (10-50)
  - Convergence affects where depth is focused
  - Auto usually best (calculated per frame)
  
Try manual values:
  20: Close focus (for 3rd person view)
  50: Medium focus (typical gameplay)
  100: Far focus (wide open areas)
```

**Step 3: Verify Display 3D Enabled**
- 3D TV: Check TV shows "3D: Active" status
- Projector: Verify stereo enabled
- Display: Check polarization on

**Step 4: Test Glasses**
```
Put on glasses and look at test image:
  - Red/cyan vertical stripes pattern
  - Should see each color distinctly
  - If see blending/blur: glasses faulty

Alternative test:
  - Print red-cyan test pattern from web
  - View with glasses
  - Should see 3D effect clearly
```

---

### "3D Effect Too Strong (Excessive Depth)"

**Symptom**: Depth effect excessive, causes eye strain, feels unnatural

**Causes**:
1. Eye Separation too high
2. Convergence point too close
3. Screen too close to face
4. Wrong display distance setting

**Solution**:

**Step 1: Reduce Eye Separation**
```
Current value: Check in Settings
Try: Decrease by 0.2-0.3

Example progression:
  Start: 1.4 (too strong)
  Try: 1.2 (better)
  Try: 1.0 (default, usually good)
  Try: 0.8 (subtle, comfortable)
```

**Step 2: Check Viewing Distance**
```
Recommended distances:
  PC Monitor: 60-80 cm (2-2.5 feet)
  3D TV: 1.5-2.5 meters (5-8 feet)
  Projector: 2-4 meters (6-12 feet)

Sitting too close intensifies depth effect.
  - Back away from screen if possible
  - Increases comfort and naturalness
```

**Step 3: Adjust Convergence**
```
If everything feels "pushed out" of screen:
  - Increase convergence (move focus point farther)
  - Try: 30-50 for medium focus
  - This pulls depth "into" the screen
```

**Step 4: Take Breaks**
```
Eye strain from excessive 3D:
  - Play 30 minutes, break 5 minutes
  - Reduce separation while taking break
  - Normal eye function resumes in ~10-30 minutes
```

---

### "Depth Focus Doesn't Feel Right"

**Symptom**: 3D effect present but focus point seems wrong

**Cause**: Convergence distance incorrect

**Solution**:

**Understanding Convergence**:
```
Convergence = where the eyes "cross" in 3D space

Too Close (convergence value too low):
  - Objects at distance appear flat
  - All focus at near ground
  
Too Far (convergence value too high):
  - Nearby objects appear flat
  - All focus on horizon
  
Right (convergence ~20-50):
  - Player car focused at natural depth
  - Ground and far objects both have depth
```

**Step 1: Set Auto Convergence**
```
Settings → Stereo → Convergence Distance
  - Set to 0 (AUTO)
  - Game calculates based on camera distance
  - Usually most natural feeling
```

**Step 2: Manual Convergence (if needed)**
```
For specific focus preferences:
  
Narrow/confined area (alley):
    Try: 10-20 (close focus)
    
Street gameplay (typical):
    Try: 25-40 (medium focus)
    
Wide open areas:
    Try: 50-75 (far focus)
    
City overview:
    Try: 100+ (very far focus)
```

**Step 3: Real-time Adjustment**
```
While playing (if UI supports it):
  - Press Stereo Settings key (F12)
  - Adjust convergence slider while playing
  - Watch game preview
  - Close when comfortable
```

---

## Visual Artifacts

### "Ghosting Visible (Color Fringing)"

**Symptom**: Anaglyph mode shows red/cyan halos around objects

**Cause**: Color crosstalk in anaglyph composition

**Solution**: Use higher-quality anaglyph matrix

**Steps**:
1. Game Menu → Settings → Graphics → Stereo
2. Look for "Anaglyph Matrix" or "Color Matrix"
3. Try each option:
   - Simple → Fast but high ghosting
   - Optimized → Better quality
   - Dubois → Highest quality, scientifically optimized
   - Green-Magenta → Alternative if have those glasses
4. Select one and play test

**Recommendations**:
- Weak GPU? → Simple matrix
- Good GPU? → Optimized or Dubois
- Want best? → Dubois (small performance cost)

**Advanced**:
If ghosting still visible after matrix selection:
1. Check "Chromatic Aberration" setting (if available)
2. Enable if disabled
3. This corrects lens-based color fringing
4. Further reduces ghosting

---

### "Flickering in Interlaced Mode"

**Symptom**: Interlaced mode shows temporal flicker/strobing

**Causes**:
1. Projector refresh rate too low
2. Temporal filtering disabled
3. Motion too fast for update rate

**Solution**:

**Step 1: Increase Refresh Rate**
```
Interlaced stereo needs high refresh rate:
  Minimum: 120 Hz (still may see flicker)
  Recommended: 240 Hz (smooth)
  
Check current:
  Projector menu → Display settings
  Should show 120 Hz or higher
  
If lower:
  - May not support higher
  - Consider different stereo mode
  - Or upgrade projector
```

**Step 2: Enable Temporal Filtering**
```
If temporal filtering available in settings:
  - Should be ON by default
  - Helps blend frames to reduce flicker
  - Slight image blur as tradeoff
  
Try quality presets:
  Performance → Balanced (enables filtering)
  Balanced or higher → Full filtering
```

**Step 3: Reduce Motion Speed**
```
Flicker more visible in fast movement:
  - Play slower action sequences
  - Flicker less apparent in slow scenes
  - Adjust camera movement speed if possible
```

**If still flickering**:
- Interlaced mode may not be ideal for your setup
- Try different stereo mode if available
- Check projector manual for stereo optimization

---

### "Image Blurry or Soft Focus"

**Symptom**: 3D image appears blurry compared to 2D mode

**Causes**:
1. Anaglyph color bleed
2. Temporal filtering too strong
3. Display resolution reduced
4. Interlaced mode at low refresh rate

**Solution**:

**For Anaglyph Modes**:
1. Try Full Color instead of Simple
2. Select better color matrix (Dubois preferred)
3. Both reduce color bleed that causes blur

**For All Modes**:
1. Check resolution setting:
   Settings → Graphics → Stereo → Resolution
   - Should be "Native" (full resolution)
   - If set to 75% or 50%: increase to Native
   
2. For interlaced mode:
   - Increase refresh rate to 120+ Hz
   - Lower refresh = less vertical resolution
   - More flicker-blending = softer image

3. Check temporal filtering:
   - Disable if available and causing blur
   - Tradeoff: may cause flicker
   - Balance between blur and flicker

---

### "Color Accuracy Poor (Oversaturated/Washed Out)"

**Symptom**: Colors look wrong, too bright/dim, oversaturated

**Causes**:
1. Anaglyph color matrix not optimized
2. Tone mapping disabled
3. Display color calibration off
4. Quality preset too low

**Solution**:

**Step 1: Enable Tone Mapping**
```
Settings → Graphics → Stereo → Advanced
  - Look for "Tone Mapping" or "Color Correction"
  - Should be ON
  - Automatically adjusts for scene
```

**Step 2: Increase Quality Preset**
```
Settings → Graphics → Stereo → Quality
  Current: Performance or Balanced?
  Try: High (enables more color correction)
  Or: Ultra (full color optimization)
```

**Step 3: Select Better Anaglyph Matrix** (anaglyph only)
```
Matrix choice affects color accuracy:
  - Simple: Poor color (acceptable for quick test)
  - Optimized: Better color
  - Dubois: Excellent color reproduction
  - Select Dubois for best results
```

**Step 4: Calibrate Display**
```
3D TV/Monitor:
  1. Enter TV/Monitor settings menu
  2. Find Color or Picture settings
  3. Run color calibration tool (if available)
  4. Settings → Brightness, Contrast, Color balance
  
Manual adjustment:
  - If colors too bright: Lower brightness/contrast
  - If colors washed out: Increase saturation
  - If colors oversaturated: Decrease saturation
```

---

### "Viewport Edges Rough/Jagged"

**Symptom**: Edges where left/right images meet appear rough

**Causes**:
1. Edge blending disabled
2. Mode doesn't support edge smoothing
3. Anti-aliasing settings

**Solution**:

**Enable Edge Blending** (if available):
```
Settings → Graphics → Stereo → Advanced
  - Enable "Edge Blending"
  - Smooths viewport transitions
  - Slight performance cost
```

**Increase Quality Preset**:
```
Higher presets enable edge blending:
  Performance: No edge blending
  Balanced: Light edge blending
  High: Full edge blending
  Ultra: Advanced edge blending
  
Try: Increase to High or Ultra
```

**Note**: Some modes (anaglyph) don't have viewport edges since they combine images. Edge artifacts only visible in spatial separation modes (side-by-side, top-bottom).

---

## Performance Issues

### "Low Frame Rate / Stuttering"

**Symptom**: Game runs at < 50 FPS or feels choppy

**Check Baseline**:
```
First, verify non-stereo performance:
  1. Disable stereo: Settings → Stereo → Disabled
  2. Play same scene
  3. Check FPS (press TAB)
  4. Note baseline FPS
```

**If Non-Stereo Performance OK**:
Then stereo overhead is issue. See Stereo Performance Issues below.

**If Non-Stereo Performance Also Low**:
Then general GPU issue, not stereo-related. See General Performance Issues.

---

### Stereo Performance Issues

**Symptom**: Stereo FPS much lower than non-stereo baseline

**Diagnosis**:
1. Check which mode causing slowdown
2. Identify if mode-specific or system-wide
3. Test with different settings

**Steps**:

**Step 1: Identify Stereo Overhead**
```
Baseline FPS (stereo disabled): 60
Stereo FPS: 45
Overhead: (60 - 45) / 60 = 25%

Normal stereo overhead: 15-25%
High overhead: > 25%
```

**Step 2: Test Different Modes**
```
Try each stereo mode:
  Disabled: baseline FPS
  Anaglyph Simple: should be ~45-50 FPS
  Anaglyph Full Color: should be ~42-50 FPS
  Side-by-Side: should be ~45-50 FPS
  Interlaced: should be ~45-50 FPS
  Polarized: might be ~35-45 FPS (higher cost)
  
Worse performers indicate issues with that mode.
```

**Step 3: Lower Quality Preset**
```
Settings → Graphics → Stereo → Quality
  Current: Balanced or High?
  Try: Performance
  
Performance preset disables:
    - Chromatic aberration correction
    - Advanced edge blending
    - Tone mapping optimizations
    - Scene analysis
  
Should gain 2-5 FPS.
```

**Step 4: Reduce Eye Separation**
```
Settings → Graphics → Stereo → Eye Separation
  Current: 1.0?
  Try: 0.8 or 0.7
  
Lower separation slightly reduces GPU load
  (less difference between left/right images)
```

**Step 5: Disable Advanced Features**
```
If available in settings:
  - Disable edge blending
  - Disable tone mapping
  - Disable chromatic aberration
  - Each can save 1-2 FPS
```

**If Still Slow**:
→ GPU may be at limit. See general performance section.

---

### General Performance Issues

**Symptom**: Both stereo and non-stereo performance low

**Not a stereo problem** - general GPU limitation.

**Check GPU**:
```
Press TAB to show performance metrics:
  - GPU Usage should be 90-99%
  - If high: GPU is bottleneck
  
If not visible:
  Download GPU monitor (GPU-Z, afterburner, etc.)
  Check during gameplay
```

**If GPU Bottleneck**:

**Option 1: Lower Graphics Quality**
```
Settings → Graphics → Quality
  - Try: Medium or Low
  - Reduces polygon count, texture detail
  - Can gain 10-20 FPS
```

**Option 2: Lower Resolution**
```
Settings → Graphics → Resolution
  - Try: 1280×720 or 1600×900
  - Reduces pixels to render
  - Significant FPS improvement
```

**Option 3: Lower Target FPS**
```
Settings → Graphics → Frame Rate
  - Try: 30 FPS instead of 60
  - Halves GPU workload
  - Trade-off: less smooth motion
```

**Option 4: Hardware Upgrade**
```
If GPU at limit after quality reductions:
  - GPU may be below minimum specs
  - Recommend upgrading to:
    • NVIDIA GTX 1060 6GB or better
    • AMD RX 580 or better
    • Modern integrated graphics (Iris Xe, RTX 3050 Ti)
```

---

### CPU Performance Issues

**Symptom**: GPU usage low but FPS still low (CPU bottleneck)

**Check CPU**:
```
Use CPU monitor while playing:
  - One core at 95-100%? → Single-thread CPU issue
  - All cores high? → General CPU bottleneck
  
If single core maxed:
  - Game is single-threaded
  - Stereo doubles workload on that core
  - Limited by CPU architecture
```

**Solutions**:

**If Single-Core Bottleneck**:
1. Enable performance mode (Power Options → High Performance)
2. Close background apps
3. Reduce game quality to lower single-core workload
4. Consider CPU upgrade for future

**If Multi-Core Bottleneck**:
1. Close background applications
2. Disable visual effects in game
3. Reduce population/traffic (game settings)
4. Reduce resolution
5. Consider system upgrade

---

## Compatibility Issues

### "Stereo Works but Games Crashes"

**Symptom**: Stereo mode enabled, but game crashes during play

**Causes**:
1. Shader compilation error
2. Memory allocation failure
3. GPU driver issue
4. Mode not supported by GPU

**Solution**:

**Step 1: Check GPU Compatibility**
```
Visit GPU manufacturer website:
  NVIDIA: https://www.nvidia.com
  AMD: https://www.amd.com
  
Your GPU model → Specifications
  - Look for "OpenGL 3.3+" support
  - Check for stereo support
  - Verify not EOL (end of life)
```

**Step 2: Update GPU Drivers**
```
1. Download latest drivers for your GPU
2. Uninstall current drivers
   Device Manager → Display adapters → Uninstall
3. Restart
4. Install new drivers
5. Restart again
```

**Step 3: Try Different Stereo Mode**
```
If crash with Polarized mode:
  - Try Anaglyph or Side-by-Side instead
  - Polarized has highest requirements
  
If crash with Anaglyph:
  - Try Simple instead of Full Color
  - Simple is least GPU-intensive anaglyph
```

**Step 4: Check Log File**
```
Enable debug logging:
  Settings → Graphics → Stereo → Debug Logging: ON
  
Then play until crash.

Check log:
  [Game Folder]/debug/stereo_debug.log
  
Look for ERROR lines:
  - Shader compilation error?
  - Texture allocation failed?
  - FBO creation failed?
  
Share error details when reporting.
```

---

### "Mode Works on Other Computer But Not Mine"

**Symptom**: Same stereo mode works elsewhere but not on your PC

**Likely Cause**: GPU difference

**Solution**:

**Check GPU Compatibility Table**:

| Mode | Min GPU | Recommended |
|------|---------|-------------|
| Anaglyph | GTX 750 | GTX 960+ |
| Side-by-Side | GTX 960 | GTX 1660+ |
| Top-Bottom | GTX 960 | GTX 1660+ |
| Interlaced | GTX 960 | GTX 1660+ |
| Polarized | GTX 1080 | RTX 2070+ |
| Checkerboard | RTX 2070 | RTX 3080+ |

**Find Your GPU**:
1. Right-click Desktop → NVIDIA Control Panel (or AMD equivalent)
2. Look for GPU name
3. Compare to table above

**If Below Recommended**:
1. Try Performance quality preset (saves GPU)
2. Lower game graphics quality
3. Reduce resolution
4. Try different (less demanding) stereo mode
5. Consider upgrading GPU

---

### "Works on Windows But Not Linux"

**Stereo on Linux** requires:
1. GPU driver supporting stereo (nvidia)
2. OpenGL 3.3+ support
3. Proper X11 extension support

**Check Linux Support**:
```bash
glxinfo | grep stereo
# Should output: stereo: available

glxinfo | grep "OpenGL version"
# Should show OpenGL 3.3 or higher
```

**If Not Supported**:
1. Update GPU drivers to latest
2. Verify OpenGL version with glxinfo
3. Check if GPU supports stereo
   - Most modern NVIDIA cards do
   - AMD support varies
   - Intel integrated limited

**Recommendation**: Use anaglyph modes on Linux (work with any GPU)

---

## Advanced Troubleshooting

### Enabling Debug Mode

For detailed diagnostics:

```
1. Set environment variable:
   Windows: set RD2_STEREO_DEBUG=1
   Linux: export RD2_STEREO_DEBUG=1

2. Launch game from command line

3. Debug output prints to console

4. Play until issue reproduces

5. Check output for error messages
```

### Collecting Diagnostics

For issue reporting, gather:

```
1. Log file:
   [Game Folder]/debug/stereo_debug.log
   → Copy entire file

2. GPU info:
   Run GPU-Z or nvidia-smi
   → Save screenshot or output

3. System specs:
   Windows: dxdiag.exe → Save All Info
   Linux: inxi -F

4. Stereo settings:
   Screenshot of Settings → Graphics → Stereo panel

5. FPS/Performance:
   Screenshot of performance overlay (TAB key)

6. Steps to reproduce:
   Write exact steps that cause issue
```

### Profiling Performance

Generate detailed performance report:

```
1. Enable profiling:
   Settings → Graphics → Stereo → Debug: Profiling

2. Play game for 5-10 minutes

3. Exit to main menu

4. Check generated report:
   [Game Folder]/debug/stereo_profile_report.csv

5. Open in Excel/LibreOffice

6. Analyze:
   - Which component takes most time?
   - Is overhead > 20%?
   - Are there spikes?
```

---

### Manual Configuration Editing

For advanced tuning, edit config file directly:

```
File: [Game Folder]/config/stereo_config.ini

[STEREO]
mode=3                    # Stereo mode (0-7)
swap_eyes=0              # Swap left/right (0/1)
convergence_distance=0.0 # Auto (0) or value
eye_separation=1.0       # Separation amount
quality_preset=1         # 0=Performance, 3=Ultra
anaglyph_matrix=1        # Color matrix type
debug_log=1              # Enable logging (0/1)

[QUALITY]
enable_tone_mapping=1
enable_chromatic_aberration=1
enable_edge_blending=1
enable_scene_analysis=1

[PERFORMANCE]
profiling_enabled=1
optimization_flags=31    # Bitwise OR of optimization flags
```

**After editing**:
1. Save file
2. Restart game
3. Settings loaded on startup

---

## Getting Help

### Before Reporting Issue

Check this list:
- [ ] Tried all troubleshooting steps applicable to symptom?
- [ ] Verified non-stereo mode works?
- [ ] Updated GPU drivers?
- [ ] Checked stereo mode matches equipment?
- [ ] Reviewed relevant section in this guide?

### Information to Include

When reporting issue, provide:

1. **System Info**:
   - GPU model (GTX 1070, RTX 3060, RX 6800, etc.)
   - GPU VRAM (2GB, 4GB, 6GB, etc.)
   - CPU model
   - RAM amount
   - OS (Windows 10, Ubuntu 20.04, etc.)

2. **Stereo Setup**:
   - Stereo mode selected
   - Display type (3D TV, projector, anaglyph, etc.)
   - Glasses type (active shutter, passive, anaglyph, etc.)

3. **Issue Details**:
   - Exact symptom description
   - When it occurs (always, sometimes, specific scenes)
   - Steps to reproduce

4. **Performance Data**:
   - FPS with stereo disabled
   - FPS with stereo enabled
   - Frame time with stereo (TAB key)

5. **Log Files**:
   - stereo_debug.log
   - Any crash logs
   - Performance report (if generated)

6. **Screenshots**:
   - Settings panel screenshot
   - Performance overlay screenshot
   - Issue reproduction screenshot

### Support Resources

- **Quick Start**: stereo_quickstart.md
- **User Guide**: stereo_user_guide.md
- **Technical Details**: stereo_technical_reference.md
- **Performance Guide**: stereo_performance.md
- **Developer Guide**: stereo_developer_guide.md

### Common Issue Quick Links

- [No 3D effect](#stereo-effect-not-working) → See "3D Effect Problems"
- [Ghosting visible](#ghosting-visible-color-fringing) → See "Visual Artifacts"
- [FPS too low](#low-frame-rate--stuttering) → See "Performance Issues"
- [Crash on startup](#stereo-works-but-games-crashes) → See "Compatibility Issues"
- [Display not detected](#stereo-mode-shows-as-unsupported) → See "Display Configuration"

---

## FAQ for Troubleshooting

**Q: My GPU meets min specs but stereo is slow**  
A: Minimum specs allow basic functionality. For good performance, upgrade to recommended specs (see stereo_performance.md).

**Q: Stereo works sometimes but not always**  
A: Check if issue is temperature-related (GPU overheating). Use GPU monitor to check temps. Should stay below 80°C.

**Q: Can I use stereo with mods?**  
A: Yes, most mods compatible. Some mods that change rendering may conflict. Try disabling mods if issue occurs.

**Q: Does stereo work in multiplayer?**  
A: Yes, split-screen 2-player supports stereo. Online multiplayer depends on your connection.

**Q: Why is interlaced mode flickering?**  
A: Refresh rate too low. Need 120+ Hz, preferably 240 Hz. Check projector specs.

---

**Still need help?** See the other documentation files or contact support with details from "Getting Help" section above.

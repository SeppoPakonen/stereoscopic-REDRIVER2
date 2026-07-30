# REDRIVER2 Stereoscopic Rendering - Quick Start Guide

**Version**: 1.0  
**Last Updated**: July 30, 2026  
**Status**: Production Ready

---

## Table of Contents

1. [5-Minute Setup](#5-minute-setup)
2. [Basic Usage](#basic-usage)
3. [Common Scenarios](#common-scenarios)
4. [FAQ](#faq)
5. [Next Steps](#next-steps)

---

## 5-Minute Setup

### Prerequisites
- REDRIVER2 game executable
- 3D glasses (optional, depending on stereo mode)
- Display that supports your chosen stereo mode

### Step 1: Launch the Game
Start REDRIVER2 normally. The game will run in non-stereo mode by default.

### Step 2: Access Stereo Settings
1. From the main menu, navigate to **Settings** → **Graphics**
2. Look for **Stereo Rendering** option
3. Change from **Disabled** to your preferred mode

### Step 3: Select Stereo Mode (30 seconds)

Choose one of 8 modes based on your equipment:

| Mode | Equipment | Best For |
|------|-----------|----------|
| **Anaglyph Simple** | Red-cyan glasses | Quick testing, low cost |
| **Anaglyph Full Color** | Red-cyan glasses | Better colors with glasses |
| **Side-by-Side** | 3D monitor/TV | Horizontal display |
| **Top-Bottom** | 3D TV | Vertical stacking |
| **Interlaced** | 3D projector | Alternating scanlines |
| **Polarized** | Polarized display | Theater/professional |
| **Checkerboard** | Special display | Pixel-level separation |
| **Disabled** | (None) | Standard 2D gameplay |

### Step 4: Adjust Settings (2 minutes)

Once you select a mode:

1. **Convergence Distance**: Controls where the 3D effect "focuses"
   - Slide left for close objects (near crossover point)
   - Slide right for distant objects
   - Default (0) = automatic based on camera distance

2. **Eye Separation**: Controls 3D depth intensity
   - Lower = subtle effect, less eye strain
   - Higher = dramatic depth, more separation
   - Start at 1.0 (default) and adjust to comfort

3. **Swap Eyes** (optional): Reverses left/right eye rendering
   - Only needed if 3D effect appears reversed

4. **Quality Preset**: Choose visual quality level
   - Performance: Maximum FPS, basic quality
   - Balanced: Good balance (default)
   - High: Better visuals, minor FPS impact
   - Ultra: Maximum quality, significant FPS cost

### Step 5: Play (1 minute)

Put on appropriate 3D glasses and start playing! The game will display in 3D mode.

**Pro Tip**: For anaglyph glasses, dim the room lighting slightly for better contrast.

---

## Basic Usage

### During Gameplay

#### Adjusting 3D in Real-Time
- Press **Stereo Settings Key** (default: F12) to pause and show settings panel
- Adjust sliders while watching game preview
- Return to gameplay with **ESC**

#### Switching Modes
1. Press **Stereo Settings Key** to open settings
2. Select new mode from dropdown
3. Game switches immediately (no restart needed)

#### Disabling Stereo Temporarily
- Set mode to "Disabled"
- Game continues playing in 2D
- All settings preserved

### Menu Navigation
All menus work normally in stereo mode. The 2D interface renders at screen depth (no 3D effect on menus).

### Multiplayer (2-Player Split-Screen)
Stereo works with 2-player mode:
- Left screen: Left eye rendering of player 1 view
- Right screen: Right eye rendering of player 2 view
- Each player sees their own 3D view

---

## Common Scenarios

### Scenario 1: "I Have Red-Cyan Glasses"

1. **Select Mode**: Anaglyph Simple or Anaglyph Full Color
2. **Quality**: Choose based on GPU (Low-Mid end = Simple, High end = Full Color)
3. **Lighting**: Dim the room for better color perception
4. **Convergence**: Start at default, adjust if 3D looks "inside-out"
5. **Separation**: Start at 1.0, lower if eyes get tired

**Best For**: Undercover missions, Take A Ride mode (less fast action)

### Scenario 2: "I Have a 3D Smart TV"

1. **Select Mode**: 
   - Horizontal TV = **Side-by-Side**
   - Vertical TV = **Top-Bottom**
   - TV with polarized support = **Polarized**

2. **TV Settings**: Activate 3D mode on your TV remote
3. **Glasses**: Put on active shutter or passive polarized glasses
4. **Quality**: Set to High or Ultra (TVs typically have good GPU)
5. **Play**: Enjoy full resolution 3D

**Best For**: Single-player campaigns, Take A Ride mode

### Scenario 3: "I Have a 3D Projector with Interlaced Support"

1. **Select Mode**: **Interlaced**
2. **Projector Settings**: Enable interlaced stereo mode
3. **Glasses**: Use active shutter glasses compatible with projector
4. **Quality**: High (Interlaced mode is GPU-efficient)
5. **Separation**: Start at 1.2 (slightly higher for projection)

**Best For**: Cinematic campaigns, cutscenes

### Scenario 4: "I Just Want to Try It"

1. **Print red-cyan glasses** or buy cheap pair (~$5)
2. **Select Mode**: Anaglyph Simple
3. **Convergence**: Default
4. **Separation**: 0.8-1.2
5. **Play**: 15-minute test

**Tip**: Eye strain indicates convergence or separation needs adjustment

### Scenario 5: "I Have High Eye Strain"

1. **Lower Separation**: Reduce from 1.0 to 0.5-0.7
2. **Auto Convergence**: Enable (default is auto)
3. **Switch Modes**: Try different mode if one causes strain
4. **Take Breaks**: Stereo viewing can cause fatigue—rest eyes every 30 minutes
5. **Check Glasses**: Worn/damaged glasses increase strain

---

## FAQ

### Q: Will stereo mode work without 3D glasses?
**A**: Only if using Anaglyph modes (red-cyan). Other modes require:
- 3D TV/monitor for Side-by-Side, Top-Bottom, Polarized
- 3D Projector for Interlaced
- Other modes need hardware support

For hardware-based modes without proper display, game still runs in stereo mode but won't create 3D effect.

### Q: Which mode gives the best quality?
**A**: Quality depends on equipment:
- **Best overall**: Polarized (if available)
- **Best anaglyph**: Anaglyph Full Color (high-end GPUs)
- **Best accessibility**: Anaglyph Simple (works with any display)

### Q: Does stereo impact performance?
**A**: Yes, typically 15-25% frame time increase:
- GPU-intensive (most impact)
- Can be reduced with lower quality settings
- Use performance monitoring (stereo_performance.md) to measure

### Q: Can I use stereo with controller/keyboard?
**A**: Yes, input works normally. Stereo only affects rendering, not controls.

### Q: What's the difference between convergence and separation?
**A**: 
- **Separation**: How far apart the two eye images are (intensity of 3D)
- **Convergence**: Where the eyes "cross" (where near/far objects appear)

### Q: Will it work on my Linux system?
**A**: Yes, if SDL2 and OpenGL drivers support stereo. Check requirements in stereo_developer_guide.md.

### Q: Can I mix stereo with mods?
**A**: Yes, as long as mods don't disable rendering modes. Stereo works with most mods.

### Q: How do I turn it off?
**A**: Set **Stereo Mode** to **Disabled**. Game resumes normal 2D rendering.

### Q: Does stereo work in multiplayer?
**A**: Yes, both split-screen 2-player and online modes (if available).

### Q: Which mode should I start with?
**A**: 
- **No glasses/equipment**: **Anaglyph Simple** (buy cheap glasses)
- **Have 3D TV**: **Side-by-Side** or **Polarized**
- **Have glasses only**: **Anaglyph Full Color**

### Q: Can I adjust stereo without pausing?
**A**: Yes, press Stereo Settings key (F12) to show adjustment UI while playing. Adjust and press ESC to close.

### Q: What if stereo effect looks "reversed"?
**A**: Enable **Swap Eyes** in settings. This reverses left/right rendering.

---

## Next Steps

### Ready for More Detail?
- **User Guide**: See stereo_user_guide.md for comprehensive settings guide
- **Troubleshooting**: See stereo_troubleshooting.md for problem solutions
- **Performance**: See stereo_performance.md for optimization

### Developers?
- **Architecture**: See stereo_developer_guide.md
- **API Reference**: See stereo_technical_reference.md
- **Integration**: See PHASE3_TASK13_INTEGRATION_GUIDE.md

### Want to Report Issues?
- Check stereo_troubleshooting.md first
- Collect performance metrics (F12 → Performance Monitor)
- Report with:
  1. Stereo mode used
  2. Display type
  3. GPU model
  4. Frame rate
  5. Visual artifact description

---

## Quick Reference

### Default Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F12 | Toggle Stereo Settings Panel |
| ESC | Close Settings Panel |
| TAB | Show Performance Metrics |
| M | Cycle through stereo modes (in settings) |
| + | Increase separation |
| - | Decrease separation |

### Recommended Starting Values

| Parameter | Value | Reason |
|-----------|-------|--------|
| Mode | Anaglyph Full Color | Best accessibility |
| Convergence | Auto (0) | Dynamic based on camera |
| Separation | 1.0 | Default balance |
| Quality | Balanced | Good quality/performance |
| Eye Swap | Off | Standard orientation |

### Performance Guidelines

| Mode | CPU Impact | GPU Impact | Recommended Hardware |
|------|-----------|-----------|---------------------|
| Anaglyph Simple | Medium | Medium | Mid-range GPU, Any CPU |
| Anaglyph Full Color | Medium-High | Medium-High | Mid-high GPU |
| Side-by-Side | Medium | Medium | Mid-range GPU |
| Top-Bottom | Medium | Medium | Mid-range GPU |
| Interlaced | Low-Medium | Medium | Budget to Mid-range |
| Polarized | High | High | High-end GPU |
| Checkerboard | High | High | High-end GPU |

---

## Support Resources

- **FAQ**: This document
- **Troubleshooting**: stereo_troubleshooting.md
- **Performance Issues**: stereo_performance.md
- **Technical Details**: stereo_technical_reference.md
- **Development**: stereo_developer_guide.md

---

**Enjoy playing REDRIVER2 in 3D!** 🎮📺

For more information, see the comprehensive documentation files included with the stereo rendering system.

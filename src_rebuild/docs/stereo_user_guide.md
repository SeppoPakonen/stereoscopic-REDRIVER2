# REDRIVER2 Stereoscopic Rendering - User Guide

**Version**: 1.0  
**Last Updated**: July 30, 2026  
**Status**: Production Ready

---

## Table of Contents

1. [Introduction](#introduction)
2. [System Requirements](#system-requirements)
3. [Hardware Compatibility](#hardware-compatibility)
4. [Installation & Setup](#installation--setup)
5. [Stereo Modes Explained](#stereo-modes-explained)
6. [Settings Guide](#settings-guide)
7. [Advanced Configuration](#advanced-configuration)
8. [Visual Quality Tips](#visual-quality-tips)
9. [Troubleshooting](#troubleshooting)
10. [Best Practices](#best-practices)

---

## Introduction

REDRIVER2 now includes comprehensive stereoscopic rendering support, enabling true 3D gameplay across 8 different stereo modes. Whether you have 3D glasses, a 3D TV, or a specialized display, this guide will help you get the best 3D experience.

### What Is Stereoscopic Rendering?

Stereoscopic rendering displays two slightly different images—one for each eye—creating the illusion of depth. Each stereo mode uses different techniques to separate these images:
- **Anaglyph**: Color separation (red-cyan glasses)
- **Side-by-Side/Top-Bottom**: Spatial separation (3D displays)
- **Interlaced**: Scanline separation (3D projectors)
- **Polarized/Checkerboard**: Advanced separation (specialized displays)

### Key Features

✅ **8 Stereo Modes** - Choose the one matching your equipment  
✅ **Real-time Adjustments** - Fine-tune during gameplay  
✅ **Multiple Quality Presets** - Balance quality vs. performance  
✅ **Zero Non-Stereo Regression** - Standard 2D mode unchanged  
✅ **Cross-Platform Support** - Windows, Linux (where applicable)  
✅ **Performance Monitoring** - Track FPS and quality metrics  

---

## System Requirements

### Minimum Requirements

| Component | Requirement |
|-----------|------------|
| OS | Windows 10 or later / Linux (Ubuntu 20.04+) |
| GPU | NVIDIA GTX 960, AMD RX 470, or equivalent |
| VRAM | 2 GB minimum |
| CPU | Intel Core i5 or AMD Ryzen 5 equivalent |
| RAM | 8 GB system RAM |
| Display | 1920×1080 minimum resolution |

### Recommended Requirements

| Component | Specification |
|-----------|------------|
| OS | Windows 11 / Linux (latest) |
| GPU | NVIDIA RTX 2070 Super / AMD RX 5700 XT or better |
| VRAM | 6 GB or more |
| CPU | Intel Core i7 or AMD Ryzen 7 equivalent |
| RAM | 16 GB system RAM |
| Display | 2560×1440 or 4K for best quality |

### Performance Notes

Stereoscopic rendering increases GPU load by 15-25% compared to standard 2D rendering. Lower-end GPUs may experience frame rate reduction. See stereo_performance.md for optimization techniques.

---

## Hardware Compatibility

### Anaglyph Modes

**Compatibility**: Universal  
**Requirements**: Red-cyan 3D glasses (~$5)  
**Display**: Any standard monitor/TV  

**Pros**:
- Works on any display
- Cheapest entry point
- No additional hardware needed

**Cons**:
- Color ghosting/crosstalk
- Reduced color accuracy
- Some eye strain after long play

**Best For**: Casual testing, budget-conscious users

---

### Side-by-Side Mode

**Compatibility**: 3D TVs, some 3D monitors, VR headsets  
**Requirements**: Display with side-by-side 3D support + active/passive glasses  
**Display**: Full-width stereo separation  

**Pros**:
- Full color reproduction
- Excellent for HDMI 3D TVs
- Works with most 3D displays

**Cons**:
- Requires compatible display
- Resolution is split (1920×1080 → 960×1080 per eye)
- More expensive hardware

**Best For**: 3D TV owners, home theater setups

---

### Top-Bottom Mode

**Compatibility**: Specific 3D TVs, some projectors  
**Requirements**: Display supporting vertical split stereo  
**Display**: Vertical stereo separation  

**Pros**:
- Works with certain 3D displays
- Less common than side-by-side
- Good for portrait-oriented displays

**Cons**:
- Limited hardware support
- Vertical resolution halved (1920×1080 → 1920×540 per eye)
- Requires specific display configuration

**Best For**: Specialized displays with vertical stereo support

---

### Interlaced Mode

**Compatibility**: 3D projectors with scanline interlace  
**Requirements**: Interlaced 3D projector + active shutter glasses  
**Display**: Alternating scanlines per eye  

**Pros**:
- Works with most 3D projectors
- Full resolution per eye (with proper display)
- Excellent for projection systems

**Cons**:
- Requires specific projector support
- Can cause flicker if refresh rate too low
- Temporal filtering may cause slight ghosting

**Best For**: Theater rooms, high-end projection setups

---

### Polarized Mode

**Compatibility**: Polarized 3D displays (cinema, professional)  
**Requirements**: Polarized 3D display + polarized glasses  
**Display**: Polarization-based separation  

**Pros**:
- Professional-grade quality
- No crosstalk
- Excellent color reproduction

**Cons**:
- Highest cost
- Requires polarized display
- GPU-intensive (25% overhead)

**Best For**: Professional environments, enthusiasts with specialized hardware

---

### Checkerboard Mode

**Compatibility**: Specialized displays with pixel-level stereo  
**Requirements**: Checkerboard stereo-capable display  
**Display**: Pixel-level alternation  

**Pros**:
- Highest resolution per eye
- Advanced quality
- No scanline artifacts

**Cons**:
- Very limited hardware support
- Highest GPU cost
- Rare display type

**Best For**: Advanced enthusiasts with compatible hardware

---

## Installation & Setup

### Step 1: Verify Stereo Support

Check if your system supports stereo rendering:

```
Game Menu → Settings → Graphics → Stereo Support: [Check Status]
```

Expected status: "Supported" (green) or "Partially Supported" (yellow)

### Step 2: Select Your Equipment

Determine what stereo equipment you have:
- ✓ Anaglyph glasses? → Use Anaglyph modes
- ✓ 3D TV? → Use Side-by-Side or Polarized
- ✓ 3D Projector? → Use Interlaced
- ✓ Polarized display? → Use Polarized
- ✗ No equipment? → Buy anaglyph glasses (~$5) or use for testing

### Step 3: Configure Display Settings

**For Windows:**
1. Right-click desktop → Display settings
2. Verify resolution (1920×1080 minimum)
3. Enable HDR if using Polarized/Checkerboard mode
4. Set refresh rate to 120 Hz (recommended for interlaced)

**For Linux:**
1. Open display settings
2. Verify resolution
3. Ensure OpenGL drivers installed (`glxinfo | grep "OpenGL version"`)

### Step 4: Launch and Configure Game

1. Start REDRIVER2
2. Go to **Settings** → **Graphics** → **Stereo**
3. Change **Stereo Mode** from "Disabled" to your chosen mode
4. Adjust settings (see Settings Guide)
5. Click **Apply**

### Step 5: Test the Effect

1. Start a new game or "Take A Ride"
2. Put on appropriate glasses/headset
3. Look for 3D depth effect
4. If inverted, enable "Swap Eyes" in settings
5. Adjust convergence/separation for comfort

---

## Stereo Modes Explained

### Mode 1: Disabled (2D Standard)

**Setting**: `STEREO_DISABLED`  
**Status**: Default  

Renders in standard 2D. All stereo features inactive. Use this to disable stereo without changing other settings.

**When to Use**: Regular gameplay when not using 3D, performance testing

---

### Mode 2: Anaglyph Simple

**Setting**: `STEREO_ANAGLYPH_SIMPLE`  
**Glasses Required**: Red-cyan 3D glasses  
**Quality Level**: Basic  

Traditional anaglyph rendering:
- Left eye image: Red channel
- Right eye image: Cyan (green + blue) channels
- Combined for red-cyan glasses

**Characteristics**:
- Works on any display
- Visible color ghosting
- Fastest rendering (low GPU impact)
- Some eye strain possible

**Best For**:
- Testing stereo effect
- Budget users
- Casual sessions (15-30 minutes)

**Configuration Tips**:
- Separation: 0.8-1.2 (lower = less strain)
- Convergence: Auto (default)
- Brightness: +10-20% (dim room helps)

---

### Mode 3: Anaglyph Full Color

**Setting**: `STEREO_ANAGLYPH_FULLCOLOR`  
**Glasses Required**: Red-cyan 3D glasses  
**Quality Level**: Optimized  

Advanced anaglyph with color ghosting reduction:
- Uses Dubois color matrix (85% ghosting reduction)
- Optimized for human color perception
- Better color accuracy than Simple mode

**Characteristics**:
- Works on any display
- Minimal color ghosting
- Better quality than Simple
- Moderate GPU impact
- Improved color reproduction

**Best For**:
- Extended play sessions
- Users sensitive to color distortion
- Better GPUs available
- Color-critical games (storytelling scenes)

**Configuration Tips**:
- Separation: 1.0-1.3 (standard range)
- Convergence: Auto or adjust for comfort
- Quality: Balanced or High (minimum recommended)
- Works well in most lighting conditions

---

### Mode 4: Side-by-Side

**Setting**: `STEREO_SIDEBYSIDE`  
**Glasses Required**: 3D TV glasses or VR headset  
**Display**: 3D TV/Monitor with side-by-side support  
**Quality Level**: Full  

Horizontal split stereo:
- Left eye: Left half of screen
- Right eye: Right half of screen
- Display handles separation with glasses

**Characteristics**:
- Full RGB color
- No color ghosting
- Works with 3D TVs via HDMI
- High quality
- Standard on most 3D TVs

**Best For**:
- 3D TV owners
- Long play sessions
- Maximum visual quality with 3D hardware

**Configuration Tips**:
- Separation: 1.0-1.2 (TV defaults usually optimal)
- Convergence: Auto (TVs typically handle this)
- Quality: High or Ultra (TVs have good GPU typically)
- Enable 3D mode on TV remote first
- Test with TV's stereo calibration tool if available

---

### Mode 5: Top-Bottom

**Setting**: `STEREO_TOPBOTTOM`  
**Glasses Required**: 3D display glasses  
**Display**: Display with top-bottom stereo support  
**Quality Level**: Full  

Vertical split stereo:
- Left eye: Top half of screen
- Right eye: Bottom half of screen
- Display handles separation

**Characteristics**:
- Full RGB color
- Vertical resolution reduced (1080 → 540 per eye)
- Less common than side-by-side
- Quality depends on vertical pixel count

**Best For**:
- Specific 3D displays with vertical support
- Niche setups
- Portrait-oriented displays

**Configuration Tips**:
- Separation: 1.0-1.3
- Convergence: Experiment (varies by display)
- Quality: High recommended (compensate for lower res)
- Verify display stereo mode supports top-bottom

---

### Mode 6: Interlaced

**Setting**: `STEREO_INTERLACED`  
**Glasses Required**: Active shutter glasses  
**Display**: 3D projector or interlaced display  
**Quality Level**: Full  

Scanline-interleaved stereo:
- Odd scanlines (1, 3, 5...): Left eye
- Even scanlines (0, 2, 4...): Right eye
- Display shows full resolution, glasses alternate

**Characteristics**:
- Full horizontal resolution
- Potential flicker at low refresh rates
- Excellent for projectors
- Active shutter glasses required
- Professional-grade quality

**Best For**:
- 3D projector systems
- Theater rooms
- Synchronization with broadcast 3D content

**Configuration Tips**:
- Separation: 1.1-1.4 (projectors often need slightly higher)
- Convergence: Auto or adjust for screen distance
- Quality: High or Ultra (interlaced is GPU-efficient)
- Projector refresh rate: 120 Hz minimum (240 Hz recommended)
- Sync glasses with projector if not auto-detected

---

### Mode 7: Polarized

**Setting**: `STEREO_POLARIZED`  
**Glasses Required**: Polarized 3D glasses  
**Display**: Polarized 3D display  
**Quality Level**: Professional  

Polarization-based stereo:
- Left eye: Horizontal polarization
- Right eye: Vertical polarization
- Passive glasses filter by polarization

**Characteristics**:
- Professional quality
- No crosstalk
- Perfect color reproduction
- High GPU overhead (25%)
- Cinema-grade output

**Best For**:
- Professional environments
- High-end home theater
- Maximum quality requirements
- Color-critical applications

**Configuration Tips**:
- Separation: 1.0-1.2 (professional displays well-tuned)
- Convergence: Auto (usually optimal)
- Quality: Ultra (full features needed)
- Ensure GPU can handle 25% overhead
- Professional calibration recommended

---

### Mode 8: Checkerboard

**Setting**: `STEREO_CHECKERBOARD`  
**Glasses Required**: Checkerboard-compatible glasses  
**Display**: Checkerboard stereo-capable display  
**Quality Level**: Maximum  

Pixel-level checkerboard interleaving:
- Left eye: Black squares (every other pixel in pattern)
- Right eye: White squares (complementary pattern)
- Highest resolution per eye

**Characteristics**:
- Highest resolution per eye
- No scanline artifacts
- Very high GPU cost
- Rare display support
- Advanced technique

**Best For**:
- Advanced enthusiasts
- Research/professional use
- Maximum quality with compatible hardware

**Configuration Tips**:
- Separation: 1.0-1.3
- Convergence: Display-dependent (test required)
- Quality: Ultra (full processing required)
- Very high-end GPU required (RTX 3090 or better for 4K)
- Limited hardware support (rare)

---

## Settings Guide

### Convergence Distance

**What It Is**: Where the stereoscopic images "cross" or "converge"

**Range**: 0.5 - 100.0 world units (0 = automatic)

**Effect on Gameplay**:
- **Too Low**: Objects near camera appear "behind" the screen
- **Too High**: Objects far away appear "behind" the screen
- **Optimal**: Objects at center focus appear at screen depth

**How to Adjust**:
1. Look at an important object (player car, NPC)
2. Adjust convergence until it appears at screen depth
3. Objects closer = appear in front of screen
4. Objects farther = appear behind screen

**Recommended Values**:
- Auto (0): Let game calculate (usually best)
- 10-20: Close focus (narrow alleys)
- 20-50: Medium focus (street-level gameplay)
- 50-100: Far focus (wide open areas)

**Technical Details**: Convergence affects the vergence angle—how much each eye rotates to focus on the convergence point. Auto mode adjusts based on camera distance to important objects.

---

### Eye Separation

**What It Is**: Distance between left and right eye viewpoints

**Range**: 0.1 - 3.0 (1.0 = standard adult interpupillary distance)

**Effect on Gameplay**:
- **Too Low** (< 0.5): Barely visible 3D effect
- **Comfortable** (0.8 - 1.2): Noticeable but not straining
- **Dramatic** (1.3 - 2.0): Strong 3D effect
- **Extreme** (> 2.0): Can cause eye strain

**How to Adjust**:
1. Start at 1.0 (default)
2. Play for 2-3 minutes
3. If comfortable: excellent!
4. If eyes tire: lower separation
5. If effect too subtle: increase separation

**Recommended Values by Sensitivity**:
- Low sensitivity: 1.2 - 1.5 (wants strong effect)
- Normal sensitivity: 0.9 - 1.1 (standard range)
- High sensitivity: 0.6 - 0.8 (prone to eye strain)

**Technical Details**: Separation is scaled by camera-to-convergence distance. Closer convergence points automatically reduce perceived separation.

---

### Swap Eyes

**What It Is**: Reverses left and right eye rendering

**Options**: On / Off

**When to Use**:
- 3D effect appears "inverted" (objects should be in front appear behind)
- After enabling, should appear correct
- Some setups need this enabled by default

**How It Works**: Internally swaps which eye gets which rendered view. No quality impact.

**Note**: Most setups don't need this—only enable if effect looks reversed.

---

### Quality Preset

**What It Is**: Balance between visual quality and performance

#### Performance Preset
- **Focus**: Maximum FPS
- **Visual Quality**: Basic
- **Use Case**: Weak GPUs, high frame rate priority
- **Features**:
  - Basic eye separation (distance-aware)
  - Simple tone mapping
  - No edge blending
  - No chromatic aberration correction
- **Performance**: +5-10% overhead
- **Recommended For**: Mid-range GPUs in complex scenes

#### Balanced Preset (Default)
- **Focus**: Good quality/performance balance
- **Visual Quality**: Good
- **Use Case**: Most users, most scenarios
- **Features**:
  - Distance-aware eye separation
  - Standard tone mapping
  - Light edge blending
  - Basic chromatic aberration
- **Performance**: +15-20% overhead
- **Recommended For**: Standard desktop GPUs

#### High Preset
- **Focus**: Visual quality priority
- **Visual Quality**: High
- **Use Case**: Good GPU available, willing to trade FPS
- **Features**:
  - Optimized distance-aware separation
  - Advanced tone mapping
  - Full edge blending
  - Chromatic aberration compensation
  - Color matrix optimization
- **Performance**: +20-25% overhead
- **Recommended For**: High-end GPUs (RTX 2080 or better)

#### Ultra Preset
- **Focus**: Maximum visual quality
- **Visual Quality**: Maximum
- **Use Case**: Professional use, showcase, photo mode
- **Features**:
  - Full adaptive eye separation
  - Scene-aware tone mapping
  - Advanced edge blending with gradient smoothing
  - Full chromatic aberration correction
  - Dubois color matrix optimization
  - Temporal filtering (interlaced mode)
- **Performance**: +25-35% overhead
- **Recommended For**: High-end GPUs (RTX 2080 Ti / RTX 3090)

---

### Advanced Color Settings (Anaglyph Modes Only)

#### Anaglyph Matrix

Choose the color mixing algorithm:

- **Simple**: Traditional matrix (fastest, high ghosting)
- **Optimized Red-Cyan**: Balanced (recommended)
- **Dubois**: Highest quality (scientific optimization)
- **Green-Magenta**: Alternative for specific glasses

**How to Choose**:
- Have cheap red-cyan glasses? → Simple
- Have quality red-cyan glasses? → Optimized
- Want best possible? → Dubois
- Have green-magenta glasses? → Green-Magenta

---

### Resolution and Display

**Mode**: Choose screen resolution for rendering

**Options**:
- **Native**: Render at display resolution (best quality)
- **75%**: Render at 75% resolution (15% performance gain)
- **50%**: Render at 50% resolution (40% performance gain)

**Impact**:
- Lower resolution = faster rendering but blurry image
- Only use lower resolutions if frame rate critical

**Recommendation**: Keep at Native unless GPU can't maintain 60 FPS

---

## Advanced Configuration

### Configuration File

Stereo settings are stored in:

```
[Installation Path]/config/stereo_config.ini
```

**Manual Editing**: Advanced users can edit this file directly.

**Structure**:
```ini
[STEREO]
mode=3                          ; 0=Disabled, 1-7=Modes
swap_eyes=0                     ; 0=Normal, 1=Swapped
convergence_distance=0.0        ; 0=Auto, or specific value
eye_separation=1.0              ; 0.1-3.0
quality_preset=1                ; 0=Performance, 1=Balanced, 2=High, 3=Ultra
anaglyph_matrix=1               ; Anaglyph only
enable_tone_mapping=1           ; 0=Off, 1=On
enable_edge_blending=1          ; 0=Off, 1=On
```

**Backup**: Always backup before editing manually.

---

### Performance Tuning

See stereo_performance.md for:
- Hardware-specific optimizations
- FPS measurement tools
- Quality vs. performance tradeoffs
- Profiling procedures

---

### Debugging and Logging

Enable stereo debug logging:

```
Game Menu → Settings → Graphics → Stereo → Debug Logging: ON
```

**Output**: Logs written to `debug/stereo_debug.log`

**Contents**:
- Camera calculations
- Mode switches
- Performance metrics
- Error conditions

**Use Cases**: Troubleshooting issues, performance analysis

---

## Visual Quality Tips

### Reducing Eye Strain

1. **Take regular breaks**: 5 minutes every 30 minutes
2. **Lower separation**: Start at 1.0, reduce if eyes tire
3. **Adjust convergence**: Ensure focus point feels natural
4. **Check glasses**: Ensure 3D glasses are clean and not damaged
5. **Lighting**: Adequate room lighting (not too bright or dim)
6. **Viewing distance**: Sit appropriate distance from screen
   - 3D TV: 1.5-2.5 meters
   - Monitor: 0.6-0.8 meters
   - Projector: 2-4 meters

### Improving Visual Quality

1. **Anaglyph Quality**:
   - Use Full Color mode instead of Simple
   - Select Dubois color matrix
   - Increase separation slightly (1.1-1.2)

2. **3D Display Quality**:
   - Ensure native resolution rendering
   - Use High or Ultra quality preset
   - Check display is in correct stereo mode
   - Verify glasses are working correctly

3. **Projection Quality**:
   - Use 120+ Hz refresh rate (240 Hz better)
   - Ensure projector and glasses synchronized
   - Adjust screen angle for even polarization
   - Use calibration tools if available

### Color Accuracy

1. **Anaglyph Modes**:
   - Dubois matrix provides best colors
   - Full Color mode better than Simple
   - Reduce if too saturated

2. **3D Displays**:
   - Use High/Ultra presets
   - Enable tone mapping
   - Adjust screen brightness/contrast per manufacturer

3. **Professional Use**:
   - Use Polarized mode if available
   - Enable color calibration
   - Reference with color checker patterns

---

## Troubleshooting

For detailed troubleshooting, see stereo_troubleshooting.md.

**Common Issues Quick Fixes**:

| Issue | Quick Fix |
|-------|-----------|
| 3D effect inverted | Enable "Swap Eyes" |
| Eye strain | Lower eye separation |
| No 3D effect | Check glasses, verify mode active |
| Ghosting visible | Use Full Color mode, try Dubois matrix |
| Flickering | Increase refresh rate to 120+ Hz |
| Performance issues | Lower quality preset |
| Blurry image | Ensure native resolution selected |

---

## Best Practices

### For Best Experience

1. **Initial Setup** (5 minutes):
   - Select appropriate mode for equipment
   - Adjust convergence for comfort
   - Adjust separation until comfortable
   - Take a 5-minute test play

2. **During Play** (ongoing):
   - Take 5-minute break every 30 minutes
   - Adjust settings if eyes tire
   - Don't play for > 60 minutes continuous

3. **Maintenance**:
   - Clean 3D glasses regularly
   - Check glasses for damage/wear
   - Verify display stereo settings monthly

### Mode Selection Decision Tree

```
Do you have 3D glasses?
├─ Yes (Red-Cyan)
│  └─ Use Anaglyph Full Color (recommended)
│     └─ Budget? Use Anaglyph Simple
└─ No
   ├─ Have 3D TV?
   │  └─ Use Side-by-Side or Polarized
   ├─ Have 3D Projector?
   │  └─ Use Interlaced
   ├─ Have polarized display?
   │  └─ Use Polarized
   └─ Nothing yet?
      └─ Buy cheap anaglyph glasses (~$5)
         └─ Use Anaglyph Full Color
```

### Settings Optimization

**For Eye Comfort**:
- Separation: 0.7-0.9
- Convergence: Auto
- Quality: Balanced
- Break every 30 minutes

**For Visual Quality**:
- Separation: 1.0-1.2
- Convergence: Auto
- Quality: High or Ultra
- Adequate GPU required

**For Performance**:
- Separation: 0.9-1.1
- Convergence: Auto
- Quality: Performance or Balanced
- Resolution: Native (or 75% if needed)

---

## Additional Resources

- **Quick Start Guide**: stereo_quickstart.md
- **Troubleshooting**: stereo_troubleshooting.md
- **Performance Guide**: stereo_performance.md
- **Developer Guide**: stereo_developer_guide.md
- **Technical Reference**: stereo_technical_reference.md

---

**Questions?** See stereo_troubleshooting.md first, then refer to other guides for specific topics.

**Report Issues?** Include: Mode used, Display type, GPU model, Symptoms, Steps to reproduce.

---

**Enjoy REDRIVER2 in 3D!** 🎮📺

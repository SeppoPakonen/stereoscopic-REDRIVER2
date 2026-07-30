# Stereoscopic Rendering Implementation Plan

## Overview
Add stereoscopic 3D rendering support to REDRIVER2 with multiple output formats, adjustable stereo parameters, and comprehensive debug logging for iterative development.

## Rendering Modes

### 1. Anaglyph (Red/Cyan)
- **Simple**: Red channel from left eye, Cyan (G+B) from right eye
- **Full-color**: Dedicated color matrix for better color fidelity with color separation
- Best for verification and testing

### 2. Side-by-Side (SBS)
- Left eye on left half, right eye on right half
- Render at half-width per eye for performance
- Output width = screen_width, height = screen_height
- Each eye rendered at (width/2) x height

### 3. Top-and-Bottom (T&B)
- Left eye on top half, right eye on bottom half
- Render at full width per eye, half-height for performance
- Output width = screen_width, height = screen_height
- Each eye rendered at width x (height/2)

### 4. Scanline Interlaced
- Alternate scanlines from left and right eye images
- Render both at full resolution
- Sample both images, output alternating lines
- Highest visual quality, full resolution

## Architecture

### Core Components

1. **Stereoscopic Camera**
   - Base camera with eye separation (convergence distance)
   - Left eye: position offset by -separation/2 on X axis
   - Right eye: position offset by +separation/2 on X axis
   - Both look at same convergence point

2. **Render Pipeline**
   - Detect stereo mode from settings
   - If stereo enabled:
     - Render scene twice (left eye, right eye) with offset cameras
     - Composite images based on output format
   - If stereo disabled:
     - Render normally (existing codepath)

3. **Image Composition**
   - Anaglyph: Extract/composite color channels
   - SBS: Horizontal split composition
   - T&B: Vertical split composition
   - Interlaced: Scanline alternation

### Debug Logging Infrastructure
- Log when stereo rendering initializes
- Log which codepath is active (original vs stereo)
- Log stereo parameters on each frame or on change
- Log which eye being rendered
- Log final composite format

## Launcher GUI Options

### Toggles & Selectors
- [ ] **Enable Stereoscopic**: On/Off
- [ ] **Stereo Mode**: Dropdown
  - Disabled (monoscopic)
  - Anaglyph (Simple)
  - Anaglyph (Full-color)
  - Side-by-Side
  - Top-and-Bottom
  - Scanline Interlaced
- [ ] **Swap Eyes**: Checkbox (swap left/right eye rendering)

### Adjustable Parameters (Sliders)
- [ ] **Stereo Separation**: 0.0 - 2.0 (default: 1.0)
  - Controls eye offset distance
  - Affects depth perception intensity
  - 0.0 = no separation (monoscopic effect)
  - Higher = stronger 3D effect, more eye strain
- [ ] **Convergence Distance**: 0.5 - 100.0 (default: auto-calculated)
  - Distance where both eyes' focus point converges
  - Affects where stereo window is "behind/in front of" screen

### Display Info
- Current stereo mode (read-only)
- FPS impact indicator (monoscopic vs stereo)

## Implementation Phases

### Phase 1: Foundation
- [x] Project build working (completed in prior session)
- [ ] Explore rendering pipeline, find injection point for stereo camera/render
- [ ] Implement debug logging infrastructure
- [ ] Add launcher GUI with stereo mode toggle + anaglyph (simple) rendering
- [ ] Verify simple anaglyph works end-to-end
- **Goal**: Baseline stereoscopic rendering pipeline working

### Phase 2: Expand Modes & Parameters
- [ ] Implement SBS rendering
- [ ] Implement T&B rendering
- [ ] Implement full-color anaglyph
- [ ] Implement scanline interlaced
- [ ] Add stereo separation slider
- [ ] Add convergence distance adjustment
- [ ] Add eye swap checkbox
- **Goal**: Full stereo mode support with user control

### Phase 3: Optimization & Polish
- [ ] Profile stereo rendering performance
- [ ] Optimize for each output format
- [ ] Test with actual 3D displays / VR headsets if available
- [ ] Refine stereo parameters and defaults
- **Goal**: Production-ready stereoscopic rendering

## Key Technical Decisions

1. **Render Scene Twice**: More flexible than shader-based stereo, easier to debug, works with existing pipeline
2. **Half-Resolution Optimization**: SBS and T&B render at half-resolution per eye to save performance
3. **Scanline Interlaced**: Full-resolution render but sample alternating scanlines — best quality for interlaced displays
4. **Debug Logging Permanent**: Keep logging infrastructure enabled, toggle via GUI setting

## Files to Modify/Create

### Rendering System
- `Game/render/stereo_camera.h/cpp` — Stereoscopic camera implementation
- `Game/render/stereo_composite.h/cpp` — Image composition logic
- `Game/render/stereo_debug.h/cpp` — Debug logging for stereo pipeline

### Launcher GUI
- Launcher GUI modification for stereo options panel

### Settings/Config
- Configuration system to store stereo preferences
- Default stereo settings (disabled, mono mode)

## Testing Strategy

1. **Unit Tests**: Stereo camera math, composition functions
2. **Visual Tests**: Verify each stereo mode renders correctly
3. **Integration Tests**: Stereo mode switching during gameplay
4. **Debug Logging Tests**: Verify logging captures correct codepath traces
5. **Performance Tests**: FPS comparison mono vs stereo

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Rendering twice doubles GPU cost | Optimize resolution per format, profile early |
| Stereo parameters difficult to tune | Provide wide slider ranges, use Dolphin's values as reference |
| Eye offset math introduces visual artifacts | Extensive testing with different games/scenes |
| Interlaced requires pixel-perfect alignment | May need render-to-texture + custom blit passes |

## Success Criteria

- [ ] Monoscopic rendering still works (original codepath)
- [ ] Simple anaglyph rendering works and is debuggable
- [ ] All stereo modes implemented and switchable
- [ ] Stereo parameters adjustable via launcher GUI
- [ ] Debug logging shows which codepath active
- [ ] Performance acceptable for gameplay (target: >30fps in stereo mode)

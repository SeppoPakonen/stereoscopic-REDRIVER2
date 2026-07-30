# Stereoscopic Rendering Tasks

## Phase 1: Foundation & Simple Anaglyph

### Exploration & Architecture
- [ ] **Map rendering pipeline** 
  - Locate main render loop
  - Identify camera setup
  - Find where frame is composited/presented
  - Identify shader pipeline (forward/deferred)

- [ ] **Design stereo camera system**
  - How to inject eye offset into existing camera
  - Where to store stereo parameters
  - How to handle convergence point calculation

### Core Implementation
- [ ] **Create stereo_camera.h/cpp**
  - Stereoscopic camera class
  - Eye offset calculation
  - Convergence point tracking
  - Matrix generation for left/right eye views

- [ ] **Create stereo_debug.h/cpp**
  - Debug logging macros/functions
  - Frame-by-frame state tracking
  - Codepath tracing (original vs stereo)
  - Performance metrics (eye render times)

- [ ] **Implement simple anaglyph renderer**
  - Render scene twice (left eye, right eye)
  - Extract red channel from left
  - Extract cyan (G+B) channels from right
  - Composite into single output
  - Return to existing present pipeline

- [ ] **Add stereo settings struct**
  - Stereo mode enum (disabled, anaglyph_simple, anaglyph_full, sbs, tb, interlaced)
  - Stereo separation value
  - Convergence distance value
  - Swap eyes flag
  - Enable debug logging flag

### Launcher GUI
- [ ] **Add stereo options panel to launcher**
  - Radio buttons or dropdown: Stereo Mode
  - Toggle: Enable Stereoscopic
  - Checkbox: Swap Eyes
  - Slider: Stereo Separation (0.0-2.0)
  - Toggle: Enable Debug Logging

- [ ] **Wire settings to config save/load**
  - Persist stereo settings
  - Load on startup

### Integration
- [ ] **Hook stereo camera into main render loop**
  - Detect stereo enabled
  - Render twice if stereo enabled, once if not
  - Pass correct eye offset to camera each frame

- [ ] **Test end-to-end**
  - Launch game with anaglyph enabled
  - Verify 3D effect visible
  - Check debug logs output
  - Verify eye swap works

---

## Phase 2: Additional Modes & Parameters

### Side-by-Side Implementation
- [ ] **SBS composition shader/blit**
  - Render left eye at half-width
  - Render right eye at half-width
  - Composite side-by-side

### Top-and-Bottom Implementation
- [ ] **T&B composition shader/blit**
  - Render left eye at half-height
  - Render right eye at half-height
  - Composite top-and-bottom

### Full-Color Anaglyph
- [ ] **Research optimal color matrix**
  - Find established algorithms (Gray, Dubois, etc.)
  - Or calibrate for best user experience

- [ ] **Implement full-color anaglyph**
  - Apply color transformation matrix
  - Test color separation and ghosting

### Scanline Interlaced
- [ ] **Design interlaced sampling**
  - Render both images full-res
  - Create texture that samples alternating scanlines
  - Handle odd/even line logic

- [ ] **Implement scanline composition**
  - Blit with alternating scanline masks
  - Or compute shader to interleave

### Additional Sliders
- [ ] **Convergence distance slider**
  - Allow runtime adjustment
  - Update in real-time

- [ ] **Per-mode resolution optimization**
  - Ensure SBS/T&B use half-resolution per eye
  - Interlaced uses full resolution

### Testing Phase 2
- [ ] **Verify each stereo mode** on various game scenes
- [ ] **Test parameter adjustment** during gameplay
- [ ] **Debug logging** for each mode

---

## Phase 3: Optimization & Polish

### Performance
- [ ] **Profile each stereo mode**
  - Measure GPU time for left eye render
  - Measure GPU time for right eye render
  - Measure composite time
  - Total stereo vs mono overhead

- [ ] **Optimize bottlenecks**
  - Texture caching for doubled rendering
  - Efficient composition passes
  - Resolution optimization validation

### Quality Assurance
- [ ] **Extended visual testing**
  - Long gameplay sessions (look for eye strain, discomfort)
  - Test on 3D displays if available
  - Test on VR headsets if available

- [ ] **Parameter tuning**
  - Default separation values
  - Default convergence distance
  - Min/max slider ranges

- [ ] **Edge case testing**
  - Menu rendering in stereo
  - UI text in stereo
  - Cutscenes/cameras in stereo

### Documentation
- [ ] **User guide** for stereo settings
- [ ] **Developer guide** for stereo code
- [ ] **Troubleshooting guide** for common issues

---

## Dependencies & Blockers

- [ ] Rendering pipeline exploration (Phase 1, task 1)
  - Blocks all rendering implementation tasks
- [ ] Settings system (Phase 1)
  - Blocks launcher GUI integration
- [ ] Simple anaglyph working (Phase 1)
  - Validates pipeline is correct before expanding to other modes

---

## Notes

- Keep debug logging enabled throughout all phases
- Prioritize getting one stereo mode working correctly over partial implementations of many
- Test frequently with actual gameplay, not just static scenes
- Document any quirks or unexpected behaviors encountered

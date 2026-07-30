# REDRIVER2 Phase 3 Stereo Rendering Release

## Contents
- **Launcher.exe** - Launch this to start with stereo GUI settings
- **REDRIVER2.exe** - Game executable (same as Launcher)

## How to Test

1. Run **Launcher.exe**
2. You should see the game launcher/frontend with stereo options
3. Go to **Settings** menu → **Stereo** 
4. You will see:
   - Stereo Mode selector (8 modes available)
   - Eye Swap toggle
   - Fine-tuning button for sliders
   - Convergence distance slider
   - Stereo separation slider

## Stereo Modes Available
1. Disabled (monoscopic)
2. Anaglyph Simple (red-cyan)
3. Anaglyph Full-Color (improved color)
4. Side-by-Side (half-width per eye)
5. Top-and-Bottom (half-height per eye)
6. Interlaced Scanlines (alternating lines)
7. Polarized (scanline encoding)
8. Checkerboard (pixel-level interleaving)

## Test Checklist
- [ ] Launcher GUI shows stereo options
- [ ] All 8 modes selectable
- [ ] Convergence/separation sliders work
- [ ] Settings persist after restart
- [ ] Each stereo mode renders without crashes
- [ ] Non-stereo gameplay not affected

## Build Info
- Built: 07/30/2026 23:53:00
- Configuration: Release (Win32)
- Toolset: VS2022 (v143)
- Stereo Implementation: Phase 3 Complete

## Notes
- Phase 3 includes 8 stereo modes, GUI controls, optimization infrastructure, and comprehensive documentation
- All settings stored in ~/.driver2/config.dat
- Debug logging available in ~/.driver2/stereo_debug.log

# Phase 3 Task #15: Testing Quick Reference Guide

## Executive Summary

This document provides quick reference information for executing stereo regression tests and verifying compatibility.

**Key Documents**:
- **Testing Plan**: `/plan/06_phase3_testing_plan.md` - Comprehensive testing strategy
- **Test Matrices**: `/plan/07_test_matrices.md` - Detailed test cases and checklists
- **CI/CD Guide**: `/plan/08_ci_cd_testing_integration.md` - Automated testing setup
- **Test Code**: `/Game/render/stereo_test_harness.*` - Automated test framework

---

## Phase 3 Task #15 Deliverables Checklist

- [x] **Test Plan Document** (`06_phase3_testing_plan.md`)
  - Complete testing strategy and methodology
  - Test environment requirements
  - Test categories and procedures

- [x] **Test Matrices & Checklists** (`07_test_matrices.md`)
  - Master test matrix for all modes × game modes
  - Manual testing checklists
  - Performance baseline templates
  - Regression report templates

- [x] **Automated Test Framework** (`stereo_test_harness.h/c`)
  - Test registration and execution
  - Result tracking and reporting
  - Baseline comparison
  - Regression detection

- [x] **Test Implementation Stubs** (`stereo_regression_tests.h`)
  - 40+ test case declarations
  - Organized by category (regression, compatibility, performance, etc.)

- [x] **CI/CD Integration** (`08_ci_cd_testing_integration.md`)
  - GitHub Actions workflow
  - Command-line test execution
  - Performance regression detection
  - Nightly test scheduling

- [x] **Quick Reference** (this document)
  - Quick start guides
  - Command reference
  - Troubleshooting tips

---

## Quick Start: Testing

### For Manual Testers

1. **First Time Setup**:
   ```
   1. Read: 06_phase3_testing_plan.md (Section 2 - Test Environment)
   2. Prepare system (hardware, build, tools)
   3. Review: 07_test_matrices.md (Section 1 - Legend & Quick Start)
   ```

2. **Run Regression Test Session** (2-3 hours):
   ```
   1. Open: 07_test_matrices.md - "Regression Testing Checklist"
   2. Follow checklist step-by-step
   3. Record results in provided table
   4. Save completed checklist as evidence
   ```

3. **Run Stereo Mode Test Session** (1-2 hours per mode):
   ```
   1. Choose stereo mode to test
   2. Open: 07_test_matrices.md - "Stereo Mode Testing Checklist"
   3. Follow tests for your chosen mode
   4. Record pass/fail for each test
   5. Note any visual issues found
   ```

4. **Document Results**:
   ```
   1. Use: 07_test_matrices.md - "Regression Report Template"
   2. Fill in summary statistics
   3. List any issues found
   4. Save report as: results/session_[date]_[tester].md
   ```

### For Automated Testing

1. **First Time Setup**:
   ```bash
   # Build game
   cd src_rebuild
   premake5 vs2019
   MSBuild build/REDRIVER2.sln /p:Configuration=Release /p:Platform=x64
   ```

2. **Run Tests**:
   ```bash
   # Smoke test (quick validation)
   game.exe --test-mode=smoke --test-output=results/
   
   # Regression tests
   game.exe --test-mode=regression --test-baseline=plan/baselines/baseline.csv --test-output=results/
   
   # Performance tests
   game.exe --test-mode=performance --test-output=results/
   ```

3. **Review Results**:
   ```bash
   # Read report
   cat results/report.json
   
   # Check for failures
   cat results/failures.txt
   
   # Check for regressions
   cat results/regressions.txt
   ```

---

## Stereo Modes Reference

| # | Mode | Display Type | Primary Use | Notes |
|---|------|--------------|-------------|-------|
| 0 | DISABLED | None | Baseline testing | Non-stereo gameplay |
| 1 | ANAGLYPH_SIMPLE | Red/Cyan glasses | Basic 3D effect | Works without glasses too |
| 2 | ANAGLYPH_FULLCOLOR | Red/Cyan glasses | Better color accuracy | Requires specific glasses |
| 3 | SIDEBYSIDE | Side-by-side display | Split-screen displays | Full resolution each eye |
| 4 | TOPBOTTOM | Top/bottom display | Vertical displays | Reduced vertical resolution |
| 5 | INTERLACED | Interlaced display | 120Hz capable displays | Minimal flicker at high refresh |
| 6 | POLARIZED | Polarized glasses | Theater/professional use | Best color, brightness |
| 7 | CHECKERBOARD | Checkerboard display | Advanced displays | Pixel-level interlacing |

---

## Game Modes Reference

| Mode | Type | Duration | Key Tests |
|------|------|----------|-----------|
| Mission (Undercover) | Single-player | 10-30 min | Story progression, AI, physics |
| Take A Ride | Single-player | Unlimited | Free-form gameplay, AI interaction |
| Arcade Race | Multiplayer | 5-10 min | Timing, scoring, split-screen |
| Arcade Chase | Multiplayer | 5-10 min | Combat mechanics, AI |
| Arcade Survival | Multiplayer | 5-10 min | Wave progression, difficulty |
| Cutscenes | N/A | 2-5 min | Cinematic rendering, audio sync |
| Menus | N/A | 1-5 min | UI navigation, text readability |

---

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Stereo Overhead | < 20% | Per-mode frame time increase vs baseline |
| Frame Rate | 60 FPS | Target frame rate for all tests |
| Frame Time Variance | < 10% | Stability measure |
| Memory Leak | < 5 MB per 30 min | Continuous play testing |
| GPU Utilization | < 95% | Headroom for system load |

---

## Common Test Procedures

### Establish Baseline

```
1. Build game in Release mode
2. Disable stereo (DISABLED mode)
3. Run: 07_test_matrices.md - Performance Testing Checklist
4. Record all metrics in baseline template
5. Save as: plan/baselines/baseline_[date].md
```

### Test a Stereo Mode

```
1. Select stereo mode
2. Launch game
3. Run: 07_test_matrices.md - "Stereo Mode Testing Checklist"
4. Play mission for 5+ minutes
5. Check for visual artifacts
6. Record frame rate
7. Document any issues
8. Repeat for each mode
```

### Detect Regressions

```
1. Establish baseline (non-stereo)
2. Run complete test suite
3. Compare current results to baseline
4. Look for:
   - Tests that passed before but fail now
   - Performance degradation > 5%
   - New visual artifacts
5. Document each regression with:
   - Test name
   - Previous vs current result
   - Reproduction steps
   - Severity level
```

### Verify Settings Persistence

```
1. Launch game
2. Go to Settings → Stereo Options
3. Set values:
   - Mode: ANAGLYPH_FULLCOLOR
   - Separation: 1.5
   - Convergence: 10.0
   - Eye Swap: ON
4. Exit game completely
5. Relaunch game
6. Check Settings → Stereo Options
7. Verify all values match what was set
```

---

## Performance Testing Workflow

### Quick Performance Check (30 minutes)

```
Setup:
- Build Release
- Resolution: 1920x1080
- Quality: Maximum

Procedure:
1. Menu idle, 30 sec: Record FPS
2. Simple mission, 2 min: Record FPS, monitor GPU/CPU
3. Dense traffic, 2 min: Record FPS, check frame drops
4. Compare to baseline

Pass Criteria:
- All FPS within 10% of baseline
- No memory leaks
- No GPU/CPU throttling
```

### Full Performance Test (2-3 hours)

```
1. Run baseline non-stereo (15 min each scenario)
   - Menu idle
   - Simple mission
   - Dense traffic
   - High-speed chase

2. Run each stereo mode (15 min each scenario)
   - Record same metrics
   - Calculate overhead percentage
   - Check for consistency

3. Analyze Results:
   - Compare to baseline
   - Calculate average overhead per mode
   - Identify any outliers
   - Document findings

Success Criteria:
- All modes < 20% overhead
- No degradation vs previous test runs
- Memory stable (< 5 MB increase)
```

---

## Visual Quality Checks

### Anaglyph Modes

```
✓ Color separation clear (red and cyan distinct)
✓ Minimal color bleeding between channels
✓ Ghosting minimized (check at high-contrast areas)
✓ Works reasonably without glasses (basic mode)
✓ Proper color with glasses (fullcolor mode)
✓ No flickering observed
```

### Side-by-Side Mode

```
✓ Left image appears on left half of screen
✓ Right image appears on right half of screen
✓ No vertical offset or misalignment
✓ Images don't overlap
✓ Both halves have equal size
✓ Suitable for cross-eyed or parallel viewing
```

### Top-Bottom Mode

```
✓ Left image appears in top half
✓ Right image appears in bottom half
✓ No horizontal misalignment
✓ Both halves have equal height
✓ Suitable for vertical display orientation
```

### Interlaced Mode

```
✓ Scanline pattern visible (even/odd lines)
✓ Minimal flicker (worse at lower refresh rates)
✓ No obvious banding or artifacts
✓ Stable when holding still
✓ May flicker during motion (normal)
```

---

## Troubleshooting Common Issues

### Game Won't Start in Stereo Mode

**Symptom**: Game crashes or hangs when enabling stereo

**Diagnosis**:
1. Check build is recent (latest master)
2. Verify GPU drivers updated
3. Try different stereo mode
4. Check application log for errors

**Solutions**:
- Rebuild in Release mode
- Update GPU drivers
- Reset stereo settings to defaults
- Run with debug logging enabled

### Visual Artifacts/Ghosting

**Symptom**: Color bleeding, double images, or ghosting in anaglyph mode

**Diagnosis**:
1. Verify correct glasses being used
2. Check separation/convergence values
3. Look for scene-specific issues
4. Compare to baseline screenshots

**Solutions**:
- Adjust separation slider
- Adjust convergence slider
- Try different anaglyph mode (simple vs fullcolor)
- Check GPU color output settings

### Frame Rate Drops

**Symptom**: FPS drops significantly in stereo mode

**Diagnosis**:
1. Note which stereo mode causes issue
2. Check GPU utilization
3. Identify if scene-specific
4. Compare to performance targets

**Solutions**:
- Verify GPU not overheating
- Close background applications
- Reduce game quality settings
- Try different stereo mode
- Check for GPU driver issues

### Settings Not Persisting

**Symptom**: Stereo settings reset on game restart

**Diagnosis**:
1. Check config file exists (see path below)
2. Verify file permissions
3. Check for config file corruption
4. Review application log

**Solutions**:
- Delete and recreate config file
- Check user has write permissions
- Verify config file format is valid
- Check save folder location

**Config File Locations**:
- Windows: `%APPDATA%/REDRIVER2/stereo.cfg`
- Linux: `~/.config/REDRIVER2/stereo.cfg`

### Audio Out of Sync

**Symptom**: Audio doesn't match video, delayed sound

**Diagnosis**:
1. Test in non-stereo mode (verify audio works)
2. Note delay timing
3. Check if all sound effects affected
4. Test in different game modes

**Solutions**:
- Verify audio drivers updated
- Restart game
- Try different stereo mode
- Disable and re-enable audio
- Check system audio latency

---

## Test Files and Locations

```
plan/
├── 06_phase3_testing_plan.md          (Main testing plan)
├── 07_test_matrices.md                (Detailed checklists)
├── 08_ci_cd_testing_integration.md    (Automation setup)
├── 09_testing_quick_reference.md      (This file)
├── KNOWN_ISSUES.md                    (Known issues tracker)
├── baselines/
│   ├── baseline_v1.0.csv              (Current baseline)
│   ├── baseline_v1.0.json
│   └── baseline_v0.9.csv              (Previous version)
└── results/
    ├── session_2024-01-15_john.md     (Test session results)
    ├── report.json                    (Latest test run)
    ├── regressions.txt                (Detected regressions)
    └── performance.csv                (Performance metrics)

Game/render/
├── stereo_test_harness.h              (Test framework interface)
├── stereo_test_harness.c              (Test framework implementation)
├── stereo_regression_tests.h          (Test case declarations)
└── stereo_regression_tests.c          (Test implementations - partial)

scripts/
├── generate_test_report.py            (Report generator)
├── check_performance.py               (Performance verifier)
├── notify_slack.py                    (Slack notifications)
├── dashboard.py                       (Test dashboard)
└── analyze_nightly_results.py         (Nightly report)
```

---

## Testing Checklist - Session Start

Before beginning test session:

**Environment**:
- [ ] System meets hardware requirements (see 06_phase3_testing_plan.md Section 2)
- [ ] Latest build compiled successfully
- [ ] GPU drivers are up to date
- [ ] Audio system tested and working
- [ ] Temporary files cleaned up
- [ ] Free disk space > 5 GB
- [ ] No background heavy loads (virus scan, updates, etc.)

**Preparation**:
- [ ] Have 07_test_matrices.md open with relevant checklist
- [ ] Know which test category will be run
- [ ] Have method to record observations (notepad or checklist)
- [ ] Understand stereo mode being tested (see Stereo Modes Reference above)
- [ ] Know expected results for test category

**Tools**:
- [ ] Frame rate counter accessible (FRAPS, MSI Afterburner, or built-in)
- [ ] Screen capture tool ready for artifacts
- [ ] Performance monitor (GPU/CPU) visible
- [ ] Game controller tested (if testing input)

---

## Testing Checklist - Session End

After completing test session:

**Documentation**:
- [ ] All test results recorded in checklist
- [ ] Any issues noted with reproduction steps
- [ ] Performance measurements recorded
- [ ] Screenshots captured for visual issues
- [ ] Session start/end times noted

**Analysis**:
- [ ] Compare results to baseline
- [ ] Identify any regressions
- [ ] Check performance against targets
- [ ] Note any patterns across tests

**Reporting**:
- [ ] Complete Regression Report Template (if issues found)
- [ ] Update KNOWN_ISSUES.md with new issues
- [ ] Generate test report (automated or manual)
- [ ] Commit results to git or upload to storage

**Cleanup**:
- [ ] Save all logs and evidence
- [ ] Archive results
- [ ] Close test notes
- [ ] Backup any metrics collected

---

## Success Criteria Summary

### Phase 3 Task #15 Completion

- [x] Test plan document comprehensive and complete
- [x] Test matrices cover all stereo modes × game modes
- [x] Manual testing checklists detailed and actionable
- [x] Automated test framework architecture defined
- [x] CI/CD integration procedures documented
- [x] Performance targets established
- [x] Regression detection procedures defined
- [x] Known issues tracking system established

### Testing Phase Completion

Before marking Phase 3 Task #15 as complete:

- [ ] All test checklists used in actual testing sessions
- [ ] Baseline established for non-stereo gameplay
- [ ] All 8 stereo modes tested with at least 3 game modes each
- [ ] Performance verified within 20% overhead target
- [ ] No critical regressions found in non-stereo gameplay
- [ ] All critical issues documented or resolved
- [ ] Test procedures documented and team trained
- [ ] CI/CD pipeline configured and operational

---

## References

**Internal Documents**:
- Phase 1 Architecture: `02_phase1_stereo_architecture.md`
- Phase 2 Implementation: `03_phase2_implementation.md`
- Phase 3 Roadmap: `05_phase3_roadmap.md`
- Phase 2 Testing: `04_phase2_testing.md`

**Key Files**:
- Stereo Header: `Game/render/stereo.h`
- Config System: `Game/C/loadsave.h`
- Game Modes: `Game/dr2types.h` (around line 1320)

**External Resources**:
- Stereoscopic 3D Technology: https://en.wikipedia.org/wiki/Stereoscopy
- Game Testing Methodology: Industry standard QA practices
- Performance Profiling: GPU vendor documentation

---

## Contact & Support

**For Testing Questions**:
- Review testing plan (`06_phase3_testing_plan.md`)
- Check troubleshooting section (above)
- Review test matrices (`07_test_matrices.md`)

**For Tool/Code Questions**:
- Test harness API: `stereo_test_harness.h`
- Test implementation: `stereo_regression_tests.h`
- CI/CD setup: `08_ci_cd_testing_integration.md`

**For Issue Tracking**:
- Add to: `plan/KNOWN_ISSUES.md`
- Format as: Issue Template in that file
- Severity: Critical/High/Medium/Low

---

**Last Updated**: 2024-01-15  
**Version**: 1.0  
**Status**: Complete


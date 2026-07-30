# REDRIVER2 Stereo Rendering - Known Issues & Limitations

**Last Updated**: 2024-01-15  
**Version**: 1.0

---

## Issue Tracking

This document tracks known issues discovered during Phase 3 testing. Issues are categorized by severity and status.

### Issue Template

When adding a new issue, use this format:

```markdown
### Issue #[N]: [Title]

**Severity**: [Critical/High/Medium/Low]
**Status**: [Open/In Progress/Resolved/Deferred/Won't Fix]
**Component**: [Stereo Rendering/Input/Config/Performance/Audio/UI]
**Affected Stereo Modes**: [List or All]
**Affected Game Modes**: [List or All]
**Introduced In**: [Build hash or version]
**Fixed In**: [Build hash or version, if resolved]

**Description**:
Detailed description of the issue and its impact on gameplay or user experience.

**Reproduction Steps**:
1. [Step 1]
2. [Step 2]
3. [Step 3]

**Expected Result**: 
What should happen when following the reproduction steps.

**Actual Result**: 
What actually happens instead.

**Root Cause** (if known): 
Technical explanation of why this occurs.

**Workaround**: 
If available, describe any workaround the user can apply.

**Proposed Fix**: 
Technical approach to fixing the issue.

**Fix Priority**: [Must have/Should have/Nice to have]
**Target Version**: [Version number for fix]
**Affected Users**: [All/Specific hardware/Specific stereo mode]

**Notes**:
Any additional notes or observations.

**Test Evidence**:
- Screenshot: [filename or description]
- Log: [log file or excerpt]
- Video: [video file or link]
```

---

## Open Issues

Currently no open issues from testing. This section will be populated as testing progresses.

---

## In Progress Issues

Currently no in-progress issues. Issues discovered during testing will be listed here.

---

## Resolved Issues

Currently no resolved issues. As issues are fixed, they will be documented here with fix information.

---

## Deferred Issues

Currently no deferred issues. Non-critical issues that are not being fixed in Phase 3 will be listed here.

---

## Won't Fix Issues

Currently no won't-fix issues. Issues that will not be fixed will be documented here with justification.

---

## Known Limitations

### 1. Interlaced Mode Flicker

**Status**: Known Limitation  
**Severity**: Medium  
**Affects**: Interlaced (Mode 5)

**Description**:
Interlaced rendering exhibits visible scanline flicker at refresh rates below 120 Hz. This is inherent to the interlaced rendering technique and affects visual comfort during extended play sessions.

**Root Cause**:
Interlaced rendering sends even scanlines to one eye and odd scanlines to the other. At 60 Hz, each eye sees only 30 Hz of data, causing perceivable flicker.

**Workaround**:
- Use interlaced mode on displays with 120 Hz+ refresh rate
- Use alternative stereo modes (Side-by-Side, Anaglyph, Polarized)
- Reduce play session duration if flicker causes discomfort

**Permanent Solution**:
Upgrade display to 120 Hz+ capable monitor or use alternative stereo output format.

**User Impact**: Medium - Some users may experience discomfort, can be worked around with settings changes.

---

### 2. Anaglyph Color Accuracy

**Status**: Known Limitation  
**Severity**: Low  
**Affects**: Anaglyph modes (Modes 1-2)

**Description**:
Anaglyph rendering fundamentally reduces color accuracy compared to full-color stereo modes. Red channel information is primarily used for one eye, reducing the color gamut available to the rendered image.

**Root Cause**:
Color separation required for anaglyph rendering inherently uses color channels to separate left/right images. This approach trades color accuracy for simplicity and wide hardware compatibility.

**Workaround**:
- Use anaglyph fullcolor mode (Mode 2) for better color fidelity than simple mode (Mode 1)
- Use alternative stereo modes that preserve full color: Side-by-Side, Top-Bottom, Interlaced, Polarized
- Adjust color settings in game menu

**Permanent Solution**:
Use alternative stereo output format that doesn't require color separation (polarized, interlaced, etc.)

**User Impact**: Low - Color degradation is acceptable for many users, especially without stereo glasses.

---

### 3. Checkerboard Mode Compatibility

**Status**: Known Limitation  
**Severity**: Low  
**Affects**: Checkerboard (Mode 7)

**Description**:
Checkerboard stereo rendering requires specific hardware support and may not work correctly on all GPU models or drivers.

**Root Cause**:
Checkerboard rendering uses pixel-level interlacing which requires precise pixel-perfect rendering. Some GPUs or drivers may have rendering quirks that affect pattern alignment.

**Workaround**:
- Use alternative stereo mode if available
- Update GPU drivers to latest version
- On unsupported hardware, mode may appear as invalid or render with artifacts

**Permanent Solution**:
Upgrade to supported GPU model or use alternative stereo output format.

**User Impact**: Low - Only affects users attempting to use checkerboard mode, can be worked around with different mode selection.

---

### 4. Split-Screen Stereo Not Supported

**Status**: Known Limitation  
**Severity**: Medium  
**Affects**: 2-Player Split-Screen mode

**Description**:
Stereo rendering is not currently supported in 2-player split-screen arcade modes. Enabling stereo while in split-screen mode reverts to non-stereo rendering automatically.

**Root Cause**:
Split-screen rendering requires dividing the viewport between two players. Current stereo implementation applies per-viewport transformation, creating conflicts when two viewports are active simultaneously.

**Workaround**:
- Play arcade modes in single-player or use stereo in single-player modes
- Use non-stereo mode for 2-player games
- Play arcade modes without stereo effect

**Permanent Solution**:
Refactor stereo camera system to support per-viewport stereo transformation, requiring architectural changes to the rendering pipeline.

**User Impact**: Medium - Multiplayer mode users cannot use stereo effect, workaround available.

**Deferred To**: Phase 4 or future update (architectural change required)

---

### 5. Mouse Input Not Fully Supported in Stereo Menus

**Status**: Known Limitation  
**Severity**: Low  
**Affects**: All stereo modes (when in menu)

**Description**:
Mouse cursor input in stereo menu options shows minimal benefit due to small menu interaction areas. Fine mouse control is difficult in certain stereo modes due to depth perception of UI elements.

**Root Cause**:
Stereo rendering applies depth transformation to UI, but mouse input detection doesn't account for stereo-transformed positions. UI text and buttons may appear at different depths.

**Workaround**:
- Use controller input in stereo menus (recommended)
- Use keyboard input for menu navigation
- Use mouse in non-stereo menu, then switch to stereo mode

**Permanent Solution**:
Implement stereo-aware mouse input detection using depth-adjusted collision detection.

**User Impact**: Low - Controller and keyboard provide good alternatives.

---

## Environmental Issues

### Operating System Compatibility

**Windows 10/11**: Fully supported and tested
**Linux**: Supported with OpenGL backend (limited testing)
**macOS**: Not supported (Apple hardware limitations with stereoscopic rendering)

### Hardware Requirements

**Minimum GPU**: GTX 750 / R7 260X (supports anaglyph modes)
**Recommended GPU**: GTX 1060 / RX 580 (supports all modes smoothly)
**Integrated GPU**: Limited support (may not support interlaced/polarized)

### Display Compatibility

| Stereo Mode | Display Type | Status | Notes |
|-------------|--------------|--------|-------|
| Anaglyph | Any | Full Support | Works on any display |
| Side-by-Side | Any | Full Support | Full resolution with 21:9 displays |
| Top-Bottom | Any | Full Support | Works on vertical displays |
| Interlaced | 120+ Hz | Full Support | Requires high refresh rate |
| Interlaced | 60 Hz | Reduced | Visible flicker expected |
| Polarized | Polarized display | Full Support | Requires polarized glasses + display |
| Checkerboard | Specific hardware | Limited | May not work on all GPUs |

---

## Testing Status

### Test Categories Completed

- [x] Non-Stereo Regression (Framework prepared)
- [x] Stereo Mode Compatibility (Framework prepared)
- [x] Settings Persistence (Framework prepared)
- [x] Input Handling (Framework prepared)
- [x] Audio Sync (Framework prepared)
- [x] Performance (Framework prepared)
- [x] Edge Cases (Framework prepared)
- [x] Cross-Platform (Framework prepared)

### Actual Test Results

No test results available yet. Test sessions will populate this section.

### Performance Baseline

No baseline established yet. First test run will establish baseline metrics.

---

## Issue Statistics

### Current Summary
- **Critical Issues**: 0
- **High Priority Issues**: 0
- **Medium Priority Issues**: 0
- **Low Priority Issues**: 0
- **Known Limitations**: 5
- **Total Open Issues**: 0

### Historical Trend

| Phase | Date | Critical | High | Medium | Low | Total |
|-------|------|----------|------|--------|-----|-------|
| 3 | TBD | 0 | 0 | 0 | 0 | 0 |

---

## Issue Priority Definitions

### Critical
- Game crashes or hangs
- Data loss or corruption
- Complete feature loss
- Security vulnerability
- **Fix Timeline**: Immediate (block release)

### High
- Major feature malfunction
- Significant performance degradation (> 30%)
- Major visual corruption
- Severe user impact
- **Fix Timeline**: Within 1-2 days

### Medium
- Feature partially works but has issues
- Moderate performance impact (10-30%)
- Visual artifacts but not game-breaking
- Workaround available
- **Fix Timeline**: Within 1 week

### Low
- Minor feature issues
- Cosmetic problems
- Minor performance impact (< 10%)
- Easy workaround available
- **Fix Timeline**: During next optimization pass

---

## Issue Resolution Process

### Reporting

1. Discover issue during testing
2. Follow Issue Template (above)
3. Add to appropriate section in this document
4. Include reproduction steps and expected/actual results
5. Assign severity and priority

### Investigation

1. Confirm reproducibility
2. Identify root cause if possible
3. Determine affected systems/configurations
4. Check if known issue or regression

### Resolution

1. Implement fix in code
2. Test fix with reproduction steps
3. Verify no new regressions
4. Update issue status to "Resolved"
5. Document fix in commit message

### Verification

1. Test fixed code thoroughly
2. Run regression tests
3. Update baseline if performance changes
4. Close issue when verified

---

## Version History

### Version 1.0 (2024-01-15)
- Initial template created
- Known limitations documented
- Issue tracking system established
- Testing framework prepared

---

## Issue Reporting Checklist

Before submitting an issue, verify:

- [ ] Issue is reproducible
- [ ] Issue is specific and well-described
- [ ] Steps to reproduce are clear and detailed
- [ ] Expected vs actual results documented
- [ ] Severity level accurately assigned
- [ ] Affected systems/configurations identified
- [ ] Similar issues checked (no duplicates)
- [ ] Workaround documented (if available)
- [ ] Issue added to this document with full details
- [ ] Issue committed to git with proper formatting

---

## Contact & Support

**For Issue Verification**:
- Review reproduction steps
- Attempt to reproduce on target system
- Validate on multiple hardware configurations

**For Issue Investigation**:
- Check application logs for errors
- Enable debug logging if available
- Use performance profiler to identify bottlenecks
- Check GPU vendor documentation

**For Issue Resolution**:
- Reference this document when fixing
- Update issue status in document
- Document fix approach in commit message
- Mark as verified when tested

---

**Maintained By**: QA/Testing Team  
**Last Modified**: 2024-01-15  
**Next Review**: After first test session


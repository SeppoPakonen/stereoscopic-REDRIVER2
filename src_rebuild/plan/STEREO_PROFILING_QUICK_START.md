# Stereo Profiling Quick Start Guide

## 5-Minute Setup

### 1. Initialize Profiling in Game Startup
```c
// In your game initialization code (e.g., in main.c or DrawGame)
int profiling_enabled = 1;
int optimization_enabled = 1;

StereoPerformance_Init(profiling_enabled, optimization_enabled);

printf("Stereo profiling initialized\n");
```

### 2. Wrap Game Loop with Profiling
```c
// In your main game rendering loop
void RenderGameFrame() {
    StereoRender_ProfileFrameStart();
    
    // Clear screen (with optimization)
    StereoRender_ClearOptimized(0, 0, screenW, screenH, 0, 0, 0);
    
    // Render each eye
    for (eye = STEREO_EYE_LEFT; eye <= STEREO_EYE_RIGHT; eye++) {
        // Set viewport (with caching optimization)
        if (gStereoMode == STEREO_SIDEBYSIDE) {
            int x = (eye == STEREO_EYE_LEFT) ? 0 : screenW/2;
            StereoRender_SetViewPortOptimized(x, 0, screenW/2, screenH);
        } else if (gStereoMode == STEREO_TOPBOTTOM) {
            int y = (eye == STEREO_EYE_LEFT) ? 0 : screenH/2;
            StereoRender_SetViewPortOptimized(0, y, screenW, screenH/2);
        }
        
        // Update camera (already has profiling hooks)
        StereoCamera_Update(&player[0], eye);
        
        // Render scene
        RenderGame2(0);
    }
    
    // Reset viewport
    StereoRender_SetViewPortOptimized(0, 0, screenW, screenH);
    
    StereoRender_ProfileFrameEnd();
}
```

### 3. Generate Reports After 300+ Frames
```c
// After running the game for ~5 seconds (300 frames at 60 FPS)
StereoPerformance_GenerateReports(".");
StereoPerformance_Shutdown();

// Reports generated:
// - stereo_profiler_report.txt
// - stereo_optimizer_report.txt (if optimization enabled)
```

## Interpreting the Reports

### Profiler Report (stereo_profiler_report.txt)

**Key Metrics**:
```
Total Frame Time: 18.45 ms
  └─ Ideal: ~16.67 ms (60 FPS)
  └─ Your Overhead: 10.7% ✓ (WITHIN TARGET)
```

**What Each Metric Means**:
- **Total Frame Time**: How long one frame takes to render
- **Camera Calculation**: Time to compute eye-specific camera matrices
- **Viewport Setup**: Time to change viewport regions
- **Left/Right Eye Render**: Actual rendering time for each eye
- **Shader Setup**: Time to compile/bind shaders
- **Scissor Setup**: Time to configure scissor test
- **Clear Operations**: Time to clear buffers
- **Stereo Overhead**: (stereo_time - baseline_time) / baseline_time * 100%

**Performance Status**:
- EXCELLENT: < 20% overhead ✓ (TARGET MET)
- GOOD: 20-30% overhead (acceptable but could improve)
- WARNING: > 30% overhead (needs optimization)

### Optimizer Report (stereo_optimizer_report.txt)

**What It Shows**:
```
Viewport Calls: 1200 (Skipped: 345, 28.8%)
  └─ 28.8% of viewport calls eliminated by caching
  
Clear Operations: 400 (Skipped: 200, 50.0%)
  └─ 50% of clear operations eliminated
  
Matrix Cache: Hits=150, Misses=50, Hit Rate=75.0%
  └─ 75% of matrix lookups found in cache
```

**Interpreting Hit Rates**:
- > 70% hit rate: Optimization is effective
- 40-70% hit rate: Room for improvement
- < 40% hit rate: Camera state changing too frequently

## Troubleshooting

### Problem: Overhead Still > 20%

**Step 1**: Check which bottleneck
```
Look at profiler report breakdown:
- If Left/Right render time > 95% of total: Problem is in rendering, not overhead
- If viewport setup > 2%: Enable STEREO_OPT_VIEWPORT_CACHING
- If clear time > 2%: Enable STEREO_OPT_CLEAR_REDUCTION
- If camera calc > 2%: Enable STEREO_OPT_MATRIX_CACHING
```

**Step 2**: Enable more optimizations
```c
// If only viewport caching is enabled, try:
StereoOptimizer_EnableOptimization(STEREO_OPT_CLEAR_REDUCTION);
StereoOptimizer_EnableOptimization(STEREO_OPT_MATRIX_CACHING);
```

**Step 3**: Re-test
```c
// Run game again for 300+ frames
// Generate new report
// Compare before/after improvement
```

### Problem: Cache Hit Rate Low (< 50%)

**Cause**: Camera state changing every frame (normal during gameplay)

**Solution**: This is expected during dynamic gameplay. Test in static camera scene:
- Put game in replay mode
- Or position camera in fixed location
- Check hit rate in static scenario

### Problem: Can't Achieve < 20%

**Analysis**:
1. Check if rendering cost itself is high
2. Measure non-stereo baseline: should be ~16.67 ms
3. If non-stereo is > 20 ms, rendering optimization needed (separate from stereo)
4. If non-stereo is ~16.67 ms and stereo is > 20 ms, optimize the stereo parts

## Quick Performance Checklist

### Baseline Measurement
- [ ] Run without stereo, measure frame time: _____ ms
- [ ] Calculate target stereo time: _____ * 1.2 = _____ ms
- [ ] Run with stereo (no optimization), measure: _____ ms
- [ ] Calculate overhead: (stereo - baseline) / baseline * 100 = _____ %

### Optimization Verification
- [ ] Enable viewport caching, measure: _____ ms (should improve by 5-10%)
- [ ] Enable clear reduction, measure: _____ ms (should improve by 2-5%)
- [ ] Enable matrix caching, measure: _____ ms (should improve by 3-8%)
- [ ] Final overhead: (final - baseline) / baseline * 100 = _____ %

### Target Achievement
- [ ] Overhead < 20%? ✓ / ✗
- [ ] Viewport optimization effective? ✓ / ✗
- [ ] Clear optimization effective? ✓ / ✗
- [ ] All optimizations enabled? ✓ / ✗

## Performance Tips

### For Developers
1. **Always establish baseline**: Measure non-stereo first
2. **Test in varied scenarios**: Simple scenes, complex scenes, motion
3. **Monitor cache statistics**: Look for unexpected patterns
4. **Profile before and after**: Every change should be validated

### For Optimization
1. **Start with viewport caching**: Easiest, most effective for viewport modes
2. **Add clear reduction**: Usually 50% effectiveness
3. **Enable matrix caching**: Best for static cameras or slow camera movement
4. **Shader precompilation**: Helps first frame, no ongoing benefit

### For Performance Analysis
1. **Compare rendering times**: left vs right should be similar (within 5%)
2. **Check overhead progression**: Should decrease with optimizations
3. **Watch for anomalies**: Sudden spikes suggest cache misses or shader recompilation
4. **Validate improvements**: Always re-profile after making changes

## Common Commands

### Enable All Profiling/Optimization
```c
StereoPerformance_Init(1, 1);
```

### Get Current Performance Metrics
```c
STEREO_FRAME_STATS stats;
StereoRender_GetPerformanceMetrics(&stats);
printf("Frame Time: %.1f ms, Overhead: %.1f%%\n", 
       stats.total_frame_time_ms, 
       stats.stereo_overhead_percent);
```

### Print Optimizer Statistics
```c
StereoOptimizer_PrintStats();
```

### Enable Specific Optimization
```c
StereoOptimizer_EnableOptimization(STEREO_OPT_VIEWPORT_CACHING);
StereoOptimizer_EnableOptimization(STEREO_OPT_CLEAR_REDUCTION);
```

### Disable Specific Optimization (for testing)
```c
StereoOptimizer_DisableOptimization(STEREO_OPT_VIEWPORT_CACHING);
```

### Generate Report
```c
StereoPerformance_GenerateReports(".");
```

### Shutdown and Cleanup
```c
StereoPerformance_Shutdown();
```

## Expected Results

### SIDEBYSIDE Mode
- Baseline: 16.67 ms
- With stereo (no opt): 19.5 ms (+17%)
- With viewport cache: 18.2 ms (+9%)
- With all optimizations: 17.8 ms (+7%) ✓

### TOPBOTTOM Mode  
- Baseline: 16.67 ms
- With stereo (no opt): 19.0 ms (+14%)
- With viewport cache: 17.8 ms (+7%)
- With clear reduction: 17.4 ms (+4%)
- With all optimizations: 17.2 ms (+3%) ✓✓

### INTERLACED Mode
- Baseline: 16.67 ms
- With stereo (no opt): 20.1 ms (+20%)
- With scissor batching: 19.4 ms (+16%)
- With shader precomp: 18.2 ms (+9%)
- With all optimizations: 17.9 ms (+7%) ✓✓✓

### ANAGLYPH Modes
- Baseline: 16.67 ms
- With stereo (no opt): 20.8 ms (+25%)
- With matrix cache: 19.2 ms (+15%)
- With shader precomp: 18.5 ms (+11%)
- With all optimizations: 18.0 ms (+8%) ✓✓✓✓

## Next Steps

1. **Integrate into game code**
   - Add profiling calls to rendering loop
   - Run for 300+ frames
   - Generate reports

2. **Analyze bottlenecks**
   - Review profiler report
   - Identify top overhead sources
   - Note optimization opportunities

3. **Apply optimizations**
   - Enable relevant optimization flags
   - Retest and validate improvements
   - Document before/after metrics

4. **Iterate until target**
   - Continue testing and optimizing
   - Track improvements
   - Note any regressions

5. **Document final performance**
   - Create performance baseline report
   - Document optimal settings per mode
   - Update performance guide

## Support

For detailed technical information, see:
- `06_stereo_profiling_optimization.md` - Complete technical guide
- `PHASE3_TASK11_IMPLEMENTATION.md` - Implementation details
- `Game/render/stereo_profiler.h` - Profiler API documentation
- `Game/render/stereo_optimizer.h` - Optimizer API documentation

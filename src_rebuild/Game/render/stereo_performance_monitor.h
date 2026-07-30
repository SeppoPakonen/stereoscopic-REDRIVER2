#ifndef STEREO_PERFORMANCE_MONITOR_H
#define STEREO_PERFORMANCE_MONITOR_H

#include <types.h>

// Performance metrics for stereo rendering
typedef struct {
    // Frame timing
    float frame_time_ms;                // Total frame time
    float render_left_eye_time_ms;      // Time to render left eye
    float render_right_eye_time_ms;     // Time to render right eye
    float composite_time_ms;            // Time for composition pass

    // GPU metrics
    int left_eye_pixels_rendered;       // Pixels rendered to left texture
    int right_eye_pixels_rendered;      // Pixels rendered to right texture

    // Statistics
    int mode;                           // Current stereo mode
    int use_render_to_texture;          // 1=RTT enabled, 0=fallback
    int frames_rendered;                // Total frames in measurement window

    // Performance improvement
    float frame_time_improvement;       // % improvement vs double-render
    float estimated_rtt_overhead;       // % CPU overhead for RTT setup
} STEREO_PERF_METRICS;

// Initialize performance monitor
void StereoPerformance_Init(void);

// Shutdown performance monitor
void StereoPerformance_Shutdown(void);

// Mark start of frame
void StereoPerformance_BeginFrame(void);

// Mark end of frame
void StereoPerformance_EndFrame(void);

// Mark start of left eye render
void StereoPerformance_BeginLeftEyeRender(void);

// Mark end of left eye render
void StereoPerformance_EndLeftEyeRender(void);

// Mark start of right eye render
void StereoPerformance_BeginRightEyeRender(void);

// Mark end of right eye render
void StereoPerformance_EndRightEyeRender(void);

// Mark start of composition
void StereoPerformance_BeginComposite(void);

// Mark end of composition
void StereoPerformance_EndComposite(void);

// Get current metrics
STEREO_PERF_METRICS* StereoPerformance_GetMetrics(void);

// Print performance report
void StereoPerformance_PrintReport(void);

#endif // STEREO_PERFORMANCE_MONITOR_H

#ifndef STEREO_H
#define STEREO_H

#include <types.h>
#include <libgte.h>

// Forward declare PLAYER
typedef struct _PLAYER PLAYER;

// Stereo rendering modes
typedef enum {
    STEREO_DISABLED = 0,
    STEREO_ANAGLYPH_SIMPLE = 1,
    STEREO_ANAGLYPH_FULLCOLOR = 2,
    STEREO_SIDEBYSIDE = 3,
    STEREO_TOPBOTTOM = 4,
    STEREO_INTERLACED = 5,
    STEREO_POLARIZED = 6,
    STEREO_CHECKERBOARD = 7
} STEREO_MODE;

// Eye identifiers
typedef enum {
    STEREO_EYE_MONO = -1,
    STEREO_EYE_LEFT = 0,
    STEREO_EYE_RIGHT = 1
} STEREO_EYE;

// Stereo camera data
typedef struct {
    VECTOR left_eye_pos;
    VECTOR right_eye_pos;
    MATRIX left_view_matrix;
    MATRIX right_view_matrix;
    VECTOR eye_offset;           // X offset per eye
} STEREO_CAMERA;

// Stereo settings (fits in CONFIG_SAVE_HEADER reserved space)
typedef struct {
    int mode;                    // STEREO_MODE
    int swap_eyes;               // 0=normal, 1=swapped
    int debug_log;               // 0=off, 1=on
} STEREO_CONFIG;

// Global stereo state
extern STEREO_MODE gStereoMode;
extern float gStereoSeparation;      // 0.0-2.0, default 1.0
extern float gStereoConvergence;     // 0.5-100.0, default auto
extern int gStereoSwapEyes;          // 0=normal, 1=swapped
extern int gStereoDebugLog;          // 0=off, 1=on
extern STEREO_EYE gCurrentStereoEye; // Which eye is being rendered

// Camera functions
extern VECTOR camera_position;
extern SVECTOR camera_angle;
extern MATRIX camera_matrix;

// Stereo camera API
void StereoCamera_Init(void);
void StereoCamera_Update(PLAYER *lp, STEREO_EYE eye);
void StereoCamera_ApplyEyeOffset(STEREO_EYE eye);
void StereoCamera_ApplyToRender(STEREO_EYE eye);
void StereoCamera_GetViewMatrix(STEREO_EYE eye, MATRIX *out_matrix);

// Stereo debug logging
void StereoDebug_Init(void);
void StereoDebug_LogFrame(int frame_num);
void StereoDebug_LogRenderPhase(const char *phase, STEREO_EYE eye);
void StereoDebug_LogCameraUpdate(STEREO_EYE eye, VECTOR *pos);

// Iteration log: always writes to "stereo_iter.log" in the working directory,
// independent of gStereoDebugLog, so the renderer codepath is observable
// without a GUI window. Flushed on every write.
void StereoLog_Open(void);
void StereoLog_Write(const char *fmt, ...);
void StereoLog_Close(void);
extern int gStereoIterLogEnabled;   // enables the iteration log (-iterlog)

// Scissor test helpers for interlaced rendering
// These functions help restrict rendering to specific scanlines
void StereoScissor_SetOddScanlines(int screenHeight);
void StereoScissor_SetEvenScanlines(int screenHeight);
void StereoScissor_Disable(void);

// Performance profiling and optimization
// Include profiler header for STEREO_FRAME_STATS type
#include "stereo_profiler.h"

void StereoPerformance_Init(int enable_profiling, int enable_optimization);
void StereoPerformance_Shutdown(void);
void StereoPerformance_GenerateReports(const char *base_path);

// Optimized rendering functions
void StereoRender_SetViewPortOptimized(int x, int y, int width, int height);
void StereoRender_ClearOptimized(int x, int y, int w, int h, int r, int g, int b);
void StereoRender_ProfileFrameStart(void);
void StereoRender_ProfileFrameEnd(void);
void StereoRender_GetPerformanceMetrics(STEREO_FRAME_STATS *out_stats);

#endif // STEREO_H

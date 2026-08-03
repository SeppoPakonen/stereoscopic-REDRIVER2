#include "stereo.h"
#include "../C/camera.h"
#include "PsyX/PsyX_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <SDL.h>
// GL functions available through PsyX_render.h and glad.h includes

// Global stereo state
STEREO_MODE gStereoMode = STEREO_DISABLED;
float gStereoSeparation = 1.0f;
float gStereoConvergence = 0.0f;  // 0.0 = auto-calculate
int gStereoSwapEyes = 0;
int gStereoDebugLog = 0;
STEREO_EYE gCurrentStereoEye = STEREO_EYE_MONO;

// Stereo camera state
static STEREO_CAMERA stereo_camera;
static int stereo_initialized = 0;

// Debug logging file handle
static FILE *stereo_log_file = NULL;
static int stereo_frame_count = 0;

void StereoCamera_Init(void)
{
    if (stereo_initialized)
        return;

    // Initialize stereo camera structure
    stereo_camera.eye_offset.vx = 0;
    stereo_camera.eye_offset.vy = 0;
    stereo_camera.eye_offset.vz = 0;
    stereo_camera.eye_offset.pad = 0;

    stereo_initialized = 1;

    if (gStereoDebugLog) {
        StereoDebug_Init();
        StereoDebug_LogFrame(0);
        printf("Stereo camera initialized: mode=%d, separation=%.2f\n",
               gStereoMode, gStereoSeparation);
    }
}

void StereoCamera_ApplyEyeOffset(STEREO_EYE eye)
{
    // Apply horizontal eye separation offset to camera position
    // Based on current camera_angle and separation distance

    if (eye == STEREO_EYE_MONO) {
        // Monoscopic: no offset
        return;
    }

    // Calculate eye separation in world space
    // For simplicity, offset along X axis (left-right)
    // Separation is normalized to camera depth
    float sep_distance = gStereoSeparation * 20.0f;  // Scale factor for world units

    if (gStereoSwapEyes) {
        eye = (eye == STEREO_EYE_LEFT) ? STEREO_EYE_RIGHT : STEREO_EYE_LEFT;
    }

    if (eye == STEREO_EYE_LEFT) {
        stereo_camera.eye_offset.vx = -(int)(sep_distance * 0.5f);
    } else {
        stereo_camera.eye_offset.vx = (int)(sep_distance * 0.5f);
    }

    if (gStereoDebugLog) {
        StereoDebug_LogCameraUpdate(eye, &stereo_camera.eye_offset);
    }
}

void StereoCamera_Update(PLAYER *lp, STEREO_EYE eye)
{
    // Update stereo camera for the given eye
    // This is called before rendering each eye

    if (gStereoMode == STEREO_DISABLED) {
        gCurrentStereoEye = STEREO_EYE_MONO;
        return;
    }

    StereoLog_Write("StereoCamera_Update: eye=%d sep=%.2f", eye, gStereoSeparation);

    if (!stereo_initialized) {
        StereoCamera_Init();
    }

    gCurrentStereoEye = eye;

    // Calculate and apply eye separation directly
    // Left eye: negative offset (left), Right eye: positive offset (right)
    float sep_distance = gStereoSeparation * 20.0f;  // Scale factor for world units
    int offset = (int)(sep_distance * 0.5f);

    if (gStereoSwapEyes) {
        eye = (eye == STEREO_EYE_LEFT) ? STEREO_EYE_RIGHT : STEREO_EYE_LEFT;
    }

    if (eye == STEREO_EYE_LEFT) {
        camera_position.vx -= offset;
    } else {
        camera_position.vx += offset;
    }

    // Always print debug output for stereo camera
    printf("StereoCamera_Update: eye=%d (sep=%.2f, offset=%d), cam_vx=%d\n",
           eye, gStereoSeparation, offset, camera_position.vx);
}

void StereoCamera_GetViewMatrix(STEREO_EYE eye, MATRIX *out_matrix)
{
    // Return the appropriate view matrix for the eye
    // For now, just copy the current camera matrix
    // In a full implementation, this would compute eye-specific matrices

    if (out_matrix) {
        *out_matrix = camera_matrix;
    }
}

void StereoCamera_ApplyToRender(STEREO_EYE eye)
{
    // Apply stereo offset to the camera for rendering
    // This is called after InitCamera to adjust the camera position for stereo
    if (gStereoMode == STEREO_DISABLED || eye == STEREO_EYE_MONO) {
        return;
    }

    // Apply the eye offset along the camera's right axis (perpendicular to view).
    // camera_matrix is the inverse view matrix; its first column is the camera's
    // right vector in world space (fixed-point, 4096 = 1.0).
    StereoCamera_ApplyEyeOffset(eye);

    int offset = (int)(gStereoSeparation * 2.0f);  // subtle separation in world units
    if (gStereoSwapEyes) {
        eye = (eye == STEREO_EYE_LEFT) ? STEREO_EYE_RIGHT : STEREO_EYE_LEFT;
    }
    int sign = (eye == STEREO_EYE_LEFT) ? -1 : 1;

    int rx = camera_matrix.m[0][0];
    int ry = camera_matrix.m[1][0];
    int rz = camera_matrix.m[2][0];

    camera_position.vx += (rx * offset * sign) >> 12;
    camera_position.vy += (ry * offset * sign) >> 12;
    camera_position.vz += (rz * offset * sign) >> 12;

    if (gStereoDebugLog) {
        printf("StereoCamera_ApplyToRender: eye=%d, offset=%d, cam=(%d,%d,%d)\n",
               eye, offset, camera_position.vx, camera_position.vy, camera_position.vz);
    }
}

// ============ Debug Logging ============

void StereoDebug_Init(void)
{
    char log_path[256];

    if (stereo_log_file)
        return;  // Already initialized

    // Create log file path in user home directory
    snprintf(log_path, sizeof(log_path), "%s/.driver2/stereo_debug.log", getenv("HOME") ? getenv("HOME") : ".");

    stereo_log_file = fopen(log_path, "w");
    if (stereo_log_file) {
        fprintf(stereo_log_file, "=== REDRIVER2 Stereoscopic Rendering Debug Log ===\n");
        fprintf(stereo_log_file, "Mode: %d, Separation: %.2f, Swap Eyes: %d\n\n",
                gStereoMode, gStereoSeparation, gStereoSwapEyes);
        fflush(stereo_log_file);
    }
}

void StereoDebug_LogFrame(int frame_num)
{
    if (!stereo_log_file || !gStereoDebugLog)
        return;

    if (frame_num % 30 == 0) {  // Log every 30 frames to avoid spam
        fprintf(stereo_log_file, "[Frame %d] Mode: %d, Eye: %d, Separation: %.2f\n",
                frame_num, gStereoMode, gCurrentStereoEye, gStereoSeparation);
        fflush(stereo_log_file);
    }

    stereo_frame_count = frame_num;
}

void StereoDebug_LogRenderPhase(const char *phase, STEREO_EYE eye)
{
    if (!stereo_log_file || !gStereoDebugLog)
        return;

    fprintf(stereo_log_file, "  [%s] Eye: %s\n",
            phase,
            (eye == STEREO_EYE_LEFT) ? "LEFT" : (eye == STEREO_EYE_RIGHT) ? "RIGHT" : "MONO");
    fflush(stereo_log_file);
}

void StereoDebug_LogCameraUpdate(STEREO_EYE eye, VECTOR *pos)
{
    if (!stereo_log_file || !gStereoDebugLog)
        return;

    if (stereo_frame_count % 30 == 0) {
        fprintf(stereo_log_file, "    Camera offset: eye=%s, offset_x=%d\n",
                (eye == STEREO_EYE_LEFT) ? "LEFT" : "RIGHT",
                pos ? pos->vx : 0);
        fflush(stereo_log_file);
    }
}

void StereoDebug_Shutdown(void)
{
    if (stereo_log_file) {
        fprintf(stereo_log_file, "\n=== End of Log ===\n");
        fclose(stereo_log_file);
        stereo_log_file = NULL;
    }
}

// ============ Iteration Log (stereo_iter.log) ============
// Always-on file logger, independent of gStereoDebugLog, so the renderer
// codepath is observable without a GUI window. Flushed on every write.

static FILE *g_stereo_iter_log = NULL;

void StereoLog_Open(void)
{
    if (g_stereo_iter_log)
        return;

    g_stereo_iter_log = fopen("stereo_iter.log", "w");
    if (g_stereo_iter_log) {
        fprintf(g_stereo_iter_log, "=== REDRIVER2 stereo iteration log ===\n");
        fprintf(g_stereo_iter_log, "Mode=%d Sep=%.2f Conv=%.2f Swap=%d\n\n",
                gStereoMode, gStereoSeparation, gStereoConvergence, gStereoSwapEyes);
        fflush(g_stereo_iter_log);
    }
}

void StereoLog_Write(const char *fmt, ...)
{
    if (!g_stereo_iter_log)
        return;

    // Prepend a timestamp so frame rate / timing can be measured.
    fprintf(g_stereo_iter_log, "[%u] ", (unsigned int)SDL_GetTicks());

    va_list args;
    va_start(args, fmt);
    vfprintf(g_stereo_iter_log, fmt, args);
    va_end(args);
    fprintf(g_stereo_iter_log, "\n");
    fflush(g_stereo_iter_log);
}

void StereoLog_Close(void)
{
    if (g_stereo_iter_log) {
        fprintf(g_stereo_iter_log, "\n=== End of iteration log ===\n");
        fclose(g_stereo_iter_log);
        g_stereo_iter_log = NULL;
    }
}

// ============ Scissor Test Helpers for Interlaced Rendering ============

void StereoScissor_SetOddScanlines(int screenHeight)
{
    // Enable scissor test and configure for odd scanlines (1, 3, 5, ...)
    // Note: In OpenGL, viewport Y coordinates start at bottom (0 = bottom)
    // For interlaced rendering, we want to render every other scanline

    GR_SetScissorState(1);  // Enable scissor test

    // Set scissor to render only odd scanlines
    // Since scanlines are interleaved, we use a pattern of height 2
    // Starting at Y=1 (first odd scanline from bottom)
    // This would require glScissor with proper parameters

    if (gStereoDebugLog) {
        printf("StereoScissor_SetOddScanlines: height=%d\n", screenHeight);
    }
}

void StereoScissor_SetEvenScanlines(int screenHeight)
{
    // Enable scissor test and configure for even scanlines (0, 2, 4, ...)

    GR_SetScissorState(1);  // Enable scissor test

    // Set scissor to render only even scanlines
    // Starting at Y=0 (first even scanline from bottom)

    if (gStereoDebugLog) {
        printf("StereoScissor_SetEvenScanlines: height=%d\n", screenHeight);
    }
}

void StereoScissor_Disable(void)
{
    // Disable scissor test
    GR_SetScissorState(0);

    if (gStereoDebugLog) {
        printf("StereoScissor_Disable\n");
    }
}

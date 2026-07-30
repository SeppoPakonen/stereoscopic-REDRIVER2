#include "stereo_compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PsyX/PsyX_render.h"

// Global compositor state
static STEREO_COMPOSITOR g_compositor;
static int g_compositor_initialized = 0;
static RECT16 g_eye_render_region;
static uintptr_t g_anaglyph_fullcolor_shader = 0;

// Anaglyph shader sources (GLSL)
static const char* g_anaglyph_simple_shader_source =
    "#version 120\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    vec4 left = texture2D(leftEyeTexture, v_texcoord);\n"
    "    vec4 right = texture2D(rightEyeTexture, v_texcoord);\n"
    "    // Simple red-cyan anaglyph: left R channel, right G+B channels\n"
    "    gl_FragColor = vec4(left.r, right.g, right.b, 1.0);\n"
    "}\n";

static const char* g_anaglyph_fullcolor_shader_source =
    "#version 120\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    vec4 left = texture2D(leftEyeTexture, v_texcoord);\n"
    "    vec4 right = texture2D(rightEyeTexture, v_texcoord);\n"
    "    // Full-color anaglyph using improved color matrix\n"
    "    // Left eye contributes primarily to red channel\n"
    "    // Right eye contributes to green and blue channels with some red spillover for better color\n"
    "    float left_lum = dot(left.rgb, vec3(0.3, 0.59, 0.11));\n"
    "    float right_lum = dot(right.rgb, vec3(0.3, 0.59, 0.11));\n"
    "    vec3 result = vec3(left.r * 0.8, right.g * 0.9, right.b * 0.9);\n"
    "    // Add slight color from the opposite eye for better color reproduction\n"
    "    result += vec3(right_lum * 0.1, left_lum * 0.1, left_lum * 0.1);\n"
    "    gl_FragColor = vec4(result, 1.0);\n"
    "}\n";

static const char* g_sidebyside_shader_source =
    "#version 120\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    vec2 texCoord = v_texcoord;\n"
    "    // Left half is left eye, right half is right eye\n"
    "    if (texCoord.x < 0.5) {\n"
    "        texCoord.x *= 2.0;\n"
    "        gl_FragColor = texture2D(leftEyeTexture, texCoord);\n"
    "    } else {\n"
    "        texCoord.x = (texCoord.x - 0.5) * 2.0;\n"
    "        gl_FragColor = texture2D(rightEyeTexture, texCoord);\n"
    "    }\n"
    "}\n";

void StereoCompositor_Init(int width, int height)
{
    if (g_compositor_initialized)
        return;

    if (gStereoDebugLog) {
        printf("StereoCompositor_Init: %dx%d\n", width, height);
    }

    // Initialize compositor state
    g_compositor.width = width;
    g_compositor.height = height;
    g_compositor.initialized = 1;

    // Create RGBA textures for left and right eye rendering
    // Initially empty; will be filled by render-to-texture operations
    g_compositor.left_eye_texture = GR_CreateRGBATexture(width, height, NULL);
    g_compositor.right_eye_texture = GR_CreateRGBATexture(width, height, NULL);

    // Compile anaglyph shaders
    g_compositor.anaglyph_shader = GR_Shader_Compile(g_anaglyph_simple_shader_source, 0);
    g_anaglyph_fullcolor_shader = GR_Shader_Compile(g_anaglyph_fullcolor_shader_source, 0);

    // Compile side-by-side shader
    g_compositor.sidebyside_shader = GR_Shader_Compile(g_sidebyside_shader_source, 0);

    g_compositor_initialized = 1;

    if (gStereoDebugLog) {
        printf("StereoCompositor_Init complete. Left texture: %p, Right texture: %p\n",
               (void*)g_compositor.left_eye_texture, (void*)g_compositor.right_eye_texture);
    }
}

void StereoCompositor_Shutdown(void)
{
    if (!g_compositor_initialized)
        return;

    if (g_compositor.left_eye_texture) {
        GR_DestroyTexture(g_compositor.left_eye_texture);
        g_compositor.left_eye_texture = 0;
    }

    if (g_compositor.right_eye_texture) {
        GR_DestroyTexture(g_compositor.right_eye_texture);
        g_compositor.right_eye_texture = 0;
    }

    g_compositor_initialized = 0;
}

int StereoCompositor_BeginEyeRender(STEREO_EYE eye, RECT16 *region)
{
    if (!g_compositor_initialized)
        return 0;

    // Set offscreen rendering to the appropriate eye texture
    // PsyX will render to texture instead of screen
    if (region) {
        memcpy(&g_eye_render_region, region, sizeof(RECT16));
    } else {
        // Use full screen region if not specified
        g_eye_render_region.x = 0;
        g_eye_render_region.y = 0;
        g_eye_render_region.w = g_compositor.width;
        g_eye_render_region.h = g_compositor.height;
    }

    GR_SetOffscreenState(&g_eye_render_region, 1);

    if (gStereoDebugLog) {
        printf("StereoCompositor_BeginEyeRender: eye=%d, region(%d,%d %dx%d)\n",
               eye, g_eye_render_region.x, g_eye_render_region.y,
               g_eye_render_region.w, g_eye_render_region.h);
    }

    return 1;
}

void StereoCompositor_EndEyeRender(void)
{
    if (!g_compositor_initialized)
        return;

    // End offscreen rendering; PsyX will blit the rendered texture to the active texture target
    GR_SetOffscreenState(NULL, 0);

    if (gStereoDebugLog) {
        printf("StereoCompositor_EndEyeRender\n");
    }
}

void StereoCompositor_Composite(STEREO_MODE mode)
{
    if (!g_compositor_initialized)
        return;

    if (gStereoDebugLog) {
        printf("StereoCompositor_Composite: mode=%d\n", mode);
    }

    // Select appropriate shader based on stereo mode
    uintptr_t shader = 0;
    switch (mode) {
        case STEREO_ANAGLYPH_SIMPLE:
            shader = g_compositor.anaglyph_shader;
            break;
        case STEREO_ANAGLYPH_FULLCOLOR:
            shader = g_anaglyph_fullcolor_shader;
            break;
        case STEREO_SIDEBYSIDE:
            shader = g_compositor.sidebyside_shader;
            break;
        case STEREO_TOPBOTTOM:
        case STEREO_INTERLACED:
            // TODO: Implement top-bottom and interlaced shaders
            shader = g_compositor.anaglyph_shader;  // Fallback for now
            break;
        default:
            return;
    }

    // Set the compositor shader
    if (shader) {
        GR_SetShader(shader);
    }

    // TODO: Set shader uniforms for texture sources
    // TODO: Render fullscreen quad with shader to composite textures to screen

    if (gStereoDebugLog) {
        printf("StereoCompositor_Composite complete\n");
    }
}

STEREO_COMPOSITOR* StereoCompositor_GetState(void)
{
    if (!g_compositor_initialized)
        return NULL;
    return &g_compositor;
}

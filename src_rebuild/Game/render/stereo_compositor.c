#include "stereo_compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PsyX/PsyX_render.h"

// OpenGL includes for framebuffer and VAO operations
// (Optional - only if using OpenGL FBO rendering)
// #include <GL/glad.h>  // Requires explicit OpenGL configuration

// Global compositor state
static STEREO_COMPOSITOR g_compositor;
static int g_compositor_initialized = 0;
static RECT16 g_eye_render_region;
static STEREO_EYE g_current_render_eye = STEREO_EYE_MONO;
static int g_in_eye_render = 0;

// Shader with vertex/fragment separation for PsyX
static const char* g_anaglyph_simple_shader_source =
    "varying vec2 v_texcoord;\n"
    "#ifdef VERTEX\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord.xy;\n"
    "    gl_Position = a_position;\n"
    "}\n"
    "#else\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "void main() {\n"
    "    vec4 left = texture2D(leftEyeTexture, v_texcoord);\n"
    "    vec4 right = texture2D(rightEyeTexture, v_texcoord);\n"
    "    fragColor = vec4(left.r, right.g, right.b, 1.0);\n"
    "}\n"
    "#endif\n";

static const char* g_anaglyph_fullcolor_shader_source =
    "varying vec2 v_texcoord;\n"
    "#ifdef VERTEX\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord.xy;\n"
    "    gl_Position = a_position;\n"
    "}\n"
    "#else\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "void main() {\n"
    "    vec4 left = texture2D(leftEyeTexture, v_texcoord);\n"
    "    vec4 right = texture2D(rightEyeTexture, v_texcoord);\n"
    "    float left_lum = dot(left.rgb, vec3(0.3, 0.59, 0.11));\n"
    "    float right_lum = dot(right.rgb, vec3(0.3, 0.59, 0.11));\n"
    "    vec3 result = vec3(left.r * 0.8, right.g * 0.9, right.b * 0.9);\n"
    "    result += vec3(right_lum * 0.1, left_lum * 0.1, left_lum * 0.1);\n"
    "    fragColor = vec4(result, 1.0);\n"
    "}\n"
    "#endif\n";

static const char* g_sidebyside_shader_source =
    "varying vec2 v_texcoord;\n"
    "#ifdef VERTEX\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord.xy;\n"
    "    gl_Position = a_position;\n"
    "}\n"
    "#else\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "void main() {\n"
    "    vec2 texCoord = v_texcoord;\n"
    "    if (texCoord.x < 0.5) {\n"
    "        texCoord.x *= 2.0;\n"
    "        fragColor = texture2D(leftEyeTexture, texCoord);\n"
    "    } else {\n"
    "        texCoord.x = (texCoord.x - 0.5) * 2.0;\n"
    "        fragColor = texture2D(rightEyeTexture, texCoord);\n"
    "    }\n"
    "}\n"
    "#endif\n";

static const char* g_topbottom_shader_source =
    "varying vec2 v_texcoord;\n"
    "#ifdef VERTEX\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord.xy;\n"
    "    gl_Position = a_position;\n"
    "}\n"
    "#else\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "void main() {\n"
    "    vec2 texCoord = v_texcoord;\n"
    "    if (texCoord.y < 0.5) {\n"
    "        texCoord.y *= 2.0;\n"
    "        fragColor = texture2D(leftEyeTexture, texCoord);\n"
    "    } else {\n"
    "        texCoord.y = (texCoord.y - 0.5) * 2.0;\n"
    "        fragColor = texture2D(rightEyeTexture, texCoord);\n"
    "    }\n"
    "}\n"
    "#endif\n";

static const char* g_interlaced_shader_source =
    "varying vec2 v_texcoord;\n"
    "#ifdef VERTEX\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord.xy;\n"
    "    gl_Position = a_position;\n"
    "}\n"
    "#else\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "uniform vec2 screenSize;\n"
    "void main() {\n"
    "    float scanline = mod(gl_FragCoord.y, 2.0);\n"
    "    if (scanline > 0.5) {\n"
    "        fragColor = texture2D(leftEyeTexture, v_texcoord);\n"
    "    } else {\n"
    "        fragColor = texture2D(rightEyeTexture, v_texcoord);\n"
    "    }\n"
    "}\n"
    "#endif\n";

// Polarized stereoscopy shader
// Left image: even scanlines
// Right image: odd scanlines
// Each scanline is marked with polarization state (encoded in output)
static const char* g_polarized_shader_source =
    "varying vec2 v_texcoord;\n"
    "#ifdef VERTEX\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord.xy;\n"
    "    gl_Position = a_position;\n"
    "}\n"
    "#else\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "uniform vec2 screenSize;\n"
    "void main() {\n"
    "    float scanline = mod(gl_FragCoord.y, 2.0);\n"
    "    vec4 color;\n"
    "    if (scanline > 0.5) {\n"
    "        color = texture2D(rightEyeTexture, v_texcoord);\n"
    "    } else {\n"
    "        color = texture2D(leftEyeTexture, v_texcoord);\n"
    "    }\n"
    "    fragColor = color;\n"
    "}\n"
    "#endif\n";

// Checkerboard pattern shader
// Interleave pixels in checkerboard pattern
// Left eye on black squares, right eye on white squares
static const char* g_checkerboard_shader_source =
    "varying vec2 v_texcoord;\n"
    "#ifdef VERTEX\n"
    "attribute vec4 a_position;\n"
    "attribute vec4 a_texcoord;\n"
    "void main() {\n"
    "    v_texcoord = a_texcoord.xy;\n"
    "    gl_Position = a_position;\n"
    "}\n"
    "#else\n"
    "uniform sampler2D leftEyeTexture;\n"
    "uniform sampler2D rightEyeTexture;\n"
    "uniform vec2 screenSize;\n"
    "void main() {\n"
    "    float pixelX = gl_FragCoord.x;\n"
    "    float pixelY = gl_FragCoord.y;\n"
    "    float checker = mod(pixelX + pixelY, 2.0);\n"
    "    vec4 color;\n"
    "    if (checker > 0.5) {\n"
    "        color = texture2D(rightEyeTexture, v_texcoord);\n"
    "    } else {\n"
    "        color = texture2D(leftEyeTexture, v_texcoord);\n"
    "    }\n"
    "    fragColor = color;\n"
    "}\n"
    "#endif\n";

// Vertex shader for fullscreen quad rendering
static const char* g_fullscreen_quad_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec2 texCoord;\n"
    "out vec2 v_texcoord;\n"
    "void main() {\n"
    "    // Full screen quad with texture coordinates\n"
    "    // Vertices: (-1,-1), (1,-1), (-1,1), (1,1)\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "    v_texcoord = texCoord;\n"
    "}\n";

// Helper function to create framebuffer object for a texture
static uintptr_t CreateFramebufferForTexture(uintptr_t texture, int width, int height)
{
#if defined(USE_OPENGL)
    GLuint fbo = 0;
    GLuint tex = (GLuint)(uintptr_t)texture;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    // Check framebuffer status
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("StereoCompositor: Framebuffer incomplete! Status: 0x%x\n", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (gStereoDebugLog) {
        printf("StereoCompositor: Created FBO %u for texture %u\n", fbo, tex);
    }

    return (uintptr_t)fbo;
#else
    return 0;
#endif
}

// Helper function to create fullscreen quad VAO
static void CreateFullscreenQuadVAO(uintptr_t *out_vao, uintptr_t *out_vbo)
{
#if defined(USE_OPENGL)
    GLuint vao = 0, vbo = 0;

    // Fullscreen quad vertices (NDC coordinates)
    // Position (x, y) and TexCoord (u, v)
    float vertices[] = {
        // Position           TexCoord
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,  // Bottom-left
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,  // Bottom-right
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f,  // Top-left
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,  // Top-right
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Vertex position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // Texture coordinate attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    *out_vao = (uintptr_t)vao;
    *out_vbo = (uintptr_t)vbo;

    if (gStereoDebugLog) {
        printf("StereoCompositor: Created fullscreen quad VAO %u, VBO %u\n", vao, vbo);
    }
#endif
}

void StereoCompositor_Init(int width, int height)
{
    if (g_compositor_initialized)
        return;

    if (gStereoDebugLog) {
        printf("StereoCompositor_Init: %dx%d\n", width, height);
    }

    // Safety check: ensure we have valid parameters
    if (width <= 0 || height <= 0) {
        printf("StereoCompositor_Init: Invalid dimensions %dx%d\n", width, height);
        return;
    }

    // Initialize compositor state
    g_compositor.width = width;
    g_compositor.height = height;
    g_compositor.initialized = 1;
    g_compositor.use_render_to_texture = 0;  // Disable RTT by default - only enable if GL is ready
    g_compositor.last_composite_time = 0.0f;

    // Create RGBA textures for left and right eye rendering
    // Initially empty; will be filled by render-to-texture operations
    printf("StereoCompositor_Init: Skipping texture creation for now...\n");
    g_compositor.left_eye_texture = 0;
    g_compositor.right_eye_texture = 0;
    printf("StereoCompositor_Init: Texture creation skipped\n");

    if (gStereoDebugLog) {
        printf("StereoCompositor: Created left texture %p, right texture %p\n",
               (void*)g_compositor.left_eye_texture, (void*)g_compositor.right_eye_texture);
    }

#if defined(USE_OPENGL)
    // We composite from the PsyX offscreen render targets (g_offscreenRTTexture /
    // g_offscreenRTTexture2), so we only need the fullscreen quad + shaders here.
    // The left/right eye textures are set from the offscreen targets each frame.
    CreateFullscreenQuadVAO(&g_compositor.fullscreen_quad_vao, &g_compositor.fullscreen_quad_vbo);
    g_compositor.use_render_to_texture = 1;
#endif

    // Dump shaders for debugging
    FILE* shader_dump = fopen("shader_sources.txt", "w");
    if (shader_dump) {
        fprintf(shader_dump, "=== ANAGLYPH SIMPLE ===\n%s\n\n", g_anaglyph_simple_shader_source);
        fprintf(shader_dump, "=== ANAGLYPH FULLCOLOR ===\n%s\n\n", g_anaglyph_fullcolor_shader_source);
        fprintf(shader_dump, "=== SIDEBYSIDE ===\n%s\n\n", g_sidebyside_shader_source);
        fprintf(shader_dump, "=== TOPBOTTOM ===\n%s\n\n", g_topbottom_shader_source);
        fprintf(shader_dump, "=== INTERLACED ===\n%s\n\n", g_interlaced_shader_source);
        fprintf(shader_dump, "=== POLARIZED ===\n%s\n\n", g_polarized_shader_source);
        fprintf(shader_dump, "=== CHECKERBOARD ===\n%s\n\n", g_checkerboard_shader_source);
        fprintf(shader_dump, "=== VERTEX SHADER ===\n%s\n\n", g_fullscreen_quad_vertex_shader_source);
        fclose(shader_dump);
        printf("Shader sources dumped to shader_sources.txt\n");
    }

    // Compile anaglyph shaders
    printf("Compiling anaglyph simple shader...\n");
    g_compositor.anaglyph_shader = (uintptr_t)GR_Shader_Compile(g_anaglyph_simple_shader_source, 0);
    printf("Anaglyph simple shader result: %p\n", (void*)g_compositor.anaglyph_shader);

    printf("Compiling anaglyph fullcolor shader...\n");
    g_compositor.anaglyph_fullcolor_shader = (uintptr_t)GR_Shader_Compile(g_anaglyph_fullcolor_shader_source, 0);
    printf("Anaglyph fullcolor shader result: %p\n", (void*)g_compositor.anaglyph_fullcolor_shader);

    // Compile side-by-side shader
    printf("Compiling sidebyside shader...\n");
    g_compositor.sidebyside_shader = (uintptr_t)GR_Shader_Compile(g_sidebyside_shader_source, 0);
    printf("Sidebyside shader result: %p\n", (void*)g_compositor.sidebyside_shader);

    // Compile top-bottom shader
    printf("Compiling topbottom shader...\n");
    g_compositor.topbottom_shader = (uintptr_t)GR_Shader_Compile(g_topbottom_shader_source, 0);
    printf("Topbottom shader result: %p\n", (void*)g_compositor.topbottom_shader);

    // Compile interlaced scanline shader
    printf("Compiling interlaced shader...\n");
    g_compositor.interlaced_shader = (uintptr_t)GR_Shader_Compile(g_interlaced_shader_source, 0);
    printf("Interlaced shader result: %p\n", (void*)g_compositor.interlaced_shader);

    // Compile polarized stereoscopy shader
    printf("Compiling polarized shader...\n");
    g_compositor.polarized_shader = (uintptr_t)GR_Shader_Compile(g_polarized_shader_source, 0);
    printf("Polarized shader result: %p\n", (void*)g_compositor.polarized_shader);

    // Compile checkerboard pattern shader
    printf("Compiling checkerboard shader...\n");
    g_compositor.checkerboard_shader = (uintptr_t)GR_Shader_Compile(g_checkerboard_shader_source, 0);
    printf("Checkerboard shader result: %p\n", (void*)g_compositor.checkerboard_shader);

    g_compositor_initialized = 1;

    if (gStereoDebugLog) {
        printf("StereoCompositor_Init complete. RTT %s\n",
               g_compositor.use_render_to_texture ? "enabled" : "disabled");
    }
}

void StereoCompositor_Shutdown(void)
{
    if (!g_compositor_initialized)
        return;

    if (gStereoDebugLog) {
        printf("StereoCompositor_Shutdown\n");
    }

#if defined(USE_OPENGL)
    // Clean up framebuffer objects
    if (g_compositor.left_eye_fbo) {
        GLuint fbo = (GLuint)(uintptr_t)g_compositor.left_eye_fbo;
        glDeleteFramebuffers(1, &fbo);
        g_compositor.left_eye_fbo = 0;
    }

    if (g_compositor.right_eye_fbo) {
        GLuint fbo = (GLuint)(uintptr_t)g_compositor.right_eye_fbo;
        glDeleteFramebuffers(1, &fbo);
        g_compositor.right_eye_fbo = 0;
    }

    // Clean up fullscreen quad
    if (g_compositor.fullscreen_quad_vao) {
        GLuint vao = (GLuint)(uintptr_t)g_compositor.fullscreen_quad_vao;
        glDeleteVertexArrays(1, &vao);
        g_compositor.fullscreen_quad_vao = 0;
    }

    if (g_compositor.fullscreen_quad_vbo) {
        GLuint vbo = (GLuint)(uintptr_t)g_compositor.fullscreen_quad_vbo;
        glDeleteBuffers(1, &vbo);
        g_compositor.fullscreen_quad_vbo = 0;
    }
#endif

    if (g_compositor.left_eye_texture) {
        GR_DestroyTexture((uintptr_t)g_compositor.left_eye_texture);
        g_compositor.left_eye_texture = 0;
    }

    if (g_compositor.right_eye_texture) {
        GR_DestroyTexture((uintptr_t)g_compositor.right_eye_texture);
        g_compositor.right_eye_texture = 0;
    }

    g_compositor_initialized = 0;
}

int StereoCompositor_BeginEyeRender(STEREO_EYE eye, RECT16 *region)
{
    if (!g_compositor_initialized)
        return 0;

    if (!g_compositor.use_render_to_texture) {
        // Fallback: render to screen using viewport clipping
        if (gStereoDebugLog) {
            printf("StereoCompositor_BeginEyeRender: fallback mode (no RTT)\n");
        }
        return 1;
    }

    // Set up render region
    if (region) {
        memcpy(&g_eye_render_region, region, sizeof(RECT16));
    } else {
        // Use full screen region if not specified
        g_eye_render_region.x = 0;
        g_eye_render_region.y = 0;
        g_eye_render_region.w = g_compositor.width;
        g_eye_render_region.h = g_compositor.height;
    }

    g_current_render_eye = eye;

#if defined(USE_OPENGL)
    // Bind the appropriate framebuffer for this eye
    uintptr_t fbo = (eye == STEREO_EYE_LEFT) ? g_compositor.left_eye_fbo : g_compositor.right_eye_fbo;

    if (fbo) {
        GLuint gl_fbo = (GLuint)(uintptr_t)fbo;
        glBindFramebuffer(GL_FRAMEBUFFER, gl_fbo);
        glViewport(0, 0, g_eye_render_region.w, g_eye_render_region.h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        g_in_eye_render = 1;

        if (gStereoDebugLog) {
            printf("StereoCompositor_BeginEyeRender: eye=%d (RTT), FBO=%u\n", eye, gl_fbo);
        }

        return 1;
    }
#endif

    if (gStereoDebugLog) {
        printf("StereoCompositor_BeginEyeRender: eye=%d, failed to bind FBO\n", eye);
    }

    return 0;
}

void StereoCompositor_EndEyeRender(void)
{
    if (!g_compositor_initialized)
        return;

    if (!g_compositor.use_render_to_texture) {
        return;
    }

#if defined(USE_OPENGL)
    if (g_in_eye_render) {
        // Unbind framebuffer and return to screen rendering
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, g_compositor.width, g_compositor.height);

        g_in_eye_render = 0;

        if (gStereoDebugLog) {
            printf("StereoCompositor_EndEyeRender\n");
        }
    }
#endif
}

void StereoCompositor_RenderFullscreenQuad(uintptr_t shader)
{
#if defined(USE_OPENGL)
    if (!shader || !g_compositor.fullscreen_quad_vao)
        return;

    GLuint gl_shader = (GLuint)(uintptr_t)shader;
    GLuint gl_vao = (GLuint)(uintptr_t)g_compositor.fullscreen_quad_vao;

    // Use the shader program
    glUseProgram(gl_shader);

    // Set up texture uniforms
    GLint leftTexLoc = glGetUniformLocation(gl_shader, "leftEyeTexture");
    GLint rightTexLoc = glGetUniformLocation(gl_shader, "rightEyeTexture");
    GLint screenSizeLoc = glGetUniformLocation(gl_shader, "screenSize");

    // Bind textures to texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)g_compositor.left_eye_texture);
    if (leftTexLoc >= 0) {
        glUniform1i(leftTexLoc, 0);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)g_compositor.right_eye_texture);
    if (rightTexLoc >= 0) {
        glUniform1i(rightTexLoc, 1);
    }

    // Set screen size uniform if available
    if (screenSizeLoc >= 0) {
        glUniform2f(screenSizeLoc, (float)g_compositor.width, (float)g_compositor.height);
    }

    // Render fullscreen quad
    glBindVertexArray(gl_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    // Reset texture units
    glActiveTexture(GL_TEXTURE0);

    if (gStereoDebugLog) {
        printf("StereoCompositor: Rendered fullscreen quad\n");
    }
#endif
}

void StereoCompositor_Composite(STEREO_MODE mode)
{
    if (!g_compositor_initialized)
        return;

    if (gStereoDebugLog) {
        printf("StereoCompositor_Composite: mode=%d, RTT=%s\n", mode,
               g_compositor.use_render_to_texture ? "yes" : "no");
    }

    // If RTT is not available, skip composite (will use fallback double render)
    if (!g_compositor.use_render_to_texture) {
        if (gStereoDebugLog) {
            printf("StereoCompositor_Composite: RTT disabled, skipping\n");
        }
        return;
    }

    // The eye images were rendered into the PsyX offscreen render targets.
    // Point the compositor at those textures for the shader.
    extern TextureID g_offscreenRTTexture;
    extern TextureID g_offscreenRTTexture2;
    g_compositor.left_eye_texture = g_offscreenRTTexture;
    g_compositor.right_eye_texture = g_offscreenRTTexture2;

    // Select appropriate shader based on stereo mode
    uintptr_t shader = 0;
    switch (mode) {
        case STEREO_ANAGLYPH_SIMPLE:
            shader = g_compositor.anaglyph_shader;
            break;
        case STEREO_ANAGLYPH_FULLCOLOR:
            shader = g_compositor.anaglyph_fullcolor_shader;
            break;
        case STEREO_SIDEBYSIDE:
            shader = g_compositor.sidebyside_shader;
            break;
        case STEREO_TOPBOTTOM:
            shader = g_compositor.topbottom_shader;
            break;
        case STEREO_INTERLACED:
            shader = g_compositor.interlaced_shader;
            break;
        case STEREO_POLARIZED:
            shader = g_compositor.polarized_shader;
            break;
        case STEREO_CHECKERBOARD:
            shader = g_compositor.checkerboard_shader;
            break;
        default:
            if (gStereoDebugLog) {
                printf("StereoCompositor_Composite: Unknown mode %d\n", mode);
            }
            return;
    }

    if (!shader) {
        printf("StereoCompositor_Composite: Shader not compiled for mode %d\n", mode);
        return;
    }

#if defined(USE_OPENGL)
    // Make sure we're rendering to the backbuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_compositor.width, g_compositor.height);

    // Disable depth test for quad rendering
    glDisable(GL_DEPTH_TEST);

    // Render the fullscreen quad with the composition shader
    StereoCompositor_RenderFullscreenQuad(shader);

    // Re-enable depth test
    glEnable(GL_DEPTH_TEST);

    glUseProgram(0);

    if (gStereoDebugLog) {
        printf("StereoCompositor_Composite complete\n");
    }
#endif
}

STEREO_COMPOSITOR* StereoCompositor_GetState(void)
{
    if (!g_compositor_initialized)
        return NULL;
    return &g_compositor;
}

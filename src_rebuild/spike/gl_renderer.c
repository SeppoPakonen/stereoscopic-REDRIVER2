// gl_renderer.c — T4.4 modern OpenGL mono-path renderer module.
//
// Implements GlRenderer (see gl_renderer.h): SDL window + OpenGL 3.3
// core-profile context, glad loader, a VAO + interleaved pos/color VBO + indexed
// EBO, a GLSL 150 core VS/FS pair, an orthographic screen->NDC projection
// (column-major, y-flip), and a per-frame render of screen-space flat quads
// directly to the default framebuffer with a glReadPixels -> BMP capture (before
// the swap). This is the GL mirror of the DX11 mono path (T4.3) — a standard
// renderer stack, not the PSX primitive/OT model.

#include "gl_renderer.h"

#include "PsyX/common/glad.h"   // GL + gladLoadGL (needs PsyCross/include on the path)
#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// GLSL 150 core shaders. aColor is the per-vertex flat color (0..1); uProj is
// the orthographic screen->NDC matrix (column-major, standard GL convention).
// ---------------------------------------------------------------------------
static const char *VS_SRC =
    "#version 150 core\n"
    "in vec2 aPos;\n"
    "in vec3 aColor;\n"
    "out vec3 vColor;\n"
    "uniform mat4 uProj;\n"
    "void main(){\n"
    "  gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
    "  vColor = aColor;\n"
    "}\n";

static const char *FS_SRC =
    "#version 150 core\n"
    "in vec3 vColor;\n"
    "out vec4 fragColor;\n"
    "void main(){\n"
    "  fragColor = vec4(vColor, 1.0);\n"
    "}\n";

struct GlRenderer {
    SDL_Window *win;
    SDL_GLContext ctx;
    GLuint program;
    GLint uProj;
    GLuint vao, vbo, ebo;
    int width, height;
};

// ---------------------------------------------------------------------------
// Shader / program helpers.
// ---------------------------------------------------------------------------
static GLuint CompileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(s, sizeof(log), &len, log);
        fprintf(stderr, "gl_renderer: shader compile error: %.*s\n", (int)len, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint LinkProgram(const char *vsSrc, const char *fsSrc) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) { if (vs) glDeleteShader(vs); if (fs) glDeleteShader(fs); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "aPos");
    glBindAttribLocation(p, 1, "aColor");
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog(p, sizeof(log), &len, log);
        fprintf(stderr, "gl_renderer: program link error: %.*s\n", (int)len, log);
        glDeleteProgram(p);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

// ---------------------------------------------------------------------------
// BMP writing: writes a standard bottom-up 24-bit BGR BMP from a TOP-DOWN BGR
// buffer (row 0 = top-left). Matches Dx11Renderer_CaptureToBMP's format so the
// A/B compare probes both screenshots identically.
// ---------------------------------------------------------------------------
static int WriteBMP(const char *path, int w, int h, const unsigned char *bgr) {
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    int rowSize = (w * 3 + 3) & ~3;
    int dataSize = rowSize * h;
    unsigned int fileSize = 54 + (unsigned int)dataSize;
    unsigned char hdr[54] = { 0 };
    hdr[0] = 'B'; hdr[1] = 'M';
    hdr[2] = (unsigned char)(fileSize & 0xFF);
    hdr[3] = (unsigned char)((fileSize >> 8) & 0xFF);
    hdr[4] = (unsigned char)((fileSize >> 16) & 0xFF);
    hdr[5] = (unsigned char)((fileSize >> 24) & 0xFF);
    hdr[10] = 54;                                  // pixel data offset
    hdr[14] = 40;                                  // BITMAPINFOHEADER size
    hdr[18] = (unsigned char)(w & 0xFF);           // width
    hdr[19] = (unsigned char)((w >> 8) & 0xFF);
    hdr[20] = (unsigned char)((w >> 16) & 0xFF);
    hdr[21] = (unsigned char)((w >> 24) & 0xFF);
    hdr[22] = (unsigned char)(h & 0xFF);           // height
    hdr[23] = (unsigned char)((h >> 8) & 0xFF);
    hdr[24] = (unsigned char)((h >> 16) & 0xFF);
    hdr[25] = (unsigned char)((h >> 24) & 0xFF);
    hdr[26] = 1;                                   // planes
    hdr[28] = 24;                                  // bits per pixel
    if (fwrite(hdr, 1, 54, f) != 54) { fclose(f); return 1; }
    unsigned char *pad = (unsigned char *)calloc(rowSize - w * 3, 1);
    for (int y = h - 1; y >= 0; --y) {
        const unsigned char *row = bgr + (size_t)y * w * 3;
        if (fwrite(row, 1, w * 3, f) != (size_t)w * 3) { free(pad); fclose(f); return 1; }
        if (rowSize > w * 3 && fwrite(pad, 1, rowSize - w * 3, f) != (size_t)(rowSize - w * 3)) {
            free(pad); fclose(f); return 1;
        }
    }
    free(pad);
    fclose(f);
    return 0;
}

// ---------------------------------------------------------------------------
// Lifecycle.
// ---------------------------------------------------------------------------
GlRenderer *GlRenderer_Create(const GlRendererConfig *cfg) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "gl_renderer: SDL_Init failed: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window *win = SDL_CreateWindow("gl_renderer", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, cfg->width, cfg->height,
                                       SDL_WINDOW_OPENGL);
    if (!win) {
        fprintf(stderr, "gl_renderer: SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return NULL;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        fprintf(stderr, "gl_renderer: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return NULL;
    }
    if (gladLoadGL() == 0) {
        fprintf(stderr, "gl_renderer: gladLoadGL failed\n");
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return NULL;
    }

    GlRenderer *r = (GlRenderer *)calloc(1, sizeof(*r));
    if (!r) { SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win); SDL_Quit(); return NULL; }
    r->win = win;
    r->ctx = ctx;
    r->width = cfg->width;
    r->height = cfg->height;

    r->program = LinkProgram(VS_SRC, FS_SRC);
    if (!r->program) { GlRenderer_Destroy(r); return NULL; }
    r->uProj = glGetUniformLocation(r->program, "uProj");

    // VAO + interleaved pos/color VBO + indexed EBO.
    glGenVertexArrays(1, &r->vao);
    glBindVertexArray(r->vao);
    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glGenBuffers(1, &r->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(2 * sizeof(float)));
    glBindVertexArray(0);

    return r;
}

void GlRenderer_Destroy(GlRenderer *r) {
    if (!r) return;
    if (r->program) glDeleteProgram(r->program);
    if (r->ebo) glDeleteBuffers(1, &r->ebo);
    if (r->vbo) glDeleteBuffers(1, &r->vbo);
    if (r->vao) glDeleteVertexArrays(1, &r->vao);
    if (r->ctx) SDL_GL_DeleteContext(r->ctx);
    if (r->win) SDL_DestroyWindow(r->win);
    SDL_Quit();
    free(r);
}

int GlRenderer_GetWidth(const GlRenderer *r) { return r->width; }
int GlRenderer_GetHeight(const GlRenderer *r) { return r->height; }

// ---------------------------------------------------------------------------
// Per-frame render: build the vertex/index arrays, draw once, capture to BMP.
// ---------------------------------------------------------------------------
int GlRenderer_RenderFrame(GlRenderer *r, const GlQuad *quads, int numQuads,
                           const char *bmpOut) {
    int W = r->width, H = r->height;
    float *verts = (float *)malloc((size_t)numQuads * 4 * 5 * sizeof(float));
    unsigned int *indices = (unsigned int *)malloc((size_t)numQuads * 6 * sizeof(unsigned int));
    if (!verts || !indices) { free(verts); free(indices); return 1; }

    for (int qi = 0; qi < numQuads; ++qi) {
        const GlQuad *q = &quads[qi];
        float x0 = q->x, y0 = q->y, x1 = q->x + q->w, y1 = q->y + q->h;
        float cr = q->r / 255.0f, cg = q->g / 255.0f, cb = q->b / 255.0f;
        // 4 corners: TL, TR, BR, BL (winding irrelevant; culling disabled).
        float corner[4][2] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
        for (int c = 0; c < 4; ++c) {
            float *v = verts + ((size_t)qi * 4 + c) * 5;
            v[0] = corner[c][0]; v[1] = corner[c][1];
            v[2] = cr; v[3] = cg; v[4] = cb;
        }
        unsigned int base = (unsigned int)qi * 4;
        unsigned int *idx = indices + (size_t)qi * 6;
        idx[0] = base + 0; idx[1] = base + 1; idx[2] = base + 2;
        idx[3] = base + 0; idx[4] = base + 2; idx[5] = base + 3;
    }

    glViewport(0, 0, W, H);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_CULL_FACE);   // two-sided (mirrors the DX11 twoSided quads)
    glDisable(GL_DEPTH_TEST);

    glUseProgram(r->program);

    // Orthographic screen->NDC (column-major, m[col][row]): x 0..W -> -1..1,
    // y 0..H (top-down) -> +1..-1.
    float m[16] = { 0 };
    m[0]  = 2.0f / W;                   // m[0][0]
    m[5]  = -2.0f / H;                  // m[1][1]
    m[10] = -2.0f;                      // m[2][2]
    m[12] = -1.0f;                      // m[3][0]
    m[13] = 1.0f;                       // m[3][1]
    m[14] = -1.0f;                      // m[3][2]
    m[15] = 1.0f;                       // m[3][3]
    glUniformMatrix4fv(r->uProj, 1, GL_FALSE, m);

    glBindVertexArray(r->vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)numQuads * 4 * 5 * sizeof(float)),
                 verts, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)((size_t)numQuads * 6 * sizeof(unsigned int)),
                 indices, GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES, (GLsizei)(numQuads * 6), GL_UNSIGNED_INT, 0);

    // Read the default framebuffer BEFORE the swap. glReadPixels returns rows
    // bottom-up; flip so the BMP is top-down (row 0 = screen top).
    unsigned char *px = (unsigned char *)malloc((size_t)W * H * 4);
    unsigned char *bgr = (unsigned char *)malloc((size_t)W * H * 3);
    glReadPixels(0, 0, W, H, GL_BGRA, GL_UNSIGNED_BYTE, px);
    for (int y = 0; y < H; ++y) {
        const unsigned char *src = px + (size_t)(H - 1 - y) * W * 4;
        unsigned char *dst = bgr + (size_t)y * W * 3;
        for (int x = 0; x < W; ++x) {
            dst[x * 3 + 0] = src[x * 4 + 0];   // B
            dst[x * 3 + 1] = src[x * 4 + 1];   // G
            dst[x * 3 + 2] = src[x * 4 + 2];   // R
        }
    }
    int rw = WriteBMP(bmpOut, W, H, bgr);
    free(bgr);
    free(px);

    SDL_GL_SwapWindow(r->win);

    free(indices);
    free(verts);
    return rw;
}
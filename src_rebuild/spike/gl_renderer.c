// gl_renderer.c — T4.4/T4.6 modern OpenGL renderer module (mono path + stereo).
//
// Implements GlRenderer (see gl_renderer.h): SDL window + OpenGL 3.3
// core-profile context, glad loader, a VAO + interleaved pos/color VBO + indexed
// EBO, a GLSL 150 core VS/FS pair, an orthographic screen->NDC projection
// (column-major, y-flip), a per-frame render of screen-space flat quads to a
// target (default framebuffer or a per-eye offscreen FBO), a glReadPixels -> BMP
// capture (before the swap), and a fullscreen-triangle SBS/TB/MONO stereo
// composite (T4.6). This is the GL mirror of the DX11 stack (mono + per-eye RT +
// composite) — a standard renderer stack, not the PSX primitive/OT model.

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

// Composite: a fullscreen triangle generated from gl_VertexID (no VBO), sampling
// eye0/eye1 with a nearest sampler into SBS/TB/MONO halves (mirrors the DX11
// dx11_composite PS). uMode: 0=SBS, 1=TB, 2=MONO. uSwap flips the left/top eye.
static const char *COMP_VS_SRC =
    "#version 150 core\n"
    "out vec2 vUv;\n"
    "void main(){\n"
    "  vec2 p;\n"
    "  if (gl_VertexID == 0) p = vec2(-1.0, -1.0);\n"
    "  else if (gl_VertexID == 1) p = vec2(3.0, -1.0);\n"
    "  else p = vec2(-1.0, 3.0);\n"
    "  gl_Position = vec4(p, 0.0, 1.0);\n"
    "  vUv = vec2((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5);\n"
    "}\n";

static const char *COMP_FS_SRC =
    "#version 150 core\n"
    "in vec2 vUv;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D uEye0;\n"
    "uniform sampler2D uEye1;\n"
    "uniform int uMode;\n"
    "uniform int uSwap;\n"
    "void main(){\n"
    "  vec2 uv = vUv;\n"
    "  if (uMode == 2) { fragColor = vec4(texture(uEye0, uv).rgb, 1.0); return; }\n"
    "  int first;\n"
    "  if (uMode == 0) first = (uv.x < 0.5) ? 0 : 1;\n"
    "  else first = (uv.y < 0.5) ? 0 : 1;\n"
    "  if (uSwap == 1) first = 1 - first;\n"
    "  if (uMode == 0) uv.x = (uv.x < 0.5) ? uv.x * 2.0 : (uv.x - 0.5) * 2.0;\n"
    "  else uv.y = (uv.y < 0.5) ? uv.y * 2.0 : (uv.y - 0.5) * 2.0;\n"
    "  fragColor = vec4((first == 0 ? texture(uEye0, uv) : texture(uEye1, uv)).rgb, 1.0);\n"
    "}\n";

struct GlRenderer {
    SDL_Window *win;
    SDL_GLContext ctx;
    GLuint program;
    GLint uProj;
    GLuint vao, vbo, ebo;
    int width, height;
    int internalW, internalH;
    int curW, curH;          // current target size (for projection)
    // Per-eye offscreen targets (T4.6).
    GLuint eyeFBO[2], eyeTex[2];
    GLuint compProg;
    GLint compVAO;
    GLint uEye0, uEye1, uMode, uSwap;
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
    r->internalW = cfg->internalW ? cfg->internalW : cfg->width;
    r->internalH = cfg->internalH ? cfg->internalH : cfg->height;
    r->curW = cfg->width;
    r->curH = cfg->height;

    r->program = LinkProgram(VS_SRC, FS_SRC);
    if (!r->program) { GlRenderer_Destroy(r); return NULL; }
    r->uProj = glGetUniformLocation(r->program, "uProj");

    // Quad VAO + interleaved pos/color VBO + indexed EBO.
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

    // Composite program (fullscreen triangle via gl_VertexID -> empty VAO).
    r->compProg = LinkProgram(COMP_VS_SRC, COMP_FS_SRC);
    if (!r->compProg) { GlRenderer_Destroy(r); return NULL; }
    glGenVertexArrays(1, (GLuint *)&r->compVAO);
    glUseProgram(r->compProg);
    r->uEye0 = glGetUniformLocation(r->compProg, "uEye0");
    r->uEye1 = glGetUniformLocation(r->compProg, "uEye1");
    r->uMode = glGetUniformLocation(r->compProg, "uMode");
    r->uSwap = glGetUniformLocation(r->compProg, "uSwap");
    glUniform1i(r->uEye0, 0);
    glUniform1i(r->uEye1, 1);
    glUseProgram(0);

    // Per-eye offscreen FBOs (RGBA8, internal res, nearest sampling).
    glGenTextures(2, r->eyeTex);
    glGenFramebuffers(2, r->eyeFBO);
    for (int e = 0; e < 2; ++e) {
        glBindTexture(GL_TEXTURE_2D, r->eyeTex[e]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, r->internalW, r->internalH, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, r->eyeFBO[e]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               r->eyeTex[e], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "gl_renderer: eye FBO %d incomplete\n", e);
            GlRenderer_Destroy(r);
            return NULL;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return r;
}

void GlRenderer_Destroy(GlRenderer *r) {
    if (!r) return;
    if (r->compProg) glDeleteProgram(r->compProg);
    if (r->compVAO) glDeleteVertexArrays(1, (GLuint *)&r->compVAO);
    if (r->eyeFBO[0]) glDeleteFramebuffers(2, r->eyeFBO);
    if (r->eyeTex[0]) glDeleteTextures(2, r->eyeTex);
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
int GlRenderer_GetInternalWidth(const GlRenderer *r) { return r->internalW; }
int GlRenderer_GetInternalHeight(const GlRenderer *r) { return r->internalH; }

// ---------------------------------------------------------------------------
// Capability probe: can a modern GL 3.3 core context be created + glad loaded?
// Lightweight create/destroy, no rendering. Returns nonzero if usable.
// ---------------------------------------------------------------------------
int GlRenderer_Available(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 0;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window *w = SDL_CreateWindow("gl_probe", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 64, 64,
                                     SDL_WINDOW_OPENGL);
    if (!w) { SDL_Quit(); return 0; }
    SDL_GLContext c = SDL_GL_CreateContext(w);
    if (!c) { SDL_DestroyWindow(w); SDL_Quit(); return 0; }
    int ok = (gladLoadGL() != 0);
    SDL_GL_DeleteContext(c);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return ok;
}

// ---------------------------------------------------------------------------
// Target selection.
// ---------------------------------------------------------------------------
void GlRenderer_BindOffscreen(GlRenderer *r, int eye) {
    if (eye < 0 || eye > 1) return;
    glBindFramebuffer(GL_FRAMEBUFFER, r->eyeFBO[eye]);
    glViewport(0, 0, r->internalW, r->internalH);
    r->curW = r->internalW;
    r->curH = r->internalH;
}

void GlRenderer_BindDefault(GlRenderer *r) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, r->width, r->height);
    r->curW = r->width;
    r->curH = r->height;
}

void GlRenderer_BeginDraw(GlRenderer *r) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

// Draw `numQuads` quads to the current target (r->curW/curH). Shared by the
// mono RenderFrame path and the per-eye offscreen path.
void GlRenderer_DrawQuads(GlRenderer *r, const GlQuad *quads, int numQuads) {
    int W = r->curW, H = r->curH;
    float *verts = (float *)malloc((size_t)numQuads * 4 * 5 * sizeof(float));
    unsigned int *indices = (unsigned int *)malloc((size_t)numQuads * 6 * sizeof(unsigned int));
    if (!verts || !indices) { free(verts); free(indices); return; }

    for (int qi = 0; qi < numQuads; ++qi) {
        const GlQuad *q = &quads[qi];
        float x0 = q->x, y0 = q->y, x1 = q->x + q->w, y1 = q->y + q->h;
        float cr = q->r / 255.0f, cg = q->g / 255.0f, cb = q->b / 255.0f;
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

    glDisable(GL_CULL_FACE);   // two-sided (mirrors the DX11 twoSided quads)
    glDisable(GL_DEPTH_TEST);

    glUseProgram(r->program);

    // Orthographic screen->NDC (column-major, m[col][row]) for the current target.
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

    free(indices);
    free(verts);
}

unsigned int GlRenderer_GetEyeTexture(GlRenderer *r, int eye) {
    if (eye < 0 || eye > 1) return 0;
    return r->eyeTex[eye];
}

void GlRenderer_Composite(GlRenderer *r, GlStereoMode mode, int swap) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glUseProgram(r->compProg);
    glUniform1i(r->uMode, (int)mode);
    glUniform1i(r->uSwap, swap ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->eyeTex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, r->eyeTex[1]);
    glBindVertexArray((GLuint)r->compVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

// Read the default framebuffer (must be bound) into a top-down BMP.
static int CaptureDefault(GlRenderer *r, const char *bmpOut) {
    int W = r->width, H = r->height;
    unsigned char *px = (unsigned char *)malloc((size_t)W * H * 4);
    unsigned char *bgr = (unsigned char *)malloc((size_t)W * H * 3);
    if (!px || !bgr) { free(px); free(bgr); return 1; }
    // glReadPixels returns rows bottom-up; flip so the BMP is top-down.
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
    return rw;
}

int GlRenderer_CaptureDefaultToBMP(GlRenderer *r, const char *bmpOut) {
    return CaptureDefault(r, bmpOut);
}

// ---------------------------------------------------------------------------
// Mono frame render (T4.4): default framebuffer + capture to BMP.
// ---------------------------------------------------------------------------
int GlRenderer_RenderFrame(GlRenderer *r, const GlQuad *quads, int numQuads,
                           const char *bmpOut) {
    GlRenderer_BindDefault(r);
    GlRenderer_BeginDraw(r);
    GlRenderer_DrawQuads(r, quads, numQuads);
    int rw = CaptureDefault(r, bmpOut);
    SDL_GL_SwapWindow(r->win);
    return rw;
}
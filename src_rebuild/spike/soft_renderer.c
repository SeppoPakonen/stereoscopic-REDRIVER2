// soft_renderer.c — Simple software rasterizer for debugging the DX11 projection.

#include "soft_renderer.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct SoftRenderer {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int width;
    int height;
    unsigned int* framebuffer;  // RGB888, row-major
};

SoftRenderer* SoftRenderer_Create(int width, int height) {
    SoftRenderer* sr = (SoftRenderer*)calloc(1, sizeof(SoftRenderer));
    if (!sr) return NULL;

    sr->width = width;
    sr->height = height;
    sr->framebuffer = (unsigned int*)calloc(width * height, sizeof(unsigned int));
    if (!sr->framebuffer) { free(sr); return NULL; }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[SoftRenderer] SDL_Init failed: %s\n", SDL_GetError());
        free(sr->framebuffer); free(sr); return NULL;
    }

    sr->window = SDL_CreateWindow("Soft Renderer (Debug)",
                                  SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  width, height, 0);
    if (!sr->window) {
        printf("[SoftRenderer] SDL_CreateWindow failed: %s\n", SDL_GetError());
        free(sr->framebuffer); free(sr); return NULL;
    }

    sr->renderer = SDL_CreateRenderer(sr->window, -1, SDL_RENDERER_SOFTWARE);
    if (!sr->renderer) {
        printf("[SoftRenderer] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(sr->window); free(sr->framebuffer); free(sr); return NULL;
    }

    sr->texture = SDL_CreateTexture(sr->renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!sr->texture) {
        printf("[SoftRenderer] SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(sr->renderer); SDL_DestroyWindow(sr->window);
        free(sr->framebuffer); free(sr); return NULL;
    }

    return sr;
}

void SoftRenderer_Destroy(SoftRenderer* sr) {
    if (!sr) return;
    if (sr->texture) SDL_DestroyTexture(sr->texture);
    if (sr->renderer) SDL_DestroyRenderer(sr->renderer);
    if (sr->window) SDL_DestroyWindow(sr->window);
    free(sr->framebuffer);
    free(sr);
}

// Matrix helpers (row-vector).
static void Mat4_TransformPoint(const float p[4], const float m[4][4], float out[4]) {
    for (int j = 0; j < 4; ++j)
        out[j] = p[0]*m[0][j] + p[1]*m[1][j] + p[2]*m[2][j] + p[3]*m[3][j];
}

static void Mat4_Mul(const float a[4][4], const float b[4][4], float out[4][4]) {
    float t[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a[i][k] * b[k][j];
            t[i][j] = s;
        }
    memcpy(out, t, sizeof(t));
}

// Set a pixel (with bounds check).
static void SetPixel(SoftRenderer* sr, int x, int y, unsigned int color) {
    if (x >= 0 && x < sr->width && y >= 0 && y < sr->height)
        sr->framebuffer[y * sr->width + x] = color;
}

// Bresenham line drawing.
static void DrawLine(SoftRenderer* sr, int x0, int y0, int x1, int y1, unsigned int color) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        SetPixel(sr, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

// Draw a filled triangle (simple scanline).
static void DrawTriangle(SoftRenderer* sr, int x0, int y0, int x1, int y1, int x2, int y2, unsigned int color) {
    // Sort vertices by y
    if (y0 > y1) { int t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }
    if (y0 > y2) { int t=x0; x0=x2; x2=t; t=y0; y0=y2; y2=t; }
    if (y1 > y2) { int t=x1; x1=x2; x2=t; t=y1; y1=y2; y2=t; }

    if (y0 == y2) {
        // Horizontal line
        int minX = x0, maxX = x0;
        if (x1 < minX) minX = x1; if (x1 > maxX) maxX = x1;
        if (x2 < minX) minX = x2; if (x2 > maxX) maxX = x2;
        for (int x = minX; x <= maxX; ++x) SetPixel(sr, x, y0, color);
        return;
    }

    // Scanline fill
    for (int y = y0; y <= y2; ++y) {
        int xA, xB;
        if (y < y1) {
            xA = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
            xB = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        } else {
            xA = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
            xB = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        }
        if (xA > xB) { int t=xA; xA=xB; xB=t; }
        for (int x = xA; x <= xB; ++x) SetPixel(sr, x, y, color);
    }
}

// Transform 8 box corners through the pipeline and print debug info.
static void DebugPrintBoxCorners(const float boxWorld[8][3], const float view[4][4], const float proj[4][4], int width, int height) {
    float vp[4][4];
    Mat4_Mul(view, proj, vp);

    printf("\n=== SOFT RENDERER DEBUG BOX ===\n");
    printf("view matrix:\n");
    for (int i = 0; i < 4; ++i)
        printf("  [%.4f %.4f %.4f %.4f]\n", view[i][0], view[i][1], view[i][2], view[i][3]);
    printf("proj matrix:\n");
    for (int i = 0; i < 4; ++i)
        printf("  [%.4f %.4f %.4f %.4f]\n", proj[i][0], proj[i][1], proj[i][2], proj[i][3]);
    printf("vp matrix (view*proj):\n");
    for (int i = 0; i < 4; ++i)
        printf("  [%.6f %.6f %.6f %.6f]\n", vp[i][0], vp[i][1], vp[i][2], vp[i][3]);

    printf("\nBox corners (8 vertices):\n");
    for (int i = 0; i < 8; ++i) {
        float world[4] = { boxWorld[i][0], boxWorld[i][1], boxWorld[i][2], 1.0f };
        float viewPos[4], clip[4];
        Mat4_TransformPoint(world, view, viewPos);
        Mat4_TransformPoint(viewPos, proj, clip);

        float ndc[3] = { 0, 0, 0 };
        if (clip[3] != 0.0f) {
            ndc[0] = clip[0] / clip[3];
            ndc[1] = clip[1] / clip[3];
            ndc[2] = clip[2] / clip[3];
        }

        // NDC to screen: screen_x = (ndc.x + 1) * width/2, screen_y = (1 - ndc.y) * height/2
        float screenX = (ndc[0] + 1.0f) * width * 0.5f;
        float screenY = (1.0f - ndc[1]) * height * 0.5f;

        printf("  corner[%d]: world=(%.1f,%.1f,%.1f) view=(%.2f,%.2f,%.2f) clip=(%.2f,%.2f,%.2f,%.2f) NDC=(%.4f,%.4f,%.4f) screen=(%.1f,%.1f)\n",
               i, world[0], world[1], world[2],
               viewPos[0], viewPos[1], viewPos[2],
               clip[0], clip[1], clip[2], clip[3],
               ndc[0], ndc[1], ndc[2],
               screenX, screenY);
    }
}

void SoftRenderer_RenderDebugBox(SoftRenderer* sr,
                                 const float view[4][4],
                                 const float proj[4][4],
                                 const float boxWorldPos[3],
                                 float boxHalfSize) {
    if (!sr) return;

    // Clear framebuffer (dark grey).
    for (int i = 0; i < sr->width * sr->height; ++i)
        sr->framebuffer[i] = 0xFF303030;

    // Generate 8 box corners in world space.
    float boxWorld[8][3];
    float h = boxHalfSize;
    float cx = boxWorldPos[0], cy = boxWorldPos[1], cz = boxWorldPos[2];
    boxWorld[0][0] = cx - h; boxWorld[0][1] = cy - h; boxWorld[0][2] = cz - h;
    boxWorld[1][0] = cx + h; boxWorld[1][1] = cy - h; boxWorld[1][2] = cz - h;
    boxWorld[2][0] = cx + h; boxWorld[2][1] = cy + h; boxWorld[2][2] = cz - h;
    boxWorld[3][0] = cx - h; boxWorld[3][1] = cy + h; boxWorld[3][2] = cz - h;
    boxWorld[4][0] = cx - h; boxWorld[4][1] = cy - h; boxWorld[4][2] = cz + h;
    boxWorld[5][0] = cx + h; boxWorld[5][1] = cy - h; boxWorld[5][2] = cz + h;
    boxWorld[6][0] = cx + h; boxWorld[6][1] = cy + h; boxWorld[6][2] = cz + h;
    boxWorld[7][0] = cx - h; boxWorld[7][1] = cy + h; boxWorld[7][2] = cz + h;

    // Print debug info.
    DebugPrintBoxCorners(boxWorld, view, proj, sr->width, sr->height);

    // Transform all 8 corners to screen space and draw as points.
    float vp[4][4];
    Mat4_Mul(view, proj, vp);
    int screenX[8], screenY[8];
    int visible[8] = {0};
    for (int i = 0; i < 8; ++i) {
        float world[4] = { boxWorld[i][0], boxWorld[i][1], boxWorld[i][2], 1.0f };
        float clip[4];
        Mat4_TransformPoint(world, vp, clip);
        if (clip[3] > 0.0f) {
            float ndcX = clip[0] / clip[3];
            float ndcY = clip[1] / clip[3];
            screenX[i] = (int)((ndcX + 1.0f) * sr->width * 0.5f);
            screenY[i] = (int)((1.0f - ndcY) * sr->height * 0.5f);
            visible[i] = 1;
            // Draw a 5x5 pixel marker at each visible corner (yellow).
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    SetPixel(sr, screenX[i] + dx, screenY[i] + dy, 0xFF00FFFF);
        }
    }

    // Draw box edges as lines (Bresenham).
    int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},  // bottom face
        {4,5}, {5,6}, {6,7}, {7,4},  // top face
        {0,4}, {1,5}, {2,6}, {3,7}   // vertical edges
    };
    for (int e = 0; e < 12; ++e) {
        int i0 = edges[e][0], i1 = edges[e][1];
        if (visible[i0] && visible[i1])
            DrawLine(sr, screenX[i0], screenY[i0], screenX[i1], screenY[i1], 0xFF00FF00);
    }

    // Present.
    SDL_UpdateTexture(sr->texture, NULL, sr->framebuffer, sr->width * sizeof(unsigned int));
    SDL_RenderClear(sr->renderer);
    SDL_RenderCopy(sr->renderer, sr->texture, NULL, NULL);
    SDL_RenderPresent(sr->renderer);
}

void SoftRenderer_RenderFeed(SoftRenderer* sr,
                             const float view[4][4],
                             const float proj[4][4],
                             const DrawCommand* cmds,
                             int numCmds,
                             FILE* logFile) {
    if (!sr) return;

    // Clear framebuffer.
    for (int i = 0; i < sr->width * sr->height; ++i)
        sr->framebuffer[i] = 0xFF202020;

    float vp[4][4];
    Mat4_Mul(view, proj, vp);

    if (logFile) {
        fprintf(logFile, "\n=== SOFT RENDERER FEED (%d commands) ===\n", numCmds);
        fflush(logFile);
    }

    int rendered = 0;
    for (int c = 0; c < numCmds; ++c) {
        const DrawCommand* dc = &cmds[c];
        if (!dc->mesh) continue;
        const MODEL* model = dc->mesh;
        if (!model || model->num_vertices == 0) continue;

        // Build world matrix from dc->world (MATRIX struct).
        float world[4][4];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                world[i][j] = (float)dc->world.m[i][j] / 4096.0f;
        world[3][0] = (float)dc->world.t[0];
        world[3][1] = (float)dc->world.t[1];
        world[3][2] = (float)dc->world.t[2];
        world[0][3] = world[1][3] = world[2][3] = 0;
        world[3][3] = 1.0f;

        // Print first 3 commands' details.
        if (c < 3 && logFile) {
            fprintf(logFile, "\n  cmd[%d]: %d verts, world.t=(%.1f,%.1f,%.1f)\n",
                   c, model->num_vertices,
                   (float)dc->world.t[0], (float)dc->world.t[1], (float)dc->world.t[2]);
            fprintf(logFile, "    world matrix:\n");
            for (int i = 0; i < 4; ++i)
                fprintf(logFile, "      [%.4f %.4f %.4f %.4f]\n", world[i][0], world[i][1], world[i][2], world[i][3]);
        }

        // Transform vertices: local -> world -> view -> clip -> NDC -> screen.
        const SVECTOR* srcVerts = (const SVECTOR*)((const unsigned char*)model + model->vertices);
        int nv = model->num_vertices;
        float* screenXY = (float*)calloc(nv * 2, sizeof(float));
        int* vis = (int*)calloc(nv, sizeof(int));

        for (int v = 0; v < nv; ++v) {
            float local[4] = { (float)srcVerts[v].vx, (float)srcVerts[v].vy, (float)srcVerts[v].vz, 1.0f };
            float worldPos[4], viewPos[4], clip[4];
            Mat4_TransformPoint(local, world, worldPos);
            Mat4_TransformPoint(worldPos, view, viewPos);
            Mat4_TransformPoint(viewPos, proj, clip);

            if (c < 3 && v < 4 && logFile) {
                fprintf(logFile, "    vert[%d]: local=(%.0f,%.0f,%.0f) world=(%.1f,%.1f,%.1f) view=(%.2f,%.2f,%.2f) clip=(%.2f,%.2f,%.2f,%.2f)\n",
                       v, local[0], local[1], local[2],
                       worldPos[0], worldPos[1], worldPos[2],
                       viewPos[0], viewPos[1], viewPos[2],
                       clip[0], clip[1], clip[2], clip[3]);
            }

            if (clip[3] > 0.1f) {
                float ndcX = clip[0] / clip[3];
                float ndcY = clip[1] / clip[3];
                screenXY[v*2+0] = (ndcX + 1.0f) * sr->width * 0.5f;
                screenXY[v*2+1] = (1.0f - ndcY) * sr->height * 0.5f;
                vis[v] = 1;
            }
        }

        // Wireframe: draw quad edges for each polygon.
        unsigned int color = (c == 0) ? 0xFF0000FF : 0xFF00FF00;  // red for first, green for others
        const PL_POLYFT4* polys = GET_MODEL_DATA(PL_POLYFT4, model, poly_block);
        int np = model->num_polys;
        for (int p = 0; p < np; ++p) {
            int v0 = polys[p].v0, v1 = polys[p].v1, v2 = polys[p].v2, v3 = polys[p].v3;
            // Bounds check: skip if any vertex index is out of range.
            if (v0 >= nv || v1 >= nv || v2 >= nv || v3 >= nv) continue;
            // Draw quad edges: v0-v1, v1-v2, v2-v3, v3-v0
            if (vis[v0] && vis[v1]) DrawLine(sr, (int)screenXY[v0*2], (int)screenXY[v0*2+1], (int)screenXY[v1*2], (int)screenXY[v1*2+1], color);
            if (vis[v1] && vis[v2]) DrawLine(sr, (int)screenXY[v1*2], (int)screenXY[v1*2+1], (int)screenXY[v2*2], (int)screenXY[v2*2+1], color);
            if (vis[v2] && vis[v3]) DrawLine(sr, (int)screenXY[v2*2], (int)screenXY[v2*2+1], (int)screenXY[v3*2], (int)screenXY[v3*2+1], color);
            if (vis[v3] && vis[v0]) DrawLine(sr, (int)screenXY[v3*2], (int)screenXY[v3*2+1], (int)screenXY[v0*2], (int)screenXY[v0*2+1], color);
        }
        rendered++;

        free(screenXY);
        free(vis);
    }

    if (logFile) {
        fprintf(logFile, "  rendered %d commands (wireframe)\n", rendered);
        fflush(logFile);
    }

    // Present.
    SDL_UpdateTexture(sr->texture, NULL, sr->framebuffer, sr->width * sizeof(unsigned int));
    SDL_RenderClear(sr->renderer);
    SDL_RenderCopy(sr->renderer, sr->texture, NULL, NULL);
    SDL_RenderPresent(sr->renderer);
}

// Draw shared NDC edge table (from the game's -testcube mode) as wireframe
// lines, scaled to this renderer's window. Because the edges use the SAME NDC
// data as the psyx LINE_F2 path, the two windows show an identical wireframe.
void SoftRenderer_RenderNdcEdges(SoftRenderer* sr, const float edges[][4], const int* visible, int count) {
    if (!sr) return;

    // Clear framebuffer (black — matches the psyx main window background).
    for (int i = 0; i < sr->width * sr->height; ++i)
        sr->framebuffer[i] = 0xFF000000;

    for (int e = 0; e < count; ++e) {
        if (!visible[e]) continue;
        // NDC [-1,1] -> screen pixels (y flipped).
        int x0 = (int)((edges[e][0] + 1.0f) * 0.5f * sr->width);
        int y0 = (int)((1.0f - edges[e][1]) * 0.5f * sr->height);
        int x1 = (int)((edges[e][2] + 1.0f) * 0.5f * sr->width);
        int y1 = (int)((1.0f - edges[e][3]) * 0.5f * sr->height);
        DrawLine(sr, x0, y0, x1, y1, 0xFFFFFFFF);  // white wireframe (matches psyx LINE_F2)
    }

    // Present.
    SDL_UpdateTexture(sr->texture, NULL, sr->framebuffer, sr->width * sizeof(unsigned int));
    SDL_RenderClear(sr->renderer);
    SDL_RenderCopy(sr->renderer, sr->texture, NULL, NULL);
    SDL_RenderPresent(sr->renderer);
}

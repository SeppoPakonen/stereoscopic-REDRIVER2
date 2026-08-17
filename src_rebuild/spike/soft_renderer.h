// soft_renderer.h — Simple software rasterizer for debugging the DX11 projection.
// Consumes the same DrawCommand[] feed as the DX11 path, but renders a single
// debug box and prints the transformed coordinates for each pipeline stage.

#ifndef SOFT_RENDERER_H
#define SOFT_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../Game/render/drawcmd.h"
#include "../Game/engine/mdl.h"
#include <stdio.h>

typedef struct SoftRenderer SoftRenderer;

// Create a software renderer with a window of the given size.
// Returns NULL on failure.
SoftRenderer* SoftRenderer_Create(int width, int height);

// Destroy the renderer and free resources.
void SoftRenderer_Destroy(SoftRenderer* sr);

// Render a single debug box using the given view/projection matrix.
// The box is placed at `boxWorldPos` with half-extents `boxHalfSize`.
// Prints the 8 box corners at each pipeline stage (world, view, clip, NDC, screen).
// Rasterizes the box as filled triangles into the framebuffer and presents.
void SoftRenderer_RenderDebugBox(SoftRenderer* sr,
                                 const float view[4][4],
                                 const float proj[4][4],
                                 const float boxWorldPos[3],
                                 float boxHalfSize);

// Render the draw command feed (all commands). For debugging, prints the first
// few commands' transformed coordinates to `logFile` (if non-NULL). Rasterizes
// all visible triangles.
void SoftRenderer_RenderFeed(SoftRenderer* sr,
                             const float view[4][4],
                             const float proj[4][4],
                             const DrawCommand* cmds,
                             int numCmds,
                             FILE* logFile);

// Draw a shared NDC edge table as a wireframe (used by -testcube so the soft
// window matches the psyx LINE_F2 wireframe exactly). edges[i] = {x0,y0,x1,y1}
// in NDC; visible[i] != 0 means draw edge i.
void SoftRenderer_RenderNdcEdges(SoftRenderer* sr, const float edges[][4], const int* visible, int count);

#ifdef __cplusplus
}
#endif

#endif // SOFT_RENDERER_H

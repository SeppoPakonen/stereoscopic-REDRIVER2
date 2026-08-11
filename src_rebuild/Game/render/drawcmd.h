#ifndef DRAWCMD_H
#define DRAWCMD_H

#include <types.h>
#include <libgte.h>

// ============================================================================
// Draw-command list — the DX11 renderer feed (T0.2)
// ============================================================================
//
// The DX11 renderer never sees the PSX OT/primitive stream. Instead, the game's
// plot functions submit a per-frame list of high-level world-space draw
// commands (mesh + transform + material + sort key). The renderer owns the rest
// (projection, frustum culling, sorting, batching, render targets, compositing).
//
// Ownership split (agreed contract):
//   * Game keeps  — coarse potential-visibility culling (PVS / cell structure),
//                   animation, LOD/subdiv selection, and deciding WHICH objects
//                   are candidates this frame.
//   * Renderer owns — projection, frustum culling, depth sorting (opaque
//                   front-to-back / transparent back-to-front via sortKey),
//                   batching by material/state, and RT/composite selection.
//
// A command references a MODEL mesh. Because MODEL polys each carry their own
// texture_set/texture_id, the per-poly texture is resolved from the mesh; the
// MaterialRef supplies blend/state and a fallback texture for single-primitive
// commands (sprites, tiles, overlays) where mesh == NULL.

// ---------------------------------------------------------------------------
// Material
// ---------------------------------------------------------------------------
typedef enum {
    MATBLEND_OPAQUE = 0,
    MATBLEND_TRANSLUCENT = 1,   // alpha blend (semi-transparent)
    MATBLEND_ADDITIVE = 2,      // additive blend
} MaterialBlend;

typedef struct {
    u_short tpage;      // PSX texture page (from texture_pages[])
    u_short clut;       // PSX CLUT (from texture_cluts[][]; 0xFFFF = none)
    unsigned char blendMode;    // MaterialBlend
    unsigned char filter;       // 0 = nearest, 1 = linear
} MaterialRef;

// ---------------------------------------------------------------------------
// Draw command
// ---------------------------------------------------------------------------
typedef enum {
    DRAWCMD_OPAQUE      = 0x00, // opaque (depth-tested, front-to-back)
    DRAWCMD_TRANSLUCENT = 0x01, // translucent (back-to-front by sortKey)
    DRAWCMD_FLAT        = 0x00, // flat shading
    DRAWCMD_GOURAUD     = 0x02, // per-vertex (gouraud) shading
    DRAWCMD_BACKFACED   = 0x00, // backface cull on
    DRAWCMD_TWOSIDED    = 0x04, // backface cull off
    DRAWCMD_NODEPTH     = 0x08, // skip depth test/write (overlays, sprites)
} DrawCommandFlags;

typedef struct {
    // Geometry source: a MODEL mesh (world-space vertices + polys). When NULL,
    // the command is a single-primitive using `material` (sprite/tile/overlay).
    struct MODEL *mesh;

    // World transform (3x3 rotation + translation) placing the mesh in world
    // space. For single-primitives this carries the (x,y,z) placement.
    MATRIX world;

    // Material: texture (tpage/clut) + blend/state. For a whole MODEL the
    // per-poly texture (texture_set/texture_id) inside the mesh is authoritative;
    // this MaterialRef supplies the blend/state and a fallback texture.
    MaterialRef material;

    // Sort key: depth / priority for transparency ordering. The renderer sorts
    // translucent commands back-to-front by this; opaque commands are bucketed
    // by material/state.
    int sortKey;

    // Render hints (DrawCommandFlags).
    unsigned char flags;

    // Per-instance overrides (-1 = none).
    short palette;              // palette index override
    short subdiv;               // detail/subdivision hint (road tiles)
} DrawCommand;

// ---------------------------------------------------------------------------
// Per-frame draw-command buffer (arena-backed, growable)
// ---------------------------------------------------------------------------
// The buffer is reset at the start of each frame (BeginFrame) and appended to
// during RenderGame2. Commands are addressed by stable index; Submit returns
// that index. The renderer iterates 0..Count-1 after the submit pass.
void DrawCmd_BeginFrame(void);

// Appends a copy of `cmd` and returns its index, or -1 if the arena is full.
int DrawCmd_Submit(const DrawCommand *cmd);

// Number of commands submitted this frame.
int DrawCmd_Count(void);

// Pointer to the command at `index`, or NULL if out of range.
const DrawCommand *DrawCmd_At(int index);

// Pointer to the contiguous command array (count = DrawCmd_Count()). Valid until
// the next DrawCmd_BeginFrame/Submit. Used by the renderer to consume the whole
// frame's commands in one pass. Returns NULL when the arena is empty/never
// written.
const DrawCommand *DrawCmd_Data(void);

#endif // DRAWCMD_H
// dx11_textures.h — T1.3 DX11 texture system.
//
// Translates the PSX texture model (1024x512 VRAM, tpage/clut paletted TIM
// data) into DX11 shader-resource views. Decision (see T1.3 task file): each
// paletted tpage region is **baked to its own R8G8B8A8_UNORM texture on the
// CPU at upload time** using the exact PSX decode math (GET_TPAGE_FORMAT,
// tpage X/Y, GET_CLUT_X/Y, 4/8-bit palette extraction, RGB555 -> RGBA) so the
// output matches the legacy `psyx` GL path. Unlike the GL path there is no
// whole-VRAM atlas with a shader-side palette decode.
//
// Scope (T1.3):
//   * 1024x512 u16 VRAM staging array + CopyVRAM (mirrors GR_CopyVRAM).
//   * CPU decode-to-RGBA for 4-bit / 8-bit paletted textures plus 16-bit
//     direct RGB555, with a CLUT lookup and the rgLUT RGB555->RGBA expansion.
//   * Per-region D3D11 texture + SRV cache (keyed by tpage/clut/texX/texY/w/h).
//   * A single 1x1 white substitute SRV for untextured surfaces.
//
// Notes on convention (from the T0.4 / T0.5 / T1.1 work):
//   * tpage X = (tpage<<6)&0x3c0, tpage Y = ((tpage<<4)&0x100)+((tpage>>2)&0x200).
//   * clut X = (clut&0x3f)<<4, clut Y = clut>>6; a paletted index i reads the
//     CLUT word at vram[clutY*1024 + clutX + i].
//   * RGB555 -> RGBA: R=(c&31)<<3, G=((c>>5)&31)<<3, B=((c>>10)&31)<<3, A=255.

#ifndef DX11_TEXTURES_H
#define DX11_TEXTURES_H

#include <d3d11.h>
#include <dxgi.h>

#ifdef __cplusplus
extern "C" {
#endif

// PSX VRAM dimensions.
#define DX11TEX_VRAM_W 1024
#define DX11TEX_VRAM_H 512

// Texture format as decoded from the tpage bits (GET_TPAGE_FORMAT).
typedef enum {
    DX11TEX_FMT_4BIT = 0,   // 4 pixels per 16-bit VRAM word (paletted, 16-entry CLUT)
    DX11TEX_FMT_8BIT = 1,   // 2 pixels per word (paletted, up to 256-entry CLUT)
    DX11TEX_FMT_16BIT = 2,  // 1 pixel per word (direct RGB555, no CLUT)
    DX11TEX_FMT_32BIT = 3,  // (unused by VRAM; reserved)
} Dx11TexFormat;

// Opaque handle to the texture system.
typedef struct Dx11Tex Dx11Tex;

// Handle to a baked texture (index into the SRV cache). -1 = invalid.
typedef int Dx11TexHandle;

// Configuration.
typedef struct {
    int max_textures;   // capacity of the baked SRV cache (default 256)
} Dx11TexConfig;

// Result codes.
typedef enum {
    DX11TEX_OK = 0,
    DX11TEX_ERR_OUT_OF_MEMORY,
    DX11TEX_ERR_DEVICE,
    DX11TEX_ERR_CACHE_FULL,
    DX11TEX_ERR_ARG,
} Dx11TexResult;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
Dx11TexConfig Dx11Tex_DefaultConfig(void);

// Creates the VRAM staging array, the SRV cache and the white 1x1 substitute.
// `ctx` is borrowed (owned by the caller; not released). Returns NULL on
// failure; *outResult carries the code.
Dx11Tex *Dx11Tex_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                        const Dx11TexConfig *cfg, Dx11TexResult *outResult);

void Dx11Tex_Destroy(Dx11Tex *t);

// ---------------------------------------------------------------------------
// VRAM staging (mirrors GR_CopyVRAM)
// ---------------------------------------------------------------------------
// Copies a w*h region of u16 words from `src` (row-major, stride `w`) into the
// internal VRAM at (dst_x, dst_y). `src` may be NULL to copy within VRAM
// (stride = VRAM width). Reads/writes are clamped to the VRAM bounds.
void Dx11Tex_CopyVRAM(Dx11Tex *t, const unsigned short *src,
                      int x, int y, int w, int h, int dst_x, int dst_y);

// Returns the raw VRAM word at (x, y), or 0 if out of bounds.
unsigned short Dx11Tex_GetVRAMWord(Dx11Tex *t, int x, int y);

// ---------------------------------------------------------------------------
// Bake + SRV access
// ---------------------------------------------------------------------------
// Decodes the paletted texel region at the tpage/clut position (offset tex_x,
// tex_y within the tpage, in texels; width/height in texels) into an
// R8G8B8A8_UNORM texture + SRV. Result is cached by key; a second call with
// the same key returns the existing handle. Returns -1 on failure (cache full
// or device error).
Dx11TexHandle Dx11Tex_Bake(Dx11Tex *t, unsigned short tpage, unsigned short clut,
                           int tex_x, int tex_y, int width, int height);

// The SRV for a baked handle, or NULL if invalid.
ID3D11ShaderResourceView *Dx11Tex_GetSRV(Dx11Tex *t, Dx11TexHandle h);

// The white 1x1 substitute SRV (untextured surfaces).
ID3D11ShaderResourceView *Dx11Tex_GetWhiteSRV(Dx11Tex *t);

// The baked texture's size in texels. Returns 0 on success.
int Dx11Tex_GetSize(Dx11Tex *t, Dx11TexHandle h, int *outW, int *outH);

// ---------------------------------------------------------------------------
// Verification aid
// ---------------------------------------------------------------------------
// Copies the baked texture to a staging resource and reads its RGBA (w*h*4
// bytes) into `out` (caller owns; must hold w*h*4 bytes). Returns 0 on success.
int Dx11Tex_ReadBack(Dx11Tex *t, ID3D11DeviceContext *ctx, Dx11TexHandle h,
                     unsigned char *out, int maxBytes);

// Read-only debug dump of the VRAM staging + a baked texture's first pixels.
void Dx11Tex_DebugDump(Dx11Tex *t, ID3D11DeviceContext *ctx, Dx11TexHandle h,
                       const char *path);

#ifdef __cplusplus
}
#endif

#endif // DX11_TEXTURES_H
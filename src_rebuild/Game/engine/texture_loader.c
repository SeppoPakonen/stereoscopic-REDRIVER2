// texture_loader.c — Load a PNG and upload it to the game's VRAM as a 16-bit
// RGB555 texture page (what a POLYFT4 with texture_set pointing at the returned
// tpage samples). No palette/CLUT is needed for 16-bit pages.

#define STB_IMAGE_IMPLEMENTATION
#include "texture_loader.h"
#include "stb_image.h"
#include "../driver2.h" // LoadTPage/LoadTPage/GetTPage, u_short/u_int
#include <stdlib.h>
#include <stdio.h>

// Free VRAM slot for the debug texture. A 16-bit PSX texture page is 256x256
// texels, so this occupies x=[512,768), y=[256,512).
#define TEX_LOADER_VRAM_X 512
#define TEX_LOADER_VRAM_Y 256

int TextureLoader_LoadPng(const char* filename, unsigned short* outTpage, int* outW, int* outH)
{
	int w, h, n;
	unsigned char* rgba = stbi_load(filename, &w, &h, &n, 4);
	if (!rgba)
	{
		fprintf(stderr, "[TextureLoader] stbi_load failed: %s\n", filename);
		return 1;
	}

	// A direct-color PSX page is 256x256 texels. Always fill the page so the
	// model's standard 0..255 page-relative UVs address the uploaded image.
	int tw = 256, th = 256;
	unsigned short* px = (unsigned short*)malloc((size_t)tw * th * sizeof(unsigned short));
	if (!px)
	{
		stbi_image_free(rgba);
		return 2;
	}
	for (int ty = 0; ty < th; ++ty)
	{
		// PNG rows are top-down while the PSX VRAM texture page is sampled with
		// its origin at the opposite edge by the PsyX and DX11 page paths.
		int sy = ((th - 1 - ty) * h) / th;
		for (int tx = 0; tx < tw; ++tx)
		{
			int sx = (tx * w) / tw;
			const unsigned char* p = &rgba[(sy * w + sx) * 4];
			unsigned char r = p[0], g = p[1], b = p[2];
			// RGB555 zero is PSX chroma-key transparency, so preserve opaque PNG
			// black as an almost-black nonzero texel instead of discarding it.
			unsigned short color = (unsigned short)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
			px[ty * tw + tx] = color ? color : 0x0421;
		}
	}

	// Upload the RGB555 pixels into VRAM, then build the tpage word for
	// texture_pages[set]. (LoadTPage is not exported by the PsyCross build the
	// game links; GetTPage + LoadImage are, and are what LoadTPage would do.)
	RECT16 imageArea;
	imageArea.x = TEX_LOADER_VRAM_X;
	imageArea.y = TEX_LOADER_VRAM_Y;
	imageArea.w = tw;
	imageArea.h = th;
	LoadImage(&imageArea, (u_long*)px);
	// LoadImage updates the emulated VRAM command stream. Flush it before the
	// fixture renders so PsyX's GL VRAM texture sees the uploaded page.
	DrawSync(0);
	unsigned short tpage = GetTPage(2, 0, TEX_LOADER_VRAM_X, TEX_LOADER_VRAM_Y);

	stbi_image_free(rgba);
	free(px);

	if (outTpage) *outTpage = tpage;
	if (outW) *outW = tw;
	if (outH) *outH = th;
	return 0;
}

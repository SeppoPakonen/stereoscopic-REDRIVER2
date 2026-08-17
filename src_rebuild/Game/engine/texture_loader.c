// texture_loader.c — Load a PNG and upload it to the game's VRAM as a 16-bit
// RGB555 texture page (what a POLYFT4 with texture_set pointing at the returned
// tpage samples). No palette/CLUT is needed for 16-bit pages.

#define STB_IMAGE_IMPLEMENTATION
#include "texture_loader.h"
#include "stb_image.h"
#include "../driver2.h" // LoadTPage/LoadTPage/GetTPage, u_short/u_int
#include <stdlib.h>
#include <stdio.h>

// Free VRAM slot for the debug texture. Row y=256 x∈[512,960) is unused by the
// level's tpage slots (they end at x=448; the CLUT column lives at x>=960).
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

	// PSX 16-bit pages are 64x64 texels, so downscale the PNG (nearest neighbour)
	// to fit one page without overflowing the VRAM row (512x512 would run past
	// the 1024x512 VRAM). If it is already within one page, upload as-is.
	int tw = w, th = h;
	if (w > 64 || h > 64) { tw = 64; th = 64; }
	unsigned short* px = (unsigned short*)malloc((size_t)tw * th * sizeof(unsigned short));
	if (!px)
	{
		stbi_image_free(rgba);
		return 2;
	}
	for (int ty = 0; ty < th; ++ty)
	{
		int sy = (ty * h) / th;
		for (int tx = 0; tx < tw; ++tx)
		{
			int sx = (tx * w) / tw;
			const unsigned char* p = &rgba[(sy * w + sx) * 4];
			unsigned char r = p[0], g = p[1], b = p[2];
			px[ty * tw + tx] = (unsigned short)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10));
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
	unsigned short tpage = GetTPage(2, 0, TEX_LOADER_VRAM_X, TEX_LOADER_VRAM_Y);

	stbi_image_free(rgba);
	free(px);

	if (outTpage) *outTpage = tpage;
	if (outW) *outW = tw;
	if (outH) *outH = th;
	return 0;
}
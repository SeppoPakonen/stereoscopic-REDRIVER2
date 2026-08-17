#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

// Load a PNG texture (stb_image) and upload it to the game's VRAM as a 16-bit
// RGB555 texture page. Returns 0 on success and writes the GPU tpage code that
// the caller should store into texture_pages[set].
//   outTpage - decoded tpage word (GetTPage/LoadTPage result)
//   outW/outH - texture dimensions
int TextureLoader_LoadPng(const char* filename, unsigned short* outTpage, int* outW, int* outH);

#ifdef __cplusplus
}
#endif

#endif // TEXTURE_LOADER_H
# 003 — PNG Texture Loader

## Goal
Load PNG texture files and upload them to the game's texture system (tpage/clut).

## Scope
- Load PNG file (using stb_image or similar library)
- Convert RGBA → game texture format (4-bit or 8-bit paletted, or 16-bit direct)
- Upload to game's texture system (texture_pages[], texture_cluts[][])
- Return texture_set and texture_id for use in MODEL

## Input
- PNG filename (e.g., `cube.png`)

## Output
- Texture uploaded to game texture system
- Returns texture_set and texture_id

## API
```c
// Load PNG texture and upload to game. Returns 0 on success.
// Sets *outSet and *outId to the assigned texture_set and texture_id.
int TextureLoader_LoadPng(const char* filename, int* outSet, int* outId);
```

## Texture Upload Details
- **PNG → RGBA**: Use stb_image (`stbi_load()`) to load PNG as RGBA8888.
- **RGBA → Game format**: Game uses 4-bit or 8-bit paletted textures, or 16-bit direct color.
  - For simplicity, use 16-bit direct color (no palette). Convert RGBA8888 → RGB555 (16-bit).
  - RGB555: `r5 = r >> 3; g5 = g >> 3; b5 = b >> 3; rgb555 = r5 | (g5 << 5) | (b5 << 10);`
- **Upload**: Use game's texture upload API (e.g., `UploadTexture()` or direct VRAM write).
  - Assign to next available texture_set (e.g., 127, last slot).
  - texture_id = 0 (single texture per set for now).

## Acceptance Criteria
- [ ] Load cube.png (64x64 or 128x128 RGBA)
- [ ] Convert to 16-bit RGB555
- [ ] Upload to texture_set=127, texture_id=0
- [ ] Verify texture appears correctly when rendered (visual test)

## Notes
- stb_image is a single-header library — include `stb_image.h` in the loader.
- Game texture system uses tpage (texture page index) and clut (color lookup table index). For 16-bit direct color, clut is not used (set to 0).
- For now, assign texture_set=127 (last available slot). Later, implement proper texture atlas management.
- PNG alpha channel: for now, ignore alpha (set to opaque). Later, handle transparency.

## Dependencies
- stb_image.h (single-header library, include in spike/ or Game/C/)

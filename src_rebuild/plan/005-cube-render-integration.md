# 005 — Cube Render Integration

## Goal
Integrate all previous tasks (001–004) to render the test cube in both psyx and soft renderers.

## Scope
- Load cube.obj/cube.mtl/cube.png using loaders from 001–003
- Build game MODEL using builder from 002
- Set up test camera from 004
- Render cube using RenderModel() for psyx renderer
- Render cube using DrawCommand feed for soft renderer
- Verify cube appears correctly in both renderer windows

## Integration Steps

### 1. Load Assets (at startup)
```c
// In main() or InitGame(), after texture system is initialized:
ObjModel cubeObj;
if (ObjLoad("cube.obj", &cubeObj) == 0) {
    gTestCubeModel = ModelBuilder_FromObj(&cubeObj, 100.0f);
    int texSet, texId;
    TextureLoader_LoadPng("cube.png", &texSet, &texId);
    // Assign texture to all faces of the model.
    // (For now, all faces use texture_set=127, texture_id=0 from 003.)
}
```

### 2. Set Up Test Camera (per frame)
```c
// In RenderGame2(), when gTestMode is enabled:
if (gTestMode) {
    // Bypass normal camera setup (from 004).
    camera_position.vx = 0;
    camera_position.vy = 0;
    camera_position.vz = -500;
    memset(&inv_camera_matrix, 0, sizeof(MATRIX));
    inv_camera_matrix.m[0][0] = 4096;
    inv_camera_matrix.m[1][1] = 4096;
    inv_camera_matrix.m[2][2] = 4096;
}
```

### 3. Render Cube (psyx renderer)
```c
// In RenderGame2(), after camera setup:
if (gTestMode && gTestCubeModel) {
    VECTOR pos = { 0, 0, 0, 0 };
    MATRIX identity;
    memset(&identity, 0, sizeof(MATRIX));
    identity.m[0][0] = identity.m[1][1] = identity.m[2][2] = 4096;
    RenderModel(gTestCubeModel, &identity, &pos, 0, 0, 0, 0);
}
```

### 4. Render Cube (soft renderer)
```c
// In RenderGame2(), after psyx render:
if (gTestMode && Renderer_IsSoft() && gTestCubeModel) {
    // Build DrawCommand for the cube.
    DrawCommand cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.mesh = gTestCubeModel;
    cmd.world.m[0][0] = cmd.world.m[1][1] = cmd.world.m[2][2] = 4096;
    cmd.world.t[0] = cmd.world.t[1] = cmd.world.t[2] = 0;
    cmd.material.tpage = texture_pages[127]; // from 003
    cmd.material.clut = texture_cluts[127][0];
    DrawCmd_Submit(&cmd);
}
```

### 5. Verify
- Run game with `-renderer soft -mission 50 -test`
- Verify cube appears in soft renderer window (already working from previous session)
- Verify cube appears in psyx renderer window (main window)
- Visual check: cube should be centered, correctly oriented, textured with cube.png

## Acceptance Criteria
- [ ] Load cube.obj/cube.mtl/cube.png at startup
- [ ] Build game MODEL from OBJ data
- [ ] Upload cube.png texture to texture_set=127
- [ ] Set up test camera at (0, 0, -500) looking at origin
- [ ] Render cube using RenderModel() for psyx renderer
- [ ] Render cube using DrawCommand feed for soft renderer
- [ ] Visual test: cube appears correctly in both renderer windows
- [ ] Cube is textured with cube.png (not just colored)

## Notes
- Test mode is enabled by `-test` command-line argument (sets `gTestMode = 1`).
- Normal game rendering is bypassed in test mode (no DrawMapPSX, DrawCars, etc.).
- For now, cube is static (no rotation/animation). Later, add rotation for visual verification.
- Texture mapping: for now, all faces use the same texture (cube.png). Later, support multiple materials/textures per model.

## Dependencies
- 001-obj-mtl-loader (ObjLoad, ObjFree)
- 002-model-builder (ModelBuilder_FromObj, ModelBuilder_Free)
- 003-png-texture-loader (TextureLoader_LoadPng)
- 004-test-camera-setup (gTestMode, camera setup)

# 002 — Model Builder (OBJ → Game MODEL)

## Goal
Convert parsed OBJ data into the game's native MODEL format (SVECTOR vertices, PL_POLYFT4 quads, MODEL struct).

## Scope
- Convert ObjVertex[] → SVECTOR[] (scale from OBJ units to game units)
- Convert ObjFace[] → PL_POLYFT4[] (quad indices, texture set/id)
- Build MODEL struct with vertices, poly_block, num_vertices, num_polys
- Handle texture mapping (assign texture_set/id based on material)

## Input
- `ObjModel` from 001-obj-mtl-loader

## Output
```c
// Game MODEL structure (already defined in engine/mdl.h)
typedef struct MODEL {
    u_short shape_flags;
    u_short flags2;
    short instance_number;
    u_char tri_verts;
    unsigned char zBias;
    short bounding_sphere;
    u_short num_point_normals;
    u_short num_vertices;
    u_short num_polys;
    int vertices;        // offset to SVECTOR[]
    int poly_block;      // offset to PL_POLYFT4[]
    int normals;
    int point_normals;
    int collision_block;
} MODEL;
```

## API
```c
// Build game MODEL from OBJ data. Returns allocated MODEL* (caller must free).
// scale: multiplier to convert OBJ units to game units (e.g., 100.0f).
MODEL* ModelBuilder_FromObj(const ObjModel* obj, float scale);

// Free allocated MODEL and its data.
void ModelBuilder_Free(MODEL* model);
```

## Conversion Details
- **Vertices**: ObjVertex (float x,y,z) → SVECTOR (short vx,vy,vz). Scale by `scale` parameter.
  ```c
  svec.vx = (short)(obj->vertices[i].x * scale);
  svec.vy = (short)(obj->vertices[i].y * scale);
  svec.vz = (short)(obj->vertices[i].z * scale);
  ```
- **Faces**: ObjFace (quad) → PL_POLYFT4. Assign texture_set=0, texture_id=0 for now (single texture).
  ```c
  poly->v0 = face->v[0]; poly->v1 = face->v[1];
  poly->v2 = face->v[2]; poly->v3 = face->v[3];
  poly->texture_set = 0; poly->texture_id = 0;
  ```
- **MODEL struct**: Allocate single block containing MODEL + SVECTOR[] + PL_POLYFT4[].
  ```c
  int vertSize = obj->numVertices * sizeof(SVECTOR);
  int polySize = obj->numFaces * sizeof(PL_POLYFT4);
  MODEL* model = malloc(sizeof(MODEL) + vertSize + polySize);
  model->vertices = sizeof(MODEL);
  model->poly_block = sizeof(MODEL) + vertSize;
  model->num_vertices = obj->numVertices;
  model->num_polys = obj->numFaces;
  ```

## Acceptance Criteria
- [ ] Convert cube.obj (8 vertices, 6 quads) → MODEL with 8 SVECTOR, 6 PL_POLYFT4
- [ ] Verify vertex positions are scaled correctly (OBJ -0.5..0.5 → game -50..50 with scale=100)
- [ ] Verify face indices match OBJ faces (0-based)
- [ ] Unit test: build MODEL from cube.obj, verify num_vertices=8, num_polys=6
- [ ] Memory cleanup: ModelBuilder_Free() releases all allocated memory

## Notes
- OBJ can have triangles (3 vertices per face) — convert to quad by duplicating last vertex? Or create PL_POLYFT3? For now, assume all faces are quads (cube.obj has only quads).
- Texture mapping: for now, assign texture_set=0, texture_id=0 to all faces. Later (003-png-texture-loader) will load actual texture and assign correct set/id.
- MODEL struct uses offsets (vertices, poly_block) instead of pointers — this is for relocation/serialization. We allocate single block and set offsets accordingly.

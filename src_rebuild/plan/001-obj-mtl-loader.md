# 001 — OBJ/MTL Loader

## Goal
Create loaders for Wavefront OBJ (.obj) and material (.mtl) files to load 3D models into the game engine.

## Scope
- Parse OBJ file: vertices (v), texture coordinates (vt), normals (vn), faces (f)
- Parse MTL file: material names, diffuse color (Kd), texture map (map_Kd)
- Store parsed data in intermediate structures (not yet converted to game MODEL format)

## Input Files
- `C:\Users\sblo\Dev\ai-upp\share\models\cube.obj`
- `C:\Users\sblo\Dev\ai-upp\share\models\cube.mtl`

## Output Structures
```c
typedef struct {
    float x, y, z;
} ObjVertex;

typedef struct {
    float u, v;
} ObjTexCoord;

typedef struct {
    float x, y, z;
} ObjNormal;

typedef struct {
    int v[4];      // vertex indices (0-based)
    int vt[4];     // texture coord indices (0-based)
    int vn[4];     // normal indices (0-based)
    int numVerts;  // 3 (triangle) or 4 (quad)
    char materialName[64];
} ObjFace;

typedef struct {
    char name[64];
    float Kd[3];   // diffuse color (r, g, b)
    char mapKd[256]; // texture filename
} ObjMaterial;

typedef struct {
    ObjVertex* vertices;
    int numVertices;
    ObjTexCoord* texCoords;
    int numTexCoords;
    ObjNormal* normals;
    int numNormals;
    ObjFace* faces;
    int numFaces;
    ObjMaterial* materials;
    int numMaterials;
} ObjModel;
```

## API
```c
// Load OBJ file, returns 0 on success.
int ObjLoad(const char* filename, ObjModel* model);

// Free allocated memory.
void ObjFree(ObjModel* model);
```

## Acceptance Criteria
- [ ] Parse cube.obj: 8 vertices, 4 texcoords, 6 normals, 12 faces (6 quads = 12 triangles in OBJ, but we'll treat as 6 quads)
- [ ] Parse cube.mtl: 1 material "cube" with map_Kd="cube.png"
- [ ] Unit test: load cube.obj, verify vertex/face counts
- [ ] Memory cleanup: ObjFree() releases all allocated memory

## Notes
- OBJ faces can be triangles (f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3) or quads (f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 v4/vt4/vn4)
- OBJ indices are 1-based, convert to 0-based
- OBJ faces can also be v1/vt1 or v1//vn1 or just v1 — handle all cases
- MTL file is referenced by OBJ via `mtllib` directive

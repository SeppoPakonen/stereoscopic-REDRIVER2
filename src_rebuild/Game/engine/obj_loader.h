#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

// Intermediate structures for parsed OBJ data.
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

// Load OBJ file, returns 0 on success.
// Allocates memory for all arrays — caller must call ObjFree() when done.
int ObjLoad(const char* filename, ObjModel* model);

// Free allocated memory in ObjModel.
void ObjFree(ObjModel* model);

#ifdef __cplusplus
}
#endif

#endif // OBJ_LOADER_H

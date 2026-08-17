// test_obj_loader.c — Simple test for OBJ loader.
#include "../Game/engine/obj_loader.h"
#include <stdio.h>

int main(int argc, char** argv) {
    const char* filename = (argc > 1) ? argv[1] : "data/cube.obj";
    printf("Loading OBJ: %s\n", filename);

    ObjModel model;
    if (ObjLoad(filename, &model) != 0) {
        printf("FAILED to load OBJ\n");
        return 1;
    }

    printf("Loaded:\n");
    printf("  Vertices: %d\n", model.numVertices);
    printf("  TexCoords: %d\n", model.numTexCoords);
    printf("  Normals: %d\n", model.numNormals);
    printf("  Faces: %d\n", model.numFaces);
    printf("  Materials: %d\n", model.numMaterials);

    // Print first few vertices.
    printf("First 3 vertices:\n");
    for (int i = 0; i < 3 && i < model.numVertices; i++) {
        printf("  v[%d]: (%.2f, %.2f, %.2f)\n", i,
               model.vertices[i].x, model.vertices[i].y, model.vertices[i].z);
    }

    // Print first few faces.
    printf("First 3 faces:\n");
    for (int i = 0; i < 3 && i < model.numFaces; i++) {
        ObjFace* f = &model.faces[i];
        printf("  f[%d]: %d verts, material='%s'\n", i, f->numVerts, f->materialName);
        for (int j = 0; j < f->numVerts; j++) {
            printf("    v[%d]=%d vt[%d]=%d vn[%d]=%d\n",
                   j, f->v[j], j, f->vt[j], j, f->vn[j]);
        }
    }

    ObjFree(&model);
    printf("SUCCESS\n");
    return 0;
}

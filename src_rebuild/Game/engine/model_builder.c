#include "model_builder.h"
#include "../driver2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MODEL* ModelBuilder_FromObj(const ObjModel* obj, float scale, int textureSet, int textureId) {
    if (!obj || obj->numVertices == 0 || obj->numFaces == 0) return NULL;

    // OBJ faces are triangles (3 vertices each). 12 triangles = 6 quads.
    // Merge consecutive triangle pairs into quads.
    int numQuads = obj->numFaces / 2;
    if (obj->numFaces % 2 != 0) {
        fprintf(stderr, "[ModelBuilder] Warning: odd number of faces (%d), ignoring last triangle\n", obj->numFaces);
    }

    // Allocate MODEL + SVECTOR[] + PL_POLYFT4[] in a single block.
    int vertSize = obj->numVertices * sizeof(SVECTOR);
    int polySize = numQuads * sizeof(PL_POLYFT4);
    int totalSize = sizeof(MODEL) + vertSize + polySize;
    MODEL* model = (MODEL*)malloc(totalSize);
    if (!model) return NULL;
    memset(model, 0, totalSize);

    // Set MODEL fields.
    model->num_vertices = (u_short)obj->numVertices;
    model->num_polys = (u_short)numQuads;
    model->vertices = sizeof(MODEL);
    model->poly_block = sizeof(MODEL) + vertSize;
    model->shape_flags = 0;
    model->flags2 = 0;
    model->zBias = 0;
    model->bounding_sphere = 0;

    // Copy vertices (scale from OBJ units to game units).
    SVECTOR* verts = (SVECTOR*)((unsigned char*)model + model->vertices);
    for (int i = 0; i < obj->numVertices; i++) {
        verts[i].vx = (short)(obj->vertices[i].x * scale);
        verts[i].vy = (short)(obj->vertices[i].y * scale);
        verts[i].vz = (short)(obj->vertices[i].z * scale);
    }

    // Merge triangle pairs into quads.
    PL_POLYFT4* polys = (PL_POLYFT4*)((unsigned char*)model + model->poly_block);
    for (int i = 0; i < numQuads; i++) {
        int t0 = i * 2;      // first triangle of the pair
        int t1 = i * 2 + 1;  // second triangle of the pair

        ObjFace* f0 = &obj->faces[t0];
        ObjFace* f1 = &obj->faces[t1];

        // Build quad from the two triangles.
        // The two triangles share an edge. The quad vertices are the 4 unique vertices
        // from the two triangles. In OBJ, the two triangles for a quad are:
        //   f0: v0, v1, v2
        //   f1: v2, v1, v3 (or v0, v2, v3 etc.)
        // We need to find the 4 unique vertices and order them properly.
        // Simple approach: take v0, v1 from f0, and add the vertex from f1 that is not in f0.
        int v0 = f0->v[0], v1 = f0->v[1], v2 = f0->v[2];
        // Find the vertex in f1 that is not v0, v1, v2.
        int v3 = f1->v[0];
        if (v3 == v0 || v3 == v1 || v3 == v2) v3 = f1->v[1];
        if (v3 == v0 || v3 == v1 || v3 == v2) v3 = f1->v[2];

        // Check that this is a valid quad (v3 is unique).
        if (v3 == v0 || v3 == v1 || v3 == v2) {
            // Fallback: use the third vertex from f1 directly.
            v3 = f1->v[2];
        }

        polys[i].id = 11;  // PL_POLYFT4 type
        polys[i].texture_set = (u_char)textureSet;
        polys[i].texture_id = (u_char)textureId;
        polys[i].th = 0;
        polys[i].v0 = (u_char)v0;
        polys[i].v1 = (u_char)v1;
        polys[i].v2 = (u_char)v2;
        polys[i].v3 = (u_char)v3;

        // Copy UVs from the first triangle's first two vertices and the second triangle's third vertex.
        polys[i].uv0.u = (u_char)(obj->texCoords[f0->vt[0]].u * 255.0f);
        polys[i].uv0.v = (u_char)((1.0f - obj->texCoords[f0->vt[0]].v) * 255.0f);
        polys[i].uv1.u = (u_char)(obj->texCoords[f0->vt[1]].u * 255.0f);
        polys[i].uv1.v = (u_char)((1.0f - obj->texCoords[f0->vt[1]].v) * 255.0f);
        // Find the UV from f1 that corresponds to v3.
        int uv3 = f1->vt[0];
        if (v3 == f1->v[1]) uv3 = f1->vt[1];
        else if (v3 == f1->v[2]) uv3 = f1->vt[2];
        polys[i].uv2.u = (u_char)(obj->texCoords[uv3].u * 255.0f);
        polys[i].uv2.v = (u_char)((1.0f - obj->texCoords[uv3].v) * 255.0f);
        polys[i].uv3.u = polys[i].uv2.u;
        polys[i].uv3.v = polys[i].uv2.v;
    }

    return model;
}

void ModelBuilder_Free(MODEL* model) {
    free(model);
}
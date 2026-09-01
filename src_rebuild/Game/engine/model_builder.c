#include "model_builder.h"
#include "../driver2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

MODEL* ModelBuilder_FromObj(const ObjModel* obj, float scale, int textureSet, int textureId) {
    if (!obj || obj->numVertices == 0 || obj->numFaces == 0) return NULL;

    // OBJ input is already triangulated. Keep each source triangle independent
    // instead of reconstructing quads: OBJ triangle pairs do not carry a
    // universal quad-corner order, and a wrong reconstruction creates a
    // diagonal gap or crossed UVs. PL_POLYFT4 can represent a triangle by
    // duplicating v2/uv2 into v3/uv3.
    int numPolys = obj->numFaces;

    // Allocate MODEL + SVECTOR[] + PL_POLYFT4[] in a single block.
    int vertSize = obj->numVertices * sizeof(SVECTOR);
    int polySize = numPolys * sizeof(PL_POLYFT4);
    int totalSize = sizeof(MODEL) + vertSize + polySize;
    MODEL* model = (MODEL*)malloc(totalSize);
    if (!model) return NULL;
    memset(model, 0, totalSize);

    // Set MODEL fields. On the PC build (PSX not defined) GET_MODEL_DATA resolves
    // via _MDL_GETTER_*/_MDL_GETTER_poly_block which compute (MDL + FIELD), so
    // vertices/poly_block hold a BYTE OFFSET into the model's data block.
    model->num_vertices = (u_short)obj->numVertices;
    model->num_polys = (u_short)numPolys;
    model->vertices = sizeof(MODEL);
    model->poly_block = sizeof(MODEL) + vertSize;
    model->shape_flags = 0;
    model->flags2 = 0;
    // RenderModel subtracts 64 from zBias before selecting the OT base.
    // 64 is the neutral game value; zero places primitives before the OT.
    model->zBias = 64;
    model->bounding_sphere = 0;
    // Not an instanced model: -1 keeps _MDL_GETTER_vertices/_normals/etc from
    // redirecting to modelpointers[] (which are unloaded in the test loop and
    // NULL/instance-0 would be dereferenced, crashing).
    model->instance_number = -1;

    // Copy vertices (scale from OBJ units to game units).
    SVECTOR* verts = (SVECTOR*)((unsigned char*)model + model->vertices);
    for (int i = 0; i < obj->numVertices; i++) {
        verts[i].vx = (short)(obj->vertices[i].x * scale);
        verts[i].vy = (short)(obj->vertices[i].y * scale);
        verts[i].vz = (short)(obj->vertices[i].z * scale);
    }

    // Store each OBJ triangle as a degenerate textured quad. RenderModel emits
    // the first triangle normally; the duplicate second triangle has zero area.
    PL_POLYFT4* polys = (PL_POLYFT4*)((unsigned char*)model + model->poly_block);
    for (int i = 0; i < numPolys; i++) {
        const ObjFace* face = &obj->faces[i];

        polys[i].id = 11;  // PL_POLYFT4 type
        polys[i].texture_set = (u_char)textureSet;
        polys[i].texture_id = (u_char)textureId;
        polys[i].th = 0;
        polys[i].v0 = (u_char)face->v[0];
        polys[i].v1 = (u_char)face->v[1];
        polys[i].v2 = (u_char)face->v[2];
        polys[i].v3 = (u_char)face->v[2];

        polys[i].uv0.u = (u_char)(obj->texCoords[face->vt[0]].u * 255.0f);
        polys[i].uv0.v = (u_char)((1.0f - obj->texCoords[face->vt[0]].v) * 255.0f);
        polys[i].uv1.u = (u_char)(obj->texCoords[face->vt[1]].u * 255.0f);
        polys[i].uv1.v = (u_char)((1.0f - obj->texCoords[face->vt[1]].v) * 255.0f);
        polys[i].uv2.u = (u_char)(obj->texCoords[face->vt[2]].u * 255.0f);
        polys[i].uv2.v = (u_char)((1.0f - obj->texCoords[face->vt[2]].v) * 255.0f);
        polys[i].uv3 = polys[i].uv2;
    }

    return model;
}

void ModelBuilder_Free(MODEL* model) {
    free(model);
}

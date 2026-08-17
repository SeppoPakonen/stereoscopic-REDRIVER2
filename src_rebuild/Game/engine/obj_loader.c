#include "obj_loader.h"
#include "../driver2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dynamic array helpers.
#define ARRAY_GROW(arr, count, cap, type) \
    do { \
        if ((count) >= (cap)) { \
            (cap) = (cap) == 0 ? 16 : (cap) * 2; \
            (arr) = (type*)realloc((arr), (cap) * sizeof(type)); \
        } \
    } while (0)

// Parse a face vertex index (e.g., "1/2/3" or "1//3" or "1").
static void ParseFaceVertex(const char* str, int* v, int* vt, int* vn) {
    *v = *vt = *vn = 0;
    if (sscanf(str, "%d/%d/%d", v, vt, vn) == 3) {
        // v/vt/vn
    } else if (sscanf(str, "%d//%d", v, vn) == 2) {
        // v//vn
        *vt = 0;
    } else if (sscanf(str, "%d/%d", v, vt) == 2) {
        // v/vt
        *vn = 0;
    } else {
        sscanf(str, "%d", v);
        *vt = *vn = 0;
    }
    // Convert 1-based to 0-based.
    if (*v > 0) (*v)--;
    if (*vt > 0) (*vt)--;
    if (*vn > 0) (*vn)--;
}

int ObjLoad(const char* filename, ObjModel* model) {
    if (!filename || !model) return 1;
    memset(model, 0, sizeof(ObjModel));

    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "[ObjLoad] Failed to open %s\n", filename);
        return 1;
    }

    // Dynamic arrays.
    int vCap = 0, vtCap = 0, vnCap = 0, fCap = 0;
    char line[1024];
    char currentMaterial[64] = "";

    while (fgets(line, sizeof(line), f)) {
        // Skip comments and empty lines.
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        if (strncmp(line, "v ", 2) == 0) {
            // Vertex.
            ARRAY_GROW(model->vertices, model->numVertices, vCap, ObjVertex);
            sscanf(line + 2, "%f %f %f",
                   &model->vertices[model->numVertices].x,
                   &model->vertices[model->numVertices].y,
                   &model->vertices[model->numVertices].z);
            model->numVertices++;
        } else if (strncmp(line, "vt ", 3) == 0) {
            // Texture coordinate.
            ARRAY_GROW(model->texCoords, model->numTexCoords, vtCap, ObjTexCoord);
            sscanf(line + 3, "%f %f",
                   &model->texCoords[model->numTexCoords].u,
                   &model->texCoords[model->numTexCoords].v);
            model->numTexCoords++;
        } else if (strncmp(line, "vn ", 3) == 0) {
            // Normal.
            ARRAY_GROW(model->normals, model->numNormals, vnCap, ObjNormal);
            sscanf(line + 3, "%f %f %f",
                   &model->normals[model->numNormals].x,
                   &model->normals[model->numNormals].y,
                   &model->normals[model->numNormals].z);
            model->numNormals++;
        } else if (strncmp(line, "f ", 2) == 0) {
            // Face.
            ARRAY_GROW(model->faces, model->numFaces, fCap, ObjFace);
            ObjFace* face = &model->faces[model->numFaces];
            memset(face, 0, sizeof(ObjFace));
            strncpy(face->materialName, currentMaterial, sizeof(face->materialName) - 1);

            // Parse face vertices.
            char* token = strtok(line + 2, " \t\n\r");
            int numVerts = 0;
            while (token && numVerts < 4) {
                ParseFaceVertex(token, &face->v[numVerts], &face->vt[numVerts], &face->vn[numVerts]);
                numVerts++;
                token = strtok(NULL, " \t\n\r");
            }
            face->numVerts = numVerts;
            model->numFaces++;
        } else if (strncmp(line, "usemtl ", 7) == 0) {
            // Use material.
            sscanf(line + 7, "%63s", currentMaterial);
        } else if (strncmp(line, "mtllib ", 7) == 0) {
            // Material library — load MTL file.
            char mtlFilename[256];
            sscanf(line + 7, "%255s", mtlFilename);
            // TODO: Load MTL file (001-obj-mtl-loader task).
            // For now, just store the filename for later.
        }
    }

    fclose(f);
    return 0;
}

void ObjFree(ObjModel* model) {
    if (!model) return;
    free(model->vertices);
    free(model->texCoords);
    free(model->normals);
    free(model->faces);
    free(model->materials);
    memset(model, 0, sizeof(ObjModel));
}

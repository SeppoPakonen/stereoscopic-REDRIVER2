#ifndef MODEL_BUILDER_H
#define MODEL_BUILDER_H

#include "../driver2.h"
#include "mdl.h"
#include "obj_loader.h"

#ifdef __cplusplus
extern "C" {
#endif

// Build game MODEL from OBJ data. Returns allocated MODEL* (caller must free).
// scale: multiplier to convert OBJ units to game units (e.g., 100.0f).
// textureSet/textureId: assigned to all faces (for now, single texture).
MODEL* ModelBuilder_FromObj(const ObjModel* obj, float scale, int textureSet, int textureId);

// Free allocated MODEL and its data.
void ModelBuilder_Free(MODEL* model);

#ifdef __cplusplus
}
#endif

#endif // MODEL_BUILDER_H

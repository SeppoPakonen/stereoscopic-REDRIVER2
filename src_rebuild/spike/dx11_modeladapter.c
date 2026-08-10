// dx11_modeladapter.c — T1.6 MODEL -> arena adapter implementation.
//
// See dx11_modeladapter.h for the design. Per poly the adapter resolves + bakes
// the texture (white substitute when untextured), converts the referenced int16
// vertices to float (per-vertex rgb for gouraud, white for flat), pushes them
// (duplicated so same-material consecutive polys are index-contiguous) with
// normalized UVs, pushes the triangle indices, computes the local bbox, and
// submits one Dx11DrawCmdItem to the executor.

#include "dx11_modeladapter.h"

#include <string.h>

int Dx11ModelAdapter_Submit(Dx11Res *res, Dx11Tex *tex, Dx11DrawCmds *cmds,
                            const Dx11ModelMesh *mesh, const float world[4][4],
                            void *texUser, Dx11ModelTexResolve texResolve,
                            int *outPolys)
{
    if (!res || !tex || !cmds || !mesh || !mesh->polys || mesh->num_verts <= 0)
        return 1;

    float ident[4][4];
    memset(ident, 0, sizeof(ident));
    for (int i = 0; i < 4; ++i) ident[i][i] = 1.0f;
    const float (*W)[4] = world ? world : (const float (*)[4])ident;

    int polys = 0;
    for (int p = 0; p < mesh->num_polys; ++p)
    {
        const Dx11ModelPoly *poly = &mesh->polys[p];

        // PSX semi-transparent polys blend at 50% (stp); opaque at 100%.
        int translucent = (poly->blend == DX11SH_BLEND_NONE) ? 0 : 1;
        float alpha = translucent ? 0.5f : 1.0f;

        // Resolve + bake the texture (white substitute when untextured).
        Dx11TexHandle texh = -1;
        if (texResolve)
        {
            Dx11ModelTexture mt;
            if (texResolve(texUser, poly->texture_set, poly->texture_id, &mt) == 0)
                texh = Dx11Tex_Bake(tex, mt.tpage, mt.clut, mt.tex_x, mt.tex_y,
                                    mt.width, mt.height);
        }
        int tw = 1, th = 1;
        if (texh >= 0) Dx11Tex_GetSize(tex, texh, &tw, &th);

        // Triangle vs quad: vi3 == vi2 marks a triangle.
        int isQuad = (poly->vi3 != poly->vi2);
        int nv = isQuad ? 4 : 3;
        int vIdx[4] = { poly->vi0, poly->vi1, poly->vi2, poly->vi3 };
        int uv[4][2] = {
            { poly->u0, poly->v0 }, { poly->u1, poly->v1 },
            { poly->u2, poly->v2 }, { poly->u3, poly->v3 },
        };

        // Push vertices (float, model-local; world applied via the command CB).
        int baseV = Dx11Res_VertexCount(res);
        float bmin[3] = {  1e30f,  1e30f,  1e30f };
        float bmax[3] = { -1e30f, -1e30f, -1e30f };
        for (int k = 0; k < nv; ++k)
        {
            const Dx11ModelVertex *sv = &mesh->verts[vIdx[k]];
            Dx11ResVertex v;
            v.x = (float)sv->x; v.y = (float)sv->y; v.z = (float)sv->z;
            if (poly->shade == DX11SH_COLOR_GOURAUD)
            {
                v.r = sv->r / 255.0f; v.g = sv->g / 255.0f;
                v.b = sv->b / 255.0f; v.a = alpha;
            }
            else
            {
                v.r = v.g = v.b = 1.0f; v.a = alpha;
            }
            v.u = uv[k][0] / (float)tw;
            v.v = uv[k][1] / (float)th;
            if (Dx11Res_PushVertex(res, &v) < 0)
                return 1;
            if (v.x < bmin[0]) bmin[0] = v.x;
            if (v.y < bmin[1]) bmin[1] = v.y;
            if (v.z < bmin[2]) bmin[2] = v.z;
            if (v.x > bmax[0]) bmax[0] = v.x;
            if (v.y > bmax[1]) bmax[1] = v.y;
            if (v.z > bmax[2]) bmax[2] = v.z;
        }

        // Push indices (tri v0,v1,v2; quad v0,v1,v3 + v0,v3,v2).
        unsigned short ind[6];
        int nic;
        int baseI = Dx11Res_IndexCount(res);
        if (isQuad)
        {
            ind[0] = (unsigned short)(baseV + 0);
            ind[1] = (unsigned short)(baseV + 1);
            ind[2] = (unsigned short)(baseV + 3);
            ind[3] = (unsigned short)(baseV + 0);
            ind[4] = (unsigned short)(baseV + 3);
            ind[5] = (unsigned short)(baseV + 2);
            nic = 6;
        }
        else
        {
            ind[0] = (unsigned short)(baseV + 0);
            ind[1] = (unsigned short)(baseV + 1);
            ind[2] = (unsigned short)(baseV + 2);
            nic = 3;
        }
        for (int k = 0; k < nic; ++k)
            if (Dx11Res_PushIndex(res, ind[k]) < 0)
                return 1;

        // Submit one executor command.
        Dx11DrawCmdItem it;
        memset(&it, 0, sizeof(it));
        it.vertexOffset = baseV;
        it.vertexCount = nv;
        it.indexOffset = baseI;
        it.indexCount = nic;
        it.bboxMin[0] = bmin[0]; it.bboxMin[1] = bmin[1]; it.bboxMin[2] = bmin[2];
        it.bboxMax[0] = bmax[0]; it.bboxMax[1] = bmax[1]; it.bboxMax[2] = bmax[2];
        memcpy(it.world, W, sizeof(it.world));
        it.texture = texh;
        if (poly->shade == DX11SH_COLOR_GOURAUD)
        {
            it.shade = DX11SH_COLOR_GOURAUD;
            it.flatColor[0] = it.flatColor[1] = it.flatColor[2] = 1.0f;
            it.flatColor[3] = alpha;
        }
        else
        {
            it.shade = DX11SH_COLOR_FLAT;
            it.flatColor[0] = poly->r / 255.0f;
            it.flatColor[1] = poly->g / 255.0f;
            it.flatColor[2] = poly->b / 255.0f;
            it.flatColor[3] = alpha;
        }
        it.blend = poly->blend;
        it.twoSided = poly->twoSided;
        it.nodepth = poly->nodepth;
        it.translucent = (unsigned char)translucent;
        it.sortKey = poly->sortKey;

        if (Dx11DrawCmds_Submit(cmds, &it) != 0)
            return 1;
        ++polys;
    }

    if (outPolys) *outPolys = polys;
    return 0;
}
// dx11_shaders.c — T1.4 DX11 shaders + render state implementation.
//
// Universal VS + flat/gouraud textured PS, input layout (matching
// Dx11ResVertex), the PSX blend/depth-stencil/rasterizer states, and a
// per-draw flat-color CB (b2). Compiled as C++ by premake (compileas "C++").

#define WIN32_LEAN_AND_MEAN
#include "dx11_shaders.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Embedded HLSL (b0 = view/proj, b1 = per-draw world, b2 = flat color,
// t0/s0 = texture/sampler).
// ---------------------------------------------------------------------------
static const char *kVS = R"(
cbuffer ViewProj : register(b0) { float4x4 viewProj; };
cbuffer World : register(b1) { float4x4 world; };
struct VSIn { float3 pos : POSITION; float4 col : COLOR; float2 uv : TEXCOORD; };
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(mul(float4(i.pos, 1.0f), world), viewProj);
    o.col = i.col;
    o.uv = i.uv;
    return o;
}
)";

static const char *kPSFlat = R"(
cbuffer FlatColor : register(b2) { float4 flatColor; };
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD; };
float4 main(VSOut i) : SV_Target {
    return tex.Sample(samp, i.uv) * flatColor;
}
)";

static const char *kPSGouraud = R"(
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; float2 uv : TEXCOORD; };
float4 main(VSOut i) : SV_Target {
    return tex.Sample(samp, i.uv) * i.col;
}
)";

struct Dx11Shaders {
    ID3D11Device        *dev;
    ID3D11DeviceContext *ctx;
    ID3D11VertexShader  *vs;
    ID3D11PixelShader   *psFlat;
    ID3D11PixelShader   *psGouraud;
    ID3D11InputLayout   *layout;
    ID3D11BlendState    *blend[5];
    ID3D11DepthStencilState *dsOpaque;
    ID3D11DepthStencilState *dsTranslucent;
    ID3D11RasterizerState  *rsCull;
    ID3D11RasterizerState  *rsNoCull;
    ID3D11Buffer        *cbFlat; // b2
};

// Compile a shader blob; returns 0 on success.
static int CompileBlob(const char *src, const char *entry, const char *target,
                       ID3DBlob **out) {
    ID3DBlob *err = NULL;
    if (FAILED(D3DCompile(src, strlen(src), "shader", NULL, NULL,
                          entry, target, 0, 0, out, &err))) {
        if (err) {
            fprintf(stderr, "%s: %s\n", entry,
                    (const char *)err->GetBufferPointer());
            err->Release();
        }
        return 1;
    }
    return 0;
}

Dx11Shaders *Dx11Shaders_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                                Dx11ShadersResult *outResult) {
    if (outResult) *outResult = DX11SH_ERR_ARG;

    Dx11Shaders *s = (Dx11Shaders *)calloc(1, sizeof(Dx11Shaders));
    if (!s) { if (outResult) *outResult = DX11SH_ERR_ARG; return NULL; }
    s->dev = dev;
    s->ctx = ctx;

    // Hoisted locals (declared before any goto so the cleanup labels don't
    // cross initializations).
    D3D11_BLEND_DESC bd = {};
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    D3D11_RASTERIZER_DESC rsd = {};
    D3D11_BUFFER_DESC cbd = {};
    D3D11_SUBRESOURCE_DATA cbsd = {};
    float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    ID3DBlob *vsBlob = NULL, *psFlatBlob = NULL, *psGouraudBlob = NULL;
    D3D11_INPUT_ELEMENT_DESC elems[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (CompileBlob(kVS, "main", "vs_4_0", &vsBlob)) { if (outResult) *outResult = DX11SH_ERR_COMPILE; goto fail; }
    if (CompileBlob(kPSFlat, "main", "ps_4_0", &psFlatBlob)) { if (outResult) *outResult = DX11SH_ERR_COMPILE; goto fail; }
    if (CompileBlob(kPSGouraud, "main", "ps_4_0", &psGouraudBlob)) { if (outResult) *outResult = DX11SH_ERR_COMPILE; goto fail; }

    if (FAILED(dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &s->vs))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }
    if (FAILED(dev->CreatePixelShader(psFlatBlob->GetBufferPointer(), psFlatBlob->GetBufferSize(), NULL, &s->psFlat))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }
    if (FAILED(dev->CreatePixelShader(psGouraudBlob->GetBufferPointer(), psGouraudBlob->GetBufferSize(), NULL, &s->psGouraud))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    if (FAILED(dev->CreateInputLayout(elems, 3, vsBlob->GetBufferPointer(),
                                      vsBlob->GetBufferSize(), &s->layout))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    // Blend states (PSX/PsyCross table, from GR_SetBlendMode).
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bd, &s->blend[DX11SH_BLEND_NONE]))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    if (FAILED(dev->CreateBlendState(&bd, &s->blend[DX11SH_BLEND_AVERAGE]))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    if (FAILED(dev->CreateBlendState(&bd, &s->blend[DX11SH_BLEND_ADD]))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_SUBTRACT; // dst - src
    if (FAILED(dev->CreateBlendState(&bd, &s->blend[DX11SH_BLEND_SUBTRACT]))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_BLEND_FACTOR; // 0.5 constant (matches GL)
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    if (FAILED(dev->CreateBlendState(&bd, &s->blend[DX11SH_BLEND_ADD_QUATER]))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    // Depth-stencil: opaque (write on) / translucent (write off).
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(dev->CreateDepthStencilState(&dsd, &s->dsOpaque))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (FAILED(dev->CreateDepthStencilState(&dsd, &s->dsTranslucent))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    // Rasterizer: cull back / two-sided.
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_BACK;
    rsd.DepthClipEnable = TRUE;
    if (FAILED(dev->CreateRasterizerState(&rsd, &s->rsCull))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }
    rsd.CullMode = D3D11_CULL_NONE;
    if (FAILED(dev->CreateRasterizerState(&rsd, &s->rsNoCull))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    // Flat-color CB (b2). DEFAULT, 16 bytes, updated via UpdateSubresource.
    cbd.ByteWidth = 16;
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbsd.pSysMem = ident;
    if (FAILED(dev->CreateBuffer(&cbd, &cbsd, &s->cbFlat))) { if (outResult) *outResult = DX11SH_ERR_DEVICE; goto fail; }

    vsBlob->Release(); psFlatBlob->Release(); psGouraudBlob->Release();
    if (outResult) *outResult = DX11SH_OK;
    return s;

fail:
    if (vsBlob) vsBlob->Release();
    if (psFlatBlob) psFlatBlob->Release();
    if (psGouraudBlob) psGouraudBlob->Release();
    Dx11Shaders_Destroy(s);
    return NULL;
}

void Dx11Shaders_Destroy(Dx11Shaders *s) {
    if (!s)
        return;
    if (s->cbFlat) s->cbFlat->Release();
    if (s->rsNoCull) s->rsNoCull->Release();
    if (s->rsCull) s->rsCull->Release();
    if (s->dsTranslucent) s->dsTranslucent->Release();
    if (s->dsOpaque) s->dsOpaque->Release();
    for (int i = 0; i < 5; ++i) if (s->blend[i]) s->blend[i]->Release();
    if (s->layout) s->layout->Release();
    if (s->psGouraud) s->psGouraud->Release();
    if (s->psFlat) s->psFlat->Release();
    if (s->vs) s->vs->Release();
    // ctx is borrowed; do not Release.
    free(s);
}

void Dx11Shaders_Bind(Dx11Shaders *s, ID3D11DeviceContext *ctx, Dx11ShadersColor color) {
    if (!s) return;
    if (!ctx) ctx = s->ctx;
    ctx->IASetInputLayout(s->layout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(s->vs, NULL, 0);
    ID3D11PixelShader *ps = (color == DX11SH_COLOR_FLAT) ? s->psFlat : s->psGouraud;
    ctx->PSSetShader(ps, NULL, 0);
}

void Dx11Shaders_SetFlatColor(Dx11Shaders *s, ID3D11DeviceContext *ctx,
                              float r, float g, float b, float a) {
    if (!s) return;
    if (!ctx) ctx = s->ctx;
    float c[4] = { r, g, b, a };
    ctx->UpdateSubresource(s->cbFlat, 0, NULL, c, 0, 0);
    ctx->VSSetConstantBuffers(2, 0, NULL); // (not used by this VS)
    ctx->PSSetConstantBuffers(2, 1, &s->cbFlat);
}

void Dx11Shaders_SetBlend(Dx11Shaders *s, ID3D11DeviceContext *ctx,
                          Dx11ShadersBlend blend) {
    if (!s) return;
    if (!ctx) ctx = s->ctx;
    if (blend < DX11SH_BLEND_NONE || blend > DX11SH_BLEND_ADD_QUATER)
        blend = DX11SH_BLEND_NONE;
    float factor[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    ctx->OMSetBlendState(s->blend[blend], factor, 0xFFFFFFFF);
}

void Dx11Shaders_SetDepthOpaque(Dx11Shaders *s, ID3D11DeviceContext *ctx, int opaque) {
    if (!s) return;
    if (!ctx) ctx = s->ctx;
    ctx->OMSetDepthStencilState(opaque ? s->dsOpaque : s->dsTranslucent, 0);
}

void Dx11Shaders_SetRaster(Dx11Shaders *s, ID3D11DeviceContext *ctx, int twoSided) {
    if (!s) return;
    if (!ctx) ctx = s->ctx;
    ctx->RSSetState(twoSided ? s->rsNoCull : s->rsCull);
}

ID3D11InputLayout *Dx11Shaders_GetLayout(Dx11Shaders *s) {
    return s ? s->layout : NULL;
}
// dx11_composite.c — T2.3 DX11 stereo composite implementation.
//
// Fullscreen-triangle composite pass: samples the two per-eye SRVs (t0/t1)
// into the SBS/TB halves of the target, or pass-through (MONO). Compiled as
// C++ by premake (compileas "C++").

#define WIN32_LEAN_AND_MEAN
#include "dx11_composite.h"

#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Embedded HLSL. b0 = (mode, swap). t0/t1 = eye0/eye1, s0 = point sampler.
// The fullscreen triangle is generated from SV_VertexID (no vertex buffer).
// ---------------------------------------------------------------------------
static const char *kVS = R"(
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VSOut main(uint id : SV_VertexID) {
    VSOut o;
    float2 p;
    if (id == 0u) p = float2(-1.0f, -1.0f);
    else if (id == 1u) p = float2(3.0f, -1.0f);
    else p = float2(-1.0f, 3.0f);
    o.pos = float4(p, 0.0f, 1.0f);
    // uv (0,0)=top-left of the texture, matching the output's top-left corner.
    o.uv = float2((p.x + 1.0f) * 0.5f, (1.0f - p.y) * 0.5f);
    return o;
}
)";

static const char *kPS = R"(
Texture2D t0 : register(t0);
Texture2D t1 : register(t1);
SamplerState samp : register(s0);
cbuffer Params : register(b0) { float2 modeSwap; };
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

float4 SampleEye(float2 uv, float sel) {
    return sel > 0.5 ? t1.Sample(samp, uv) : t0.Sample(samp, uv);
}

float4 main(VSOut i) : SV_Target {
    float m = modeSwap.x;
    float sw = modeSwap.y;
    float2 uv = i.uv;

    if (m > 2.5 && m < 3.5) {              // ANAGLYPH simple (red-cyan)
        float4 L = t0.Sample(samp, uv);
        float4 R = t1.Sample(samp, uv);
        if (sw > 0.5) { float4 T = L; L = R; R = T; }
        return float4(L.r, R.g, R.b, 1.0);
    }
    if (m > 3.5 && m < 4.5) {              // ANAGLYPH full-color (luminance)
        float4 L = t0.Sample(samp, uv);
        float4 R = t1.Sample(samp, uv);
        if (sw > 0.5) { float4 T = L; L = R; R = T; }
        float ll = dot(L.rgb, float3(0.3, 0.59, 0.11));
        float rl = dot(R.rgb, float3(0.3, 0.59, 0.11));
        float3 res = float3(L.r * 0.8, R.g * 0.9, R.b * 0.9);
        res += float3(rl * 0.1, ll * 0.1, ll * 0.1);
        return float4(res, 1.0);
    }
    if (m > 4.5 && m < 5.5) {              // INTERLACED: odd rows = eye0
        float scan = fmod(i.pos.y, 2.0);
        float sel = (scan > 0.5) ? 0.0 : 1.0;
        if (sw > 0.5) sel = 1.0 - sel;
        return SampleEye(uv, sel);
    }
    if (m > 5.5 && m < 6.5) {              // POLARIZED: odd rows = eye1
        float scan = fmod(i.pos.y, 2.0);
        float sel = (scan > 0.5) ? 1.0 : 0.0;
        if (sw > 0.5) sel = 1.0 - sel;
        return SampleEye(uv, sel);
    }
    if (m > 6.5 && m < 7.5) {              // CHECKERBOARD: pixel interleave
        float chk = fmod(i.pos.x + i.pos.y, 2.0);
        float sel = (chk > 0.5) ? 1.0 : 0.0;
        if (sw > 0.5) sel = 1.0 - sel;
        return SampleEye(uv, sel);
    }
    if (m > 1.5 && m < 2.5)                // MONO: eye0 pass-through
        return t0.Sample(samp, i.uv);

    // SBS / TB halves.
    bool first = (m < 0.5) ? (uv.x < 0.5) : (uv.y < 0.5);
    if (sw > 0.5) first = !first;          // swap flips left/top eye
    float sel = first ? 0.0 : 1.0;
    if (m < 0.5) {                         // SBS
        if (uv.x < 0.5) uv.x *= 2.0f; else uv.x = (uv.x - 0.5f) * 2.0f;
    } else {                               // TB
        if (uv.y < 0.5) uv.y *= 2.0f; else uv.y = (uv.y - 0.5f) * 2.0f;
    }
    return SampleEye(uv, sel);
}
)";

struct Dx11Composite {
    ID3D11Device        *dev;
    ID3D11DeviceContext *ctx;
    ID3D11VertexShader  *vs;
    ID3D11PixelShader   *ps;
    ID3D11Buffer        *cbParams; // b0
    ID3D11SamplerState  *sampler;  // point
    ID3D11RasterizerState *rsNoCull;
    ID3D11DepthStencilState *dsDisabled;
    ID3D11BlendState    *blendNone;
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

Dx11Composite *Dx11Composite_Create(ID3D11Device *dev, ID3D11DeviceContext *ctx,
                                    Dx11CompositeResult *outResult) {
    if (outResult) *outResult = DX11C_ERR_ARG;

    Dx11Composite *c = (Dx11Composite *)calloc(1, sizeof(Dx11Composite));
    if (!c) { if (outResult) *outResult = DX11C_ERR_ARG; return NULL; }
    c->dev = dev;
    c->ctx = ctx;

    // Hoisted locals (declared before any goto so the cleanup labels don't
    // cross initializations).
    D3D11_BUFFER_DESC cbd = {};
    D3D11_SUBRESOURCE_DATA cbsd = {};
    D3D11_SAMPLER_DESC sd = {};
    D3D11_RASTERIZER_DESC rsd = {};
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    D3D11_BLEND_DESC bd = {};
    float zero[4] = { 0, 0, 0, 0 };
    ID3DBlob *vsBlob = NULL, *psBlob = NULL;

    if (CompileBlob(kVS, "main", "vs_4_0", &vsBlob)) { if (outResult) *outResult = DX11C_ERR_COMPILE; goto fail; }
    if (CompileBlob(kPS, "main", "ps_4_0", &psBlob)) { if (outResult) *outResult = DX11C_ERR_COMPILE; goto fail; }

    if (FAILED(dev->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), NULL, &c->vs))) { if (outResult) *outResult = DX11C_ERR_DEVICE; goto fail; }
    if (FAILED(dev->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &c->ps))) { if (outResult) *outResult = DX11C_ERR_DEVICE; goto fail; }

    // Params CB (b0): (mode, swap) as a 16-byte DEFAULT buffer.
    cbd.ByteWidth = 16;
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbsd.pSysMem = zero;
    if (FAILED(dev->CreateBuffer(&cbd, &cbsd, &c->cbParams))) { if (outResult) *outResult = DX11C_ERR_DEVICE; goto fail; }

    // Point sampler (nearest, matching the legacy glBlitFramebuffer NEAREST).
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (FAILED(dev->CreateSamplerState(&sd, &c->sampler))) { if (outResult) *outResult = DX11C_ERR_DEVICE; goto fail; }

    // Rasterizer: no culling (fullscreen triangle must not be dropped).
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    if (FAILED(dev->CreateRasterizerState(&rsd, &c->rsNoCull))) { if (outResult) *outResult = DX11C_ERR_DEVICE; goto fail; }

    // Depth-stencil: disabled entirely for the blit.
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(dev->CreateDepthStencilState(&dsd, &c->dsDisabled))) { if (outResult) *outResult = DX11C_ERR_DEVICE; goto fail; }

    // Blend: none (opaque blit).
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bd, &c->blendNone))) { if (outResult) *outResult = DX11C_ERR_DEVICE; goto fail; }

    vsBlob->Release(); psBlob->Release();
    if (outResult) *outResult = DX11C_OK;
    return c;

fail:
    if (vsBlob) vsBlob->Release();
    if (psBlob) psBlob->Release();
    Dx11Composite_Destroy(c);
    return NULL;
}

void Dx11Composite_Destroy(Dx11Composite *c) {
    if (!c)
        return;
    if (c->blendNone) c->blendNone->Release();
    if (c->dsDisabled) c->dsDisabled->Release();
    if (c->rsNoCull) c->rsNoCull->Release();
    if (c->sampler) c->sampler->Release();
    if (c->cbParams) c->cbParams->Release();
    if (c->ps) c->ps->Release();
    if (c->vs) c->vs->Release();
    // dev/ctx are borrowed; do not Release.
    free(c);
}

void Dx11Composite_Composite(Dx11Composite *c, ID3D11DeviceContext *ctx,
                             Dx11CompositeMode mode, int swap,
                             ID3D11ShaderResourceView *eye0,
                             ID3D11ShaderResourceView *eye1,
                             int w, int h) {
    if (!c) return;
    if (!ctx) ctx = c->ctx;
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    float params[4] = { (float)mode, (swap ? 1.0f : 0.0f), 0, 0 };
    ctx->UpdateSubresource(c->cbParams, 0, NULL, params, 0, 0);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(c->vs, NULL, 0);
    ctx->PSSetShader(c->ps, NULL, 0);
    ctx->PSSetConstantBuffers(0, 1, &c->cbParams);
    ctx->PSSetShaderResources(0, 1, &eye0);
    ctx->PSSetShaderResources(1, 1, &eye1);
    ctx->PSSetSamplers(0, 1, &c->sampler);
    ctx->RSSetState(c->rsNoCull);
    ctx->OMSetDepthStencilState(c->dsDisabled, 0);
    ctx->OMSetBlendState(c->blendNone, NULL, 0xFFFFFFFF);

    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0; vp.TopLeftY = 0;
    vp.Width = (float)w; vp.Height = (float)h;
    vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    ctx->Draw(3, 0);
}
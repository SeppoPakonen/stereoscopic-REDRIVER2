// dx11_shaders_test.cpp — T1.4 shaders + render-state harness.
//
// Verifies the four shader modes (flat/gouraud x textured/untextured) and the
// PSX AVERAGE blend on top of dx11_renderer + dx11_textures (a baked 4-bit
// checker SRV, white substitute for untextured) + dx11_shaders.
//
// Layout (clip space, 800x600):
//   background : full-screen flat opaque blue (opaque blend)
//   A (bottl): flat + textured   (checker, flatColor white)
//   B (bottr): gouraud + textured (white->dark vertex gradient over checker)
//   C (topleft): flat + untextured (white sub, flatColor green)
//   D (topright): gouraud + untextured (yellow->red vertex gradient)
//   E (center) : translucent flat red, alpha 0.5, AVERAGE blend over blue bg
// Every mode is probed against the backbuffer BMP; the center probe must equal
// 0.5*red + 0.5*blue => (100,0,100), proving the blend matches the psyx table.

#define WIN32_LEAN_AND_MEAN
#include "dx11_renderer.h"
#include "dx11_textures.h"
#include "dx11_shaders.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Vertex: pos@0 (12), color@12 (16), uv@28 (8) = 36 bytes (matches layout).
// ---------------------------------------------------------------------------
struct V { float x, y, z, r, g, b, a, u, v; };

static void AddQuad(V *v, int &nv, unsigned short *idx, int &ni,
                    float xl, float yt, float xr, float yb,
                    float r00,float g00,float b00,   // top-left  (u0,v0)
                    float r10,float g10,float b10,   // top-right (u1,v0)
                    float r11,float g11,float b11,   // bot-right (u1,v1)
                    float r01,float g01,float b01,   // bot-left  (u0,v1)
                    float u0, float v0, float u1, float v1) {
    int base = nv;
    v[nv++] = { xl, yt, 0, r00, g00, b00, 1, u0, v0 };
    v[nv++] = { xr, yt, 0, r10, g10, b10, 1, u1, v0 };
    v[nv++] = { xr, yb, 0, r11, g11, b11, 1, u1, v1 };
    v[nv++] = { xl, yb, 0, r01, g01, b01, 1, u0, v1 };
    idx[ni++] = base; idx[ni++] = base + 1; idx[ni++] = base + 2;
    idx[ni++] = base; idx[ni++] = base + 2; idx[ni++] = base + 3;
}

// ---------------------------------------------------------------------------
// BMP probe (bottom-up, 24-bit BGR, rowSize padded to 4).
// ---------------------------------------------------------------------------
static int ProbeBMP(const char *path, int x, int y, int *r, int *g, int *b) {
    FILE *f = fopen(path, "rb");
    if (!f) return 1;
    unsigned char hdr[54];
    if (fread(hdr, 1, 54, f) != 54) { fclose(f); return 1; }
    int w = hdr[18] | (hdr[19] << 8) | (hdr[20] << 16) | (hdr[21] << 24);
    int h = hdr[22] | (hdr[23] << 8) | (hdr[24] << 16) | (hdr[25] << 24);
    if (x < 0 || x >= w || y < 0 || y >= h) { fclose(f); return 1; }
    int rowSize = (w * 3 + 3) & ~3;
    long off = 54 + (long)(h - 1 - y) * rowSize + (long)x * 3;
    if (fseek(f, off, SEEK_SET) != 0) { fclose(f); return 1; }
    unsigned char px[3];
    if (fread(px, 1, 3, f) != 3) { fclose(f); return 1; }
    *b = px[0]; *g = px[1]; *r = px[2];
    fclose(f);
    return 0;
}

static void Fail(const char *msg, HRESULT hr) {
    char buf[512];
    sprintf(buf, "%s\nHRESULT 0x%08lX", msg, (unsigned long)hr);
    MessageBoxA(NULL, buf, "dx11_shaders_test: fatal error", MB_OK | MB_ICONERROR);
    exit(2);
}

static int ParsePair(const char *s, int *a, int *b) {
    return (sscanf(s, "%dx%d", a, b) == 2 && *a > 0 && *b > 0) ? 1 : 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int) {
    Dx11RendererConfig rcfg = { 800, 600, 320, 240, 0, 0 };
    {
        char buf[400];
        strncpy(buf, lpCmdLine ? lpCmdLine : "", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, " \t");
        while (tok) {
            if (!strcmp(tok, "-res") && (tok = strtok(NULL, " \t"))) {
                int w, hh; if (ParsePair(tok, &w, &hh)) { rcfg.windowW = w; rcfg.windowH = hh; }
            }
            tok = strtok(NULL, " \t");
        }
    }

    Dx11RendererResult rr;
    Dx11Renderer *ren = Dx11Renderer_Create(&rcfg, &rr);
    if (!ren) { MessageBoxA(NULL, "Dx11Renderer_Create failed", "dx11_shaders_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11Device *dev = Dx11Renderer_GetDevice(ren);
    ID3D11DeviceContext *ctx = Dx11Renderer_GetContext(ren);

    Dx11ShadersResult sr;
    Dx11Shaders *sh = Dx11Shaders_Create(dev, ctx, &sr);
    if (!sh) { MessageBoxA(NULL, "Dx11Shaders_Create failed", "dx11_shaders_test", MB_OK | MB_ICONERROR); return 2; }

    // Checker texture (4-bit white/red) via dx11_textures.
    Dx11TexResult tr;
    Dx11Tex *texmod = Dx11Tex_Create(dev, ctx, NULL, &tr);
    if (!texmod) { MessageBoxA(NULL, "Dx11Tex_Create failed", "dx11_shaders_test", MB_OK | MB_ICONERROR); return 2; }
    unsigned short vram[DX11TEX_VRAM_W * DX11TEX_VRAM_H] = { 0 };
    unsigned short pal[8] = { 0x7FFF, 0x001F, 0, 0, 0, 0, 0, 0 }; // white, red
    for (int x = 0; x < 8; ++x)
        for (int y = 0; y < 8; ++y) {
            int ax = x, ay = y, idx = ((x + y) % 2) & 0x0f;
            int wordCol = ax / 4, byteShift = ((ax & 2) >> 1) * 8, nibShift = (ax & 1) ? 4 : 0;
            vram[ay * 1024 + wordCol] |= (unsigned short)(idx << (nibShift + byteShift));
        }
    for (int i = 0; i < 8; ++i) vram[8 * 1024 + i] = pal[i]; // clutX=0, clutY=8
    Dx11Tex_CopyVRAM(texmod, vram, 0, 0, DX11TEX_VRAM_W, DX11TEX_VRAM_H, 0, 0);
    Dx11TexHandle hChecker = Dx11Tex_Bake(texmod, 0, 512, 0, 0, 8, 8);
    if (hChecker < 0) { MessageBoxA(NULL, "bake checker failed", "dx11_shaders_test", MB_OK | MB_ICONERROR); return 2; }
    ID3D11ShaderResourceView *srvChecker = Dx11Tex_GetSRV(texmod, hChecker);
    ID3D11ShaderResourceView *srvWhite = Dx11Tex_GetWhiteSRV(texmod);

    // Identity viewProj (b0) + world (b1) so clip-space quads pass through.
    float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    D3D11_BUFFER_DESC cbd = {}; cbd.ByteWidth = 64; cbd.Usage = D3D11_USAGE_DEFAULT; cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cbsd = {}; cbsd.pSysMem = ident;
    ID3D11Buffer *cbVP = NULL, *cbWorld = NULL;
    if (FAILED(dev->CreateBuffer(&cbd, &cbsd, &cbVP))) Fail("CreateBuffer(b0)", 0);
    if (FAILED(dev->CreateBuffer(&cbd, &cbsd, &cbWorld))) Fail("CreateBuffer(b1)", 0);

    // Point sampler.
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState *samp = NULL;
    if (FAILED(dev->CreateSamplerState(&sd, &samp))) Fail("CreateSamplerState", 0);

    // ----------------------------------------------------------------------
    // Geometry (clip space, y=-1 bottom .. +1 top).
    // ----------------------------------------------------------------------
    V verts[24]; unsigned short idx[36]; int nv = 0, ni = 0;
    // Background: full-screen opaque blue.
    AddQuad(verts, nv, idx, ni, -1, 1, 1, -1,
            0,0,1, 0,0,1, 0,0,1, 0,0,1, 0,0, 1,1);
    // A: bottom-left flat + textured (checker, white flatColor).
    AddQuad(verts, nv, idx, ni, -1, -1.0f/6.0f, -0.125f, -1,
            1,1,1, 1,1,1, 1,1,1, 1,1,1, 0,0, 1,1);
    // B: bottom-right gouraud + textured (white->dark gradient).
    AddQuad(verts, nv, idx, ni, 0.125f, -1.0f/6.0f, 1, -1,
            1,1,1, 0.3f,0.3f,0.3f, 0.3f,0.3f,0.3f, 1,1,1, 0,0, 1,1);
    // C: top-left flat + untextured (white sub, green flatColor).
    AddQuad(verts, nv, idx, ni, -1, 1, -0.125f, 1.0f/6.0f,
            1,1,1, 1,1,1, 1,1,1, 1,1,1, 0,0, 1,1);
    // D: top-right gouraud + untextured (yellow->red gradient).
    AddQuad(verts, nv, idx, ni, 0.125f, 1, 1, 1.0f/6.0f,
            1,1,0, 1,0,0, 1,0,0, 1,1,0, 0,0, 1,1);
    // E: center translucent flat red (alpha .5), AVERAGE blend.
    AddQuad(verts, nv, idx, ni, -0.25f, 1.0f/3.0f, 0.25f, -1.0f/3.0f,
            1,0,0, 1,0,0, 1,0,0, 1,0,0, 0,0, 1,1);

    D3D11_BUFFER_DESC vbd = {}; vbd.ByteWidth = sizeof(verts); vbd.Usage = D3D11_USAGE_DEFAULT; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vsrd = {}; vsrd.pSysMem = verts;
    ID3D11Buffer *vb = NULL;
    if (FAILED(dev->CreateBuffer(&vbd, &vsrd, &vb))) Fail("CreateBuffer(VB)", 0);
    D3D11_BUFFER_DESC ibd = {}; ibd.ByteWidth = sizeof(idx); ibd.Usage = D3D11_USAGE_DEFAULT; ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA isrd = {}; isrd.pSysMem = idx;
    ID3D11Buffer *ib = NULL;
    if (FAILED(dev->CreateBuffer(&ibd, &isrd, &ib))) Fail("CreateBuffer(IB)", 0);

    const char *bmpPath = "dx11_shaders_frame.bmp";
    const char *statsPath = "dx11_shaders_frame.txt";

    // ----------------------------------------------------------------------
    // Draw one frame.
    // ----------------------------------------------------------------------
    if (Dx11Renderer_BeginFrame(ren) == 0) {
        Dx11Renderer_BindBackbuffer(ren);
        ctx->VSSetConstantBuffers(0, 1, &cbVP);
        ctx->VSSetConstantBuffers(1, 1, &cbWorld);
        ctx->PSSetSamplers(0, 1, &samp);
        UINT stride = (UINT)sizeof(V), offset = 0;
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);

        // Background: flat opaque blue.
        Dx11Shaders_Bind(sh, ctx, DX11SH_COLOR_FLAT);
        Dx11Shaders_SetFlatColor(sh, ctx, 0, 0, 200.0f / 255.0f, 1);
        Dx11Shaders_SetBlend(sh, ctx, DX11SH_BLEND_NONE);
        Dx11Shaders_SetDepthOpaque(sh, ctx, 1);
        Dx11Shaders_SetRaster(sh, ctx, 0);
        ID3D11ShaderResourceView *srv = srvWhite;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(6, 0, 0);

        // A: flat + textured (checker, white flatColor).
        Dx11Shaders_Bind(sh, ctx, DX11SH_COLOR_FLAT);
        Dx11Shaders_SetFlatColor(sh, ctx, 1, 1, 1, 1);
        srv = srvChecker;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(6, 6, 0);

        // B: gouraud + textured.
        Dx11Shaders_Bind(sh, ctx, DX11SH_COLOR_GOURAUD);
        srv = srvChecker;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(6, 12, 0);

        // C: flat + untextured (white sub, green).
        Dx11Shaders_Bind(sh, ctx, DX11SH_COLOR_FLAT);
        Dx11Shaders_SetFlatColor(sh, ctx, 0, 200.0f / 255.0f, 0, 1);
        srv = srvWhite;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(6, 18, 0);

        // D: gouraud + untextured (yellow->red).
        Dx11Shaders_Bind(sh, ctx, DX11SH_COLOR_GOURAUD);
        srv = srvWhite;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(6, 24, 0);

        // E: translucent flat red, AVERAGE blend, depth write off.
        Dx11Shaders_Bind(sh, ctx, DX11SH_COLOR_FLAT);
        Dx11Shaders_SetFlatColor(sh, ctx, 200.0f / 255.0f, 0, 0, 0.5f);
        Dx11Shaders_SetBlend(sh, ctx, DX11SH_BLEND_AVERAGE);
        Dx11Shaders_SetDepthOpaque(sh, ctx, 0);
        srv = srvWhite;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(6, 30, 0);

        Dx11Renderer_CaptureToBMP(ren, NULL, bmpPath, statsPath);
        Dx11Renderer_Present(ren);
    }

    // ----------------------------------------------------------------------
    // Probe the frame and assert each mode + the blend.
    // ----------------------------------------------------------------------
    FILE *res = fopen("dx11_shaders_result.txt", "w");
    if (!res) { MessageBoxA(NULL, "cannot open result file", "dx11_shaders_test", MB_OK); return 2; }
    int r, g, b, fails = 0;

    // Background at (200,300) (the middle band, left of the blend quad) = blue.
    if (ProbeBMP(bmpPath, 200, 300, &r, &g, &b) == 0) {
        int ok = (b > 180 && r < 40 && g < 40);
        fprintf(res, "background (200,300)=(%d,%d,%d) %s\n", r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { fprintf(res, "background probe unavailable\n"); ++fails; }

    // A: flat + textured. At (175,475) the checker texel is white -> R high.
    if (ProbeBMP(bmpPath, 175, 475, &r, &g, &b) == 0) {
        int ok = (r > 200);
        fprintf(res, "flat_tex (175,475)=(%d,%d,%d) R>200 %s\n", r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(res, "flat_tex probe unavailable\n"); }

    // B: gouraud + textured. Left is brighter than right.
    int rl, gl, bl, r2, g2, b2;
    if (ProbeBMP(bmpPath, 475, 475, &rl, &gl, &bl) == 0 &&
        ProbeBMP(bmpPath, 775, 475, &r2, &g2, &b2) == 0) {
        int ok = (rl > r2 + 50);
        fprintf(res, "goraud_tex left(475,475)=(%d,%d,%d) right(775,475)=(%d,%d,%d) left>right+50 %s\n",
                rl, gl, bl, r2, g2, b2, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(res, "goraud_tex probes unavailable\n"); }

    // C: flat + untextured green.
    if (ProbeBMP(bmpPath, 175, 125, &r, &g, &b) == 0) {
        int ok = (g > 180 && r < 40 && b < 40);
        fprintf(res, "flat_untex (175,125)=(%d,%d,%d) green %s\n", r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(res, "flat_untex probe unavailable\n"); }

    // D: gouraud + untextured. Left (yellow) has high G, right (red) low G.
    if (ProbeBMP(bmpPath, 500, 125, &rl, &gl, &bl) == 0 &&
        ProbeBMP(bmpPath, 750, 125, &r2, &g2, &b2) == 0) {
        int ok = (gl > g2 + 50);
        fprintf(res, "goraud_untex left(500,125)=(%d,%d,%d) right(750,125)=(%d,%d,%d) leftG>rightG+50 %s\n",
                rl, gl, bl, r2, g2, b2, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(res, "goraud_untex probes unavailable\n"); }

    // E: AVERAGE blend at center (400,300) = 0.5*red + 0.5*blue = (100,0,100).
    if (ProbeBMP(bmpPath, 400, 300, &r, &g, &b) == 0) {
        int ok = (abs(r - 100) < 15 && g < 15 && abs(b - 100) < 15);
        fprintf(res, "avg_blend center(400,300)=(%d,%d,%d) expect(100,0,100)+-15 %s\n",
                r, g, b, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    } else { ++fails; fprintf(res, "avg_blend probe unavailable\n"); }

    fprintf(res, "TOTAL_FAILS=%d SHADERS=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(res);

    ib->Release(); vb->Release(); samp->Release();
    cbWorld->Release(); cbVP->Release();
    Dx11Tex_Destroy(texmod);
    Dx11Shaders_Destroy(sh);
    Dx11Renderer_Destroy(ren);
    return 0;
}
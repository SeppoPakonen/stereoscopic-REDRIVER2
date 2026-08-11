// dx11_rendererselect_test.cpp — T4.1 renderer-selection A/B harness (extended
// for the GL backend in T4.5).
//
// Headless A/B verification of the `-renderer dx11|psyx|gl` backend selection
// used by DrawGame's dispatch (main.c): all three backends resolve correctly,
// the Renderer_IsDX11/IsPsyX/IsGL flags reflect the selection, and the DX11 and
// modern GL backends are actually usable on this machine (Dx11Renderer_Available
// / GlRenderer_Available). This is the verifiable slice of the "all renderers
// usable" acceptance when the full game cannot reach the gameplay loop headless.
//
// Probes:
//   SELECT — Renderer_FromName("dx11") -> DX11, "psyx" -> PSYX, "gl" -> GL.
//   FLAG   — Renderer_IsDX11()/IsPsyX()/IsGL() reflect gRenderer.
//   AVAIL  — Dx11Renderer_Available() and GlRenderer_Available() return 1.

#define WIN32_LEAN_AND_MEAN
#include "renderer.h"         // Game/render/renderer.h (Renderer_FromName etc.)
#include "dx11_renderer.h"    // spike/dx11_renderer.h (Dx11Renderer_Available)
#include "gl_renderer.h"      // spike/gl_renderer.h (GlRenderer_Available)

#include <stdio.h>

// gRenderer is defined in the game (Game/C/main.c); define it here for the
// standalone harness.
RendererId gRenderer = RENDERER_DX11;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int fails = 0;
    FILE *resf = fopen("dx11_rendererselect_result.txt", "w");
    if (!resf) return 2;

    // SELECT: "dx11" -> DX11, "psyx" -> PSYX, "gl" -> GL.
    int selDx11 = (Renderer_FromName("dx11") == RENDERER_DX11);
    int selPsyx = (Renderer_FromName("psyx") == RENDERER_PSYX);
    int selGl = (Renderer_FromName("gl") == RENDERER_GL);
    fprintf(resf, "SELECT dx11->DX11 %s | psyx->PSYX %s | gl->GL %s\n",
            selDx11 ? "PASS" : "FAIL", selPsyx ? "PASS" : "FAIL", selGl ? "PASS" : "FAIL");
    if (!selDx11) ++fails;
    if (!selPsyx) ++fails;
    if (!selGl) ++fails;

    // FLAG: the Is* helpers reflect gRenderer for each backend.
    gRenderer = RENDERER_DX11;
    int flagDx = Renderer_IsDX11() && !Renderer_IsPsyX() && !Renderer_IsGL();
    gRenderer = RENDERER_PSYX;
    int flagPy = Renderer_IsPsyX() && !Renderer_IsDX11() && !Renderer_IsGL();
    gRenderer = RENDERER_GL;
    int flagGl = Renderer_IsGL() && !Renderer_IsDX11() && !Renderer_IsPsyX();
    gRenderer = RENDERER_DX11;   // restore default
    fprintf(resf, "FLAG dx11:%s psyx:%s gl:%s\n",
            flagDx ? "PASS" : "FAIL", flagPy ? "PASS" : "FAIL", flagGl ? "PASS" : "FAIL");
    if (!flagDx) ++fails;
    if (!flagPy) ++fails;
    if (!flagGl) ++fails;

    // AVAIL: the DX11 and modern GL backends are usable on this machine.
    int availDx = Dx11Renderer_Available();
    fprintf(resf, "AVAIL Dx11Renderer_Available=%d %s\n", availDx, availDx ? "PASS" : "FAIL");
    if (!availDx) ++fails;
    int availGl = GlRenderer_Available();
    fprintf(resf, "AVAIL GlRenderer_Available=%d %s\n", availGl, availGl ? "PASS" : "FAIL");
    if (!availGl) ++fails;

    fprintf(resf, "TOTAL_FAILS=%d RENDERERSELECT=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);
    return 0;
}
// dx11_rendererselect_test.cpp — T4.1 renderer-selection A/B harness.
//
// Headless A/B verification of the `-renderer dx11|psyx` backend selection used
// by DrawGame's dispatch (main.c): both backends resolve correctly, the
// Renderer_IsDX11/IsPsyX flags reflect the selection, and the DX11 backend is
// actually usable on this machine (Dx11Renderer_Available). This is the
// verifiable slice of the "both renderers usable" acceptance when the full game
// cannot reach the gameplay loop headless.
//
// Probes:
//   SELECT — Renderer_FromName("dx11") -> DX11 and "psyx" -> PSYX (A/B).
//   FLAG   — Renderer_IsDX11()/Renderer_IsPsyX() reflect gRenderer.
//   AVAIL  — Dx11Renderer_Available() returns 1 (DX11 usable).

#define WIN32_LEAN_AND_MEAN
#include "renderer.h"         // Game/render/renderer.h (Renderer_FromName etc.)
#include "dx11_renderer.h"    // spike/dx11_renderer.h (Dx11Renderer_Available)

#include <stdio.h>

// gRenderer is defined in the game (Game/C/main.c); define it here for the
// standalone harness.
RendererId gRenderer = RENDERER_DX11;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int fails = 0;
    FILE *resf = fopen("dx11_rendererselect_result.txt", "w");
    if (!resf) return 2;

    // SELECT: "dx11" -> DX11, "psyx" -> PSYX.
    int selDx11 = (Renderer_FromName("dx11") == RENDERER_DX11);
    int selPsyx = (Renderer_FromName("psyx") == RENDERER_PSYX);
    fprintf(resf, "SELECT dx11->DX11 %s | psyx->PSYX %s\n",
            selDx11 ? "PASS" : "FAIL", selPsyx ? "PASS" : "FAIL");
    if (!selDx11) ++fails;
    if (!selPsyx) ++fails;

    // FLAG: the Is* helpers reflect gRenderer for each backend.
    gRenderer = RENDERER_DX11;
    int flagDx = Renderer_IsDX11() && !Renderer_IsPsyX();
    gRenderer = RENDERER_PSYX;
    int flagPy = Renderer_IsPsyX() && !Renderer_IsDX11();
    gRenderer = RENDERER_DX11;   // restore default
    fprintf(resf, "FLAG dx11:%s psyx:%s\n", flagDx ? "PASS" : "FAIL", flagPy ? "PASS" : "FAIL");
    if (!flagDx) ++fails;
    if (!flagPy) ++fails;

    // AVAIL: the DX11 backend is usable on this machine.
    int avail = Dx11Renderer_Available();
    fprintf(resf, "AVAIL Dx11Renderer_Available=%d %s\n", avail, avail ? "PASS" : "FAIL");
    if (!avail) ++fails;

    fprintf(resf, "TOTAL_FAILS=%d RENDERERSELECT=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);
    return 0;
}
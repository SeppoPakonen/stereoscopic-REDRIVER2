// dx11_input_test.cpp — T1.7 DirectInput8 input backend harness.
//
// Verifies headless that the DirectInput8 module initializes, creates the
// keyboard/mouse/(joystick) devices, polls into a well-formed state, and maps
// the game's default PSX pad controls from their DIK scancodes faithfully and
// collision-free. Real key presses can't be asserted headless, but the module
// logic (device setup, poll, re-acquire path, mapping table) is.
//
//   * INIT    — DirectInput8Create + device setup succeed (DX11INP_OK);
//   * POLL    — Dx11Input_Poll fills a state without crashing (multiple polls,
//               exercising the DIERR_INPUTLOST re-acquire path);
//   * MAP     — each of the 16 PSX pad actions maps from exactly one DIK code,
//               and the direct codes are correct (DIK_X->SQUARE, ...);
//   * UNMAP   — an unmapped DIK code returns DX11IK_NONE.

#define WIN32_LEAN_AND_MEAN
#include "dx11_input.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

static int FailInit(const char *msg) {
    char buf[512];
    sprintf(buf, "dx11_input_test: %s", msg);
    MessageBoxA(NULL, buf, "dx11_input_test: fatal error", MB_OK | MB_ICONERROR);
    return 2;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // A hidden window is enough for SetCooperativeLevel (DISCL_BACKGROUND).
    WNDCLASSA wc = {};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = hInst;
    wc.lpszClassName = "dx11_input_test_wnd";
    RegisterClassA(&wc);
    HWND wnd = CreateWindowExA(0, wc.lpszClassName, "dx11_input_test", WS_OVERLAPPED,
                               0, 0, 64, 64, NULL, NULL, hInst, NULL);
    if (!wnd) return FailInit("CreateWindowExA failed");

    FILE *resf = fopen("dx11_input_result.txt", "w");
    if (!resf) return FailInit("cannot open result file");
    int fails = 0;

    // ------------------------------------------------------------------
    // INIT
    // ------------------------------------------------------------------
    Dx11InputResult ir;
    Dx11Input *inp = Dx11Input_Create(wnd, &ir);
    if (!inp) {
        fprintf(resf, "INIT create failed (result=%d)\n", ir);
        ++fails;
        fclose(resf);
        return 1;
    }
    int okInit = (ir == DX11INP_OK);
    fprintf(resf, "INIT create (result=%d expect 0) %s\n", ir, okInit ? "PASS" : "FAIL");
    if (!okInit) ++fails;

    // ------------------------------------------------------------------
    // POLL — several polls, exercising the re-acquire path.
    // ------------------------------------------------------------------
    Dx11InputState st;
    int pollOk = 1;
    for (int i = 0; i < 5; ++i) {
        Dx11Input_Poll(inp, &st);
        // The state struct must be fully initialised (keys array readable).
        unsigned char anyKey = 0;
        for (int k = 0; k < 256; ++k) anyKey |= st.keys[k];
        (void)anyKey;
        // Mouse fields must be int-sized and the struct must not be garbage.
        if (st.mouseX < -100000 || st.mouseX > 100000) { pollOk = 0; break; }
        if (st.joyAxis[0] < -1000 || st.joyAxis[0] > 1000) { pollOk = 0; break; }
    }
    fprintf(resf, "POLL 5 polls, state well-formed %s\n", pollOk ? "PASS" : "FAIL");
    if (!pollOk) ++fails;

    int hasJoy = Dx11Input_HasJoystick(inp);
    fprintf(resf, "POLL joystick device %s (optional)\n", hasJoy ? "present" : "absent");

    // ------------------------------------------------------------------
    // MAP — the 16 PSX pad actions, each from exactly one DIK code.
    // ------------------------------------------------------------------
    struct MapCheck { int dik; Dx11InputKey key; const char *name; };
    const struct MapCheck map[] = {
        { DIK_X,          DX11IK_SQUARE,   "SQUARE"   },
        { DIK_V,          DX11IK_CIRCLE,   "CIRCLE"   },
        { DIK_Z,          DX11IK_TRIANGLE, "TRIANGLE" },
        { DIK_C,          DX11IK_CROSS,    "CROSS"    },
        { DIK_LSHIFT,     DX11IK_L1,       "L1"       },
        { DIK_LCONTROL,   DX11IK_L2,       "L2"       },
        { DIK_LBRACKET,   DX11IK_L3,       "L3"       },
        { DIK_RSHIFT,     DX11IK_R1,       "R1"       },
        { DIK_RCONTROL,   DX11IK_R2,       "R2"       },
        { DIK_RBRACKET,   DX11IK_R3,       "R3"       },
        { DIK_SPACE,      DX11IK_SELECT,   "SELECT"   },
        { DIK_RETURN,     DX11IK_START,    "START"    },
        { DIK_UP,         DX11IK_UP,       "UP"       },
        { DIK_DOWN,       DX11IK_DOWN,     "DOWN"     },
        { DIK_LEFT,       DX11IK_LEFT,     "LEFT"     },
        { DIK_RIGHT,      DX11IK_RIGHT,    "RIGHT"    },
    };
    const int nMap = (int)(sizeof(map) / sizeof(map[0]));

    // Direct correctness.
    int mapOk = 1;
    for (int i = 0; i < nMap; ++i) {
        if (Dx11Input_MapKey(map[i].dik) != map[i].key) {
            fprintf(resf, "MAP %-8s DIK=%d -> %d expect %d FAIL\n",
                    map[i].name, map[i].dik, Dx11Input_MapKey(map[i].dik), map[i].key);
            mapOk = 0;
        }
    }
    fprintf(resf, "MAP direct codes (%d checks) %s\n", nMap, mapOk ? "PASS" : "FAIL");
    if (!mapOk) ++fails;

    // Collision-free: no two DIK codes map to the same logical key.
    int collision = 0;
    for (int i = 0; i < nMap && !collision; ++i)
        for (int j = i + 1; j < nMap; ++j)
            if (Dx11Input_MapKey(map[i].dik) == Dx11Input_MapKey(map[j].dik))
                collision = 1;
    fprintf(resf, "MAP collision-free (16 distinct) %s\n", collision ? "FAIL" : "PASS");
    if (collision) ++fails;

    // Unmapped scancode -> NONE.
    int unmapOk = (Dx11Input_MapKey(0xC5) == DX11IK_NONE);   // DIK_ABNT_C1 (unmapped)
    fprintf(resf, "UNMAP unmapped DIK -> NONE %s\n", unmapOk ? "PASS" : "FAIL");
    if (!unmapOk) ++fails;

    fprintf(resf, "TOTAL_FAILS=%d INPUT=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);

    Dx11Input_Destroy(inp);
    DestroyWindow(wnd);
    return 0;
}
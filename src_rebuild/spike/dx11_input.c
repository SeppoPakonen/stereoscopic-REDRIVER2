// dx11_input.c — T1.7 DirectInput8 input backend implementation.
//
// See dx11_input.h. DirectInput8 is COM-based: CoInitializeEx +
// DirectInput8Create, then device objects for keyboard / mouse / joystick are
// created, given a data format, a cooperative level and Acquire'd. Polling
// reads each device's state; lost devices are re-acquired and zeroed.

#include "dx11_input.h"

#include <string.h>

struct Dx11Input {
    IDirectInput8        *di8;
    IDirectInputDevice8  *keyboard;
    IDirectInputDevice8  *mouse;
    IDirectInputDevice8  *joy;
};

// Context passed to the joystick enumeration callback.
struct EnumJoyCtx {
    IDirectInput8       *di8;
    HWND                 hwnd;
    IDirectInputDevice8 *out;   // becomes the first usable joystick
};

// Range the axis objects of a joystick (so poll values are in a known range).
static BOOL CALLBACK EnumAxesProc(const DIDEVICEOBJECTINSTANCE *doi, void *pv) {
    IDirectInputDevice8 *dev = (IDirectInputDevice8 *)pv;
    DIPROPRANGE range;
    memset(&range, 0, sizeof(range));
    range.diph.dwSize = sizeof(DIPROPRANGE);
    range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    range.diph.dwObj = doi->dwType;
    range.diph.dwHow = DIPH_BYID;
    range.lMin = -1000;
    range.lMax = 1000;
    dev->SetProperty(DIPROP_RANGE, &range.diph);
    return DIENUM_CONTINUE;
}

// Create + configure the first attached joystick/gamepad.
static BOOL CALLBACK EnumJoyProc(const DIDEVICEINSTANCE *inst, void *pv) {
    struct EnumJoyCtx *c = (struct EnumJoyCtx *)pv;
    if (c->out)
        return DIENUM_STOP;   // already found one

    IDirectInputDevice8 *dev = NULL;
    if (FAILED(c->di8->CreateDevice(inst->guidInstance, &dev, NULL)))
        return DIENUM_CONTINUE;
    if (FAILED(dev->SetDataFormat(&c_dfDIJoystick2))) {
        dev->Release();
        return DIENUM_CONTINUE;
    }
    if (FAILED(dev->SetCooperativeLevel(c->hwnd,
                                        DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))) {
        dev->Release();
        return DIENUM_CONTINUE;
    }
    dev->EnumObjects(EnumAxesProc, dev, DIDFT_AXIS);
    dev->Acquire();   // may fail (no focus) — poll re-acquires later
    c->out = dev;
    return DIENUM_STOP;
}

Dx11Input *Dx11Input_Create(HWND hwnd, Dx11InputResult *outResult)
{
    if (outResult) *outResult = DX11INP_ERR_COM;

    Dx11Input *inp = (Dx11Input *)calloc(1, sizeof(Dx11Input));
    if (!inp) { if (outResult) *outResult = DX11INP_ERR_COM; return NULL; }

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        free(inp);
        if (outResult) *outResult = DX11INP_ERR_COM;
        return NULL;
    }

    if (FAILED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION,
                                  IID_IDirectInput8, (void **)&inp->di8, NULL))) {
        CoUninitialize();
        free(inp);
        if (outResult) *outResult = DX11INP_ERR_CREATE;
        return NULL;
    }

    // Keyboard.
    if (FAILED(inp->di8->CreateDevice(GUID_SysKeyboard, &inp->keyboard, NULL)) ||
        FAILED(inp->keyboard->SetDataFormat(&c_dfDIKeyboard)) ||
        FAILED(inp->keyboard->SetCooperativeLevel(hwnd,
                                                  DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))) {
        if (inp->keyboard) inp->keyboard->Release();
        inp->keyboard = NULL;
        inp->di8->Release();
        CoUninitialize();
        free(inp);
        if (outResult) *outResult = DX11INP_ERR_KEYBOARD;
        return NULL;
    }
    inp->keyboard->Acquire();

    // Mouse.
    if (FAILED(inp->di8->CreateDevice(GUID_SysMouse, &inp->mouse, NULL)) ||
        FAILED(inp->mouse->SetDataFormat(&c_dfDIMouse)) ||
        FAILED(inp->mouse->SetCooperativeLevel(hwnd,
                                               DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))) {
        if (inp->mouse) inp->mouse->Release();
        inp->mouse = NULL;
        inp->di8->Release();
        inp->keyboard->Release();
        CoUninitialize();
        free(inp);
        if (outResult) *outResult = DX11INP_ERR_MOUSE;
        return NULL;
    }
    inp->mouse->Acquire();

    // Joystick (optional — a missing gamepad is not an error).
    {
        struct EnumJoyCtx jc;
        memset(&jc, 0, sizeof(jc));
        jc.di8 = inp->di8;
        jc.hwnd = hwnd;
        inp->di8->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoyProc, &jc,
                              DIEDFL_ATTACHEDONLY);
        inp->joy = jc.out;
    }

    if (outResult) *outResult = DX11INP_OK;
    return inp;
}

void Dx11Input_Destroy(Dx11Input *inp)
{
    if (!inp) return;
    if (inp->joy) { inp->joy->Unacquire(); inp->joy->Release(); }
    if (inp->mouse) { inp->mouse->Unacquire(); inp->mouse->Release(); }
    if (inp->keyboard) { inp->keyboard->Unacquire(); inp->keyboard->Release(); }
    if (inp->di8) inp->di8->Release();
    CoUninitialize();
    free(inp);
}

int Dx11Input_HasJoystick(Dx11Input *inp)
{
    return inp && inp->joy != NULL;
}

void Dx11Input_Poll(Dx11Input *inp, Dx11InputState *state)
{
    memset(state, 0, sizeof(*state));
    if (!inp) return;

    if (inp->keyboard) {
        HRESULT hr = inp->keyboard->GetDeviceState(256, state->keys);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            inp->keyboard->Acquire();
            memset(state->keys, 0, 256);
        }
    }

    if (inp->mouse) {
        DIMOUSESTATE ms;
        memset(&ms, 0, sizeof(ms));
        HRESULT hr = inp->mouse->GetDeviceState(sizeof(DIMOUSESTATE), &ms);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            inp->mouse->Acquire();
            memset(&ms, 0, sizeof(ms));
        }
        state->mouseX = ms.lX;
        state->mouseY = ms.lY;
        state->mouseZ = ms.lZ;
        state->mouseButtons = (unsigned char)ms.rgbButtons[0];
    }

    if (inp->joy) {
        DIJOYSTATE2 js;
        HRESULT hr = inp->joy->GetDeviceState(sizeof(DIJOYSTATE2), &js);
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
            inp->joy->Acquire();
            memset(&js, 0, sizeof(js));
        }
        for (int i = 0; i < (int)DX11INP_MAX_JOY_BUTTONS && i < 128; ++i)
            state->joyButtons[i] = (js.rgbButtons[i] & 0x80) ? 1 : 0;
        state->joyAxis[0] = js.lX;
        state->joyAxis[1] = js.lY;
        state->joyAxis[2] = js.lZ;
        state->joyAxis[3] = js.lRx;
        state->joyAxis[4] = js.lRy;
        state->joyAxis[5] = js.lRz;
        state->joyConnected = 1;
    }
}

Dx11InputKey Dx11Input_MapKey(int dik)
{
    switch (dik) {
        case DIK_X:          return DX11IK_SQUARE;
        case DIK_V:          return DX11IK_CIRCLE;
        case DIK_Z:          return DX11IK_TRIANGLE;
        case DIK_C:          return DX11IK_CROSS;
        case DIK_LSHIFT:     return DX11IK_L1;
        case DIK_LCONTROL:   return DX11IK_L2;
        case DIK_LBRACKET:   return DX11IK_L3;
        case DIK_RSHIFT:     return DX11IK_R1;
        case DIK_RCONTROL:   return DX11IK_R2;
        case DIK_RBRACKET:   return DX11IK_R3;
        case DIK_SPACE:      return DX11IK_SELECT;
        case DIK_RETURN:     return DX11IK_START;
        case DIK_UP:         return DX11IK_UP;
        case DIK_DOWN:       return DX11IK_DOWN;
        case DIK_LEFT:       return DX11IK_LEFT;
        case DIK_RIGHT:      return DX11IK_RIGHT;
        case DIK_ESCAPE:     return DX11IK_ESC;
        case DIK_BACK:       return DX11IK_BACKSPACE;
        case DIK_F1:         return DX11IK_F1;
        case DIK_F2:         return DX11IK_F2;
        case DIK_F12:        return DX11IK_F12;
        default:             return DX11IK_NONE;
    }
}
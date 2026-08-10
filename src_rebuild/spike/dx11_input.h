// dx11_input.h — T1.7 DirectInput8 input backend.
//
// The DX11 renderer's native input layer (the SDL2 input in PsyX_main.cpp stays
// for the GL backends). Game-agnostic: it exposes a raw poll state (keyboard
// scan array, mouse relative + buttons, joystick buttons + axes) and a DIK ->
// logical-key map mirroring the game's default PSX pad controls
// (PsyCross/src/PsyX_main.cpp). The game shim later maps logical keys to the
// game's Pads[].
//
// Devices are created with DISCL_NONEXCLUSIVE | DISCL_BACKGROUND (no window
// focus needed) and Acquire'd; a missing joystick/gamepad is not an error.

#ifndef DX11_INPUT_H
#define DX11_INPUT_H

#include <windows.h>
#include <dinput.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the DirectInput8 backend.
typedef struct Dx11Input Dx11Input;

// Logical keys (the game's default PSX pad controls + a few extras), mapped
// from DIK scancodes by Dx11Input_MapKey.
typedef enum {
    DX11IK_NONE = 0,
    DX11IK_SQUARE,          // DIK_X
    DX11IK_CIRCLE,          // DIK_V
    DX11IK_TRIANGLE,        // DIK_Z
    DX11IK_CROSS,           // DIK_C
    DX11IK_L1,              // DIK_LSHIFT
    DX11IK_L2,              // DIK_LCONTROL
    DX11IK_L3,              // DIK_LBRACKET
    DX11IK_R1,              // DIK_RSHIFT
    DX11IK_R2,              // DIK_RCONTROL
    DX11IK_R3,              // DIK_RBRACKET
    DX11IK_SELECT,          // DIK_SPACE
    DX11IK_START,           // DIK_RETURN
    DX11IK_UP,              // DIK_UP
    DX11IK_DOWN,            // DIK_DOWN
    DX11IK_LEFT,            // DIK_LEFT
    DX11IK_RIGHT,           // DIK_RIGHT
    DX11IK_ESC,             // DIK_ESCAPE
    DX11IK_BACKSPACE,       // DIK_BACK
    DX11IK_ENTER,           // DIK_RETURN (alias, unused by default map)
    DX11IK_SPACE,           // DIK_SPACE (alias, unused by default map)
    DX11IK_F1,              // DIK_F1
    DX11IK_F2,              // DIK_F2
    DX11IK_F12,             // DIK_F12
    DX11IK_COUNT
} Dx11InputKey;

#define DX11INP_MAX_JOY_BUTTONS 32
#define DX11INP_MAX_AXES 6

// Polled input state (one call to Dx11Input_Poll fills it).
typedef struct {
    // Keyboard: raw 256-byte DirectInput scan state (1 = down).
    unsigned char keys[256];
    // Mouse: relative deltas this poll + button bitmask (bits 0..7).
    int mouseX, mouseY, mouseZ;
    unsigned char mouseButtons;
    // First attached joystick/gamepad. joyConnected = 0 if none acquired.
    unsigned char joyButtons[DX11INP_MAX_JOY_BUTTONS];
    int joyAxis[DX11INP_MAX_AXES];   // -1000..1000 (ranged)
    int joyConnected;
} Dx11InputState;

typedef enum {
    DX11INP_OK = 0,
    DX11INP_ERR_COM,        // CoInitializeEx failed
    DX11INP_ERR_CREATE,     // DirectInput8Create failed
    DX11INP_ERR_KEYBOARD,   // keyboard device create/setup failed
    DX11INP_ERR_MOUSE,      // mouse device create/setup failed
} Dx11InputResult;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
// CoInitializeEx + DirectInput8Create, then create + configure the keyboard,
// mouse and (optionally) the first attached joystick. `hwnd` is used for
// SetCooperativeLevel (may be a hidden window). Returns NULL on failure;
// *outResult carries the code. A missing joystick is NOT an error.
Dx11Input *Dx11Input_Create(HWND hwnd, Dx11InputResult *outResult);

void Dx11Input_Destroy(Dx11Input *inp);

// ---------------------------------------------------------------------------
// Poll
// ---------------------------------------------------------------------------
// GetDeviceState for each device into `state`. On DIERR_INPUTLOST /
// DIERR_NOTACQUIRED the device is re-Acquire'd and its region zeroed.
void Dx11Input_Poll(Dx11Input *inp, Dx11InputState *state);

// Whether a joystick/gamepad device was created (attached at Create time).
int Dx11Input_HasJoystick(Dx11Input *inp);

// ---------------------------------------------------------------------------
// Mapping
// ---------------------------------------------------------------------------
// Map a DirectInput DIK_* scancode to a logical key (DX11IK_NONE if unmapped).
Dx11InputKey Dx11Input_MapKey(int dik);

#ifdef __cplusplus
}
#endif

#endif // DX11_INPUT_H
// dx11_audio.h — T1.8 XAudio2 audio backend.
//
// The DX11 renderer's native audio layer (OpenAL in PsyX_SPUAL.cpp stays for
// the GL backends). Game-agnostic: it exposes the same sound-source primitives
// the OpenAL path offers (create a source voice for a PCM format, submit raw
// PCM samples with optional looping, play/stop, set volume/pan), so the later
// game shim can map the SPU calls onto it.
//
// XAudio2 2.8 (Windows 8+) uses XAudio2Create, not CoCreateInstance(CLSID_XAudio2).

#ifndef DX11_AUDIO_H
#define DX11_AUDIO_H

#include <xaudio2.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handles.
typedef struct Dx11Audio Dx11Audio;
typedef struct Dx11AudioVoice Dx11AudioVoice;

typedef enum {
    DX11AUDIO_OK = 0,
    DX11AUDIO_ERR_COM,       // CoInitializeEx failed
    DX11AUDIO_ERR_CREATE,    // XAudio2Create failed
    DX11AUDIO_ERR_MASTER,    // CreateMasteringVoice failed (no output device)
} Dx11AudioResult;

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------
// CoInitializeEx + XAudio2Create + CreateMasteringVoice (stereo, 44100). On a
// machine with no audio output device, CreateMasteringVoice fails but the
// engine is still returned (HasOutput() == 0); the caller can then skip sound.
Dx11Audio *Dx11Audio_Create(Dx11AudioResult *outResult);

// 1 if a mastering voice (audio output) is available.
int Dx11Audio_HasOutput(Dx11Audio *a);

void Dx11Audio_Destroy(Dx11Audio *a);

// ---------------------------------------------------------------------------
// Source voices (one per sound)
// ---------------------------------------------------------------------------
// Create a source voice for a PCM format. Returns 0 on success.
int Dx11Audio_CreateVoice(Dx11Audio *a, int channels, int sampleRate,
                          Dx11AudioVoice **out);

void Dx11Audio_DestroyVoice(Dx11AudioVoice *v);

// Queue raw 16-bit PCM samples (pcmBytes bytes). `loop` != 0 loops infinitely.
int Dx11Audio_SubmitBuffer(Dx11AudioVoice *v, const void *pcm, int pcmBytes,
                           int loop);

int Dx11Audio_Play(Dx11AudioVoice *v);
void Dx11Audio_Stop(Dx11AudioVoice *v);

// 1 if the voice is currently playing (SourceState.Playing).
int Dx11Audio_IsPlaying(Dx11AudioVoice *v);

// Volume 0..1.
void Dx11Audio_SetVolume(Dx11AudioVoice *v, float vol);

// Pan -1 (full left) .. +1 (full right), via the 2-channel output matrix.
void Dx11Audio_SetPan(Dx11AudioVoice *v, float pan);

#ifdef __cplusplus
}
#endif

#endif // DX11_AUDIO_H
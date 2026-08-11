// dx11_audio.c — T1.8 XAudio2 audio backend implementation.
//
// See dx11_audio.h. XAudio2 2.8: XAudio2Create + CreateMasteringVoice, then a
// source voice per sound with a WAVEFORMATEX PCM format; raw 16-bit PCM samples
// are queued via SubmitSourceBuffer and started/stopped, with volume via
// SetVolume and pan via the 2-channel output matrix.

#include "dx11_audio.h"

#include <stdlib.h>
#include <string.h>

struct Dx11Audio {
    IXAudio2               *xa;
    IXAudio2MasteringVoice *master;
    int hasOutput;
};

struct Dx11AudioVoice {
    IXAudio2SourceVoice *voice;
    int channels;
};

Dx11Audio *Dx11Audio_Create(Dx11AudioResult *outResult)
{
    if (outResult) *outResult = DX11AUDIO_ERR_COM;

    Dx11Audio *a = (Dx11Audio *)calloc(1, sizeof(Dx11Audio));
    if (!a) { if (outResult) *outResult = DX11AUDIO_ERR_COM; return NULL; }

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        free(a);
        if (outResult) *outResult = DX11AUDIO_ERR_COM;
        return NULL;
    }

    if (FAILED(XAudio2Create(&a->xa, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
        CoUninitialize();
        free(a);
        if (outResult) *outResult = DX11AUDIO_ERR_CREATE;
        return NULL;
    }

    // Stereo 44100 mastering voice. May fail on a machine with no audio output.
    if (SUCCEEDED(a->xa->CreateMasteringVoice(&a->master, 2, 44100, 0, 0, NULL))) {
        a->hasOutput = 1;
    } else {
        a->master = NULL;
        a->hasOutput = 0;
    }

    if (outResult) *outResult = DX11AUDIO_OK;
    return a;
}

int Dx11Audio_HasOutput(Dx11Audio *a)
{
    return a && a->hasOutput;
}

void Dx11Audio_Destroy(Dx11Audio *a)
{
    if (!a) return;
    if (a->master) a->master->DestroyVoice();
    if (a->xa) a->xa->Release();
    CoUninitialize();
    free(a);
}

int Dx11Audio_CreateVoice(Dx11Audio *a, int channels, int sampleRate,
                          Dx11AudioVoice **out)
{
    if (!a || !out) return 1;
    *out = NULL;
    if (channels < 1 || channels > 2 || sampleRate <= 0)
        return 1;

    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = (WORD)channels;
    wf.nSamplesPerSec = (DWORD)sampleRate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = (WORD)(channels * 2);
    wf.nAvgBytesPerSec = (DWORD)(sampleRate * channels * 2);
    wf.cbSize = 0;

    IXAudio2SourceVoice *sv = NULL;
    if (FAILED(a->xa->CreateSourceVoice(&sv, &wf, 0, XAUDIO2_DEFAULT_FREQ_RATIO,
                                        NULL, NULL, NULL)))
        return 1;

    Dx11AudioVoice *v = (Dx11AudioVoice *)calloc(1, sizeof(Dx11AudioVoice));
    if (!v) { sv->DestroyVoice(); return 1; }
    v->voice = sv;
    v->channels = channels;
    *out = v;
    return 0;
}

void Dx11Audio_DestroyVoice(Dx11AudioVoice *v)
{
    if (!v) return;
    if (v->voice) v->voice->DestroyVoice();
    free(v);
}

int Dx11Audio_SubmitBuffer(Dx11AudioVoice *v, const void *pcm, int pcmBytes,
                           int loop)
{
    if (!v || !v->voice || !pcm || pcmBytes <= 0)
        return 1;
    XAUDIO2_BUFFER buf;
    memset(&buf, 0, sizeof(buf));
    buf.AudioBytes = (UINT32)pcmBytes;
    buf.pAudioData = (const BYTE *)pcm;
    buf.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;
    return FAILED(v->voice->SubmitSourceBuffer(&buf)) ? 1 : 0;
}

int Dx11Audio_Play(Dx11AudioVoice *v)
{
    if (!v || !v->voice) return 1;
    return FAILED(v->voice->Start(0, XAUDIO2_COMMIT_NOW)) ? 1 : 0;
}

void Dx11Audio_Stop(Dx11AudioVoice *v)
{
    if (v && v->voice) v->voice->Stop(0, XAUDIO2_COMMIT_NOW);
}

// 1 if the voice has audio in flight (a queued buffer the engine hasn't
// consumed). XAUDIO2 has no direct "playing" flag; BuffersQueued drops to 0
// once the submitted buffer has finished.
int Dx11Audio_IsPlaying(Dx11AudioVoice *v)
{
    XAUDIO2_VOICE_STATE st;
    memset(&st, 0, sizeof(st));
    if (v && v->voice) v->voice->GetState(&st);
    return st.BuffersQueued > 0;
}

void Dx11Audio_SetVolume(Dx11AudioVoice *v, float vol)
{
    if (v && v->voice) v->voice->SetVolume(vol < 0.0f ? 0.0f : (vol > 1.0f ? 1.0f : vol));
}

void Dx11Audio_SetPan(Dx11AudioVoice *v, float pan)
{
    if (!v || !v->voice) return;
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    // 2-channel output matrix (mono source: [in0->out0, in0->out1]).
    float m[4];
    if (v->channels == 1) {
        float left  = (pan <= 0.0f) ? 1.0f : (1.0f - pan);
        float right = (pan >= 0.0f) ? 1.0f : (1.0f + pan);
        m[0] = left; m[1] = right;
        v->voice->SetOutputMatrix(NULL, 1, 2, m, 0);
    } else {
        // stereo passthrough
        m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; m[3] = 1.0f;
        v->voice->SetOutputMatrix(NULL, 2, 2, m, 0);
    }
}
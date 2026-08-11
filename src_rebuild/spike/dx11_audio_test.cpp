// dx11_audio_test.cpp — T1.8 XAudio2 audio backend harness.
//
// Verifies headless that the XAudio2 module initializes an engine + mastering
// voice, creates a source voice for a PCM format, plays a PCM buffer (voice
// state), and handles volume/pan without crashing. If the machine has no audio
// output device the play probes are SKIPped (the engine create still PASSes).
//
//   * ENGINE  — CoInitializeEx + XAudio2Create succeed (DX11AUDIO_OK);
//   * VOICE   — CreateSourceVoice for a mono 44100 PCM format succeeds;
//   * PLAY    — a submitted 0.5 s sine buffer is in flight after Start
//               (BuffersQueued > 0), then drains after it finishes;
//   * VOLPAN  — SetVolume/SetPan on a second voice don't crash;
//   * DESTROY — voices + engine are released cleanly.

#define WIN32_LEAN_AND_MEAN
#include "dx11_audio.h"

#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    FILE *resf = fopen("dx11_audio_result.txt", "w");
    if (!resf) return 2;
    int fails = 0;

    // ------------------------------------------------------------------
    // ENGINE
    // ------------------------------------------------------------------
    Dx11AudioResult ar;
    Dx11Audio *a = Dx11Audio_Create(&ar);
    if (!a) {
        fprintf(resf, "ENGINE create failed (result=%d) FAIL\n", ar);
        ++fails;
        fclose(resf);
        return 1;
    }
    int okEngine = (ar == DX11AUDIO_OK);
    fprintf(resf, "ENGINE create (result=%d expect 0) %s\n", ar, okEngine ? "PASS" : "FAIL");
    if (!okEngine) ++fails;

    int hasOut = Dx11Audio_HasOutput(a);
    fprintf(resf, "ENGINE mastering voice %s\n", hasOut ? "present" : "absent");

    if (!hasOut) {
        fprintf(resf, "PLAY  no audio output device -> SKIP\n");
        fprintf(resf, "VOLPAN no audio output device -> SKIP\n");
        fprintf(resf, "TOTAL_FAILS=%d AUDIO=%s (SKIP play)\n", fails, fails == 0 ? "PASS" : "FAIL");
        fclose(resf);
        Dx11Audio_Destroy(a);
        return 0;
    }

    // ------------------------------------------------------------------
    // VOICE + PLAY — a 0.5 s mono 44100 Hz 16-bit sine wave.
    // ------------------------------------------------------------------
    const int nSamples = 22050;   // 0.5 s
    short *sine = (short *)malloc((size_t)nSamples * 2);
    if (!sine) { ++fails; fprintf(resf, "PLAY alloc FAIL\n"); fclose(resf); Dx11Audio_Destroy(a); return 1; }
    for (int i = 0; i < nSamples; ++i)
        sine[i] = (short)(32000.0 * sin(2.0 * 3.14159265 * 440.0 * i / 44100.0));

    Dx11AudioVoice *v = NULL;
    int okVoice = (Dx11Audio_CreateVoice(a, 1, 44100, &v) == 0);
    fprintf(resf, "VOICE mono 44100 create %s\n", okVoice ? "PASS" : "FAIL");
    if (!okVoice) ++fails;

    int okSubmit = 0, okPlay = 0, okPlaying = 0, okDrained = 0;
    if (okVoice) {
        okSubmit = (Dx11Audio_SubmitBuffer(v, sine, nSamples * 2, 0) == 0);
        fprintf(resf, "PLAY submit sine buffer %s\n", okSubmit ? "PASS" : "FAIL");
        if (!okSubmit) ++fails;

        okPlay = (Dx11Audio_Play(v) == 0);
        fprintf(resf, "PLAY start %s\n", okPlay ? "PASS" : "FAIL");
        if (!okPlay) ++fails;

        // Immediately after Start the buffer should be in flight.
        okPlaying = Dx11Audio_IsPlaying(v);
        fprintf(resf, "PLAY buffer in flight after Start %s\n", okPlaying ? "PASS" : "FAIL");
        if (!okPlaying) ++fails;

        // Drain: wait past the 0.5 s buffer; BuffersQueued should hit 0.
        Sleep(900);
        okDrained = !Dx11Audio_IsPlaying(v);
        fprintf(resf, "PLAY buffer drained after 0.9 s %s\n", okDrained ? "PASS" : "FAIL");
        if (!okDrained) ++fails;
    }

    // ------------------------------------------------------------------
    // VOLPAN — a second voice, no crash.
    // ------------------------------------------------------------------
    Dx11AudioVoice *v2 = NULL;
    int okVolPan = (Dx11Audio_CreateVoice(a, 1, 44100, &v2) == 0);
    if (okVolPan) {
        Dx11Audio_SubmitBuffer(v2, sine, nSamples * 2, 0);
        Dx11Audio_SetVolume(v2, 0.5f);
        Dx11Audio_SetPan(v2, -0.5f);
        Dx11Audio_Play(v2);
        Sleep(50);
        Dx11Audio_Stop(v2);
    }
    fprintf(resf, "VOLPAN set volume/pan (voice create %s) %s\n",
            okVolPan ? "ok" : "failed", okVolPan ? "PASS" : "FAIL");
    if (!okVolPan) ++fails;

    // ------------------------------------------------------------------
    // DESTROY
    // ------------------------------------------------------------------
    if (v2) Dx11Audio_DestroyVoice(v2);
    if (v) Dx11Audio_DestroyVoice(v);
    Dx11Audio_Destroy(a);
    free(sine);
    fprintf(resf, "DESTROY voices + engine clean %s\n", "PASS");

    fprintf(resf, "TOTAL_FAILS=%d AUDIO=%s\n", fails, fails == 0 ? "PASS" : "FAIL");
    fclose(resf);
    return 0;
}
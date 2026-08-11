// drawcmd.c — per-frame draw-command arena (T0.2)
//
// Growable contiguous buffer of DrawCommand. Reset each frame by BeginFrame;
// Submit appends copies and returns a stable index (realloc preserves indices).
// A hard growth cap bounds memory; the capacity grows geometrically up to it.

#include "drawcmd.h"

#include <stdlib.h>

#define DRAWCMD_INITIAL_CAPACITY 1024
#define DRAWCMD_MAX_CAPACITY      (1 << 20)   // 1M commands, upper bound

static DrawCommand *s_cmd;
static int s_count;
static int s_capacity;

void DrawCmd_BeginFrame(void)
{
    s_count = 0;
}

int DrawCmd_Submit(const DrawCommand *cmd)
{
    if (!cmd)
        return -1;

    if (s_count >= s_capacity)
    {
        int newCap = s_capacity ? s_capacity * 2 : DRAWCMD_INITIAL_CAPACITY;
        if (newCap > DRAWCMD_MAX_CAPACITY)
            return -1;                              // arena full

        DrawCommand *grown = (DrawCommand *)realloc(s_cmd, (size_t)newCap * sizeof(DrawCommand));
        if (!grown)
            return -1;                              // OOM
        s_cmd = grown;
        s_capacity = newCap;
    }

    s_cmd[s_count] = *cmd;
    return s_count++;
}

int DrawCmd_Count(void)
{
    return s_count;
}

const DrawCommand *DrawCmd_At(int index)
{
    if (index < 0 || index >= s_count)
        return NULL;
    return &s_cmd[index];
}

const DrawCommand *DrawCmd_Data(void)
{
    return s_cmd;
}
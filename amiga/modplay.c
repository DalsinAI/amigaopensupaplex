/*
 * A small ProTracker (M.K., 4 channels) replayer that mixes into a 16-bit
 * mono buffer. It knows the effects OpenSupaplex's tunes use: set speed
 * and tempo (Fxx), pattern break (Dxx), set volume (Cxx) and volume slide
 * (Axy). Other effects are ignored.
 *
 * The AmiCradle Team. GPL 3, like OpenSupaplex.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "modplay.h"

#define PAULA_CLOCK 3546895UL /* PAL: 7093789.2 / 2 */

typedef struct {
    const int8_t *data;
    uint32_t length;      /* bytes */
    uint32_t loopStart;
    uint32_t loopLength;  /* bytes; 2 or less: no loop */
    uint8_t volume;
} ModSample;

typedef struct {
    const ModSample *sample;
    uint32_t pos;         /* 16.16 */
    uint32_t step;        /* 16.16 */
    uint16_t period;
    int volume;
    uint8_t effect, param;
} ModChannel;

static uint8_t *gMod;
static ModSample gSamples[31];
static const uint8_t *gPatterns;
static uint8_t gOrder[128];
static int gSongLength;
static ModChannel gChan[4];
static int gRate = 22050;
static int gPlaying;
static int gSpeed, gBpm, gTick, gRow, gOrderPos, gBreakRow;
static uint32_t gTickLeft; /* output samples left in this tick */

static uint32_t be16(const uint8_t *p) { return ((uint32_t)p[0] << 8) | p[1]; }

void modFree(void)
{
    gPlaying = 0;
    free(gMod);
    gMod = NULL;
}

int modLoad(const char *path, int rate)
{
    FILE *f;
    long size;
    int i, patterns = 0;
    const uint8_t *p;

    modFree();
    gRate = rate;
    f = fopen(path, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    gMod = malloc(size);
    if (!gMod || fread(gMod, 1, size, f) != (size_t)size || size < 1084 ||
        memcmp(gMod + 1080, "M.K.", 4))
    {
        fclose(f);
        modFree();
        return -1;
    }
    fclose(f);

    gSongLength = gMod[950];
    memcpy(gOrder, gMod + 952, 128);
    for (i = 0; i < 128; i++)
        if (gOrder[i] + 1 > patterns)
            patterns = gOrder[i] + 1;
    gPatterns = gMod + 1084;
    p = gPatterns + patterns * 1024;
    for (i = 0; i < 31; i++)
    {
        const uint8_t *h = gMod + 20 + i * 30;
        ModSample *s = &gSamples[i];
        s->length = be16(h + 22) * 2;
        s->volume = h[25] > 64 ? 64 : h[25];
        s->loopStart = be16(h + 26) * 2;
        s->loopLength = be16(h + 28) * 2;
        s->data = (const int8_t *)p;
        if (p + s->length > gMod + size)
            s->length = (uint32_t)(gMod + size - p);
        if (s->loopStart + s->loopLength > s->length)
            s->loopLength = 0;
        p += s->length;
    }
    return 0;
}

void modStart(void)
{
    if (!gMod)
        return;
    memset(gChan, 0, sizeof(gChan));
    gSpeed = 6;
    gBpm = 125;
    gTick = 0;
    gRow = 0;
    gOrderPos = 0;
    gBreakRow = -1;
    gTickLeft = 0;
    gPlaying = 1;
}

void modStop(void)
{
    gPlaying = 0;
}

void modResume(void)
{
    if (gMod)
        gPlaying = 1;
}

static void setPeriod(ModChannel *c, uint16_t period)
{
    c->period = period;
    c->step = period ? (uint32_t)(((uint64_t)PAULA_CLOCK << 16) / period / gRate) : 0;
}

static void playRow(void)
{
    const uint8_t *row = gPatterns + gOrder[gOrderPos] * 1024 + gRow * 16;
    int ch;

    for (ch = 0; ch < 4; ch++)
    {
        const uint8_t *n = row + ch * 4;
        ModChannel *c = &gChan[ch];
        int ins = (n[0] & 0xf0) | (n[2] >> 4);
        uint16_t period = (uint16_t)(((n[0] & 0x0f) << 8) | n[1]);

        c->effect = n[2] & 0x0f;
        c->param = n[3];
        if (ins >= 1 && ins <= 31)
        {
            c->sample = &gSamples[ins - 1];
            c->volume = c->sample->volume;
        }
        if (period && c->sample)
        {
            setPeriod(c, period);
            c->pos = 0;
        }
        switch (c->effect)
        {
            case 0xC: c->volume = c->param > 64 ? 64 : c->param; break;
            case 0xD: gBreakRow = (c->param >> 4) * 10 + (c->param & 0x0f); break;
            case 0xF:
                if (c->param == 0) break;
                if (c->param < 0x20) gSpeed = c->param; else gBpm = c->param;
                break;
            default: break;
        }
    }
}

static void nextRow(void)
{
    if (gBreakRow >= 0 || ++gRow >= 64)
    {
        gRow = gBreakRow >= 0 && gBreakRow < 64 ? gBreakRow : 0;
        gBreakRow = -1;
        if (++gOrderPos >= gSongLength)
            gOrderPos = 0;
    }
}

static void tick(void)
{
    int ch;

    if (gTick == 0)
        playRow();
    else
        for (ch = 0; ch < 4; ch++)
        {
            ModChannel *c = &gChan[ch];
            if (c->effect == 0xA)
            {
                if (c->param >> 4) c->volume += c->param >> 4;
                else c->volume -= c->param & 0x0f;
                if (c->volume < 0) c->volume = 0;
                if (c->volume > 64) c->volume = 64;
            }
        }
    if (++gTick >= gSpeed)
    {
        gTick = 0;
        nextRow();
    }
    gTickLeft = (uint32_t)gRate * 5 / (gBpm * 2);
}

void modMix(int16_t *out, unsigned len, int volume)
{
    unsigned i;
    int ch;

    if (!gPlaying || volume <= 0)
        return;
    while (len)
    {
        unsigned n;
        if (gTickLeft == 0)
            tick();
        n = len < gTickLeft ? len : gTickLeft;
        for (ch = 0; ch < 4; ch++)
        {
            ModChannel *c = &gChan[ch];
            const ModSample *s = c->sample;
            int vol = c->volume * volume;
            if (!s || !s->length || !c->step || !vol)
                continue;
            for (i = 0; i < n; i++)
            {
                uint32_t at = c->pos >> 16;
                uint32_t end = s->loopLength > 2 ? s->loopStart + s->loopLength : s->length;
                int v;
                if (at >= end)
                {
                    if (s->loopLength > 2)
                    {
                        c->pos -= s->loopLength << 16;
                        at = c->pos >> 16;
                    }
                    else
                    {
                        c->step = 0;
                        break;
                    }
                }
                v = out[i] + ((s->data[at] * vol) >> 4);
                out[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
                c->pos += c->step;
            }
        }
        out += n;
        len -= n;
        gTickLeft -= n;
    }
}

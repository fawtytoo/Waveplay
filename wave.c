//  Copyright (C)       2026 Steve Clark

//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.

//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:

//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required. 
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//  3. This notice may not be removed or altered from any source distribution.

#include "wave.h"

#define LE16(i)             ((i)[0] | ((i)[1] << 8))
#define LE32(i)             (LE16(i) | (LE16(i + 2) << 16))

// wave ------------------------------------------------------------------------
#define WAVE_HDRSIZE        12
#define WAVE_FMTSIZE        sizeof(FORMAT)

#define FMT_PCM             0x0001
#define FMT_EXTENSIBLE      0xfffe

#define SIZE(s)             (s + (s & 1)) //((s + 1) & 0xfffffffe)

#define L                   0
#define R                   1

// timer -----------------------------------------------------------------------
typedef struct
{
    int     rate;
    int     acc;
    int     remainder;
    int     divisor;
}
TIMER;

static TIMER    timeTimer = {0, 0, 10, SAMPLERATE}, channelTimer, sampleTimer;

// wave ------------------------------------------------------------------------
typedef struct
{
    u16     format;         // see FMT_* above
    u16     channels;
    u32     samplerate;
    u32     bytespersec;    // samplerate * channels * bitspersample / 8
    u16     blockalign;     // channels * bitspersample / 8
    u16     bitspersample;  // bit depth
}
FORMAT;

typedef struct
{
    char    id[4];
    u8      size[4];
}
CHUNK;

static FORMAT       waveFormat;
static CHUNK        chunkDummy = {.id = "    ", .size = {0, 0, 0, 0}};
static CHUNK        *waveData = &chunkDummy;
static CHUNK        *waveTitle = &chunkDummy;

static u8           *dataPos, *dataEnd;
static int          waveSampleSize;

static int          waveTime = 0;

static bool         waveLoaded = false;
static bool         wavePlaying = false;

 // misc -----------------------------------------------------------------------
static bool ID(void *ptr, char *check)
{
    char    *id = (char *)ptr;

    while (*check)
    {
        if (*id++ != *check++)
        {
            return false;
        }
    }

    return true;
}

// timer -----------------------------------------------------------------------
static int Timer_Update(TIMER *timer)
{
    timer->acc += timer->remainder;
    if (timer->acc < timer->divisor)
    {
        return timer->rate;
    }

    timer->acc -= timer->divisor;

    return timer->rate + 1;
}

static void Timer_Set(TIMER *timer, int numerator, int divisor)
{
    timer->acc = 0;
    timer->rate = numerator / divisor;
    timer->remainder = numerator - timer->rate * divisor;
    timer->divisor = divisor;
}

// wave ------------------------------------------------------------------------
void Wave_Reset()
{
    waveTime = 0;
    dataPos = (u8 *)(waveData) + sizeof(CHUNK);
}

char *Wave_Title(char *name, int *length)
{
    if (LE32(waveTitle->size) > 0)
    {
        *length = LE32(waveTitle->size);
        return (char *)(waveTitle) + sizeof(CHUNK);
    }

    return name;
}

int Wave_Time()
{
    return waveTime;
}

bool Wave_IsPlaying()
{
    return dataPos < dataEnd ? true : false;
}

void Wave_Play(bool playing)
{
    if (dataPos == dataEnd)
    {
        Wave_Reset();
    }

    wavePlaying = playing & waveLoaded;
}

static void Get_Sample(u8 *data, int *sample)
{
    short       out = 0;

    switch (waveSampleSize)
    {
      case 3:
        data++;
        // FALLTHRU
      case 2:
        out |= *data++;
        // FALLTHRU
      case 1:
        out |= (*data << 8);
        break;
    }

    *sample += out;
}

void Wave_Sample(short sample[2])
{
    int     acc[2] = {0, 0};
    int     rate, divisor = 1;

    if (wavePlaying)
    {
        rate = Timer_Update(&sampleTimer);
        divisor = 0;

        do
        {
            Get_Sample(dataPos, &acc[L]);
            dataPos += Timer_Update(&channelTimer) * waveSampleSize;
            Get_Sample(dataPos, &acc[R]);
            dataPos += Timer_Update(&channelTimer) * waveSampleSize;

            divisor++;
        }
        while (--rate > 0);

        waveTime += Timer_Update(&timeTimer);

        wavePlaying = Wave_IsPlaying();
    }

    sample[L] = acc[L] / divisor;
    sample[R] = acc[R] / divisor;
}

bool Wave_Load(char *wave, int size)
{
    CHUNK       *format, *data, *list;
    CHUNK       *chunk;
    char        *start, *end;
    u8          *ptr;

    wavePlaying = false;
    waveLoaded = false;

    if (size < WAVE_HDRSIZE)
    {
        return false;
    }

    chunk = (CHUNK *)wave;
    if (!ID(chunk->id, "RIFF") || LE32(chunk->size) > size - sizeof(CHUNK))
    {
        return false;
    }
    if (!ID(wave + sizeof(CHUNK), "WAVE"))
    {
        return false;
    }

    format = &chunkDummy;
    data = &chunkDummy;
    list = &chunkDummy;

    end = wave + size;
    wave += WAVE_HDRSIZE;

    while (wave < end)
    {
        chunk = (CHUNK *)wave;
        wave += sizeof(CHUNK);
        if (ID(chunk->id, "fmt "))
        {
            format = chunk;
        }
        else if (ID(chunk->id, "data"))
        {
            data = chunk;
        }
        else if (ID(chunk->id, "LIST") && ID(wave, "INFO"))
        {
            list = chunk;
        }

        wave += SIZE(LE32(chunk->size));
    }

    if (LE32(chunk->size) < WAVE_FMTSIZE) // FMT_PCM
    {
        return false;
    }
    ptr = (u8 *)((u8 *)format + sizeof(CHUNK));
    waveFormat.format = LE16(ptr);
    if (waveFormat.format != FMT_PCM && waveFormat.format != FMT_EXTENSIBLE)
    {
        return false;
    }

    waveFormat.channels = LE16(ptr + 2);
    waveFormat.samplerate = LE32(ptr + 4);
    waveFormat.bytespersec = LE32(ptr + 8);
    waveFormat.blockalign = LE16(ptr + 12);
    waveFormat.bitspersample = LE16(ptr + 14);

    if (LE32(data->size) == 0)
    {
        return false;
    }
    waveData = data;

    chunk = list;
    waveTitle = &chunkDummy;

    if (LE32(chunk->size) > sizeof(CHUNK))
    {
        start = (char *)list + sizeof(CHUNK) + 4; // +4 = INFO
        end = start + LE32(chunk->size);
        while (start < end)
        {
            chunk = (CHUNK *)start;
            start += sizeof(CHUNK);
            if (ID(chunk->id, "INAM"))
            {
                waveTitle = chunk;
                break;
            }
            start += SIZE(LE32(chunk->size));
        }
    }

    waveTime = (int)((float)(LE32(waveData->size) / waveFormat.blockalign) / (float)waveFormat.samplerate * 10.0f);
    waveSampleSize = waveFormat.bitspersample / 8;
    dataEnd = (u8 *)(waveData) + sizeof(CHUNK) + LE32(waveData->size);
    dataPos = dataEnd;

    Timer_Set(&channelTimer, 1, waveFormat.channels ^ 3);
    Timer_Set(&sampleTimer, waveFormat.samplerate, SAMPLERATE);

    waveLoaded = true;

    return true;
}

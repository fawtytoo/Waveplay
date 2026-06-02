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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <SDL2/SDL.h>

#include "wave.h"

#define ERROR(s)    printf("\t%s\n\r", s)

#define STATUS      printf("\r[%s%s]", playing ? " " : "P", mixed ? "M" : " ")
#define TIME(c)     printf(" %*i:%02i.%i\33[%iC", count, clock / 600, (clock / 10) % 60, clock % 10, c)

typedef struct stat     stat_t;

static int      *playList[2];
static int      playCount;

static int DoDigits(int number, int count)
{
    if (number > 9)
    {
        count = DoDigits(number / 10, count + 1);
    }

    return count;
}

static void SdlCallback(void *unused, Uint8 *stream, int length)
{
    (void)unused;

    short   *output = (short *)stream;
    short   sample[2], *left = &sample[0], *right = &sample[1];

    length /= sizeof(short);

    while (length)
    {
        Wave_Sample(sample);
        *output++ = *left;
        *output++ = *right;

        length -= 2;
    }
}

static void Mix(u16 seed)
{
    int     i, n, e;

    for (i = 0; i < playCount; i++)
    {
        playList[0][i] = i; // ordered
        playList[1][i] = i; // mixed
    }

    for (i = 0; i < playCount; i++)
    {
        seed ^= seed << 5;
        seed ^= seed >> 9;
        seed ^= seed << 2;

        n = seed % playCount;
        e = playList[1][n];
        playList[1][n] = playList[1][i];
        playList[1][i] = e;
    }
}

static bool FileRead(char *filename, char **buffer, int *size)
{
    stat_t      status;
    FILE        *file;

    *buffer = NULL;
    *size = 0;

    if (stat(filename, &status) < 0 || status.st_size == 0)
    {
        return false;
    }

    if (S_ISDIR(status.st_mode) || !S_ISREG(status.st_mode))
    {
        return false;
    }

    if ((file = fopen(filename, "r")) == NULL)
    {
        return false;
    }

    *buffer = malloc(status.st_size);
    fread(*buffer, 1, status.st_size, file);
    fclose(file);

    *size = status.st_size;

    return true;
}

int main(int argc, char **argv)
{
    SDL_AudioSpec   want;
    int             arg;

    struct termios  termAttr;
    tcflag_t        localMode, outputMode;
    int             blockMode;

    char            key;

    int             pcdigits;

    bool            playing = false;
    bool            mixed = false;

    char            *filename;
    int             filesize;
    char            *buffer[2] = {NULL, NULL};
    bool            nexttrack = false;
    bool            trytrack = false;
    bool            prevtrack = false;

    char            *name, *title = NULL;
    int             length;

    int             count;
    int             clock;

    if (argc < 2)
    {
        return 1;
    }

    printf("P Play/Pause | N Next | B Back | R Replay | M Mix | Q Quit\n");

    blockMode = fcntl(STDIN_FILENO, F_GETFL, FNONBLOCK);
    tcgetattr(STDIN_FILENO, &termAttr);
    localMode = termAttr.c_lflag;
    outputMode = termAttr.c_oflag;
    termAttr.c_lflag &= ~(ICANON | ECHO | ISIG);
    termAttr.c_oflag &= ~ONLCR;
    tcsetattr(STDIN_FILENO, TCSANOW, &termAttr);
    fcntl(STDIN_FILENO, F_SETFL, FNONBLOCK);

    SDL_Init(SDL_INIT_AUDIO);

    want.freq = SAMPLERATE;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 1024;
    want.callback = SdlCallback;

    SDL_OpenAudio(&want, NULL);
    SDL_PauseAudio(SDL_FALSE);

    playCount = argc - 1;
    playList[0] = malloc(sizeof(int) * playCount);
    playList[1] = malloc(sizeof(int) * playCount);
    srand(time(NULL));
    Mix(rand() % 65536);

    pcdigits = DoDigits(playCount, 1);

    for (arg = 0; arg < playCount; arg += (prevtrack ? -1 : 1))
    {
        if (arg == 0)
        {
            prevtrack = false;
        }

        if (nexttrack == false)
        {
            filename = argv[playList[mixed][arg] + 1];

            if (FileRead(filename, &buffer[0], &filesize) == false)
            {
                printf("%*i/%i: %s\r\n", pcdigits, arg + 1, playCount, filename);
                ERROR("Read error");
                continue;
            }
        }

        nexttrack = false;

        if (Wave_Load(buffer[0], filesize) == false)
        {
            printf("%*i/%i: %s\r\n", pcdigits, arg + 1, playCount, filename);
            ERROR("Not a wave file");
            continue;
        }

        prevtrack = false;

        if (buffer[1] != NULL)
        {
            free(buffer[1]);
        }

        buffer[1] = buffer[0];
        buffer[0] = NULL;

        if ((name = strrchr(filename, '/')) == NULL)
        {
            name = filename;
        }
        else
        {
            name++;
        }
        length = strlen(name);
        title = Wave_Title(name, &length);

        printf("%*i/%i: %.*s\r\n", pcdigits, arg + 1, playCount, length, title);
        STATUS;
        clock = Wave_Time();
        count = DoDigits(clock / 600, 1);
        printf(" %*i:00.0 /", count, 0);
        TIME(1);

        if (arg < playCount - 1)
        {
            trytrack = true;
        }
        else
        {
            trytrack = false;
        }

        Wave_Play(playing);

        while (Wave_IsPlaying())
        {
            SDL_Delay(1);

            key = getc(stdin);
            if (key == 'p')
            {
                playing = !playing;
                Wave_Play(playing);
            }
            else if (key == 'n')
            {
                nexttrack = false;
                break;
            }
            else if (key == 'q')
            {
                arg = playCount;
                break;
            }
            else if (key == 'b')
            {
                if (arg > 0)
                {
                    prevtrack = true;
                    nexttrack = false;
                    break;
                }
            }
            else if (key == 'r')
            {
                Wave_Reset();
            }
            else if (key == 'm' && playing == false)
            {
                mixed = !mixed;
                arg--;
                printf("\33[1A");
                nexttrack = false;
                break;
            }

            STATUS;
            clock = Wave_Time();
            TIME(count + 9);

            if (trytrack)
            {
                if (nexttrack == false)
                {
                    filename = argv[playList[mixed][arg + 1] + 1];
                    if (FileRead(filename, &buffer[0], &filesize) == true)
                    {
                        nexttrack = true;
                    }
                }
                // don't want to do this more than once
                // event if FileRead fails
                trytrack = false;
            }
        }

        Wave_Play(false);

        printf("\r\33[K\r");
    }

    if (buffer[0] != NULL)
    {
        free(buffer[0]);
    }

    if (buffer[1] != NULL)
    {
        free(buffer[1]);
    }

    free(playList[0]);
    free(playList[1]);

    SDL_PauseAudio(SDL_TRUE);
    SDL_CloseAudio();
    SDL_Quit();

    termAttr.c_lflag = localMode;
    termAttr.c_oflag = outputMode;
    tcsetattr(STDIN_FILENO, TCSANOW, &termAttr);
    fcntl(STDIN_FILENO, F_SETFL, blockMode);

    return 0;
}

/* SDL Driver for P.E.Op.S Sound Plugin
 * Copyright (c) 2010, Wei Mingzhi <whistler_wmz@users.sf.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA
 */

#include <stdlib.h>
#include <SDL/SDL.h>
#include "out.h"
#include "../coredebug.h"
#include "../Gamecube/DEBUG.h"
#include "../psxcommon.h"

#define BUFFER_SIZE        22050
//#define BUFFER_SIZE        12000

short            *pSndBuffer = NULL;
volatile int    iReadPos = 0, iWritePos = 0;
static int sposTmp = 0x10000L;
static int16_t lastSampleL;
static int16_t lastSampleR;
//extern char audioEnabled;

static void SOUND_FillAudio(void *unused, Uint8 *stream, int len) {
    extern int stop;
    if (stop == 1)
    {
        return;
    }

    int16_t *p = (int16_t *)stream;

    //len >>= 1;
    len >>= 2;

//    while (iReadPos != iWritePos && len > 0) {
//        *p++ = pSndBuffer[iReadPos++];
//        if (iReadPos >= BUFFER_SIZE) iReadPos = 0;
//        --len;
//    }
    // pitch data from 44100 to 48000
    while (iReadPos != iWritePos && len > 0)
    {
        while (sposTmp >= 0x10000L)
        {
            lastSampleL = pSndBuffer[iReadPos++];
            if (iReadPos >= BUFFER_SIZE) iReadPos = 0;
            lastSampleR = pSndBuffer[iReadPos++];
            if (iReadPos >= BUFFER_SIZE) iReadPos = 0;
            sposTmp -= 0x10000L;
        }

        // Because it is in BIG_ENDIAN format, it is necessary to swap the left and right audio channels.
        *p++ = lastSampleR;
        *p++ = lastSampleL;
        sposTmp += SINC;
        --len;
    }

    #ifdef SHOW_DEBUG
    if (len > 0) {
        sprintf(txtbuffer, "Spu Speed slow %d \n", len * 2);
        DEBUG_print(txtbuffer, DBG_SPU2);
    }
    #endif // DISP_DEBUG
}

static void InitSDL() {
    if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    } else {
        SDL_Init(SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE);
    }
}

static void DestroySDL() {
    if (SDL_WasInit(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO)) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    } else {
        SDL_Quit();
    }
}

static int sdl_init(void) {
    SDL_AudioSpec                spec;

    if (pSndBuffer != NULL) return -1;
    //fill_buffer = play_buffer = 0;

    InitSDL();

    spec.freq = WII_SPU_FREQ;
    spec.format = AUDIO_S16SYS; // AUDIO_S16LSB // //AUDIO_S16MSB; //
    spec.channels = 2;
    spec.samples = 2048;
    spec.callback = SOUND_FillAudio;

    if (SDL_OpenAudio(&spec, NULL) < 0) {
        DestroySDL();
        return -1;
    }

    pSndBuffer = (short *)malloc(BUFFER_SIZE * sizeof(short));
    if (pSndBuffer == NULL) {
        SDL_CloseAudio();
        return -1;
    }

    iReadPos = 0;
    iWritePos = 0;

    SDL_PauseAudio(0);
    return 0;
}

static void sdl_finish(void) {
    if (pSndBuffer == NULL) return;

    SDL_CloseAudio();
    DestroySDL();

    free(pSndBuffer);
    pSndBuffer = NULL;
}

static int sdl_busy(void) {
    int size;

    if (pSndBuffer == NULL) return 1;

    size = iReadPos - iWritePos;
    if (size <= 0) size += BUFFER_SIZE;

    if (size < BUFFER_SIZE / 2) {
        #ifdef SHOW_DEBUG
        //sprintf(txtbuffer, "sdl_busy size = %d\n", size);
        //DEBUG_print(txtbuffer, DBG_SPU1);
        #endif // DISP_DEBUG
        return 1;
    }

    return 0;
}

static int sdl_feed(void *pSound, int lBytes) {
    short *p = (short *)pSound;

    if (pSndBuffer == NULL) return;

    while (lBytes > 0) {
        ++iWritePos;
        if (iWritePos >= BUFFER_SIZE) iWritePos = 0;

        if (iWritePos == iReadPos)
        {
            #ifdef SHOW_DEBUG
            sprintf(txtbuffer, "SdlBuffer not enough %d %d %d \n", lBytes, iWritePos, iReadPos);
            DEBUG_print(txtbuffer, DBG_SPU1);
            #endif // DISP_DEBUG
            iWritePos--;
            if (iWritePos < 0)
            {
                iWritePos = BUFFER_SIZE - 1;
            }
            break;
        }

		pSndBuffer[iWritePos] = *p++;
		lBytes -= sizeof(short);
	}

    return 0;
}

void out_register_sdl(struct out_driver *drv)
{
    drv->name = "sdl";
    drv->init = sdl_init;
    drv->finish = sdl_finish;
    drv->busy = sdl_busy;
    drv->feed = sdl_feed;
}

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

//#define BUFFER_SIZE		22050
#define BUFFER_SIZE		24000

short			*pSndBuffer = NULL;
//int				iBufSize = 0;
volatile int	iReadPos = 0, iWritePos = 0;
//extern char audioEnabled;
//#define NUM_BUFFERS 4
//static struct { void* buffer; int len; } buffers[NUM_BUFFERS];
//static int fill_buffer, play_buffer;
//static int usedBuf = 0;

static void SOUND_FillAudio(void *unused, Uint8 *stream, int len) {
	short *p = (short *)stream;

	//len /= sizeof(short);
	len >>= 1;

	while (iReadPos != iWritePos && len > 0) {
		*p++ = pSndBuffer[iReadPos++];
		if (iReadPos >= BUFFER_SIZE) iReadPos = 0;
		--len;
	}

    #ifdef SHOW_DEBUG
    if (len > 0) {
        DEBUG_print("SOUND_FillAudio === error", DBG_SPU2);
    }
    #endif // DISP_DEBUG
	// Fill remaining space with zero
	while (len > 0) {
		*p++ = 0;
		--len;
	}
	/*int minLen = buffers[play_buffer].len;
	if (len < minLen)
    {
        minLen = len;
    }
    memcpy(stream, buffers[play_buffer].buffer, minLen);

	play_buffer = (play_buffer + 1) & 3;
	usedBuf--;
	if (usedBuf < 0)
    {
        usedBuf = 0;
    }*/
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
	SDL_AudioSpec				spec;

	if (pSndBuffer != NULL) return -1;
	//fill_buffer = play_buffer = 0;

	InitSDL();

	spec.freq = BUFFER_SIZE << 1;
	spec.format = AUDIO_S16SYS; // AUDIO_S16LSB // //AUDIO_S16MSB; //
	spec.channels = 2;
	spec.samples = 2048;
	spec.callback = SOUND_FillAudio;

	if (SDL_OpenAudio(&spec, NULL) < 0) {
		DestroySDL();
		return -1;
	}

	//iBufSize = BUFFER_SIZE;

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
        DEBUG_print("sdl_busy =sdl_busy== ", DBG_SPU1);
        #endif // DISP_DEBUG
        return 1;
	}

	return 0;
	/*if (usedBuf > 2)
    {
        return 1;
    }

    return 0;*/
}

static void sdl_feed(void *pSound, int lBytes) {
	short *p = (short *)pSound;

	if (pSndBuffer == NULL) return;

	while (lBytes > 0) {
//		if (((iWritePos + 1) % BUFFER_SIZE) == iReadPos)
//		{
//		    #ifdef DISP_DEBUG
//            PRINT_LOG1("sdl_feed === error: %d ", lBytes);
//            #endif // DISP_DEBUG
//		    break;
//		}

        ++iWritePos;
        if (iWritePos >= BUFFER_SIZE) iWritePos = 0;

        if (iWritePos == iReadPos)
        {
            #ifdef SHOW_DEBUG
            DEBUG_print("sdl_feed === error", DBG_SPU1);
            #endif // DISP_DEBUG
            iWritePos--;
            if (iWritePos < 0)
            {
                iWritePos = BUFFER_SIZE - 1;
            }
            break;
        }

		pSndBuffer[iWritePos] = *p++;
		//++iWritePos;
		//if (iWritePos >= BUFFER_SIZE) iWritePos = 0;

		lBytes -= sizeof(short);
	}

	//if(!audioEnabled) return;

	/*buffers[fill_buffer].buffer = pSound;
	buffers[fill_buffer].len = lBytes;

	fill_buffer = (fill_buffer + 1) & 3;
	usedBuf++;
	if (usedBuf > NUM_BUFFERS)
    {
        usedBuf = NUM_BUFFERS;
    }*/
}

void out_register_sdl(struct out_driver *drv)
{
	drv->name = "sdl";
	drv->init = sdl_init;
	drv->finish = sdl_finish;
	drv->busy = sdl_busy;
	drv->feed = sdl_feed;
}

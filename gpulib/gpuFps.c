/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

#include <gccore.h>
#include <malloc.h>
#include <time.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include "../SoftGPU/stdafx.h"
#include "../SoftGPU/externals.h"
#include "../SoftGPU/gpu.h"
#include "../SoftGPU/draw.h"
#include "../Gamecube/DEBUG.h"
#include "../Gamecube/wiiSXconfig.h"
#include "plugin_lib.h"

// Frame limiting/calculation routines, taken from PeopsSoftGPU, adapted for Wii/GC.
#define       MAXSKIP 120
#define       MAXLACE 16
char          fpsInfo[32];
BOOL          bSkipNextFrame = FALSE;
static DWORD  dwLaceCnt = 0;
static BOOL   bInitCap = TRUE;
static float  fps_skip = 0;
static float  fps_cur  = 0;

static unsigned long timeGetTime()
{
    long long nowTick = gettime();
    return diff_usec(0, nowTick) / 10;
}

static void FrameCap (void)
{
    static unsigned long curticks, lastticks, _ticks_since_last_update;
    static unsigned long TicksToWait = 0;

    curticks = timeGetTime();
    _ticks_since_last_update = curticks - lastticks;

    if ((_ticks_since_last_update > TicksToWait) ||
            (curticks <lastticks))
    {
        lastticks = curticks;

        if ((_ticks_since_last_update-TicksToWait) > cbs->gpu_peops.dwFrameRateTicks)
            TicksToWait = 0;
        else TicksToWait = cbs->gpu_peops.dwFrameRateTicks-(_ticks_since_last_update-TicksToWait);
    }
    else
    {
        BOOL Waiting = TRUE;
        while (Waiting)
        {
            curticks = timeGetTime();
            _ticks_since_last_update = curticks - lastticks;
            if ((_ticks_since_last_update > TicksToWait) ||
                    (curticks < lastticks))
            {
                Waiting = FALSE;
                lastticks = curticks;
                TicksToWait = cbs->gpu_peops.dwFrameRateTicks;
            }
        }
    }
}

static void CalcFps(void)
{
    static unsigned long _ticks_since_last_update;
    static unsigned long fps_cnt = 0;
    static unsigned long fps_tck = 1;
    {
        static unsigned long lastticks;
        static unsigned long curticks;

        curticks = timeGetTime();
        _ticks_since_last_update = curticks - lastticks;

        if (cbs->frameskip && (frameLimit[0] != FRAMELIMIT_AUTO) && _ticks_since_last_update)
            fps_skip = min(fps_skip, ((float)TIMEBASE / (float)_ticks_since_last_update + 1.0f));

        lastticks = curticks;
    }

    if (cbs->frameskip && (frameLimit[0] == FRAMELIMIT_AUTO))
    {
        static unsigned long fpsskip_cnt = 0;
        static unsigned long fpsskip_tck = 1;

        fpsskip_tck += _ticks_since_last_update;

        if (++fpsskip_cnt == 2)
        {
            fps_skip = (float)2000 / (float)fpsskip_tck;
            fps_skip += 6.0f;
            fpsskip_cnt = 0;
            fpsskip_tck = 1;
        }
    }

    fps_tck += _ticks_since_last_update;

    if (++fps_cnt == 10)
    {
        fps_cur = (float)(TIMEBASE * 10) / (float)fps_tck;

        fps_cnt = 0;
        fps_tck = 1;

        if ((frameLimit[0] == FRAMELIMIT_AUTO) && fps_cur > cbs->gpu_peops.fFrameRateHz)    // optical adjust ;) avoids flickering fps display
            fps_cur = cbs->gpu_peops.fFrameRateHz;
    }

    sprintf(fpsInfo, "FPS %.2f", fps_cur);
}

void CheckFrameRate(void)
{
    if (cbs->frameskip)                           // skipping mode?
    {
//        if (frameLimit[0] == FRAMELIMIT_AUTO)
//        {
//            FrameCap();
//        }
        dwLaceCnt++;                                     // -> store cnt of vsync between frames
        if (dwLaceCnt >= MAXLACE && frameLimit[0] == FRAMELIMIT_AUTO)       // -> if there are many laces without screen toggling,
        {                                                //    do std frame limitation
            if (dwLaceCnt == MAXLACE) bInitCap = TRUE;

            FrameCap();
        }
        if (showFPSonScreen == FPS_SHOW) CalcFps();        // -> calc fps display in skipping mode
    }
    else                                                  // non-skipping mode:
    {
        if (frameLimit[0] == FRAMELIMIT_AUTO) FrameCap();                      // -> do it
        if (showFPSonScreen == FPS_SHOW) CalcFps();          // -> and calc fps display
    }
}

void FrameSkip ( void )
{
    static int   iNumSkips = 0;
    static DWORD dwLastLace = 0;                          // helper var for frame limitation

    if ( !dwLaceCnt ) return;                              // important: if no updatelace happened, we ignore it completely

    if ( iNumSkips )                                       // we are in skipping mode?
    {
        dwLastLace += dwLaceCnt;                              // -> calc frame limit helper (number of laces)
        bSkipNextFrame = TRUE;                              // -> we skip next frame
        iNumSkips--;                                        // -> ok, one done
    }
    else                                                  // ok, no additional skipping has to be done...
    {
        // we check now, if some limitation is needed, or a new skipping has to get started
        DWORD dwWaitTime;
        static DWORD curticks, lastticks, _ticks_since_last_update;

        if ( bInitCap || bSkipNextFrame )                    // first time or we skipped before?
        {
            static int iAdditionalSkip = 0;                   // number of additional frames to skip

            if ( cbs->frameskip && !bInitCap )                  // frame limit wanted and not first time called?
            {
                DWORD dwT = _ticks_since_last_update;            // -> that's the time of the last drawn frame
                dwLastLace += dwLaceCnt;                          // -> and that's the number of updatelace since the start of the last drawn frame

                curticks = timeGetTime();                       // -> now we calc the time of the last drawn frame + the time we spent skipping
                _ticks_since_last_update = dwT + curticks - lastticks;

                dwWaitTime = dwLastLace * cbs->gpu_peops.dwFrameRateTicks;         // -> and now we calc the time the real psx would have needed

                if ( _ticks_since_last_update < dwWaitTime )    // -> we were too fast?
                {
                    if ( ( dwWaitTime - _ticks_since_last_update ) > // -> some more security, to prevent
                            ( 60 * cbs->gpu_peops.dwFrameRateTicks ) )                //    wrong waiting times
                        _ticks_since_last_update = dwWaitTime;

                    while ( _ticks_since_last_update < dwWaitTime ) // -> loop until we have reached the real psx time
                    {
                        //    (that's the additional limitation, yup)
                        curticks = timeGetTime();
                        _ticks_since_last_update = dwT + curticks - lastticks;
                    }
                }
                else                                            // we were still too slow ?!!?
                {
                    if ( iAdditionalSkip < MAXSKIP )              // -> well, somewhen we really have to stop skipping on very slow systems
                    {
                        iAdditionalSkip++;                          // -> inc our watchdog var
                        dwLaceCnt = 0;                              // -> reset lace count
                        lastticks = timeGetTime();
                        return;                                     // -> done, we will skip next frame to get more speed
                    }
                }
            }

            bInitCap = FALSE;                                 // -> ok, we have inited the frameskip func
            iAdditionalSkip = 0;                              // -> init additional skip
            bSkipNextFrame = FALSE;                           // -> we don't skip the next frame
            lastticks = timeGetTime();                        // -> we store the start time of the next frame
            dwLaceCnt = 0;                                    // -> and we start to count the laces
            dwLastLace = 0;
            _ticks_since_last_update = 0;
            return;                                           // -> done, the next frame will get drawn
        }

        bSkipNextFrame = FALSE;                             // init the frame skip signal to 'no skipping' first

        curticks = timeGetTime();                           // get the current time (we are now at the end of one drawn frame)
        _ticks_since_last_update = curticks - lastticks;

        dwLastLace = dwLaceCnt;                             // store curr count (frame limitation helper)
        dwWaitTime = dwLaceCnt * cbs->gpu_peops.dwFrameRateTicks;          // calc the 'real psx lace time'

        if ( _ticks_since_last_update > dwWaitTime )        // hey, we needed way too long for that frame...
        {
            if ( cbs->frameskip )                              // if limitation, we skip just next frame,
            {
                // and decide after, if we need to do more
                iNumSkips = 0;
            }
            else
            {
                iNumSkips = _ticks_since_last_update / dwWaitTime; // -> calc number of frames to skip to catch up
                iNumSkips--;                                    // -> since we already skip next frame, one down
                if ( iNumSkips > MAXSKIP ) iNumSkips = MAXSKIP; // -> well, somewhere we have to draw a line
            }
            bSkipNextFrame = TRUE;                            // -> signal for skipping the next frame
        }
        else                                                // we were faster than real psx? fine :)
            if ( cbs->frameskip )                                // frame limit used? so we wait til the 'real psx time' has been reached
            {
                if ( dwLaceCnt > MAXLACE )                        // -> security check
                    _ticks_since_last_update = dwWaitTime;

                while ( _ticks_since_last_update < dwWaitTime )   // -> just do a waiting loop...
                {
                    curticks = timeGetTime();
                    _ticks_since_last_update = curticks - lastticks;
                }
            }

        lastticks = timeGetTime();                          // ok, start time of the next frame
    }

    dwLaceCnt = 0;                                        // init lace counter
}

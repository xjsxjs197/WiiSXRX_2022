/***************************************************************************
                           gpu.c  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    email                : BlackDove@addcom.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/

//*************************************************************************//
// History of changes:
//
// 2009/03/08 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

//#include "gpuStdafx.h"

//#include <mmsystem.h>
//#define _IN_GPU
#define _IN_GPU_LIB

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gccore.h>

#include "gpuExternals.h"
#include "gpuPlugin.h"
#include "gpuVramTiling.h"
//#include "gpuDraw.h"
//#include "gpuTexture.h"
//#include "gpuPrim.h"

#include "../gpu.h" // meh
#include "../gpulib/gpu.h"
#include "../SoftGPU/oldGpuFps.h"
#include "../mem2_manager.h"
#include "../Gamecube/wiiSXconfig.h"

//#include "NoPic.h"

#include "../gpulib/stdafx.h"

#include "../database.h"
#include "../Gamecube/DEBUG.h"
#include "../Gamecube/MEM2.h"

static short DrawSemiTrans=FALSE;
static short ly0,lx0,ly1,lx1,ly2,lx2,ly3,lx3;        // global psx vertex coords
static int   GlobalTextAddrX, GlobalTextAddrY, GlobalTextTP;
static long  GlobalTextABR,GlobalTextPAGE;
static BOOL  bUsingTWin=FALSE;
static unsigned short usMirror=0;                             // sprite mirror
static TWin_t         TWin;
static int   drawX,drawY,drawW,drawH;                 // offscreen drawing checkers
static int   iFakePrimBusy;
static BOOL  bIsFirstFrame=TRUE;

unsigned int  dwGPUVersion=0;
int           iGPUHeight=512;
int           iGPUHeightMask=511;
int           GlobalTextIL=0;
int           iTileCheat=0;

////////////////////////////////////////////////////////////////////////
// memory image of the PSX vram
////////////////////////////////////////////////////////////////////////

//unsigned char  *psxVSecure;
//unsigned char  *psxVub;
//signed   char  *psxVsb;
//unsigned short *psxVuw;
//unsigned short *psxVuw_eom;
//signed   short *psxVsw;
//unsigned int   *psxVul;
//signed   int   *psxVsl;

// macro for easy access to packet information
#define GPUCOMMAND(x) ((x>>24) & 0xff)

GLfloat         gl_z=0.0f;
BOOL            bNeedInterlaceUpdate=FALSE;
BOOL            bNeedRGB24Update=FALSE;

unsigned long   ulStatusControl[256];

////////////////////////////////////////////////////////////////////////
// global GPU vars
////////////////////////////////////////////////////////////////////////

static long     GPUdataRet;
static unsigned long gpuDataM[256];
static unsigned char gpuCommand = 0;
static long          gpuDataC = 0;
static long          gpuDataP = 0;

int             iDataWriteMode;
int             iDataReadMode;

int             lClearOnSwap;
int             lClearOnSwapColor;
//BOOL            bSkipNextFrame = FALSE;
int             iColDepth;
BOOL            bChangeRes;
BOOL            bWindowMode;

// possible psx display widths
short dispWidths[8] = {256,320,512,640,368,384,512,640};

short           imageX0,imageX1;
short           imageY0,imageY1;
BOOL            bDisplayNotSet = TRUE;
GLuint          uiScanLine=0;
//int             iUseScanLines=0;
//int             lSelectedSlot=0;
unsigned char * pGfxCardScreen=0;
int              cardTexBufSize = 0;
int             iBlurBuffer=0;
int             iScanBlend=0;
int             iRenderFVR=0;
int             iNoScreenSaver=0;
unsigned int    ulGPUInfoVals[16];
int             iRumbleVal    = 0;
int             iRumbleTime   = 0;

static unsigned char clearLargeRange = 0;
static unsigned short largeRangeX1 = 0;
static unsigned short largeRangeX2 = 0;
static unsigned short largeRangeY1 = 0;
static unsigned short largeRangeY2 = 0;

static unsigned short uploadAreaX1 = 0;
static unsigned short uploadAreaX2 = 0;
static unsigned short uploadAreaY1 = 0;
static unsigned short uploadAreaY2 = 0;

static unsigned short screenX = 0;
static unsigned short screenY = 0;
static unsigned short screenX1 = 320;
static unsigned short screenY1 = 240;
static unsigned short screenWidth = 320;
static unsigned short screenHeight = 240;
BOOL    canClearFrameBuf = FALSE;
BOOL    canShowFps = FALSE;

static BOOL    needUploadScreen = FALSE;
static BOOL    uploadedScreen = FALSE;
static BOOL    needFlipEGL = FALSE;
static unsigned short    RGB24Uploaded = 0;
static unsigned short    GPUupdateLace5Flg = 0;

// When display window / display mode has just changed, PreviousPSXDisplay may be stale.
// Skip CheckAgainstScreen() once to avoid matching the wrong previous screen area.
static BOOL    skipPreviousDisplayCheckOnce = FALSE;

#define CHECK_SCREEN_INFO() { \
    screenX = PSXDisplay.DisplayPosition.x; \
    screenY = PSXDisplay.DisplayPosition.y; \
    screenWidth = PSXDisplay.DisplayModeNew.x; \
    screenHeight = PSXDisplay.DisplayModeNew.y; \
    screenX1 = screenX + screenWidth; \
    screenY1 = screenY + screenHeight; \
}

#define CLEAR_SCREEN(x0, y0, x1, y1)  (((screenY1 - 1) <= y1) && (screenY >= y0) && ((screenX1 - 1) <= x1) && (screenX >= x0))

#define INRANGE(x1, x2, y1, y2) ((y2 <= largeRangeY2) && (y1 >= largeRangeY1) && (x2 <= largeRangeX2) && (x1 >= largeRangeX1))

static short   texChgType = 0;

/* Framebuffer feedback uses the full EFB as its texture.  Keep its coordinates
 * as floats; the normal PSX texture path intentionally stores 0..255 UVs in
 * bytes, which is not precise enough to register a 640-pixel EFB copy. */
static BOOL    gFramebufferTextureCoordsValid = FALSE;
static float   gFramebufferTextureCoords[4][2];

#ifdef DISP_DEBUG
/* Texture-diagnostic frame number.  Primitive logs emitted before flipEGL()
 * carry the number of the frame which is about to be presented. */
static unsigned int g_textureDiagFrame = 1;
static unsigned int g_textureDiagDraw = 0;
static unsigned int g_textureDiagEvent = 0;
static unsigned int g_textureDiagEfbClears = 0;
static char g_textureDiagBuffer[16384];
static unsigned int g_textureDiagBufferUsed = 0;

/* SD open/close per line noticeably disturbs audio timing.  Accumulate one
 * frame of diagnostics and issue a single file write at presentation. */
static void TextureDiagAppend(const char *line)
{
    unsigned int length;

    if (line == NULL)
        return;
#ifdef VRAM_TILING_DIAG_ONLY
    if (strncmp(line, "VTL ", 4) != 0 &&
        strncmp(line, "TDI ", 4) != 0)
        return;
#endif
    length = (unsigned int)strlen(line);
    if (length >= sizeof(g_textureDiagBuffer))
        return;
    if (g_textureDiagBufferUsed + length >= sizeof(g_textureDiagBuffer))
    {
        g_textureDiagBuffer[g_textureDiagBufferUsed] = '\0';
        writeLogFile(g_textureDiagBuffer);
        g_textureDiagBufferUsed = 0;
    }
    memcpy(g_textureDiagBuffer + g_textureDiagBufferUsed, line, length);
    g_textureDiagBufferUsed += length;
}

static void TextureDiagFlush(void)
{
    if (g_textureDiagBufferUsed == 0)
        return;
    g_textureDiagBuffer[g_textureDiagBufferUsed] = '\0';
    writeLogFile(g_textureDiagBuffer);
    g_textureDiagBufferUsed = 0;
}

#ifdef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
/* F1-style RGB15 movies update two 320x236 pages with small A0/80h
 * transfers.  Keep this probe narrowly scoped to those pages so the SD log
 * does not perturb unrelated rendering. */
#define S7_ANIM_DIAG_LIMIT 64u
static unsigned int g_s7AnimDiagFrame;
static unsigned int g_s7AnimDiagA0Count;
static unsigned int g_s7AnimDiagMoveCount;
static unsigned int g_s7AnimDiagFillCount;

static void S7AnimDiagBeginFrame(void)
{
    if (g_s7AnimDiagFrame == g_textureDiagFrame)
        return;
    g_s7AnimDiagFrame = g_textureDiagFrame;
    g_s7AnimDiagA0Count = 0;
    g_s7AnimDiagMoveCount = 0;
    g_s7AnimDiagFillCount = 0;
}

static int S7AnimDiagRectsOverlap(const GlesVramRect *rect,
                                  const GlesVramRect *target)
{
    GlesVramRectPiece pieces[GLES_VRAM_MAX_WRAP_PIECES];
    int count;
    int piece;

    if (rect == NULL || target == NULL ||
        target->width <= 0 || target->height <= 0)
        return 0;
    count = GlesVramSplitWrappedRect(rect, pieces);
    for (piece = 0; piece < count; piece++)
    {
        const GlesVramRect *part = &pieces[piece].rect;
        if (part->x < target->x + target->width &&
            target->x < part->x + part->width &&
            part->y < target->y + target->height &&
            target->y < part->y + part->height)
            return 1;
    }
    return 0;
}

static unsigned int S7AnimDiagPageMask(const GlesVramRect *rect)
{
    static const GlesVramRect pages[2] = {
        {0, 0, 320, 236},
        {512, 0, 320, 236}
    };
    GlesVramRectPiece pieces[GLES_VRAM_MAX_WRAP_PIECES];
    unsigned int mask = 0;
    int count;
    int piece;
    int page;

    if (rect == NULL)
        return 0;
    count = GlesVramSplitWrappedRect(rect, pieces);
    for (piece = 0; piece < count; piece++)
    {
        const GlesVramRect *part = &pieces[piece].rect;
        for (page = 0; page < 2; page++)
        {
            if (part->x < pages[page].x + pages[page].width &&
                pages[page].x < part->x + part->width &&
                part->y < pages[page].y + pages[page].height &&
                pages[page].y < part->y + part->height)
                mask |= 1u << page;
        }
    }
    return mask;
}

static unsigned int S7AnimDiagDisplayMask(const GlesVramRect *rect)
{
    GlesVramRect current;
    GlesVramRect previous;
    unsigned int mask = 0;

    current.x = PSXDisplay.DisplayPosition.x;
    current.y = PSXDisplay.DisplayPosition.y;
    current.width = PSXDisplay.DisplayMode.x;
    current.height = PSXDisplay.DisplayMode.y;
    previous.x = PreviousPSXDisplay.DisplayPosition.x;
    previous.y = PreviousPSXDisplay.DisplayPosition.y;
    previous.width = PSXDisplay.DisplayMode.x;
    previous.height = PSXDisplay.DisplayMode.y;
    if (S7AnimDiagRectsOverlap(rect, &current))
        mask |= 1u;
    if (S7AnimDiagRectsOverlap(rect, &previous))
        mask |= 2u;
    return mask;
}

static unsigned int S7AnimDiagTileMask(const GlesVramRect *rect)
{
    GlesVramTileSpan spans[GLES_VRAM_MAX_TILE_SPANS];
    unsigned int mask = 0;
    int count;
    int span;

    if (rect == NULL)
        return 0;
    count = GlesVramBuildTileSpans(rect, spans);
    for (span = 0; span < count; span++)
    {
        if (spans[span].tile == GLES_VRAM_TILE_LEFT)
            mask |= 1u;
        else if (spans[span].tile == GLES_VRAM_TILE_RIGHT)
            mask |= 2u;
    }
    return mask;
}
#endif
#endif

#ifdef VRAM_APERTURE_DIAG
/*
 * PS VRAM aperture diagnostic.
 *
 * Keep this independent of the renderer's EFB tracking: the purpose is to
 * observe the original GP0 command stream before any 640-pixel GX mapping is
 * applied.  Data is aggregated for a whole presented frame, then written in
 * one operation so SD I/O does not perturb command timing.
 */
typedef struct VramApertureRangeTag
{
    unsigned int rects;
    unsigned int outsideFixed;
    unsigned int wrapped;
    int valid;
    int x0, y0, x1, y1; /* Half-open bounds. */
} VramApertureRange;

static unsigned int g_vapFrame = 1;
static unsigned int g_vapCommandCount[256];
static VramApertureRange g_vapWriteRange[256];
static VramApertureRange g_vapReadRange[256];

static void VramApertureMergeBounds(VramApertureRange *range,
                                    int x0, int y0, int x1, int y1)
{
    if (x1 <= x0 || y1 <= y0)
        return;

    if (!range->valid)
    {
        range->x0 = x0;
        range->y0 = y0;
        range->x1 = x1;
        range->y1 = y1;
        range->valid = 1;
        return;
    }

    if (x0 < range->x0) range->x0 = x0;
    if (y0 < range->y0) range->y0 = y0;
    if (x1 > range->x1) range->x1 = x1;
    if (y1 > range->y1) range->y1 = y1;
}

static void VramApertureTrackRect(VramApertureRange *range,
                                  int x, int y, int width, int height,
                                  int allowWrap)
{
    int x1, y1;

    if (width <= 0 || height <= 0)
        return;

    range->rects++;
    x1 = x + width;
    y1 = y + height;

    if (x < 0 || y < 0 || x1 > 640 || y1 > 480)
        range->outsideFixed++;

    if (allowWrap && (x1 > 1024 || y1 > 512))
    {
        /* A wrapped transfer touches both VRAM edges.  Represent that as the
         * full affected axis; a single ordinary aperture cannot contain it. */
        range->wrapped++;
        if (x1 > 1024) { x = 0; x1 = 1024; }
        if (y1 > 512)  { y = 0; y1 = 512; }
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > 1024) x1 = 1024;
    if (y1 > 512) y1 = 512;
    VramApertureMergeBounds(range, x, y, x1, y1);
}

static int VramAperturePacketX(const unsigned long *packet, int index)
{
    unsigned long word = GETLE32(&packet[index]);
    return (int)(short)(word & 0xffff);
}

static int VramAperturePacketY(const unsigned long *packet, int index)
{
    unsigned long word = GETLE32(&packet[index]);
    return (int)(short)((word >> 16) & 0xffff);
}

static int VramApertureTransferWidth(unsigned long word)
{
    int width = (int)(word & 0xffff) & 0x3ff;
    return width ? width : 1024;
}

static int VramApertureTransferHeight(unsigned long word)
{
    int height = (int)((word >> 16) & 0xffff) & 0x1ff;
    return height ? height : 512;
}

static void VramApertureTrackPrimitive(unsigned char command,
                                       const unsigned long *packet,
                                       const unsigned char *indices,
                                       int vertexCount)
{
    int i, x, y;
    int minX = 32767, minY = 32767;
    int maxX = -32768, maxY = -32768;
    int clipX0, clipY0, clipX1, clipY1;

    for (i = 0; i < vertexCount; i++)
    {
        x = VramAperturePacketX(packet, indices[i]) + PSXDisplay.DrawOffset.x;
        y = VramAperturePacketY(packet, indices[i]) + PSXDisplay.DrawOffset.y;
        if (x < minX) minX = x;
        if (y < minY) minY = y;
        if (x > maxX) maxX = x;
        if (y > maxY) maxY = y;
    }

    /* DrawArea has inclusive endpoints.  Clip the primitive bounding box to
     * the pixels that can actually be written to PS VRAM. */
    clipX0 = PSXDisplay.DrawArea.x0;
    clipY0 = PSXDisplay.DrawArea.y0;
    clipX1 = PSXDisplay.DrawArea.x1 + 1;
    clipY1 = PSXDisplay.DrawArea.y1 + 1;
    if (clipX0 < 0) clipX0 = 0;
    if (clipY0 < 0) clipY0 = 0;
    if (clipX1 > 1024) clipX1 = 1024;
    if (clipY1 > 512) clipY1 = 512;
    if (minX < clipX0) minX = clipX0;
    if (minY < clipY0) minY = clipY0;
    if (maxX + 1 > clipX1) maxX = clipX1 - 1;
    if (maxY + 1 > clipY1) maxY = clipY1 - 1;

    VramApertureTrackRect(&g_vapWriteRange[command], minX, minY,
                          maxX - minX + 1, maxY - minY + 1, 0);
}

static int VramApertureIsPolylineEnd(unsigned long word)
{
    return (word & 0xF000F000) == 0x50005000;
}

static void VramApertureTrackPacket(unsigned char command,
                                    const unsigned long *packet)
{
    static const unsigned char triF[]  = { 1, 2, 3 };
    static const unsigned char triFT[] = { 1, 3, 5 };
    static const unsigned char quadF[] = { 1, 2, 3, 4 };
    static const unsigned char quadFT[] = { 1, 3, 5, 7 };
    static const unsigned char triG[] = { 1, 3, 5 };
    static const unsigned char triGT[] = { 1, 4, 7 };
    static const unsigned char quadG[] = { 1, 3, 5, 7 };
    static const unsigned char quadGT[] = { 1, 4, 7, 10 };
    static const unsigned char lineF[] = { 1, 2 };
    static const unsigned char lineG[] = { 1, 3 };
    unsigned char polylineIndices[128];
    unsigned long pos, size;
    int i, vertices;
    int x, y, width, height;

    g_vapCommandCount[command]++;

    if (command == 0x02)
    {
        pos = GETLE32(&packet[1]);
        size = GETLE32(&packet[2]);
        x = (int)(pos & 0x3f0);
        y = (int)((pos >> 16) & 0x1ff);
        width = ((int)(size & 0x3ff) + 15) & ~15;
        height = (int)((size >> 16) & 0x1ff);
        VramApertureTrackRect(&g_vapWriteRange[command], x, y,
                              width, height, 1);
    }
    else if (command >= 0x20 && command <= 0x23)
        VramApertureTrackPrimitive(command, packet, triF, 3);
    else if (command >= 0x24 && command <= 0x27)
        VramApertureTrackPrimitive(command, packet, triFT, 3);
    else if (command >= 0x28 && command <= 0x2b)
        VramApertureTrackPrimitive(command, packet, quadF, 4);
    else if (command >= 0x2c && command <= 0x2f)
        VramApertureTrackPrimitive(command, packet, quadFT, 4);
    else if (command >= 0x30 && command <= 0x33)
        VramApertureTrackPrimitive(command, packet, triG, 3);
    else if (command >= 0x34 && command <= 0x37)
        VramApertureTrackPrimitive(command, packet, triGT, 3);
    else if (command >= 0x38 && command <= 0x3b)
        VramApertureTrackPrimitive(command, packet, quadG, 4);
    else if (command >= 0x3c && command <= 0x3f)
        VramApertureTrackPrimitive(command, packet, quadGT, 4);
    else if (command >= 0x40 && command <= 0x47)
        VramApertureTrackPrimitive(command, packet, lineF, 2);
    else if (command >= 0x48 && command <= 0x4f)
    {
        vertices = 0;
        for (i = 1; i < 254 && vertices < 128; i++)
        {
            if (VramApertureIsPolylineEnd(GETLE32(&packet[i]))) break;
            polylineIndices[vertices++] = (unsigned char)i;
        }
        if (vertices >= 2)
            VramApertureTrackPrimitive(command, packet,
                                       polylineIndices, vertices);
    }
    else if (command >= 0x50 && command <= 0x57)
        VramApertureTrackPrimitive(command, packet, lineG, 2);
    else if (command >= 0x58 && command <= 0x5f)
    {
        vertices = 0;
        for (i = 1; i < 255 && vertices < 128; i += 2)
        {
            polylineIndices[vertices++] = (unsigned char)i;
            if (i + 1 < 255 &&
                VramApertureIsPolylineEnd(GETLE32(&packet[i + 1]))) break;
        }
        if (vertices >= 2)
            VramApertureTrackPrimitive(command, packet,
                                       polylineIndices, vertices);
    }
    else if (command >= 0x60 && command <= 0x67)
    {
        pos = GETLE32(&packet[1]);
        size = GETLE32(&packet[(command & 4) ? 3 : 2]);
        x = (int)(short)(pos & 0xffff) + PSXDisplay.DrawOffset.x;
        y = (int)(short)(pos >> 16) + PSXDisplay.DrawOffset.y;
        width = (int)(size & 0xffff);
        height = (int)((size >> 16) & 0xffff);
        VramApertureTrackRect(&g_vapWriteRange[command], x, y,
                              width, height, 0);
    }
    else if (command >= 0x68 && command <= 0x7f)
    {
        pos = GETLE32(&packet[1]);
        x = (int)(short)(pos & 0xffff) + PSXDisplay.DrawOffset.x;
        y = (int)(short)(pos >> 16) + PSXDisplay.DrawOffset.y;
        width = (command < 0x70) ? 1 : ((command < 0x78) ? 8 : 16);
        height = width;
        VramApertureTrackRect(&g_vapWriteRange[command], x, y,
                              width, height, 0);
    }
    else if (command == 0x80)
    {
        unsigned long src = GETLE32(&packet[1]);
        unsigned long dst = GETLE32(&packet[2]);
        size = GETLE32(&packet[3]);
        width = VramApertureTransferWidth(size);
        height = VramApertureTransferHeight(size);
        VramApertureTrackRect(&g_vapReadRange[command],
                              (int)(src & 0x3ff),
                              (int)((src >> 16) & 0x1ff),
                              width, height, 1);
        VramApertureTrackRect(&g_vapWriteRange[command],
                              (int)(dst & 0x3ff),
                              (int)((dst >> 16) & 0x1ff),
                              width, height, 1);
    }
    else if (command == 0xa0 || command == 0xc0)
    {
        pos = GETLE32(&packet[1]);
        size = GETLE32(&packet[2]);
        width = VramApertureTransferWidth(size);
        height = VramApertureTransferHeight(size);
        VramApertureTrackRect(command == 0xa0 ?
                              &g_vapWriteRange[command] :
                              &g_vapReadRange[command],
                              (int)(pos & 0x3ff),
                              (int)((pos >> 16) & 0x1ff),
                              width, height, 1);
    }
}

static void VramApertureAppend(char *buffer, unsigned int capacity,
                               unsigned int *used, const char *format, ...)
{
    va_list args;
    int written;

    if (*used >= capacity - 1)
        return;
    va_start(args, format);
    written = vsnprintf(buffer + *used, capacity - *used, format, args);
    va_end(args);
    if (written < 0)
        return;
    if ((unsigned int)written >= capacity - *used)
        *used = capacity - 1;
    else
        *used += (unsigned int)written;
}

static void VramApertureFlushFrame(void)
{
    static char buffer[32768];
    VramApertureRange writeAll = {0}, readAll = {0};
    unsigned int used = 0, total = 0;
    int command;
    int writeFit, readFit, apertureX, apertureY;

    for (command = 0; command < 256; command++)
    {
        VramApertureRange *wr = &g_vapWriteRange[command];
        VramApertureRange *rd = &g_vapReadRange[command];
        total += g_vapCommandCount[command];
        if (wr->valid)
            VramApertureMergeBounds(&writeAll,
                                    wr->x0, wr->y0, wr->x1, wr->y1);
        if (rd->valid)
            VramApertureMergeBounds(&readAll,
                                    rd->x0, rd->y0, rd->x1, rd->y1);
        writeAll.rects += wr->rects;
        writeAll.outsideFixed += wr->outsideFixed;
        writeAll.wrapped += wr->wrapped;
        readAll.rects += rd->rects;
        readAll.outsideFixed += rd->outsideFixed;
        readAll.wrapped += rd->wrapped;
    }

    writeFit = !writeAll.valid ||
               (writeAll.x1 - writeAll.x0 <= 640 &&
                writeAll.y1 - writeAll.y0 <= 480 &&
                writeAll.wrapped == 0);
    readFit = !readAll.valid ||
              (readAll.x1 - readAll.x0 <= 640 &&
               readAll.y1 - readAll.y0 <= 480 &&
               readAll.wrapped == 0);
    apertureX = writeAll.valid ? writeAll.x0 : 0;
    apertureY = writeAll.valid ? writeAll.y0 : 0;
    if (apertureX > 384) apertureX = 384;
    if (apertureY > 32) apertureY = 32;

    VramApertureAppend(buffer, sizeof(buffer), &used,
        "VAP FRAME=%u commands=%u display=%d,%d %dx%d "
        "drawarea=%d,%d-%d,%d offset=%d,%d "
        "write=%d,%d-%d,%d rects=%u outside640=%u wrap=%u fit=%d "
        "aperture=%d,%d read=%d,%d-%d,%d rects=%u outside640=%u "
        "wrap=%u fit=%d\r\n",
        g_vapFrame, total,
        PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y,
        PSXDisplay.DisplayMode.x, PSXDisplay.DisplayMode.y,
        PSXDisplay.DrawArea.x0, PSXDisplay.DrawArea.y0,
        PSXDisplay.DrawArea.x1, PSXDisplay.DrawArea.y1,
        PSXDisplay.DrawOffset.x, PSXDisplay.DrawOffset.y,
        writeAll.valid ? writeAll.x0 : 0,
        writeAll.valid ? writeAll.y0 : 0,
        writeAll.valid ? writeAll.x1 : 0,
        writeAll.valid ? writeAll.y1 : 0,
        writeAll.rects, writeAll.outsideFixed, writeAll.wrapped, writeFit,
        apertureX, apertureY,
        readAll.valid ? readAll.x0 : 0,
        readAll.valid ? readAll.y0 : 0,
        readAll.valid ? readAll.x1 : 0,
        readAll.valid ? readAll.y1 : 0,
        readAll.rects, readAll.outsideFixed, readAll.wrapped, readFit);

    for (command = 0; command < 256; command++)
    {
        VramApertureRange *wr = &g_vapWriteRange[command];
        VramApertureRange *rd = &g_vapReadRange[command];
        if (!g_vapCommandCount[command])
            continue;
        VramApertureAppend(buffer, sizeof(buffer), &used,
            "VAP OP frame=%u cmd=%02X count=%u "
            "write=%d,%d-%d,%d rects=%u outside640=%u wrap=%u "
            "read=%d,%d-%d,%d rects=%u outside640=%u wrap=%u\r\n",
            g_vapFrame, command, g_vapCommandCount[command],
            wr->valid ? wr->x0 : 0, wr->valid ? wr->y0 : 0,
            wr->valid ? wr->x1 : 0, wr->valid ? wr->y1 : 0,
            wr->rects, wr->outsideFixed, wr->wrapped,
            rd->valid ? rd->x0 : 0, rd->valid ? rd->y0 : 0,
            rd->valid ? rd->x1 : 0, rd->valid ? rd->y1 : 0,
            rd->rects, rd->outsideFixed, rd->wrapped);
    }

    buffer[used] = '\0';
    if (used)
        writeLogFile(buffer);
    memset(g_vapCommandCount, 0, sizeof(g_vapCommandCount));
    memset(g_vapWriteRange, 0, sizeof(g_vapWriteRange));
    memset(g_vapReadRange, 0, sizeof(g_vapReadRange));
    g_vapFrame++;
}
#endif

static void ResetVramReadbackState(void);
static void BuildActiveMapFromDisplay(void);
static inline unsigned short ReadGXRGB5A3PixelRaw(
    const unsigned char *buf, int texWidth, int px, int py);
static inline unsigned short GXRGB5A3ToPSX15(unsigned short gx);
void RestoreDispCopyInfo(void);
extern GXRModeObj *vmode;     /*** Graphics Mode Object ***/
#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
/* gpuPrim.c is included below and sets this only when S5 has already made
 * the complete C0 rectangle current in psxVuw. */
static int g_s5ReadActive;
#ifdef VRAM_TILING_DIAG_ONLY
/* Commands are logged before flipEGL(), so this names the frame they will
 * contribute to rather than the previously presented frame. */
static unsigned int g_s5TraceFrame = 1;
#endif
#endif

#include "gpuDraw.c"
#include "gpuTexture.c"
#include "gpuVramReadback.inc"
#include "gpuVramTilingS2.inc"
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
#define glPRIMdrawTexturedQuad GlesVramS2DrawTexturedQuad
#define glPRIMdrawTexturedTri GlesVramS2DrawTexturedTri
#define glPRIMdrawTexGouraudTriColor GlesVramS2DrawTexGouraudTriColor
#define glPRIMdrawTexGouraudTriColorQuad GlesVramS2DrawTexGouraudTriColorQuad
#define glPRIMdrawTri GlesVramS2DrawTri
#define glPRIMdrawTri2 GlesVramS2DrawTri2
#define glPRIMdrawGouraudTriColor GlesVramS2DrawGouraudTriColor
#define glPRIMdrawGouraudTri2Color GlesVramS2DrawGouraudTri2Color
#define glPRIMdrawFlatLine GlesVramS2DrawFlatLine
#define glPRIMdrawGouraudLine GlesVramS2DrawGouraudLine
#define glPRIMdrawQuad GlesVramS2DrawQuad
#endif
#include "gpuPrim.c"
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
#undef glPRIMdrawTexturedQuad
#undef glPRIMdrawTexturedTri
#undef glPRIMdrawTexGouraudTriColor
#undef glPRIMdrawTexGouraudTriColorQuad
#undef glPRIMdrawTri
#undef glPRIMdrawTri2
#undef glPRIMdrawGouraudTriColor
#undef glPRIMdrawGouraudTri2Color
#undef glPRIMdrawFlatLine
#undef glPRIMdrawGouraudLine
#undef glPRIMdrawQuad
#endif

#if defined(GLES_VRAM_LR_TILING_S2_TEST) || \
    defined(GLES_VRAM_LR_TILING_S3_TEST)
static void GlesVramS2SetTestColor(OGLVertex *vertex,
                                   unsigned char red,
                                   unsigned char green,
                                   unsigned char blue)
{
    /* OpenGX's PS primitive entry points consume BGR byte order. */
    vertex->c.col.r = blue;
    vertex->c.col.g = green;
    vertex->c.col.b = red;
    vertex->c.col.a = 255;
}

static int GlesVramS2DrawTestQuad(int x0, int y0, int x1, int y1,
                                 GXColor leftColor, GXColor rightColor)
{
    uint32_t packet[8];
    GlesVramRect drawArea = {0, 0, GLES_VRAM_WIDTH, GLES_VRAM_HEIGHT};
    GlesVramPoint drawOffset = {0, 0};
    OGLVertex testVertex[4];

    memset(packet, 0, sizeof(packet));
    PUTLE32(&packet[0], 0x38000000u);
    PUTLE32(&packet[1], ((uint32_t)(uint16_t)y0 << 16) |
                        (uint16_t)x0);
    PUTLE32(&packet[3], ((uint32_t)(uint16_t)y0 << 16) |
                        (uint16_t)x1);
    PUTLE32(&packet[5], ((uint32_t)(uint16_t)y1 << 16) |
                        (uint16_t)x0);
    PUTLE32(&packet[7], ((uint32_t)(uint16_t)y1 << 16) |
                        (uint16_t)x1);
    if (!GlesVramTilingS2BeginCommand(0x38, packet, 8,
                                      &drawArea, &drawOffset, NULL))
        return 0;

    memset(testVertex, 0, sizeof(testVertex));
    testVertex[0].x = (float)x0; testVertex[0].y = (float)y0;
    testVertex[1].x = (float)x1; testVertex[1].y = (float)y0;
    testVertex[2].x = (float)x0; testVertex[2].y = (float)y1;
    testVertex[3].x = (float)x1; testVertex[3].y = (float)y1;
    GlesVramS2SetTestColor(&testVertex[0],
        leftColor.r, leftColor.g, leftColor.b);
    GlesVramS2SetTestColor(&testVertex[1],
        rightColor.r, rightColor.g, rightColor.b);
    GlesVramS2SetTestColor(&testVertex[2],
        leftColor.r, leftColor.g, leftColor.b);
    GlesVramS2SetTestColor(&testVertex[3],
        rightColor.r, rightColor.g, rightColor.b);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glAlphaFunc(GL_ALWAYS, 0.0f);
    GlesVramS2DrawGouraudTri2Color(testVertex);
    GlesVramTilingS2EndCommand();
    return 1;
}
#endif

#ifdef GLES_VRAM_LR_TILING_S2_TEST
static void DrawGlesVramS2VisualTest(void)
{
    if (!GlesVramTilingS2VisualTestReady())
    {
        /* Establish valid black contents in both tiles before testing
         * primitive writes. */
        GlesVramTilingReset();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_LEFT);
        GlesVramTilingMarkActiveDirty();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_RIGHT);
        GlesVramTilingMarkActiveDirty();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_LEFT);

        /* Logical X=576..704 crosses exactly through X=640.  Reusing the
         * original four vertices in both passes must keep the red-blue
         * Gouraud gradient continuous at the tile boundary. */
        GlesVramS2DrawTestQuad(576, 80, 704, 432,
            (GXColor){255, 0, 0, 255},
            (GXColor){0, 0, 255, 255});

        /* V8's observed off-screen range is around X=704..767, which maps to
         * RIGHT local X=64..127.  Draw a solid green reference there. */
        GlesVramS2DrawTestQuad(704, 448, 768, 496,
            (GXColor){0, 255, 0, 255},
            (GXColor){0, 255, 0, 255});

        GlesVramTilingS2SaveAndDiscard();
        GlesVramTilingS2SetVisualTestReady();
    }

    GlesVramTilingS2DrawDebugComposite();
}
#endif

#ifdef GLES_VRAM_LR_TILING_S3_TEST
static unsigned short GlesVramS3TestColor(int red, int green, int blue)
{
    return (unsigned short)((red & 31) |
                            ((green & 31) << 5) |
                            ((blue & 31) << 10));
}

static void GlesVramS3WriteTestRect(const GlesVramRect *rect,
                                    unsigned short color)
{
    int x;
    int y;
    for (y = 0; y < rect->height; y++)
    {
        for (x = 0; x < rect->width; x++)
        {
            int px = (rect->x + x) & (GLES_VRAM_WIDTH - 1);
            int py = (rect->y + y) & (GLES_VRAM_HEIGHT - 1);
            PUTLE16(psxVuw + py * GLES_VRAM_WIDTH + px, color);
        }
    }
}

static void GlesVramS3WriteClutTest(const GlesVramRect *rect)
{
    int x;
    int y;
    for (y = 0; y < rect->height; y++)
    {
        for (x = 0; x < rect->width; x++)
        {
            unsigned short color = GlesVramS3TestColor(
                (x & 1) ? 31 : x * 2,
                (x & 2) ? 31 : 0,
                (x & 4) ? 31 : (15 - x) * 2);
            PUTLE16(psxVuw + (rect->y + y) * GLES_VRAM_WIDTH +
                    rect->x + x, color);
        }
    }
}

static void DrawGlesVramS3VisualTest(void)
{
    if (!GlesVramTilingS3VisualTestReady())
    {
        GlesVramRect partial = {632, 104, 16, 48};
        GlesVramRect clut = {704, 224, 16, 16};

        /* Start from a known CPU-only black VRAM.  The first selection of
         * each tile must lazily upload it instead of treating a clear EFB as
         * authoritative. */
        memset(psxVuw, 0, GLES_VRAM_WIDTH * GLES_VRAM_HEIGHT * 2);
        GlesVramTilingReset();

        /* GPU-new blue background straddles X=640. */
        GlesVramS2DrawTestQuad(608, 64, 672, 192,
            (GXColor){0, 0, 255, 255},
            (GXColor){0, 0, 255, 255});
        GlesVramTilingS2SaveAndDiscard();

        /* This A0-like write only partially covers the blue blocks.  Prepare
         * must resolve their untouched pixels before the CPU writes red. */
        if (GlesVramTilingS3PrepareCpuWrite(psxVuw, &partial))
        {
            GlesVramS3WriteTestRect(
                &partial, GlesVramS3TestColor(31, 0, 0));
            GlesVramTilingS3FinishCpuWrite(&partial);
        }

        /* A full 16x16 CPU-only block on RIGHT models a small animated CLUT
         * upload and must not force an immediate tile switch. */
        if (GlesVramTilingS3PrepareCpuWrite(psxVuw, &clut))
        {
            GlesVramS3WriteClutTest(&clut);
            GlesVramTilingS3FinishCpuWrite(&clut);
        }

        GlesVramTilingSelectTile(GLES_VRAM_TILE_LEFT);
        GlesVramTilingS3CaptureEfbTestDiagnostics();
        GlesVramTilingSelectTile(GLES_VRAM_TILE_RIGHT);
        GlesVramTilingS2SaveAndDiscard();
        GlesVramTilingS3CaptureTestDiagnostics();
        GlesVramTilingS3SetVisualTestReady();
    }

    GlesVramTilingS2DrawDebugComposite();
}
#endif

static void flipEGL(void);
extern void (*ogx_draw_submitted_cb)(void);

#ifdef EFB_512_HEIGHT_TEST
/*
 * Hardware validation for the PS VRAM tiling design.  Four colors are first
 * written only to EFB rows 480..511, copied from that exact source rectangle,
 * and then expanded over the normal 640x480 presentation area.  A stable
 * four-band image proves both raster access and copy-engine access below row
 * 480 without involving the XFB height.
 */
static GLuint g_efb512TestTexture = 0;

static void DrawEfb512HeightTest(void)
{
    static const unsigned char colors[4][3] = {
        { 255,   0,   0 },
        {   0, 255,   0 },
        {   0,   0, 255 },
        { 255, 255, 255 }
    };
    OGLVertex quad[4];
    int i;

    /* Extend the game rendering state to the complete PS VRAM height. */
    glViewport(0, 0, 640, 512);
    glEnable(GL_SCISSOR_TEST);
    for (i = 0; i < 4; i++)
    {
        glScissor(i * 160, 480, 160, 32);
        glClearColor2(colors[i][0], colors[i][1], colors[i][2], 255);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (!g_efb512TestTexture)
    {
        glGenTextures(1, &g_efb512TestTexture);
        if (!g_efb512TestTexture)
            return;
        glBindTextureBef(GL_TEXTURE_2D, g_efb512TestTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glInitRGBATextures(640, 32);
    }
    else
    {
        glBindTextureBef(GL_TEXTURE_2D, g_efb512TestTexture);
    }

    if (!glCaptureFramebufferTextureRect(0, 480, 640, 32))
        return;

    RestoreDispCopyInfo();
    glViewport(0, 0, 640, 480);
    glScissor(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glAlphaFunc(GL_ALWAYS, 0.0f);
    glEnable(GL_TEXTURE_2D);

    memset(quad, 0, sizeof(quad));
    quad[0].x = 0.0f;   quad[0].y = 0.0f;
    quad[1].x = 640.0f; quad[1].y = 0.0f;
    quad[2].x = 0.0f;   quad[2].y = 480.0f;
    quad[3].x = 640.0f; quad[3].y = 480.0f;
    quad[0].sow = 0.0f; quad[0].tow = 0.0f;
    quad[1].sow = 1.0f; quad[1].tow = 0.0f;
    quad[2].sow = 0.0f; quad[2].tow = 1.0f;
    quad[3].sow = 1.0f; quad[3].tow = 1.0f;
    glPRIMdrawTexturedQuad(quad, 0);

    /* The next game primitive must rebuild its own projection/scissor state. */
    bDisplayNotSet = TRUE;
    RestoreDispCopyInfo();
}
#endif

////////////////////////////////////////////////////////////////////////
// stuff to make this a true PDK module
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// snapshot funcs (saves screen to bitmap / text infos into file)
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// save text infos to file
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// GPU INIT... here starts it all (first func called by emu)
////////////////////////////////////////////////////////////////////////

#define VRAM_SIZE ((1024 * 512 * 2) + 4096)
#define VRAM_ALIGN 16
static uint16_t *vram_ptr_orig = NULL;
extern uint8_t globalVram[VRAM_SIZE + (VRAM_ALIGN - 1)];

long CALLBACK GL_GPUinit()
{
memset(ulStatusControl,0,256*sizeof(unsigned long));

bChangeRes=FALSE;
bWindowMode=FALSE;

bKeepRatio = TRUE;
// different ways of accessing PSX VRAM

 //!!! ATTENTION !!!
 if (vram_ptr_orig == NULL)
 {
     //vram_ptr_orig = calloc(VRAM_SIZE + (VRAM_ALIGN-1), 1);
     vram_ptr_orig = (uint16_t *)&globalVram[0];
 }

psxVub = (unsigned char *)vram_ptr_orig;
//psxVsb=(signed char *)psxVub;
//psxVsw=(signed short *)psxVub;
//psxVsl=(signed long *)psxVub;
psxVuw=(unsigned short *)psxVub;
//psxVul=(unsigned long *)psxVub;

psxVuw_eom=psxVuw+1024*iGPUHeight;                    // pre-calc of end of vram

memset(vram_ptr_orig,0x00,VRAM_SIZE + (VRAM_ALIGN-1));
memset(ulGPUInfoVals,0x00,16*sizeof(unsigned long));

//InitFrameCap();                                       // init frame rate stuff

PSXDisplay.RGB24        = 0;                          // init vars
PreviousPSXDisplay.RGB24= 0;
PSXDisplay.Interlaced   = 0;
PSXDisplay.InterlacedTest=0;
PSXDisplay.DrawOffset.x = 0;
PSXDisplay.DrawOffset.y = 0;
PSXDisplay.DrawArea.x0  = 0;
PSXDisplay.DrawArea.y0  = 0;
PSXDisplay.DrawArea.x1  = 320;
PSXDisplay.DrawArea.y1  = 240;
PSXDisplay.DisplayMode.x= 320;
PSXDisplay.DisplayMode.y= 240;
PSXDisplay.Disabled     = FALSE;
PreviousPSXDisplay.Range.x0 =0;
PreviousPSXDisplay.Range.x1 =0;
PreviousPSXDisplay.Range.y0 =0;
PreviousPSXDisplay.Range.y1 =0;
PSXDisplay.Range.x0=0;
PSXDisplay.Range.x1=0;
PSXDisplay.Range.y0=0;
PSXDisplay.Range.y1=0;
PreviousPSXDisplay.DisplayPosition.x = 1;
PreviousPSXDisplay.DisplayPosition.y = 1;
PSXDisplay.DisplayPosition.x = 1;
PSXDisplay.DisplayPosition.y = 1;
PreviousPSXDisplay.DisplayModeNew.y=0;
PSXDisplay.Double=1;
GPUdataRet=0x400;

PSXDisplay.DisplayModeNew.x=0;
PSXDisplay.DisplayModeNew.y=0;

//PreviousPSXDisplay.Height = PSXDisplay.Height = 239;

iDataWriteMode = DR_NORMAL;

// Reset transfer values, to prevent mis-transfer of data
memset(&VRAMWrite,0,sizeof(VRAMLoad_t));
memset(&VRAMRead,0,sizeof(VRAMLoad_t));

// device initialised already !
//lGPUstatusRet = 0x74000000;

STATUSREG = 0x14802000;
GPUIsIdle;
GPUIsReadyForCommands;

return 0;
}


////////////////////////////////////////////////////////////////////////
// OPEN interface func: attention!
// some emus are calling this func in their main Window thread,
// but all other interface funcs (to draw stuff) in a different thread!
// that's a problem, since OGL is thread safe! Therefore we cannot
// initialize the OGL stuff right here, we simply set a "bIsFirstFrame = TRUE"
// flag, to initialize OGL on the first real draw call.
// btw, we also call this open func ourselfes, each time when the user
// is changing between fullscreen/window mode (ENTER key)
// btw part 2: in windows the plugin gets the window handle from the
// main emu, and doesn't create it's own window (if it would do it,
// some PAD or SPU plugins would not work anymore)
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// I shot the sheriff... last function called from emu
////////////////////////////////////////////////////////////////////////

long CALLBACK GL_GPUshutdown()
{
 //if(psxVSecure) free(psxVSecure);                      // kill emulated vram memory
 //psxVSecure=0;

 if (pGfxCardScreen)
      {
          _mem2_free(pGfxCardScreen);
          pGfxCardScreen = 0;
      }

 vram_ptr_orig = NULL;
 GlesVramTilingShutdown();

 return 0;
}

////////////////////////////////////////////////////////////////////////
// paint it black: simple func to clean up optical border garbage
////////////////////////////////////////////////////////////////////////

void PaintBlackBorders(void)
{
// short s;
// glDisable(GL_SCISSOR_TEST); glError();
// if(bTexEnabled) {glDisable(GL_TEXTURE_2D);bTexEnabled=FALSE;} glError();
// if(bOldSmoothShaded) {glShadeModel(GL_FLAT);bOldSmoothShaded=FALSE;} glError();
// if(bBlendEnable)     {glDisable(GL_BLEND);bBlendEnable=FALSE;} glError();
// glDisable(GL_ALPHA_TEST); glError();
//
// glEnable(GL_ALPHA_TEST); glError();
// glEnable(GL_SCISSOR_TEST); glError();

}

////////////////////////////////////////////////////////////////////////
// helper to draw scanlines
////////////////////////////////////////////////////////////////////////

//__inline void XPRIMdrawTexturedQuad(OGLVertex* vertex1, OGLVertex* vertex2,
//                                    OGLVertex* vertex3, OGLVertex* vertex4)
//{
//
//}

////////////////////////////////////////////////////////////////////////
// scanlines
////////////////////////////////////////////////////////////////////////

void SetScanLines(void)
{
}

////////////////////////////////////////////////////////////////////////
// blur, babe, blur (heavy performance hit for a so-so fullscreen effect)
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// Update display (swap buffers)... called in interlaced mode on
// every emulated vsync, otherwise whenever the displayed screen region
// has been changed
////////////////////////////////////////////////////////////////////////

int iLastRGB24=0;                                      // special vars for checking when to skip two display updates
int iSkipTwo=0;

void GPUvSinc(void){
updateDisplayGl();
}

static int TiledDisplayCpuUploadPending(void)
{
#if defined(GLES_VRAM_LR_TILING_S4_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S4_TEST)
    GlesVramRect displayRect;

    if (PSXDisplay.Disabled || PSXDisplay.RGB24 ||
        PSXDisplay.DisplayMode.x <= 0 || PSXDisplay.DisplayMode.y <= 0)
        return 0;
    displayRect.x = PSXDisplay.DisplayPosition.x;
    displayRect.y = PSXDisplay.DisplayPosition.y;
    displayRect.width = PSXDisplay.DisplayMode.x;
    displayRect.height = PSXDisplay.DisplayMode.y;
    return GlesVramTilingS3RectNeedsCpuUpload(&displayRect);
#else
    return 0;
#endif
}

#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
static void PrepareRgb24PresentationEfb(void)
{
    int viewportY = iResY - (rRatioRect.top + rRatioRect.bottom);

    /* S2/S4 use the EFB as a 640x512 tile workspace and leave OpenGX's
     * private projection set to that workspace.  RGB24 UploadScreen emits
     * display-local coordinates (normally 0..320 by 0..224), so explicitly
     * restore the display projection before drawing the movie.  Clear the
     * complete EFB first: unlike an RGB15 S4 present, the movie path does not
     * otherwise cover pixels outside its quad, which exposed stale tile data
     * around the top-left quarter of the picture. */
    glScissor(0, 0, iResX, iResY); glError();
    glClearColor2(0, 0, 0, 128); glError();
    glClear(uiBufferBits); glError();

    glViewport(rRatioRect.left, viewportY,
               rRatioRect.right, rRatioRect.bottom); glError();
    glScissor(rRatioRect.left, viewportY,
              rRatioRect.right, rRatioRect.bottom); glError();
    glMatrixMode(GL_PROJECTION); glError();
    glLoadIdentity(); glError();
    glOrtho(0, PSXDisplay.DisplayMode.x,
            PSXDisplay.DisplayMode.y, 0, -1, 1); glError();
    glMatrixMode(GL_MODELVIEW); glError();
    glLoadIdentity(); glError();
    glSetLoadMtxFlg();
}
#endif

void updateDisplayGl(void)                               // UPDATE DISPLAY
{
BOOL bBlur=FALSE;
BOOL tiledCpuUploadPending=FALSE;


bFakeFrontBuffer=FALSE;
bRenderFrontBuffer=FALSE;

//if(iRenderFVR)                                        // frame buffer read fix mode still active?
// {
//  iRenderFVR--;                                       // -> if some frames in a row without read access: turn off mode
//  if(!iRenderFVR) bFullVRam=FALSE;
// }

if(iLastRGB24 && iLastRGB24!=PSXDisplay.RGB24+1)      // (mdec) garbage check
 {
  iSkipTwo=2;                                         // -> skip two frames to avoid garbage if color mode changes
 }
iLastRGB24=0;

if(PSXDisplay.RGB24)// && !bNeedUploadAfter)          // (mdec) upload wanted?
 {
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
      /* UploadScreen builds a presentation image rather than a PS VRAM
       * tile.  Persist and detach the work EFB before reusing it, otherwise
       * the next command can mistake the movie image for the active tile. */
      GlesVramTilingS2SaveAndDiscard();
#endif
      PrepareFullScreenUpload(-1);
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
      PrepareRgb24PresentationEfb();
#endif
      UploadScreen(PSXDisplay.Interlaced);                // -> upload whole screen from psx vram
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
      GlesVramTilingS7DiscardFrontPresentation();
#endif
  bNeedUploadTest=FALSE;
  bNeedInterlaceUpdate=FALSE;
  bNeedUploadAfter=FALSE;
  bNeedRGB24Update=FALSE;
 }
else
if(bNeedInterlaceUpdate)                              // smaller upload?
 {
     #ifdef DISP_DEBUG
     //sprintf(txtbuffer, "updateDisplayGl_2 %d %d %d %d %d %d %d %d %d\r\n", PSXDisplay.Disabled, lClearOnSwap, iZBufferDepth, PSXDisplay.Interlaced, bNeedRGB24Update, xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
     //writeLogFile(txtbuffer);
     #endif // DISP_DEBUG
  bNeedInterlaceUpdate=FALSE;
  xrUploadArea=xrUploadAreaIL;                        // -> upload this rect
  UploadScreen(TRUE);
 }

#ifndef GLES_VRAM_LR_TILING_S2_EXPERIMENT
if (dwActFixes & AUTO_FIX_FF9) bCheckFF9G4(NULL);                 // special game fix for FF9
#endif

if(PreviousPSXDisplay.Range.x0||                      // paint black borders around display area, if needed
   PreviousPSXDisplay.Range.y0)
 PaintBlackBorders();

if(PSXDisplay.Disabled)                               // display disabled?
 {
  // moved here
  glDisable(GL_SCISSOR_TEST); glError();
  glClearColor2(0,0,0,128); glError();                 // -> clear whole backbuffer
  glClear(uiBufferBits); glError();
  glEnable(GL_SCISSOR_TEST); glError();
  gl_z=0.0f;
  bDisplayNotSet = TRUE;
  #ifdef DISP_DEBUG
  sprintf(txtbuffer, "updateDisplayGl Disabled\r\n");
  //DEBUG_print(txtbuffer, DBG_CDR1);
  writeLogFile(txtbuffer);
  #endif // DISP_DEBUG

  //gc_vout_disabled();
  //return;
 }

if(iSkipTwo)                                          // we are in skipping mood?
 {
  iSkipTwo--;
  iDrawnSomething=0;                                  // -> simply lie about something drawn
 }

//if(iBlurBuffer && !bSkipNextFrame)                    // "blur display" activated?
// {BlurBackBuffer();bBlur=TRUE;}                       // -> blur it

// if(iUseScanLines) SetScanLines();                     // "scan lines" activated? do it

// if(usCursorActive) ShowGunCursor();                   // "gun cursor" wanted? show 'em

//if(dwActFixes&128)                                    // special FPS limitation mode?
// {
//  if(bUseFrameLimit) PCFrameCap();                    // -> ok, do it
////   if(bUseFrameSkip || ulKeybits&KEY_SHOWFPS)
//   PCcalcfps();
// }

// if(gTexPicName) DisplayPic();                         // some gpu info picture active? display it

// if(bSnapShot) DoSnapShot();                           // snapshot key pressed? cheeeese :)

// if(ulKeybits&KEY_SHOWFPS)                             // wanna see FPS?
 {
//   sprintf(szDispBuf,"%06.1f",fps_cur);
//   DisplayText();                                      // -> show it
 }

//----------------------------------------------------//
// main buffer swapping (well, or skip it)

tiledCpuUploadPending=TiledDisplayCpuUploadPending();

if(UseFrameSkip)                                     // frame skipping active ?
 {
  if(!bSkipNextFrame)
   {
    if(iDrawnSomething || tiledCpuUploadPending)
     {
#if defined(VRAM_TILING_DIAG_ONLY) && defined(DISP_DEBUG)
      if(tiledCpuUploadPending)
       {
        sprintf(txtbuffer,
                "VTL CPU_PRESENT frame=%u via=display_update "
                "disp=%d,%d,%d,%d drawn=%d\r\n",
                g_s5TraceFrame,
                PSXDisplay.DisplayPosition.x,
                PSXDisplay.DisplayPosition.y,
                PSXDisplay.DisplayMode.x,
                PSXDisplay.DisplayMode.y,
                iDrawnSomething);
        TextureDiagAppend(txtbuffer);
       }
#endif
      flipEGL();
     }
   }
//    if((fps_skip < fFrameRateHz) && !(bSkipNextFrame))
//     {bSkipNextFrame = TRUE; fps_skip=fFrameRateHz;}
//    else bSkipNextFrame = FALSE;

 }
else                                                  // no skip ?
 {
  if(iDrawnSomething || tiledCpuUploadPending)
   {
#if defined(VRAM_TILING_DIAG_ONLY) && defined(DISP_DEBUG)
    if(tiledCpuUploadPending)
     {
      sprintf(txtbuffer,
              "VTL CPU_PRESENT frame=%u via=display_update "
              "disp=%d,%d,%d,%d drawn=%d\r\n",
              g_s5TraceFrame,
              PSXDisplay.DisplayPosition.x,
              PSXDisplay.DisplayPosition.y,
              PSXDisplay.DisplayMode.x,
              PSXDisplay.DisplayMode.y,
              iDrawnSomething);
      TextureDiagAppend(txtbuffer);
     }
#endif
    flipEGL();
   }
 }

iDrawnSomething=0;

//----------------------------------------------------//

//if(lClearOnSwap)                                      // clear buffer after swap?
// {
//     #ifdef DISP_DEBUG
//     sprintf(txtbuffer, "updateDisplayGl lClearOnSwap\r\n");
//     //DEBUG_print(txtbuffer, DBG_CDR1);
//     writeLogFile(txtbuffer);
//     #endif // DISP_DEBUG
//
//  unsigned char g,b,r;
//
//  if(bDisplayNotSet)                                  // -> set new vals
//   SetOGLDisplaySettings(1);
//
//  // lClearOnSwapColor (BGR)
//  g=((unsigned char)GREEN(lClearOnSwapColor));      // -> get col
//  b=((unsigned char)BLUE(lClearOnSwapColor));
//  r=((unsigned char)RED(lClearOnSwapColor));
//  glDisable(GL_SCISSOR_TEST); glError();
//  glClearColor2(r,g,b,128); glError();                 // -> clear
//  glClear(uiBufferBits); glError();
//  glEnable(GL_SCISSOR_TEST); glError();
//  lClearOnSwap=0;                                     // -> done
// }
//else
// {
////  if(bBlur) UnBlurBackBuffer();                       // unblur buff, if blurred before
//
//  if(iZBufferDepth)                                   // clear zbuffer as well (if activated)
//   {
//       #ifdef DISP_DEBUG
//     sprintf(txtbuffer, "Not lClearOnSwap\r\n");
//     //DEBUG_print(txtbuffer, DBG_CDR1);
//     writeLogFile(txtbuffer);
//     #endif // DISP_DEBUG
//
//    //glDisable(GL_SCISSOR_TEST); glError();
//    //glClear(GL_DEPTH_BUFFER_BIT); glError();
//    //glEnable(GL_SCISSOR_TEST); glError();
//   }
// }

gl_z=0.0f;

//----------------------------------------------------//
// additional uploads immediatly after swapping

if(bNeedUploadAfter)                                  // upload wanted?
 {
  bNeedUploadAfter=FALSE;
  bNeedUploadTest=FALSE;
      #ifdef DISP_DEBUG
      sprintf(txtbuffer, "bNeedUploadAfter %d %d %d %d\r\n", xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
      //DEBUG_print(txtbuffer, DBG_CDR2);
      writeLogFile(txtbuffer);
      #endif // DISP_DEBUG
  UploadScreen(-1);                                   // -> upload
 }

if(bNeedUploadTest)
 {
  bNeedUploadTest=FALSE;
  if(PSXDisplay.InterlacedTest &&
     //iOffscreenDrawing>2 &&
     PreviousPSXDisplay.DisplayPosition.x==PSXDisplay.DisplayPosition.x &&
     PreviousPSXDisplay.DisplayEnd.x==PSXDisplay.DisplayEnd.x &&
     PreviousPSXDisplay.DisplayPosition.y==PSXDisplay.DisplayPosition.y &&
     PreviousPSXDisplay.DisplayEnd.y==PSXDisplay.DisplayEnd.y)
   {
       #ifdef DISP_DEBUG
       sprintf(txtbuffer, "bNeedUploadTest %d %d %d %d\r\n", xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
       //DEBUG_print(txtbuffer, DBG_CDR2);
       writeLogFile(txtbuffer);
       #endif // DISP_DEBUG

    PrepareFullScreenUpload(TRUE);
    UploadScreen(TRUE);
   }
 }

//----------------------------------------------------//
// rumbling (main emu pad effect)

//if(iRumbleTime)                                       // shake screen by modifying view port
// {
//  int i1=0,i2=0,i3=0,i4=0;
//
//  iRumbleTime--;
//  if(iRumbleTime)
//   {
//    i1=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//    i2=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//    i3=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//    i4=((rand()*iRumbleVal)/RAND_MAX)-(iRumbleVal/2);
//   }
//
//  #ifdef DISP_DEBUG
//       sprintf(txtbuffer, "iRumbleTime\r\n");
//       writeLogFile(txtbuffer);
//       #endif // DISP_DEBUG
//  glViewport(rRatioRect.left+i1,
//             iResY-(rRatioRect.top+rRatioRect.bottom)+i2,
//             rRatioRect.right+i3,
//             rRatioRect.bottom+i4); glError();
// }

//----------------------------------------------------//



// if(ulKeybits&KEY_RESETTEXSTORE) ResetStuff();         // reset on gpu mode changes? do it before next frame is filled
}

////////////////////////////////////////////////////////////////////////
// update front display: smaller update func, if something has changed
// in the frontbuffer... dirty, but hey... real men know no pain
////////////////////////////////////////////////////////////////////////

//void updateFrontDisplayGl(void)
//{
//if(PreviousPSXDisplay.Range.x0||
//   PreviousPSXDisplay.Range.y0)
// PaintBlackBorders();
//
////if(iBlurBuffer) BlurBackBuffer();
//
////if(iUseScanLines) SetScanLines();
//
//// if(usCursorActive) ShowGunCursor();
//
//bFakeFrontBuffer=FALSE;
//bRenderFrontBuffer=FALSE;
//
//// if(gTexPicName) DisplayPic();
//// if(ulKeybits&KEY_SHOWFPS) DisplayText();
//
//if(iDrawnSomething)                                   // linux:
//      flipEGL();
//
//
////if(iBlurBuffer) UnBlurBackBuffer();
//}

////////////////////////////////////////////////////////////////////////
// check if update needed
////////////////////////////////////////////////////////////////////////
void ChangeDispOffsetsXGl(void)                          // CENTER X
{
long lx,l;short sO;

if(!PSXDisplay.Range.x1) return;                      // some range given?

l=PSXDisplay.DisplayMode.x;

l*=(long)PSXDisplay.Range.x1;                         // some funky calculation
l/=2560;lx=l;l&=0xfffffff8;

if(l==PreviousPSXDisplay.Range.x1) return;            // some change?

sO=PreviousPSXDisplay.Range.x0;                       // store old

if(lx>=PSXDisplay.DisplayMode.x)                      // range bigger?
 {
  PreviousPSXDisplay.Range.x1=                        // -> take display width
   PSXDisplay.DisplayMode.x;
  PreviousPSXDisplay.Range.x0=0;                      // -> start pos is 0
 }
else                                                  // range smaller? center it
 {
  PreviousPSXDisplay.Range.x1=l;                      // -> store width (8 pixel aligned)
   PreviousPSXDisplay.Range.x0=                       // -> calc start pos
   (PSXDisplay.Range.x0-500)/8;
  if(PreviousPSXDisplay.Range.x0<0)                   // -> we don't support neg. values yet
   PreviousPSXDisplay.Range.x0=0;

  if((PreviousPSXDisplay.Range.x0+lx)>                // -> uhuu... that's too much
     PSXDisplay.DisplayMode.x)
   {
    PreviousPSXDisplay.Range.x0=                      // -> adjust start
     PSXDisplay.DisplayMode.x-lx;
    PreviousPSXDisplay.Range.x1+=lx-l;                // -> adjust width
   }
 }

if(sO!=PreviousPSXDisplay.Range.x0)                   // something changed?
 {
  bDisplayNotSet=TRUE;                                // -> recalc display stuff
 }
}

////////////////////////////////////////////////////////////////////////

void ChangeDispOffsetsYGl(void)                          // CENTER Y
{
int iT;short sO;                                      // store previous y size

if(PSXDisplay.PAL) iT=48; else iT=28;                 // different offsets on PAL/NTSC

if(PSXDisplay.Range.y0>=iT)                           // crossed the security line? :)
 {
  PreviousPSXDisplay.Range.y1=                        // -> store width
   PSXDisplay.DisplayModeNew.y;

  sO=(PSXDisplay.Range.y0-iT-4)*PSXDisplay.Double;    // -> calc offset
  if(sO<0) sO=0;

  PSXDisplay.DisplayModeNew.y+=sO;                    // -> add offset to y size, too
 }
else sO=0;                                            // else no offset

if(sO!=PreviousPSXDisplay.Range.y0)                   // something changed?
 {
  PreviousPSXDisplay.Range.y0=sO;
  bDisplayNotSet=TRUE;                                // -> recalc display stuff
 }
}

////////////////////////////////////////////////////////////////////////
// Aspect ratio of ogl screen: simply adjusting ogl view port
////////////////////////////////////////////////////////////////////////

void SetAspectRatio(void)
{
float xs,ys,s;RECT r;

if(!PSXDisplay.DisplayModeNew.x) return;
if(!PSXDisplay.DisplayModeNew.y) return;

#if 0
xs=(float)iResX/(float)PSXDisplay.DisplayModeNew.x;
ys=(float)iResY/(float)height;

s=min(xs,ys);
r.right =(int)((float)PSXDisplay.DisplayModeNew.x*s);
r.bottom=(int)((float)height*s);
if(r.right  > iResX) r.right  = iResX;
if(r.bottom > iResY) r.bottom = iResY;
if(r.right  < 1)     r.right  = 1;
if(r.bottom < 1)     r.bottom = 1;

r.left = (iResX-r.right)/2;
r.top  = (iResY-r.bottom)/2;
if(r.bottom<rRatioRect.bottom ||
   r.right <rRatioRect.right)
 {
  RECT rC;
  glClearColor2(0,0,0,128);

  if(r.right <rRatioRect.right)
   {
    rC.left=0;
    rC.top=0;
    rC.right=r.left;
    rC.bottom=iResY;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);
    glClear(uiBufferBits);
    rC.left=iResX-rC.right;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);

    glClear(uiBufferBits);
   }

  if(r.bottom <rRatioRect.bottom)
   {
    rC.left=0;
    rC.top=0;
    rC.right=iResX;
    rC.bottom=r.top;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);

    glClear(uiBufferBits);
    rC.top=iResY-rC.bottom;
    glScissor(rC.left,rC.top,rC.right,rC.bottom);
    glClear(uiBufferBits);
   }

  bSetClip=TRUE;
  bDisplayNotSet=TRUE;
 }

rRatioRect=r;
#else
 // pcsx-rearmed hack
 //if (rearmed_get_layer_pos != NULL)
 //  rearmed_get_layer_pos(&rRatioRect.left, &rRatioRect.top, &rRatioRect.right, &rRatioRect.bottom);
  glScissor(rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,rRatioRect.bottom);
#endif

glViewport(rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,
           rRatioRect.bottom);               // init viewport
}

////////////////////////////////////////////////////////////////////////
// big ass check, if an ogl swap buffer is needed
////////////////////////////////////////////////////////////////////////

void updateDisplayIfChangedGl(void)
{
BOOL bUp;
int txStarted = 0;
GXDisplayMap proposed;

if ((PSXDisplay.DisplayMode.y == PSXDisplay.DisplayModeNew.y) &&
    (PSXDisplay.DisplayMode.x == PSXDisplay.DisplayModeNew.x))
 {
  if((PSXDisplay.RGB24      == PSXDisplay.RGB24New) &&
     (PSXDisplay.Interlaced == PSXDisplay.InterlacedNew))
     return;                                          // nothing has changed? fine, no swap buffer needed

  if (PSXDisplay.RGB24 != PSXDisplay.RGB24New)
   {
    GetProposedActiveMap(&proposed);
    proposed.rgb24 = PSXDisplay.RGB24New;
    txStarted = OnDisplayMappingWillChange(&proposed);
   }
 }
else                                                  // some res change?
 {
    GetProposedActiveMap(&proposed);
    proposed.vram_x1 = PSXDisplay.DisplayPosition.x + PSXDisplay.DisplayModeNew.x;
    proposed.vram_y1 = PSXDisplay.DisplayPosition.y + PSXDisplay.DisplayModeNew.y + PreviousPSXDisplay.DisplayModeNew.y;
    proposed.rgb24 = PSXDisplay.RGB24New;
    txStarted = OnDisplayMappingWillChange(&proposed);

    if (originalMode == ORIGINALMODE_ENABLE)
	{
		gx_vout_wait_idle();
		switchToTVMode(PSXDisplay.DisplayModeNew.x, PSXDisplay.DisplayModeNew.y, 0);
	}
    // Check if TVMode needs to be changed (240 or 480 lines)
    if (displayModeChanged)
    {
        if (originalMode == ORIGINALMODE_ENABLE && PSXDisplay.DisplayModeNew.y <= 288)
        {
            iResX = (PSXDisplay.DisplayModeNew.x <= 320) ? 640 : PSXDisplay.DisplayModeNew.x;
            iResY = 240;
        }
        else
        {
            iResX = 640;
            iResY = 480;
        }
        rRatioRect.right  = iResX;
        rRatioRect.bottom = iResY;
        displayModeChanged = 0;
    }

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity(); glError();
  glOrtho(0,PSXDisplay.DisplayModeNew.x,              // -> new psx resolution
            PSXDisplay.DisplayModeNew.y, 0, -1, 1); glError();
  #ifdef DISP_DEBUG
  sprintf(txtbuffer, "DisplayChanged glOrtho %d %d\r\n", PSXDisplay.DisplayModeNew.x, PSXDisplay.DisplayModeNew.y);
  writeLogFile(txtbuffer);
  sprintf(txtbuffer, "DisplayChanged GX_SetScissor %d %d %d %d\r\n", rRatioRect.left,
           iResY-(rRatioRect.top+rRatioRect.bottom),
           rRatioRect.right,rRatioRect.bottom);
  writeLogFile(txtbuffer);
  #endif // DISP_DEBUG
  if(bKeepRatio) SetAspectRatio();
 }

bDisplayNotSet = TRUE;                                // re-calc offsets/display area

bUp=FALSE;
if(PSXDisplay.RGB24!=PSXDisplay.RGB24New)             // clean up textures, if rgb mode change (usually mdec on/off)
 {
  PreviousPSXDisplay.RGB24=0;                         // no full 24 frame uploaded yet
  ResetTextureArea(FALSE);
  bUp=TRUE;
 }
 #ifdef DISP_DEBUG
  sprintf(txtbuffer, "updateDisplayIfChangedGl %d %d\r\n", PSXDisplay.RGB24, PSXDisplay.RGB24New);
  //DEBUG_print(txtbuffer, DBG_SPU3);
  writeLogFile(txtbuffer);
  #endif // DISP_DEBUG

PSXDisplay.RGB24         = PSXDisplay.RGB24New;       // get new infos
PSXDisplay.DisplayMode.y = PSXDisplay.DisplayModeNew.y;
PSXDisplay.DisplayMode.x = PSXDisplay.DisplayModeNew.x;
PSXDisplay.Interlaced    = PSXDisplay.InterlacedNew;

PSXDisplay.DisplayEnd.x=                              // calc new ends
 PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
PSXDisplay.DisplayEnd.y=
 PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;
PreviousPSXDisplay.DisplayEnd.x=
 PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
PreviousPSXDisplay.DisplayEnd.y=
 PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

ChangeDispOffsetsXGl();

if(iFrameLimit==2) SetAutoFrameCap();                 // set new fps limit vals (depends on interlace)

 if(bUp)
{
    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "updateDisplayIfChangedGl swap buffer\r\n");
    writeLogFile(txtbuffer);
    #endif // DISP_DEBUG
    updateDisplayGl();                              // yeah, real update (swap buffer)
}

if (txStarted)
    OnDisplayMappingChanged();
}

////////////////////////////////////////////////////////////////////////
// window mode <-> fullscreen mode (windows)
////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////
// swap update check (called by psx vsync function)
////////////////////////////////////////////////////////////////////////

//BOOL bSwapCheck(void)
//{
//static int iPosCheck=0;
//static PSXPoint_t pO;
//static PSXPoint_t pD;
//static int iDoAgain=0;
//
//if(PSXDisplay.DisplayPosition.x==pO.x &&
//   PSXDisplay.DisplayPosition.y==pO.y &&
//   PSXDisplay.DisplayEnd.x==pD.x &&
//   PSXDisplay.DisplayEnd.y==pD.y)
//     iPosCheck++;
//else iPosCheck=0;
//
//pO=PSXDisplay.DisplayPosition;
//pD=PSXDisplay.DisplayEnd;
//
//if(iPosCheck<=4) return FALSE;
//
//iPosCheck=4;
//
//if(PSXDisplay.Interlaced) return FALSE;
//
//if (bNeedInterlaceUpdate||
//    bNeedRGB24Update ||
//    bNeedUploadAfter||
//    bNeedUploadTest ||
//    iDoAgain
//   )
// {
//  iDoAgain=0;
//  if(bNeedUploadAfter)
//   iDoAgain=1;
//  if(bNeedUploadTest && PSXDisplay.InterlacedTest)
//   iDoAgain=1;
//
//  bDisplayNotSet = TRUE;
//  updateDisplayGl();
//
//  PreviousPSXDisplay.DisplayPosition.x=PSXDisplay.DisplayPosition.x;
//  PreviousPSXDisplay.DisplayPosition.y=PSXDisplay.DisplayPosition.y;
//  PreviousPSXDisplay.DisplayEnd.x=PSXDisplay.DisplayEnd.x;
//  PreviousPSXDisplay.DisplayEnd.y=PSXDisplay.DisplayEnd.y;
//  pO=PSXDisplay.DisplayPosition;
//  pD=PSXDisplay.DisplayEnd;
//
//  return TRUE;
// }
//
//return FALSE;
//}
////////////////////////////////////////////////////////////////////////
// gun cursor func: player=0-7, x=0-511, y=0-255
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// update lace is called every VSync. Basically we limit frame rate
// here, and in interlaced mode we swap ogl display buffers.
////////////////////////////////////////////////////////////////////////

#define CALLBACK
extern void CALLBACK GPUsetframelimit(unsigned long option);
static unsigned short usFirstPos=2;

void CALLBACK GL_GPUupdateLace(void)
{
if(!(dwActFixes&AUTO_FIX_CHRONO_CROSS))
 STATUSREG^=0x80000000;                               // interlaced bit toggle, if the CC game fix is not active (see gpuReadStatus)

    static char oldframeLimit = 1;

    if ( frameLimit[0] != oldframeLimit)
        GPUsetframelimit(0);
    oldframeLimit = frameLimit[0];

//if(!(dwActFixes&128))                                 // normal frame limit func
 OldGpuCheckFrameRate();

//if(iOffscreenDrawing==4)                              // special check if high offscreen drawing is on
// {
//  if(bSwapCheck()) return;
// }

if(PSXDisplay.Interlaced)                             // interlaced mode?
 {
  if(PSXDisplay.DisplayMode.x>0 && PSXDisplay.DisplayMode.y>0)
   {
       #ifdef DISP_DEBUG
       sprintf ( txtbuffer, "GPUupdateLace1 %d %x\r\n", iDrawnSomething, RGB24Uploaded);
       writeLogFile ( txtbuffer );
       #endif // DISP_DEBUG
       updateDisplayGl();                                  // -> swap buffers (new frame)
   }
 }
else if(usFirstPos==1)                                // initial updates (after startup)
 {
     #ifdef DISP_DEBUG
    sprintf ( txtbuffer, "GPUupdateLace3\r\n");
    writeLogFile ( txtbuffer );
    #endif // DISP_DEBUG
  updateDisplayGl();
 }
 else
 {
     int tiledCpuUploadPending;
     int legacyUploaded;

     #ifdef DISP_DEBUG
     sprintf ( txtbuffer, "GPUupdateLace5 %x %d %d %d %x\r\n", iDrawnSomething, PSXDisplay.Interlaced, PSXDisplay.Disabled, PSXDisplay.InterlacedTest, RGB24Uploaded);
     writeLogFile ( txtbuffer );
     #endif // DISP_DEBUG
     GPUupdateLace5Flg = 0;
     tiledCpuUploadPending = TiledDisplayCpuUploadPending();
     legacyUploaded = CheckFullScreenUpload();
     if (tiledCpuUploadPending || legacyUploaded ||
         (needFlipEGL == TRUE && (iDrawnSomething & 0x1) == 0))
     {
#if defined(VRAM_TILING_DIAG_ONLY) && defined(DISP_DEBUG)
         if (tiledCpuUploadPending)
         {
             sprintf(txtbuffer,
                     "VTL CPU_PRESENT frame=%u via=update_lace "
                     "disp=%d,%d,%d,%d drawn=%d legacy=%d\r\n",
                     g_s5TraceFrame,
                     PSXDisplay.DisplayPosition.x,
                     PSXDisplay.DisplayPosition.y,
                     PSXDisplay.DisplayMode.x,
                     PSXDisplay.DisplayMode.y,
                     iDrawnSomething, legacyUploaded);
             TextureDiagAppend(txtbuffer);
         }
#endif
         GPUupdateLace5Flg = 1;
         flipEGL();
         iDrawnSomething = 0;
     }
 }
}

////////////////////////////////////////////////////////////////////////
// process read request from GPU status register
////////////////////////////////////////////////////////////////////////

unsigned long CALLBACK GL_GPUreadStatus(void)
{
if(dwActFixes&AUTO_FIX_CHRONO_CROSS)                                 // CC game fix
 {
  static int iNumRead=0;
  if((iNumRead++)==2)
   {
    iNumRead=0;
    STATUSREG^=0x80000000;                            // interlaced bit toggle... we do it on every second read status... needed by some games (like ChronoCross)
   }
 }

if(iFakePrimBusy)                                     // 27.10.2007 - emulating some 'busy' while drawing... pfff... not perfect, but since our emulated dma is not done in an extra thread...
 {
  iFakePrimBusy--;

  if(iFakePrimBusy&1)                                 // we do a busy-idle-busy-idle sequence after/while drawing prims
   {
    GPUIsBusy;
    GPUIsNotReadyForCommands;
   }
  else
   {
    GPUIsIdle;
    GPUIsReadyForCommands;
   }
 }

return STATUSREG;
}

////////////////////////////////////////////////////////////////////////
// processes data send to GPU status register
// these are always single packet commands.
////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUwriteStatus(unsigned long gdata)
{
unsigned long lCommand=(gdata>>24)&0xff;

if(bIsFirstFrame) GLinitialize(NULL, NULL);           // real ogl startup (needed by some emus)

ulStatusControl[lCommand]=gdata;

switch(lCommand)
 {
  //--------------------------------------------------//
  // reset gpu
  case 0x00:
   GlesVramTilingReset();
   memset(ulGPUInfoVals,0x00,16*sizeof(unsigned long));
   lGPUstatusRet=0x14802000;
   PSXDisplay.Disabled=1;
   iDataWriteMode=iDataReadMode=DR_NORMAL;
   PSXDisplay.DrawOffset.x=PSXDisplay.DrawOffset.y=0;
   drawX=drawY=0;drawW=drawH=0;
   sSetMask=0;lSetMask=0;bCheckMask=FALSE;iSetMask=0;
   usMirror=0;
   GlobalTextAddrX=0;GlobalTextAddrY=0;
   GlobalTextTP=0;GlobalTextABR=0;
   PSXDisplay.RGB24=FALSE;
   PSXDisplay.Interlaced=FALSE;
   bUsingTWin = FALSE;
   return;

  // dis/enable display
  case 0x03:
   PreviousPSXDisplay.Disabled = PSXDisplay.Disabled;
   PSXDisplay.Disabled = (gdata & 1);

   if(PSXDisplay.Disabled)
        STATUSREG|=GPUSTATUS_DISPLAYDISABLED;
   else STATUSREG&=~GPUSTATUS_DISPLAYDISABLED;

   if (iOffscreenDrawing==4 &&
        PreviousPSXDisplay.Disabled &&
       !(PSXDisplay.Disabled))
    {

     if(!PSXDisplay.RGB24)
      {
       PrepareFullScreenUpload(TRUE);
       #ifdef DISP_DEBUG
       sprintf(txtbuffer, "dis/enable display %d %d %d %d\r\n", xrUploadArea.x0, xrUploadArea.x1, xrUploadArea.y0, xrUploadArea.y1);
       //DEBUG_print(txtbuffer, DBG_CDR2);
       writeLogFile(txtbuffer);
       #endif // DISP_DEBUG
       UploadScreen(TRUE);
       updateDisplayGl();
      }
    }

   return;

  // setting transfer mode
  case 0x04:
   gdata &= 0x03;                                     // only want the lower two bits

   iDataWriteMode=iDataReadMode=DR_NORMAL;
   if(gdata==0x02) iDataWriteMode=DR_VRAMTRANSFER;
   if(gdata==0x03) iDataReadMode =DR_VRAMTRANSFER;

   STATUSREG&=~GPUSTATUS_DMABITS;                     // clear the current settings of the DMA bits
   STATUSREG|=(gdata << 29);                          // set the DMA bits according to the received data

   return;

  // setting display position
  case 0x05:
   {
    short sx=(short)(gdata & 0x3ff);
    short sy;
    GXDisplayMap proposed;
    int txStarted = 0;
#ifdef DISP_DEBUG
    short diagOldCurrentX = PSXDisplay.DisplayPosition.x;
    short diagOldCurrentY = PSXDisplay.DisplayPosition.y;
    short diagOldPreviousX = PreviousPSXDisplay.DisplayPosition.x;
    short diagOldPreviousY = PreviousPSXDisplay.DisplayPosition.y;
#endif

    if(iGPUHeight==1024)
     {
      if(dwGPUVersion==2)
           sy = (short)((gdata>>12)&0x3ff);
      else sy = (short)((gdata>>10)&0x3ff);
     }
    else sy = (short)((gdata>>10)&0x3ff);             // really: 0x1ff, but we adjust it later

    if (sy & 0x200)
     {
      sy|=0xfc00;
      PreviousPSXDisplay.DisplayModeNew.y=sy/PSXDisplay.Double;
      sy=0;
     }
    else PreviousPSXDisplay.DisplayModeNew.y=0;

    if(sx>1000) sx=0;

    if(usFirstPos)
     {
      usFirstPos--;
      if(usFirstPos)
       {
        GetProposedActiveMap(&proposed);
        proposed.vram_x0 = sx;
        proposed.vram_y0 = sy;
        proposed.vram_x1 = sx + PSXDisplay.DisplayMode.x;
        proposed.vram_y1 = sy + PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;
        txStarted = OnDisplayMappingWillChange(&proposed);

        PreviousPSXDisplay.DisplayPosition.x = sx;
        PreviousPSXDisplay.DisplayPosition.y = sy;
        PSXDisplay.DisplayPosition.x = sx;
        PSXDisplay.DisplayPosition.y = sy;

        if (txStarted)
            OnDisplayMappingChanged();
        txStarted = 0;
       }
     }

    if(dwActFixes&8)
     {
      if((!PSXDisplay.Interlaced) &&
         PreviousPSXDisplay.DisplayPosition.x == sx  &&
         PreviousPSXDisplay.DisplayPosition.y == sy)
       return;

      GetProposedActiveMap(&proposed);
      proposed.vram_x0 = PreviousPSXDisplay.DisplayPosition.x;
      proposed.vram_y0 = PreviousPSXDisplay.DisplayPosition.y;
      proposed.vram_x1 = proposed.vram_x0 + PSXDisplay.DisplayMode.x;
      proposed.vram_y1 = proposed.vram_y0 + PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;
      txStarted = OnDisplayMappingWillChange(&proposed);

      PSXDisplay.DisplayPosition.x = PreviousPSXDisplay.DisplayPosition.x;
      PSXDisplay.DisplayPosition.y = PreviousPSXDisplay.DisplayPosition.y;
      PreviousPSXDisplay.DisplayPosition.x = sx;
      PreviousPSXDisplay.DisplayPosition.y = sy;
     }
    else
     {
      if((!PSXDisplay.Interlaced) &&
         PSXDisplay.DisplayPosition.x == sx  &&
         PSXDisplay.DisplayPosition.y == sy)
       return;
      GetProposedActiveMap(&proposed);
      proposed.vram_x0 = sx;
      proposed.vram_y0 = sy;
      proposed.vram_x1 = sx + PSXDisplay.DisplayMode.x;
      proposed.vram_y1 = sy + PSXDisplay.DisplayMode.y + PreviousPSXDisplay.DisplayModeNew.y;
      txStarted = OnDisplayMappingWillChange(&proposed);

      PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
      PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
      PSXDisplay.DisplayPosition.x = sx;
      PSXDisplay.DisplayPosition.y = sy;
     }

    PSXDisplay.DisplayEnd.x=
     PSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
    PSXDisplay.DisplayEnd.y=
     PSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

    PreviousPSXDisplay.DisplayEnd.x=
     PreviousPSXDisplay.DisplayPosition.x+ PSXDisplay.DisplayMode.x;
    PreviousPSXDisplay.DisplayEnd.y=
     PreviousPSXDisplay.DisplayPosition.y+ PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

    if (txStarted)
        OnDisplayMappingChanged();

#ifdef DISP_DEBUG
    g_textureDiagEvent++;
    sprintf(txtbuffer,
            "TDI GP1 frame=%u event=%u req=%d,%d "
            "old=%d,%d/%d,%d new=%d,%d/%d,%d fix8=%d\r\n",
            g_textureDiagFrame, g_textureDiagEvent, sx, sy,
            diagOldCurrentX, diagOldCurrentY,
            diagOldPreviousX, diagOldPreviousY,
            PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y,
            PreviousPSXDisplay.DisplayPosition.x,
            PreviousPSXDisplay.DisplayPosition.y,
            (dwActFixes & 8) != 0);
    TextureDiagAppend(txtbuffer);
#endif

    bDisplayNotSet = TRUE;

    if (!(PSXDisplay.Interlaced))
     {
         #ifdef DISP_DEBUG
         sprintf(txtbuffer, "settingDispInfo05 %d %d %d %d\r\n", PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y, PSXDisplay.DisplayMode.x * PSXDisplay.Range.x1 / 2560, PSXDisplay.Height);
         //DEBUG_print(txtbuffer, DBG_CDR2);
         writeLogFile(txtbuffer);
         #endif // DISP_DEBUG
         CHECK_SCREEN_INFO();

         if (GPUupdateLace5Flg && (iDrawnSomething & ~0x4) == 0)
         {

         }
         else
         {
             skipPreviousDisplayCheckOnce = TRUE;
             updateDisplayGl();
         }
     }
    else
    if(PSXDisplay.InterlacedTest &&
       ((PreviousPSXDisplay.DisplayPosition.x != PSXDisplay.DisplayPosition.x)||
        (PreviousPSXDisplay.DisplayPosition.y != PSXDisplay.DisplayPosition.y)))
     PSXDisplay.InterlacedTest--;

    return;
   }

  // setting width
  case 0x06:
   {
    short oldRangeX0 = PSXDisplay.Range.x0;
    short oldRangeX1 = PSXDisplay.Range.x1;
    int txStarted = 0;

    PSXDisplay.Range.x0=gdata & 0x7ff;      //0x3ff;
    PSXDisplay.Range.x1=(gdata>>12) & 0xfff;//0x7ff;

    PSXDisplay.Range.x1-=PSXDisplay.Range.x0;

    if (oldRangeX0 != PSXDisplay.Range.x0 ||
        oldRangeX1 != PSXDisplay.Range.x1)
        txStarted = OnDisplayMappingWillChange(NULL);

    CHECK_SCREEN_INFO();
    #ifdef DISP_DEBUG
      sprintf(txtbuffer, "settingDispInfo06 width %d %d\r\n", screenWidth, screenHeight);
      writeLogFile(txtbuffer);
      #endif // DISP_DEBUG
    ChangeDispOffsetsXGl();

    if (txStarted)
        OnDisplayMappingChanged();

    return;
   }

  // setting height
  case 0x07:
   {
    int txStarted = 0;

    PreviousPSXDisplay.Height = PSXDisplay.Height;

    PSXDisplay.Range.y0=gdata & 0x3ff;
    PSXDisplay.Range.y1=(gdata>>10) & 0x3ff;

    PSXDisplay.Height = PSXDisplay.Range.y1 -
                        PSXDisplay.Range.y0 +
                        PreviousPSXDisplay.DisplayModeNew.y;

    if (PreviousPSXDisplay.Height != PSXDisplay.Height)
     {
      txStarted = OnDisplayMappingWillChange(NULL);

      PSXDisplay.DisplayModeNew.y=PSXDisplay.Height*PSXDisplay.Double;
      ChangeDispOffsetsYGl();

      #ifdef DISP_DEBUG
      sprintf(txtbuffer, "settingDispInfo07 height %d %d\r\n", screenWidth, screenHeight);
      writeLogFile(txtbuffer);
      #endif // DISP_DEBUG
      CHECK_SCREEN_INFO();

      skipPreviousDisplayCheckOnce = TRUE;
      updateDisplayIfChangedGl();

      if (txStarted)
          OnDisplayMappingChanged();
     }

    return;
   }

  // setting display infos
  case 0x08:
   {
    GXDisplayMap proposed;
    int txStarted;

    GetProposedActiveMap(&proposed);
    proposed.rgb24 = (gdata & 0x10) ? TRUE : FALSE;
    proposed.vram_x1 = PSXDisplay.DisplayPosition.x +
                       dispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];
    proposed.vram_y1 = PSXDisplay.DisplayPosition.y +
                       PSXDisplay.Height * ((gdata & 0x04) ? 2 : 1) +
                       PreviousPSXDisplay.DisplayModeNew.y;
    txStarted = OnDisplayMappingWillChange(&proposed);

    PSXDisplay.DisplayModeNew.x = dispWidths[(gdata & 0x03) | ((gdata & 0x40) >> 4)];

   if (gdata&0x04) PSXDisplay.Double=2;
   else            PSXDisplay.Double=1;
   PSXDisplay.DisplayModeNew.y = PSXDisplay.Height*PSXDisplay.Double;

   ChangeDispOffsetsYGl();

   PSXDisplay.PAL           = (gdata & 0x08)?TRUE:FALSE; // if 1 - PAL mode, else NTSC
   PSXDisplay.RGB24New      = (gdata & 0x10)?TRUE:FALSE; // if 1 - TrueColor
   PSXDisplay.InterlacedNew = ((gdata & 0x24) ^ 0x24)?FALSE:TRUE; // if 0 - Interlace

   STATUSREG&=~GPUSTATUS_WIDTHBITS;                   // clear the width bits

   STATUSREG|=
              (((gdata & 0x03) << 17) |
              ((gdata & 0x40) << 10));                // set the width bits

   PreviousPSXDisplay.InterlacedNew=FALSE;
   if (PSXDisplay.InterlacedNew)
    {
     if(!PSXDisplay.Interlaced)
      {
       PSXDisplay.InterlacedTest=2;
       PreviousPSXDisplay.DisplayPosition.x = PSXDisplay.DisplayPosition.x;
       PreviousPSXDisplay.DisplayPosition.y = PSXDisplay.DisplayPosition.y;
       PreviousPSXDisplay.InterlacedNew=TRUE;
      }

     STATUSREG|=GPUSTATUS_INTERLACED;
    }
   else
    {
     PSXDisplay.InterlacedTest=0;
     STATUSREG&=~GPUSTATUS_INTERLACED;
    }

   if (PSXDisplay.PAL)
        STATUSREG|=GPUSTATUS_PAL;
   else STATUSREG&=~GPUSTATUS_PAL;

   if (PSXDisplay.Double==2)
        STATUSREG|=GPUSTATUS_DOUBLEHEIGHT;
   else STATUSREG&=~GPUSTATUS_DOUBLEHEIGHT;

   if (PSXDisplay.RGB24New)
        STATUSREG|=GPUSTATUS_RGB24;
   else STATUSREG&=~GPUSTATUS_RGB24;

     CHECK_SCREEN_INFO();
     #ifdef DISP_DEBUG
     sprintf(txtbuffer, "settingDispInfo08 %d %d %d %d\r\n", PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y, screenWidth, screenHeight);
     writeLogFile(txtbuffer);
     #endif // DISP_DEBUG

   skipPreviousDisplayCheckOnce = TRUE;
   updateDisplayIfChangedGl();

   if (txStarted)
       OnDisplayMappingChanged();

   return;
   }

  //--------------------------------------------------//
  // ask about GPU version and other stuff
  case 0x10:

   gdata&=0xff;

   switch(gdata)
    {
     case 0x02:
      GPUdataRet=ulGPUInfoVals[INFO_TW];              // tw infos
      return;
     case 0x03:
      GPUdataRet=ulGPUInfoVals[INFO_DRAWSTART];       // draw start
      return;
     case 0x04:
      GPUdataRet=ulGPUInfoVals[INFO_DRAWEND];         // draw end
      return;
     case 0x05:
     case 0x06:
      GPUdataRet=ulGPUInfoVals[INFO_DRAWOFF];         // draw offset
      return;
     case 0x07:
      if(dwGPUVersion==2)
           GPUdataRet=0x01;
      else GPUdataRet=0x02;                           // gpu type
      return;
     case 0x08:
     case 0x0F:                                       // some bios addr?
      GPUdataRet=0xBFC03720;
      return;
    }
   return;
  //--------------------------------------------------//
 }
}

////////////////////////////////////////////////////////////////////////
// vram read/write helpers
////////////////////////////////////////////////////////////////////////

BOOL bNeedWriteUpload=FALSE;

static inline void FinishedVRAMWrite(void)
{
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
 int s3Handled = 0;
 GlesVramRect s3Rect;
 uint64_t s3Sequence = 0;
#endif
 if (ReadbackEnabled())
 {
  MarkCpuVramWrite(VRAMWrite.x, VRAMWrite.y,
                   VRAMWrite.Width, VRAMWrite.Height);
#ifdef DISP_DEBUG
  if (VRAMWrite.Height >= 120)
   DebugLogVramHalf("A0Done", VRAMWrite.x, VRAMWrite.y,
                    VRAMWrite.Width, VRAMWrite.Height);
#endif
 }

#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
 s3Rect.x = VRAMWrite.x;
 s3Rect.y = VRAMWrite.y;
 s3Rect.width = VRAMWrite.Width;
 s3Rect.height = VRAMWrite.Height;
 s3Sequence = GlesVramTilingS3FinishCpuWrite(&s3Rect);
 if (s3Sequence != 0)
  {
   GlesVramTileSpan spans[GLES_VRAM_MAX_TILE_SPANS];
   int count = GlesVramBuildTileSpans(&s3Rect, spans);
   int span;
   for (span = 0; span < count; span++)
    InvalidateTextureArea(spans[span].vramRect.x,
                          spans[span].vramRect.y,
                          spans[span].vramRect.width,
                          spans[span].vramRect.height);
   s3Handled = 1;
#if defined(VRAM_TILING_DIAG_ONLY) && defined(DISP_DEBUG)
   {
    GlesVramRect currentDisplay;
    GlesVramRect previousDisplay;
    int currentPending;
    int previousPending;

    currentDisplay.x = PSXDisplay.DisplayPosition.x;
    currentDisplay.y = PSXDisplay.DisplayPosition.y;
    currentDisplay.width = PSXDisplay.DisplayMode.x;
    currentDisplay.height = PSXDisplay.DisplayMode.y;
    previousDisplay.x = PreviousPSXDisplay.DisplayPosition.x;
    previousDisplay.y = PreviousPSXDisplay.DisplayPosition.y;
    previousDisplay.width = PSXDisplay.DisplayMode.x;
    previousDisplay.height = PSXDisplay.DisplayMode.y;
    currentPending = GlesVramTilingS3RectNeedsCpuUpload(&currentDisplay);
    previousPending = GlesVramTilingS3RectNeedsCpuUpload(&previousDisplay);
    sprintf(txtbuffer,
            "VTL A0 frame=%u rect=%d,%d,%d,%d active=%d "
            "cur=%d,%d pending=%d prev=%d,%d pending=%d\r\n",
            g_s5TraceFrame,
            s3Rect.x, s3Rect.y, s3Rect.width, s3Rect.height,
            GlesVramTilingActiveTile(),
            currentDisplay.x, currentDisplay.y, currentPending,
            previousDisplay.x, previousDisplay.y, previousPending);
    TextureDiagAppend(txtbuffer);
   }
#endif
  }
#if defined(VRAM_TILING_DIAG_ONLY) && defined(DISP_DEBUG)
 else if (PSXDisplay.RGB24 || (STATUSREG & GPUSTATUS_RGB24))
  {
   sprintf(txtbuffer,
           "VTL A0_LEGACY frame=%u reason=rgb24 rect=%d,%d,%d,%d "
           "displayRgb=%d statusRgb=%d\r\n",
           g_s5TraceFrame,
           s3Rect.x, s3Rect.y, s3Rect.width, s3Rect.height,
           PSXDisplay.RGB24,
           (STATUSREG & GPUSTATUS_RGB24) != 0);
   TextureDiagAppend(txtbuffer);
  }
#endif
#endif

 if(bNeedWriteUpload)
  {
   bNeedWriteUpload=FALSE;
#ifdef GLES_VRAM_LR_TILING_S3_EXPERIMENT
   if (!s3Handled)
#endif
   CheckWriteUpdate();
  }

#if defined(DISP_DEBUG) && defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG)
 {
  unsigned int pages = S7AnimDiagPageMask(&s3Rect);
  unsigned int displays = S7AnimDiagDisplayMask(&s3Rect);
  if (pages != 0 || displays != 0)
   {
    S7AnimDiagBeginFrame();
    if (g_s7AnimDiagA0Count < S7_ANIM_DIAG_LIMIT)
     {
      unsigned int tiles = S7AnimDiagTileMask(&s3Rect);
      sprintf(txtbuffer,
              "TDI ANIM_A0 f=%u n=%u r=%d,%d,%d,%d p=%u d=%u t=%u "
              "seq=%u handled=%d need=%d cur=%d prev=%d\r\n",
              g_textureDiagFrame, g_s7AnimDiagA0Count,
              s3Rect.x, s3Rect.y, s3Rect.width, s3Rect.height,
              pages, displays, tiles,
              (unsigned int)s3Sequence, s3Handled,
              GlesVramTilingS3RectNeedsCpuUpload(&s3Rect),
              PSXDisplay.DisplayPosition.x,
              PreviousPSXDisplay.DisplayPosition.x);
      TextureDiagAppend(txtbuffer);
     }
    g_s7AnimDiagA0Count++;
   }
 }
#endif

 // set register to NORMAL operation
 iDataWriteMode = DR_NORMAL;

 // reset transfer values, to prevent mis-transfer of data
 VRAMWrite.ColsRemaining = 0;
 VRAMWrite.RowsRemaining = 0;
}

__inline void FinishedVRAMRead(void)
{
 g_readbackState = READBACK_IDLE;
#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
 g_s5ReadActive = 0;
#endif

 // set register to NORMAL operation
 iDataReadMode = DR_NORMAL;
 // reset transfer values, to prevent mis-transfer of data
 VRAMRead.x = 0;
 VRAMRead.y = 0;
 VRAMRead.Width = 0;
 VRAMRead.Height = 0;
 VRAMRead.ColsRemaining = 0;
 VRAMRead.RowsRemaining = 0;

 // indicate GPU is no longer ready for VRAM data in the STATUS REGISTER
 STATUSREG&=~GPUSTATUS_READYFORVRAM;
}

////////////////////////////////////////////////////////////////////////
// vram read check ex (reading from card's back/frontbuffer if needed...
// slow!)
////////////////////////////////////////////////////////////////////////

void CheckVRamReadEx(int x, int y, int dx, int dy)
{
    #ifdef DISP_DEBUG
    //sprintf(txtbuffer, "CheckVRamReadEx  \r\n");
    //DEBUG_print(txtbuffer, DBG_CORE2);
    #endif // DISP_DEBUG
}

////////////////////////////////////////////////////////////////////////
// vram read check (reading from card's back/frontbuffer if needed...
// slow!)
////////////////////////////////////////////////////////////////////////

// don't do GL vram read
//void CheckVRamRead(int x, int y, int dx, int dy, bool bFront)
//{
//}

void RestoreDispCopyInfo(void)
{
    float yscale = GX_GetYScaleFactor(vmode->efbHeight,vmode->xfbHeight);
    int xfbHeight = GX_SetDispCopyYScale(yscale);
    GX_SetScissor(0,0,vmode->fbWidth,vmode->efbHeight);
    GX_SetDispCopySrc(0,0,vmode->fbWidth,vmode->efbHeight);
    GX_SetDispCopyDst(vmode->fbWidth,xfbHeight);
    GX_SetCopyFilter(vmode->aa,vmode->sample_pattern,GX_TRUE,vmode->vfilter);
    GX_SetFieldMode(vmode->field_rendering,((vmode->viHeight==2*vmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
}

static inline unsigned short ReadGXRGB5A3PixelRaw(const unsigned char* buf, int texWidth, int px, int py)
{
    int blocksPerRow = texWidth >> 2;
    int blockIndex   = (py >> 2) * blocksPerRow + (px >> 2);
    int blockOffset  = blockIndex << 5;
    int pixelOffset  = (((py & 3) << 2) + (px & 3)) << 1;

    const unsigned char* p = buf + blockOffset + pixelOffset;

    return (unsigned short)(((unsigned short)p[0] << 8) | (unsigned short)p[1]);
}

static inline unsigned short GXRGB5A3ToPSX15(unsigned short gx)
{
    unsigned short psx;

    if (gx & 0x8000)
    {
        unsigned short r5 = (gx >> 10) & 0x1F;
        unsigned short g5 = (gx >> 5) & 0x1F;
        unsigned short b5 = gx & 0x1F;

        /* GX RGB5A3 stores RGB from high to low bits, while PS1 VRAM
         * stores red in bits 0-4 and blue in bits 10-14. */
        psx = (unsigned short)(r5 | (g5 << 5) | (b5 << 10));
    }
    else
    {
        unsigned short r4 = (gx >> 8) & 0xF;
        unsigned short g4 = (gx >> 4) & 0xF;
        unsigned short b4 = (gx >> 0) & 0xF;

        unsigned short r5 = (r4 << 1) | (r4 >> 3);
        unsigned short g5 = (g4 << 1) | (g4 >> 3);
        unsigned short b5 = (b4 << 1) | (b4 >> 3);
        psx = (unsigned short)(r5 | (g5 << 5) | (b5 << 10));
    }

    return psx;
}

static inline void CheckVRamRead(int x, int y, int dx, int dy)
{
 if (!ReadbackEnabled()) return;
 MergeReadbackToPsxVuw(x, y, dx - x, dy - y);
}

////////////////////////////////////////////////////////////////////////
// core read from vram
////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUreadDataMem(unsigned long * pMem, int iSize)
{
int i;

#ifdef DISP_DEBUG
static unsigned int readCallCount;
readCallCount++;
if (readCallCount <= 4 || iDataReadMode == DR_VRAMTRANSFER ||
    g_readbackState == READBACK_PENDING)
 {
  sprintf(txtbuffer,
          "VRB READ call=%u size=%d mode=%d state=%d enabled=%d "
          "rect=%d,%d %dx%d\r\n",
          readCallCount, iSize, iDataReadMode, g_readbackState,
          ReadbackEnabled(), VRAMRead.x, VRAMRead.y,
          VRAMRead.Width, VRAMRead.Height);
  writeLogFile(txtbuffer);
 }
#endif

if(iDataReadMode!=DR_VRAMTRANSFER) return;

GPUIsBusy;

if (g_readbackState == READBACK_PENDING)
 {
  g_lastReadMapping = ClassifyReadMapping(VRAMRead.x, VRAMRead.y,
                                          VRAMRead.Width, VRAMRead.Height);
  if (g_lastReadMapping == MAPPING_CURRENT)
   TryCaptureLiveFrame();
  else if (g_lastReadMapping == MAPPING_PREVIOUS &&
           TryCapturePreviousReadRect(VRAMRead.x, VRAMRead.y,
                                      VRAMRead.Width, VRAMRead.Height))
   g_lastCaptureResult = 3;
  else
   g_lastCaptureResult = (g_lastReadMapping == MAPPING_PREVIOUS) ? -5 : -6;
  MergeReadbackToPsxVuw(VRAMRead.x, VRAMRead.y,
                        VRAMRead.Width, VRAMRead.Height);
#ifdef DISP_DEBUG
  sprintf(txtbuffer,
          "VRB RESULT kind=%d capture=%d merged=%u src=%u/%u/%u "
          "changed=%u old=%08X new=%08X "
          "maskOnly=%u rgbChanged=%u mask=%u/%u "
          "full=%d partial=%d "
          "map=%u mv=%d cv=%d dirty=%d contam=%d mixed=%d untracked=%d "
          "live=%d/%u/%d/%d prev=%d/%u/%d/%d\r\n",
          g_lastReadMapping, g_lastCaptureResult, g_lastMergedPixels,
          g_lastMergedCurrentPixels, g_lastMergedPresentedPixels,
          g_lastMergedRebuildPixels,
          g_lastMergedChangedPixels,
          g_lastMergeOldHash, g_lastMergeNewHash,
          g_lastMergedMaskOnlyPixels,
          g_lastMergedRgbChangedPixels,
          g_lastMergeOldMaskPixels, g_lastMergeNewMaskPixels,
          CountEfbTiles(EFB_TILE_FULL),
          CountEfbTiles(EFB_TILE_PARTIAL),
          g_activeMap.map_id, g_activeMap.map_valid,
          g_activeMap.content_valid, g_activeMap.content_dirty,
          g_efbContaminated, g_mixedMappingSeen, g_untrackedEfbWrite,
          LIVE_SNAP()->valid, LIVE_SNAP()->map_id, LIVE_SNAP()->source,
          CountSnapshotTiles(LIVE_SNAP(), EFB_TILE_FULL),
          PREV_SNAP()->valid, PREV_SNAP()->map_id, PREV_SNAP()->source,
          CountSnapshotTiles(PREV_SNAP(), EFB_TILE_FULL));
  writeLogFile(txtbuffer);
#endif
  g_readbackState = READBACK_DONE;
 }

#ifdef GLES_VRAM_LR_TILING_S5_EXPERIMENT
if (g_s5ReadActive)
 {
  for(i=0;i<iSize;i++)
   {
    int row;
    int column;
    unsigned long value;

    if (VRAMRead.ColsRemaining <= 0 || VRAMRead.RowsRemaining <= 0)
     {FinishedVRAMRead();goto ENDREAD_GL;}

    row = VRAMRead.Height - VRAMRead.ColsRemaining;
    column = VRAMRead.Width - VRAMRead.RowsRemaining;
    value = (unsigned long)GETLE16(psxVuw +
        (((VRAMRead.y + row) & iGPUHeightMask) << 10) +
        ((VRAMRead.x + column) & 0x3ff));

    VRAMRead.RowsRemaining--;
    if (VRAMRead.RowsRemaining <= 0)
     {
      VRAMRead.RowsRemaining = VRAMRead.Width;
      VRAMRead.ColsRemaining--;
     }

    /* The existing ABI always supplies a high halfword, even when the
     * requested pixel count is odd.  Read the next logical VRAM location but
     * do not consume it when the transfer ended after the low halfword. */
    row = VRAMRead.Height - VRAMRead.ColsRemaining;
    column = VRAMRead.Width - VRAMRead.RowsRemaining;
    value |= (unsigned long)GETLE16(psxVuw +
        (((VRAMRead.y + row) & iGPUHeightMask) << 10) +
        ((VRAMRead.x + column) & 0x3ff)) << 16;
    /* GL_GPUreadData() returns GPUdataRet rather than its temporary output
     * word, so keep both read entry points consistent with the legacy path. */
    GPUdataRet = value;
    PUTLE32(pMem, GPUdataRet); pMem++;

    if (VRAMRead.ColsRemaining <= 0)
     {FinishedVRAMRead();goto ENDREAD_GL;}
    VRAMRead.RowsRemaining--;
    if (VRAMRead.RowsRemaining <= 0)
     {
      VRAMRead.RowsRemaining = VRAMRead.Width;
      VRAMRead.ColsRemaining--;
     }
    if (VRAMRead.ColsRemaining <= 0)
     {FinishedVRAMRead();goto ENDREAD_GL;}
   }
  goto ENDREAD_GL;
 }
#endif

// adjust read ptr, if necessary
while(VRAMRead.ImagePtr>=psxVuw_eom)
 VRAMRead.ImagePtr-=iGPUHeight*1024;
while(VRAMRead.ImagePtr<psxVuw)
 VRAMRead.ImagePtr+=iGPUHeight*1024;

//if((iSize>1) &&
//   !(VRAMRead.x      == VRAMWrite.x     &&
//     VRAMRead.y      == VRAMWrite.y     &&
//     VRAMRead.Width  == VRAMWrite.Width &&
//     VRAMRead.Height == VRAMWrite.Height))
// if (iSize > 1)
// CheckVRamRead(VRAMRead.x,VRAMRead.y,
//               VRAMRead.x+VRAMRead.RowsRemaining,
//               VRAMRead.y+VRAMRead.ColsRemaining);

for(i=0;i<iSize;i++)
 {
  // do 2 seperate 16bit reads for compatibility (wrap issues)
  if ((VRAMRead.ColsRemaining > 0) && (VRAMRead.RowsRemaining > 0))
   {
    // lower 16 bit
    GPUdataRet=(unsigned long)GETLE16(VRAMRead.ImagePtr);

    VRAMRead.ImagePtr++;
    if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
    VRAMRead.RowsRemaining --;

    if(VRAMRead.RowsRemaining<=0)
     {
      VRAMRead.RowsRemaining = VRAMRead.Width;
      VRAMRead.ColsRemaining--;
      VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
      if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
     }

    // higher 16 bit (always, even if it's an odd width)
    GPUdataRet|=(unsigned long)GETLE16(VRAMRead.ImagePtr)<<16;
    PUTLE32(pMem, GPUdataRet); pMem++;

    if(VRAMRead.ColsRemaining <= 0)
     {FinishedVRAMRead();goto ENDREAD_GL;}

    VRAMRead.ImagePtr++;
    if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
    VRAMRead.RowsRemaining--;
    if(VRAMRead.RowsRemaining<=0)
     {
      VRAMRead.RowsRemaining = VRAMRead.Width;
      VRAMRead.ColsRemaining--;
      VRAMRead.ImagePtr += 1024 - VRAMRead.Width;
      if(VRAMRead.ImagePtr>=psxVuw_eom) VRAMRead.ImagePtr-=iGPUHeight*1024;
     }
    if(VRAMRead.ColsRemaining <= 0)
     {FinishedVRAMRead();goto ENDREAD_GL;}
   }
  else {FinishedVRAMRead();goto ENDREAD_GL;}
 }

ENDREAD_GL:
GPUIsIdle;
 #ifdef DISP_DEBUG
 //sprintf(txtbuffer, "GL_GPUreadDataMem %08x \r\n", GPUdataRet);
 //writeLogFile(txtbuffer);
 #endif // DISP_DEBUG
}

unsigned long CALLBACK GL_GPUreadData(void)
{
 unsigned long l;
 GL_GPUreadDataMem(&l,1);
 return GPUdataRet;
}

////////////////////////////////////////////////////////////////////////
// helper table to know how much data is used by drawing commands
////////////////////////////////////////////////////////////////////////
extern const unsigned char primTableCX[];
//const unsigned char primTableCX[256] =
//{
//    // 00
//    0,0,3,0,0,0,0,0,
//    // 08
//    0,0,0,0,0,0,0,0,
//    // 10
//    0,0,0,0,0,0,0,0,
//    // 18
//    0,0,0,0,0,0,0,0,
//    // 20
//    4,4,4,4,7,7,7,7,
//    // 28
//    5,5,5,5,9,9,9,9,
//    // 30
//    6,6,6,6,9,9,9,9,
//    // 38
//    8,8,8,8,12,12,12,12,
//    // 40
//    3,3,3,3,0,0,0,0,
//    // 48
////    5,5,5,5,6,6,6,6,      //FLINE
//    254,254,254,254,254,254,254,254,
//    // 50
//    4,4,4,4,0,0,0,0,
//    // 58
////    7,7,7,7,9,9,9,9,    //    LINEG3    LINEG4
//    255,255,255,255,255,255,255,255,
//    // 60
//    3,3,3,3,4,4,4,4,    //    TILE    SPRT
//    // 68
//    2,2,2,2,3,3,3,3,    //    TILE1
//    // 70
//    2,2,2,2,3,3,3,3,
//    // 78
//    2,2,2,2,3,3,3,3,
//    // 80
//    4,0,0,0,0,0,0,0,
//    // 88
//    0,0,0,0,0,0,0,0,
//    // 90
//    0,0,0,0,0,0,0,0,
//    // 98
//    0,0,0,0,0,0,0,0,
//    // a0
//    3,0,0,0,0,0,0,0,
//    // a8
//    0,0,0,0,0,0,0,0,
//    // b0
//    0,0,0,0,0,0,0,0,
//    // b8
//    0,0,0,0,0,0,0,0,
//    // c0
//    3,0,0,0,0,0,0,0,
//    // c8
//    0,0,0,0,0,0,0,0,
//    // d0
//    0,0,0,0,0,0,0,0,
//    // d8
//    0,0,0,0,0,0,0,0,
//    // e0
//    0,1,1,1,1,1,1,0,
//    // e8
//    0,0,0,0,0,0,0,0,
//    // f0
//    0,0,0,0,0,0,0,0,
//    // f8
//    0,0,0,0,0,0,0,0
//};

////////////////////////////////////////////////////////////////////////
// processes data send to GPU data register
////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUwriteDataMem(unsigned long * pMem, int iSize)
{
unsigned char command;
unsigned long gdata=0;
int i=0;
GPUIsBusy;
GPUIsNotReadyForCommands;

STARTVRAM_GL:

if(iDataWriteMode==DR_VRAMTRANSFER)
 {
//    #if defined(DISP_DEBUG)
//    sprintf ( txtbuffer, "GPUwriteDataMem DR_VRAMTRANSFER %d \r\n", iSize );
//    writeLogFile(txtbuffer);
//    #endif // DISP_DEBUG

  // make sure we are in vram
  while(VRAMWrite.ImagePtr>=psxVuw_eom)
   VRAMWrite.ImagePtr-=iGPUHeight*1024;
  while(VRAMWrite.ImagePtr<psxVuw)
   VRAMWrite.ImagePtr+=iGPUHeight*1024;

  // now do the loop
  while(VRAMWrite.ColsRemaining>0)
   {
    while(VRAMWrite.RowsRemaining>0)
     {
      if(i>=iSize) {goto ENDVRAM_GL;}
      i++;

       gdata=GETLE32(pMem); pMem++;

       // Write odd pixel - Wrap from beginning to next index if going past GPU width
       if (VRAMWrite.Width + VRAMWrite.x - VRAMWrite.RowsRemaining >= 1024)
       {
           PUTLE16(((VRAMWrite.ImagePtr++) - 1024), (unsigned short)gdata);
       }
       else
       {
           PUTLE16(VRAMWrite.ImagePtr++, (unsigned short)gdata);
       }
      if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
      VRAMWrite.RowsRemaining --;

      if(VRAMWrite.RowsRemaining <= 0)
       {
        VRAMWrite.ColsRemaining--;
        if (VRAMWrite.ColsRemaining <= 0)             // last pixel is odd width
         {
           gdata=(gdata&0xFFFF)|(((unsigned long)GETLE16(VRAMWrite.ImagePtr))<<16);
          FinishedVRAMWrite();
          goto ENDVRAM_GL;
         }
        VRAMWrite.RowsRemaining = VRAMWrite.Width;
        VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
       }

       // Write even pixel - Wrap from beginning to next index if going past GPU width
       if (VRAMWrite.Width + VRAMWrite.x - VRAMWrite.RowsRemaining >= 1024)
       {
           PUTLE16(((VRAMWrite.ImagePtr++) - 1024), (unsigned short)(gdata>>16));
       }
       else
       {
           PUTLE16(VRAMWrite.ImagePtr++, (unsigned short)(gdata>>16));
       }
      if(VRAMWrite.ImagePtr>=psxVuw_eom) VRAMWrite.ImagePtr-=iGPUHeight*1024;
      VRAMWrite.RowsRemaining --;
     }

    VRAMWrite.RowsRemaining = VRAMWrite.Width;
    VRAMWrite.ColsRemaining--;
    VRAMWrite.ImagePtr += 1024 - VRAMWrite.Width;
   }

  FinishedVRAMWrite();
 }

ENDVRAM_GL:

if(iDataWriteMode==DR_NORMAL)
 {
//    #if defined(DISP_DEBUG)
//    sprintf ( txtbuffer, "GPUwriteDataMem DR_NORMAL %d \r\n", iSize );
//    writeLogFile(txtbuffer);
//    #endif // DISP_DEBUG

  void (* *primFunc)(unsigned char *);
  if(bSkipNextFrame) primFunc=primTableSkipGx;
  else               primFunc=primTableJGx;

  for(;i<iSize;)
   {
    if(iDataWriteMode==DR_VRAMTRANSFER) goto STARTVRAM_GL;

     gdata=GETLE32(pMem); pMem++; i++;

    if(gpuDataC == 0)
     {
      command = (unsigned char)((gdata>>24) & 0xff);

      if(primTableCX[command])
       {
        gpuDataC = primTableCX[command];
        gpuCommand = command;
         PUTLE32(&gpuDataM[0], gdata);
        gpuDataP = 1;
       }
      else continue;
     }
    else
     {
       PUTLE32(&gpuDataM[gpuDataP], gdata);
      if(gpuDataC>128)
       {
        if((gpuDataC==254 && gpuDataP>=3) ||
           (gpuDataC==255 && gpuDataP>=4 && !(gpuDataP&1)))
         {
           if((gdata & 0xF000F000) == 0x50005000)
           gpuDataP=gpuDataC-1;
         }
       }
      gpuDataP++;
     }

    if(gpuDataP == gpuDataC)
     {
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
      int packetWords = gpuDataC;
      int s2Command = 0;
#endif
#ifdef VRAM_APERTURE_DIAG
      VramApertureTrackPacket(gpuCommand, gpuDataM);
#endif
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
      if (!bSkipNextFrame)
       {
        GlesVramRect drawArea;
        GlesVramRect frontDisplay;
        GlesVramPoint drawOffset;
        drawArea.x = PSXDisplay.DrawArea.x0;
        drawArea.y = PSXDisplay.DrawArea.y0;
        drawArea.width = PSXDisplay.DrawArea.x1 -
                         PSXDisplay.DrawArea.x0 + 1;
        drawArea.height = PSXDisplay.DrawArea.y1 -
                          PSXDisplay.DrawArea.y0 + 1;
        drawOffset.x = PSXDisplay.DrawOffset.x;
        drawOffset.y = PSXDisplay.DrawOffset.y;
        frontDisplay.x = PSXDisplay.DisplayPosition.x;
        frontDisplay.y = PSXDisplay.DisplayPosition.y;
        frontDisplay.width = PSXDisplay.DisplayMode.x;
        frontDisplay.height = PSXDisplay.DisplayMode.y;
        s2Command = GlesVramTilingS2BeginCommand(
            gpuCommand, gpuDataM, packetWords,
            &drawArea, &drawOffset, &frontDisplay);
       }
#endif
      gpuDataC=gpuDataP=0;
#ifdef GLES_VRAM_LR_TILING_S2_EXPERIMENT
      if (s2Command)
       {
        primFunc[gpuCommand]((unsigned char *)gpuDataM);
        GlesVramTilingS2EndCommand();
       }
      else
#endif
       {
        BeginEfbDrawContext();
        primFunc[gpuCommand]((unsigned char *)gpuDataM);
        EndEfbDrawContext();
       }

       if (dwActFixes & AUTO_FIX_GPU_BUSY)      // hack for emulating "gpu busy" in some games
       iFakePrimBusy=4;
     }
   }
 }

GPUdataRet=gdata;

GPUIsReadyForCommands;
GPUIsIdle;
}

////////////////////////////////////////////////////////////////////////

void CALLBACK GL_GPUwriteData(unsigned long gdata)
{
 PUTLE32(&gdata, gdata);
 GL_GPUwriteDataMem(&gdata,1);
}


////////////////////////////////////////////////////////////////////////
// Pete Special: make an 'intelligent' dma chain check (<-Tekken3)
////////////////////////////////////////////////////////////////////////

static unsigned long lUsedAddr[3];

__inline BOOL CheckForEndlessLoop(unsigned long laddr)
{
if(laddr==lUsedAddr[1]) return TRUE;
if(laddr==lUsedAddr[2]) return TRUE;

if(laddr<lUsedAddr[0]) lUsedAddr[1]=laddr;
else                   lUsedAddr[2]=laddr;
lUsedAddr[0]=laddr;
return FALSE;
}

////////////////////////////////////////////////////////////////////////
// core gives a dma chain to gpu: same as the gpuwrite interface funcs
////////////////////////////////////////////////////////////////////////

long CALLBACK GL_GPUdmaChain(unsigned long * baseAddrL, unsigned long addr, uint32_t *progress_addr, int32_t *cycles_last_cmd)
{
 unsigned char * baseAddrB;
 unsigned int DMACommandCounter = 0;
 long dmaWords = 0;


if(bIsFirstFrame) GLinitialize(NULL, NULL);

GPUIsBusy;

lUsedAddr[0]=lUsedAddr[1]=lUsedAddr[2]=0xffffff;

baseAddrB = (unsigned char*) baseAddrL;

do
 {
  if(iGPUHeight==512) addr&=0x1FFFFC;

  if(DMACommandCounter++ > 2000000) break;
  if(CheckForEndlessLoop(addr)) break;

   short count = baseAddrB[addr+3];
   dmaWords += 1 + count;

   unsigned long dmaMem=addr+4;

  if(count>0) GL_GPUwriteDataMem(&baseAddrL[dmaMem>>2],count);

   addr = GETLE32(&baseAddrL[addr>>2])&0xffffff;
  }
 while (!(addr & 0x800000)); // contrary to some documentation, the end-of-linked-list marker is not actually 0xFF'FFFF
                             // any pointer with bit 23 set will do.

 GPUIsIdle;

 return dmaWords;
}

////////////////////////////////////////////////////////////////////////
// save state funcs
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////

long CALLBACK GL_GPUfreeze(unsigned long ulGetFreezeData,GPUFreeze_t * pF)
{
if(ulGetFreezeData==2)
 {
  long lSlotNum=*((long *)pF);
  if(lSlotNum<0) return 0;
  if(lSlotNum>8) return 0;
  //lSelectedSlot=lSlotNum+1;
  return 1;
 }

if(!pF)                    return 0;
if(pF->ulFreezeVersion!=1) return 0;

if(ulGetFreezeData==1)
 {
  pF->ulStatus=STATUSREG;
  memcpy(pF->ulControl,ulStatusControl,256*sizeof(unsigned long));
  //memcpy(pF->psxVRam,  psxVub,         1024*iGPUHeight*2);

  return 1;
 }

if(ulGetFreezeData!=0) return 0;

STATUSREG=pF->ulStatus;
memcpy(ulStatusControl,pF->ulControl,256*sizeof(unsigned long));
//memcpy(psxVub,         pF->psxVRam,  1024*iGPUHeight*2);

ResetTextureArea(TRUE);

 GL_GPUwriteStatus(ulStatusControl[0]);
 GL_GPUwriteStatus(ulStatusControl[1]);
 GL_GPUwriteStatus(ulStatusControl[2]);
 GL_GPUwriteStatus(ulStatusControl[3]);
 GL_GPUwriteStatus(ulStatusControl[8]);
 GL_GPUwriteStatus(ulStatusControl[6]);
 GL_GPUwriteStatus(ulStatusControl[7]);
 GL_GPUwriteStatus(ulStatusControl[5]);
 GL_GPUwriteStatus(ulStatusControl[4]);
 return 1;
}

////////////////////////////////////////////////////////////////////////
// special "emu infos" / "emu effects" functions
////////////////////////////////////////////////////////////////////////

// pcsx-rearmed callbacks
void CALLBACK GL_GPUrearmedCallbacks(const struct rearmed_cbs *_cbs)
{
   #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUrearmedCallbacks 0\r\n");
 #endif // DISP_DEBUG
//   gpu.frameskip.set = _cbs->frameskip;
//  gpu.frameskip.advice = &_cbs->fskip_advice;
//  gpu.frameskip.force = &_cbs->fskip_force;
//  gpu.frameskip.dirty = (void *)&_cbs->fskip_dirty;
//  gpu.frameskip.active = 0;
//  gpu.frameskip.frame_ready = 1;
//  gpu.state.hcnt = _cbs->gpu_hcnt;
//  gpu.state.frame_count = _cbs->gpu_frame_count;
//  gpu.state.allow_interlace = _cbs->gpu_neon.allow_interlace;
//  gpu.state.enhancement_enable = _cbs->gpu_neon.enhancement_enable;
//  if (gpu.state.screen_centering_type != _cbs->screen_centering_type
//      || gpu.state.screen_centering_x != _cbs->screen_centering_x
//      || gpu.state.screen_centering_y != _cbs->screen_centering_y) {
//    gpu.state.screen_centering_type = _cbs->screen_centering_type;
//    gpu.state.screen_centering_x = _cbs->screen_centering_x;
//    gpu.state.screen_centering_y = _cbs->screen_centering_y;
//    update_width();
//    update_height();
//  }
//
//  gpu.mmap = _cbs->mmap;
//  gpu.munmap = _cbs->munmap;
//  gpu.gpu_state_change = _cbs->gpu_state_change;
//
//  // delayed vram mmap
//  if (gpu.vram == NULL)
//    map_vram();
//
//  if (_cbs->pl_vout_set_raw_vram)
//    _cbs->pl_vout_set_raw_vram(gpu.vram);
  #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUrearmedCallbacks 1\r\n");
 #endif // DISP_DEBUG
  renderer_set_config(_cbs);
  #ifdef DISP_DEBUG
 //writeLogFile("GL_GPUrearmedCallbacks 2\r\n");
 #endif // DISP_DEBUG
  vout_set_config(_cbs);
}

static void flipEGL(void)
{
    int presentSubmitted;
#if defined(GLES_VRAM_LR_TILING_S4_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S4_TEST)
    int s4Presented = 0;
#endif
#if defined(GLES_VRAM_LR_TILING_S5_EXPERIMENT) && defined(DISP_DEBUG) && \
    !defined(VRAM_TILING_DIAG_ONLY)
    static unsigned int s5DiagFrame;
    static GlesVramS5Stats s5PreviousStats;
#endif
#if defined(GLES_VRAM_LR_TILING_S5_EXPERIMENT) && defined(DISP_DEBUG) && \
    defined(VRAM_TILING_DIAG_ONLY)
    static GlesVramS5Stats s5TracePreviousStats;
#endif

#ifdef VRAM_APERTURE_DIAG
    VramApertureFlushFrame();
#endif

#if defined(GLES_VRAM_LR_TILING_S5_EXPERIMENT) && defined(DISP_DEBUG) && \
    !defined(VRAM_TILING_DIAG_ONLY)
    s5DiagFrame++;
    if ((s5DiagFrame % 60u) == 0)
    {
        GlesVramS5Stats stats;
        GlesVramTilingS5GetStats(&stats);
        if (stats.moveCount < s5PreviousStats.moveCount ||
            stats.c0Count < s5PreviousStats.c0Count ||
            stats.resolveWaits < s5PreviousStats.resolveWaits)
            memset(&s5PreviousStats, 0, sizeof(s5PreviousStats));
        sprintf(txtbuffer,
                "VTL S5 frames=60 move=%u c0=%u resolve=%u wait=%u "
                "blocks=%u totalWait=%u\r\n",
                stats.moveCount - s5PreviousStats.moveCount,
                stats.c0Count - s5PreviousStats.c0Count,
                stats.resolveCalls - s5PreviousStats.resolveCalls,
                stats.resolveWaits - s5PreviousStats.resolveWaits,
                stats.resolvedBlocks - s5PreviousStats.resolvedBlocks,
                stats.resolveWaits);
        writeLogFile(txtbuffer);
        s5PreviousStats = stats;
    }
#endif

    /* Parasite Eve II alternates two PS1 VRAM display pages while the GX
     * renderer has only one EFB. Keeping that EFB after a present allows a
     * translucent full-screen warning effect from one page to become the
     * blend destination of the other page. Restore the original clear-after-
     * copy behavior only for this title. */
    if (dwActFixes & AUTO_FIX_PE2_CLEAR_EFB)
        canClearFrameBuf = TRUE;

    #ifdef DISP_DEBUG
    sprintf(txtbuffer,
            "TDI PRESENT frame=%u events=%u draws=%u efbclears=%u "
            "swapclear=%d shown=%d drawn=%d disp=%d,%d prev=%d,%d "
            "%dx%d rgb=%d\r\n",
            g_textureDiagFrame, g_textureDiagEvent, g_textureDiagDraw,
            g_textureDiagEfbClears, canClearFrameBuf, canShowFps,
            iDrawnSomething,
            PSXDisplay.DisplayPosition.x, PSXDisplay.DisplayPosition.y,
            PreviousPSXDisplay.DisplayPosition.x,
            PreviousPSXDisplay.DisplayPosition.y,
            PSXDisplay.DisplayMode.x, PSXDisplay.DisplayMode.y,
            PSXDisplay.RGB24);
    DEBUG_print(txtbuffer, DBG_SPU3);
    TextureDiagAppend(txtbuffer);
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    {
        GlesVramS7XfbStats xfbStats;
        if (GlesVramTilingS7DiagGetXfbStats(&xfbStats))
        {
            sprintf(txtbuffer,
                    "TDI PIPE_XFB f=%u wh=%u,%u stride=%u bytes=%u "
                    "hash=%08x luma=%08x active=%u range=%u,%u\r\n",
                    xfbStats.frame, xfbStats.width, xfbStats.height,
                    xfbStats.stridePixels, xfbStats.byteCount,
                    xfbStats.hash, xfbStats.lumaHash,
                    xfbStats.lumaActive,
                    (unsigned int)xfbStats.lumaMin,
                    (unsigned int)xfbStats.lumaMax);
            TextureDiagAppend(txtbuffer);
        }
    }
#endif
#ifndef VRAM_TILING_DIAG_ONLY
#ifndef GLES_VRAM_LR_TILING_S7_ANIM_DIAG
    TextureDiagFlush();
#endif
#endif
    #endif // DISP_DEBUG

#if defined(GLES_VRAM_LR_TILING_S4_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S4_TEST)
    /* RGB24 has different byte packing and deliberately stays on the legacy
     * path.  RGB15 display windows are reconstructed from the authoritative
     * tile backings; presentation then owns the EFB and no tile remains
     * active. */
    if (!PSXDisplay.RGB24 &&
        PSXDisplay.DisplayMode.x > 0 &&
        PSXDisplay.DisplayMode.y > 0)
    {
        GlesVramRect displayRect;
        GlesVramRect presentedRect;
#if defined(DISP_DEBUG) && \
    defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG)
        int animUsedFront = 0;
        int animNeedBefore;
        int animActiveBefore;
        GlesVramS7AnimStats animStats;
#endif
        displayRect.x = PSXDisplay.DisplayPosition.x;
        displayRect.y = PSXDisplay.DisplayPosition.y;
        displayRect.width = PSXDisplay.DisplayMode.x;
        displayRect.height = PSXDisplay.DisplayMode.y;
#ifdef GLES_VRAM_LR_TILING_S7_EXPERIMENT
        #if defined(DISP_DEBUG) && \
            defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG)
        animUsedFront = GlesVramTilingS7SelectPresentation(
            &displayRect, &presentedRect);
        #else
        GlesVramTilingS7SelectPresentation(&displayRect, &presentedRect);
        #endif
#else
        presentedRect = displayRect;
#endif
#if defined(DISP_DEBUG) && \
    defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG)
        animNeedBefore = GlesVramTilingS3RectNeedsCpuUpload(&presentedRect);
        animActiveBefore = (int)GlesVramTilingActiveTile();
#endif
        s4Presented = GlesVramTilingS4Present(&presentedRect, 640, 480);
#if defined(DISP_DEBUG) && defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG)
        GlesVramTilingS7AnimGetAndResetStats(&animStats);
        S7AnimDiagBeginFrame();
        sprintf(txtbuffer,
                "TDI ANIM_PRESENT f=%u req=%d,%d sel=%d,%d wh=%d,%d "
                "front=%d act=%d p=%u t=%u need=%d/%d ok=%d "
                "a0=%u mv=%u fill=%u\r\n",
                g_textureDiagFrame,
                displayRect.x, displayRect.y,
                presentedRect.x, presentedRect.y,
                presentedRect.width, presentedRect.height,
                animUsedFront, animActiveBefore,
                S7AnimDiagPageMask(&presentedRect),
                S7AnimDiagTileMask(&presentedRect), animNeedBefore,
                GlesVramTilingS3RectNeedsCpuUpload(&presentedRect),
                s4Presented, g_s7AnimDiagA0Count,
                g_s7AnimDiagMoveCount, g_s7AnimDiagFillCount);
        TextureDiagAppend(txtbuffer);
        {
            int animTile;
            for (animTile = 0; animTile < 2; animTile++)
            {
                const GlesVramRect *upload =
                    &animStats.uploadBounds[animTile];
                const GlesVramRect *gpu = &animStats.gpuBounds[animTile];
                sprintf(txtbuffer,
                        "TDI ANIM_WORK f=%u tile=%c "
                        "up=%u,%u,%u commit=%u/%u "
                        "ub=%d,%d,%d,%d hash=%08x/%08x match=%d "
                        "gpu=%u gb=%d,%d,%d,%d "
                        "io=%u,%u,%u,%u\r\n",
                        g_textureDiagFrame, animTile == 0 ? 'L' : 'R',
                        animStats.uploadCalls[animTile],
                        animStats.uploadBlocks[animTile],
                        animStats.uploadRuns[animTile],
                        animStats.uploadCommits[animTile],
                        animStats.uploadCommitFailures[animTile],
                        upload->x, upload->y,
                        upload->width, upload->height,
                        animStats.uploadSourceHash[animTile],
                        animStats.uploadStagingHash[animTile],
                        animStats.uploadCalls[animTile] == 0 ||
                            animStats.uploadSourceHash[animTile] ==
                                animStats.uploadStagingHash[animTile],
                        animStats.gpuPasses[animTile],
                        gpu->x, gpu->y, gpu->width, gpu->height,
                        animStats.tileSelects[animTile],
                        animStats.saveCopies[animTile],
                        animStats.restores[animTile],
                        animStats.clears[animTile]);
                TextureDiagAppend(txtbuffer);
            }
        }
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
        if (animStats.pipelineValid)
        {
            unsigned int diagSegment;
            sprintf(txtbuffer,
                    "TDI PIPE f=%u src=%08x/%u cpu=%08x/%u "
                    "cpudiff=%u cb=%d,%d,%d,%d efb=%08x/%u "
                    "efbdiff=%u eb=%d,%d,%d,%d "
                    "owner=%u,%u,%u,%u pending=%u segs=%u\r\n",
                    g_textureDiagFrame,
                    animStats.sourceHash, animStats.sourceNonBlack,
                    animStats.cpuHash, animStats.cpuComparable,
                    animStats.cpuMismatch,
                    animStats.cpuMismatchBounds.x,
                    animStats.cpuMismatchBounds.y,
                    animStats.cpuMismatchBounds.width,
                    animStats.cpuMismatchBounds.height,
                    animStats.efbHash, animStats.efbNonBlack,
                    animStats.efbMismatch,
                    animStats.efbMismatchBounds.x,
                    animStats.efbMismatchBounds.y,
                    animStats.efbMismatchBounds.width,
                    animStats.efbMismatchBounds.height,
                    animStats.ownerCpuOnly, animStats.ownerGpuOnly,
                    animStats.ownerEqual, animStats.ownerInvalid,
                    animStats.ownerNeedsUpload, animStats.segmentCount);
            TextureDiagAppend(txtbuffer);
            for (diagSegment = 0;
                 diagSegment < animStats.segmentCount &&
                 diagSegment < GLES_VRAM_MAX_DISPLAY_SEGMENTS;
                 diagSegment++)
            {
                sprintf(txtbuffer,
                        "TDI PIPE_SEG f=%u n=%u hash=%08x active=%u\r\n",
                        g_textureDiagFrame, diagSegment,
                        animStats.segmentHash[diagSegment],
                        animStats.segmentNonBlack[diagSegment]);
                TextureDiagAppend(txtbuffer);
            }
            sprintf(txtbuffer,
                    "TDI PIPE_QUAD f=%u hash=%08x,%08x,%08x,%08x "
                    "active=%u,%u,%u,%u\r\n",
                    g_textureDiagFrame,
                    animStats.efbQuadrantHash[0],
                    animStats.efbQuadrantHash[1],
                    animStats.efbQuadrantHash[2],
                    animStats.efbQuadrantHash[3],
                    animStats.efbQuadrantNonBlack[0],
                    animStats.efbQuadrantNonBlack[1],
                    animStats.efbQuadrantNonBlack[2],
                    animStats.efbQuadrantNonBlack[3]);
            TextureDiagAppend(txtbuffer);
        }
#endif
#endif
    }
    if (!s4Presented)
        CapturePresentedEfbSnapshot();
#if defined(DISP_DEBUG) && defined(GLES_VRAM_LR_TILING_S7_ANIM_DIAG)
    TextureDiagFlush();
#endif
#else
    CapturePresentedEfbSnapshot();
#endif

#if defined(GLES_VRAM_LR_TILING_S5_EXPERIMENT) && \
    defined(VRAM_TILING_DIAG_ONLY) && defined(DISP_DEBUG) && \
    defined(GLES_VRAM_LR_TILING_S4_EXPERIMENT) && \
    !defined(GLES_VRAM_LR_TILING_S4_TEST)
    {
        GlesVramS5Stats stats;
        GlesVramDisplayPlan displayPlan;
        GlesVramRect displayRect;
        int displayPlanValid = 0;
        int segment;

        displayRect.x = PSXDisplay.DisplayPosition.x;
        displayRect.y = PSXDisplay.DisplayPosition.y;
        displayRect.width = PSXDisplay.DisplayMode.x;
        displayRect.height = PSXDisplay.DisplayMode.y;
        if (!PSXDisplay.RGB24 && displayRect.width > 0 &&
            displayRect.height > 0)
            displayPlanValid = GlesVramBuildDisplayPlan(&displayRect,
                                                        &displayPlan);

        GlesVramTilingS5GetStats(&stats);
        if (stats.moveCount < s5TracePreviousStats.moveCount ||
            stats.c0Count < s5TracePreviousStats.c0Count ||
            stats.resolveWaits < s5TracePreviousStats.resolveWaits)
            memset(&s5TracePreviousStats, 0,
                   sizeof(s5TracePreviousStats));
        sprintf(txtbuffer,
                "VTL FRAME frame=%u disp=%d,%d,%d,%d prev=%d,%d "
                "rgb24=%d presented=%d segments=%d active=%d drawn=%d "
                "move=%u c0=%u resolve=%u wait=%u blocks=%u\r\n",
                g_s5TraceFrame,
                displayRect.x, displayRect.y,
                displayRect.width, displayRect.height,
                PreviousPSXDisplay.DisplayPosition.x,
                PreviousPSXDisplay.DisplayPosition.y,
                PSXDisplay.RGB24, s4Presented,
                displayPlanValid ? displayPlan.segmentCount : 0,
                GlesVramTilingActiveTile(), iDrawnSomething,
                stats.moveCount - s5TracePreviousStats.moveCount,
                stats.c0Count - s5TracePreviousStats.c0Count,
                stats.resolveCalls - s5TracePreviousStats.resolveCalls,
                stats.resolveWaits - s5TracePreviousStats.resolveWaits,
                stats.resolvedBlocks - s5TracePreviousStats.resolvedBlocks);
        TextureDiagAppend(txtbuffer);
        if (displayPlanValid)
        {
            for (segment = 0; segment < displayPlan.segmentCount; segment++)
            {
                const GlesVramDisplaySegment *part =
                    &displayPlan.segment[segment];
                sprintf(txtbuffer,
                        "VTL SEG frame=%u index=%d tile=%d "
                        "src=%d,%d,%d,%d out=%d,%d,%d,%d\r\n",
                        g_s5TraceFrame, segment, part->tile,
                        part->sourceRect.x, part->sourceRect.y,
                        part->sourceRect.width, part->sourceRect.height,
                        part->outputRect.x, part->outputRect.y,
                        part->outputRect.width, part->outputRect.height);
                TextureDiagAppend(txtbuffer);
            }
        }
        s5TracePreviousStats = stats;
        TextureDiagFlush();
        g_s5TraceFrame++;
    }
#endif

    if (canShowFps)
    {
        // Write menu/debug text on screen
        showFpsAndDebugInfo();
        g_efbContaminated = TRUE;
    }

    // Check if TVMode needs to be changed (240 or 480 lines)
    if (originalMode == ORIGINALMODE_ENABLE)
    {
        extern int backFromMenu;
        if(backFromMenu)
        {
            backFromMenu = 0;
            gx_vout_wait_idle();
            switchToTVMode(PSXDisplay.DisplayModeNew.x, PSXDisplay.DisplayModeNew.y, 0);
        }
    }

#ifdef EFB_512_HEIGHT_TEST
    DrawEfb512HeightTest();
#endif

#ifdef GLES_VRAM_LR_TILING_S1_TEST
    GlesVramTilingRunS1VisualTest();
#endif

#ifdef GLES_VRAM_LR_TILING_S2_TEST
    DrawGlesVramS2VisualTest();
#endif

#ifdef GLES_VRAM_LR_TILING_S3_TEST
    DrawGlesVramS3VisualTest();
#endif

#ifdef GLES_VRAM_LR_TILING_S4_TEST
    GlesVramTilingS4DrawVisualTest();
#endif

#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
    GlesVramTilingS7DiagSetPresentFrame(g_textureDiagFrame);
#endif
    presentSubmitted = gx_vout_render(canClearFrameBuf);

    if (presentSubmitted && canClearFrameBuf)
        EfbDiscardedAfterPresent();

    clearLargeRange = 0;
    uploadedScreen = FALSE;
    needFlipEGL = presentSubmitted ? FALSE : TRUE;
    if (presentSubmitted)
        canClearFrameBuf = FALSE;
    canShowFps = FALSE;
    RGB24Uploaded = 0;
    glSetLoadMtxFlg();

    extern void resetTexCacheInfo(void);
    resetTexCacheInfo();
    ResetFramebufferTextureCapture();

#ifdef DISP_DEBUG
    g_textureDiagFrame++;
    g_textureDiagDraw = 0;
    g_textureDiagEvent = 0;
    g_textureDiagEfbClears = 0;
#endif
}

#include "../Gamecube/wiiSXconfig.h"
extern char screenMode;

long GL_GPUopen()
{
 int ret;

#ifdef VRAM_TILING_DIAG_ONLY
 g_s5TraceFrame = 1;
 g_textureDiagBufferUsed = 0;
#ifdef GLES_VRAM_LR_TILING_S7_PIPELINE_DIAG
 writeLogFile("VTL START build=S7_REGION_LOAD_FIX_DIAG\r\n");
#else
 writeLogFile("VTL START build=S5_DIAG_V2\r\n");
#endif
#endif

#ifdef VRAM_APERTURE_DIAG
 memset(g_vapCommandCount, 0, sizeof(g_vapCommandCount));
 memset(g_vapWriteRange, 0, sizeof(g_vapWriteRange));
 memset(g_vapReadRange, 0, sizeof(g_vapReadRange));
 g_vapFrame = 1;
#endif

 InitFPS();

 GPUsetframelimit(0);

 iResX = 640;
 iResY = 480;
 rRatioRect.left   = 0;
// if (screenMode != SCREENMODE_4x3)
// {
//     rRatioRect.left   = -104;
//     iResX = 744;
// }
 iOffscreenDrawing = 0;
 rRatioRect.top=0;
 rRatioRect.right  = iResX;
 rRatioRect.bottom = iResY;

 bIsFirstFrame = TRUE;
 bDisplayNotSet = TRUE;
 bSetClip = TRUE;
 CSTEXTURE = CSVERTEX = CSCOLOR = 0;
 canClearFrameBuf = FALSE;

 InitializeTextureStore();                             // init texture mem

 ret = GLinitialize(NULL, NULL);

 if (!GlesVramTilingInitialize())
 {
  GLcleanup();
  return -1;
 }

 gx_vout_open();

 ogx_draw_submitted_cb = OnEfbDrawSubmitted;

 return ret;
}

long GL_GPUclose(void)
{
 ogx_draw_submitted_cb = NULL;
 ResetVramReadbackState();
 GlesVramTilingShutdown();
 GLcleanup();                                          // close OGL
 return 0;
}

gpu_t glesGpu = {
    GL_GPUopen,
    GL_GPUinit,
    GL_GPUshutdown,
    GL_GPUclose,
    GL_GPUwriteStatus,
    GL_GPUwriteData,
    GL_GPUreadStatus,
    GL_GPUreadData,
    GL_GPUdmaChain,
    GL_GPUupdateLace,
    GL_GPUfreeze,
    GL_GPUreadDataMem,
    GL_GPUwriteDataMem,
    GPUsetframelimit
};

/***************************************************************************
                          texture.c  -  description
                             -------------------
    begin                : Sun Mar 08 2009
    copyright            : (C) 1999-2009 by Pete Bernert
    web                  : www.pbernert.com
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


////////////////////////////////////////////////////////////////////////////////////
// Texture related functions are here !
//
// The texture handling is heart and soul of this gpu. The plugin was developed
// 1999, by this time no shaders were available. Since the psx gpu is making
// heavy use of CLUT (="color lookup tables", aka palettized textures), it was
// an interesting task to get those emulated at good speed on NV TNT cards
// (which was my major goal when I created the first "gpuPeteTNT"). Later cards
// (Geforce256) supported texture palettes by an OGL extension, but at some point
// this support was dropped again by gfx card vendors.
// Well, at least there is a certain advatage, if no texture palettes extension can
// be used: it is possible to modify the textures in any way, allowing "hi-res"
// textures and other tweaks.
//
// My main texture caching is kinda complex: the plugin is allocating "n" 256x256 textures,
// and it places small psx texture parts inside them. The plugin keeps track what
// part (with what palette) it had placed in which texture, so it can re-use this
// part again. The more ogl textures it can use, the better (of course the managing/
// searching will be slower, but everything is faster than uploading textures again
// and again to a gfx card). My first card (TNT1) had 16 MB Vram, and it worked
// well with many games, but I recommend nowadays 64 MB Vram to get a good speed.
//
// Sadly, there is also a second kind of texture cache needed, for "psx texture windows".
// Those are "repeated" textures, so a psx "texture window" needs to be put in
// a whole texture to use the GL_TEXTURE_WRAP_ features. This cache can get full very
// fast in games which are having an heavy "texture window" usage, like RRT4. As an
// alternative, this plugin can use the OGL "palette" extension on texture windows,
// if available. Nowadays also a fragment shader can easily be used to emulate
// texture wrapping in a texture atlas, so the main cache could hold the texture
// windows as well (that's what I am doing in the OGL2 plugin). But currently the
// OGL1 plugin is a "shader-free" zone, so heavy "texture window" games will cause
// much texture uploads.
//
// Some final advice: take care if you change things in here. I've removed my ASM
// handlers (they didn't cause much speed gain anyway) for readability/portability,
// but still the functions/data structures used here are easy to mess up. I guess it
// can be a pain in the ass to port the plugin to another byte order :)
//
////////////////////////////////////////////////////////////////////////////////////

#ifdef _IN_GPU_LIB


#define _IN_TEXTURE

#include "../gpulib/stdafx.h"

#include "gpuDraw.h"
//#include "plugins.h"
#include "gpuExternals.h"
#include "gpuTexture.h"
#include "gpuClutKey.h"
#include "gpuPlugin.h"
#include "gpuPrim.h"

#include "../Gamecube/DEBUG.h"
#include "../gpulib/gpu.h"
#include "../mem2_manager.h"
#include "../deps/lightrec/memmanager.h"

#include <malloc.h>

////////////////////////////////////////////////////////////////////////
// texture conversion buffer ..
////////////////////////////////////////////////////////////////////////

GLubyte       ubPaletteBuffer[256][4];
GLuint        gTexMovieName=0;
GLuint        gTexBlurName=0;
GLuint        gTexFrameName=0;
static BOOL   gFramebufferTextureCaptured=FALSE;
static int    gFramebufferTextureWidth=0;
static int    gFramebufferTextureHeight=0;
static int    gFramebufferTextureDisplayX=0;
static int    gFramebufferTextureDisplayY=0;
static int    gFramebufferTextureAbr=0;
int           iTexGarbageCollection=1;
unsigned int  dwTexPageComp=0;
int           iVRamSize=0;
int           iClampType=GL_CLAMP_TO_EDGE;
int iFilter = GL_LINEAR;
void               (*LoadSubTexFn) (int,int,short,short);
//unsigned int       (*PalTexturedColourFn)  (unsigned int);

////////////////////////////////////////////////////////////////////////
// defines
////////////////////////////////////////////////////////////////////////

//#define PALCOL(x) PalTexturedColourFn (x)

#define CSUBSIZE  2048
//#define CSUBSIZEA 8192
#define CSUBSIZES 4096

//#define OFFA 0
//#define OFFB 2048
//#define OFFC 4096
//#define OFFD 6144

//#define XOFFA 0
//#define XOFFB 512
//#define XOFFC 1024
//#define XOFFD 1536

#define SOFFA 0
#define SOFFB 1024
#define SOFFC 2048
#define SOFFD 3072

//#define MAXWNDTEXCACHE 64

#define XCHECK(pos1,pos2) ((pos1.c.y2>=pos2.c.y1)&&(pos1.c.y1<=pos2.c.y2)&&(pos1.c.x2>=pos2.c.x1)&&(pos1.c.x1<=pos2.c.x2))
#define INCHECK(pos2,pos1) ((pos1.c.y2<=pos2.c.y2) && (pos1.c.y1>=pos2.c.y1) && (pos1.c.x2<=pos2.c.x2) && (pos1.c.x1>=pos2.c.x1))

////////////////////////////////////////////////////////////////////////

struct textureSubCacheEntryTagS;
struct textureSubCacheEntryTagS *CheckTextureInSubSCache(
 int TextureMode,uint64_t clutKey,unsigned short *pCache);
void            LoadSubTexturePageSort(int pageid, int mode, short cx, short cy);
//void            LoadPackedSubTexturePageSort(int pageid, int mode, short cx, short cy);
void            DefineSubTextureSort(void);
static void     PrepareSubTextureSortTexture(void);

////////////////////////////////////////////////////////////////////////
// some globals
////////////////////////////////////////////////////////////////////////

int  GlobalTexturePage;
GLint XTexS;
GLint YTexS;
GLint DXTexS;
GLint DYTexS;
int   iSortTexCnt=32;
BOOL  bUseFastMdec=FALSE;
BOOL  bUse15bitMdec=FALSE;
int   iFrameTexType=0;
int   iFrameReadType=0;

unsigned int  (*TCF[2]) (unsigned int);
//unsigned short (*PTCF[2]) (unsigned short);

////////////////////////////////////////////////////////////////////////
// texture cache implementation
////////////////////////////////////////////////////////////////////////

// "texture window" cache entry

typedef struct textureWndCacheEntryTag
{
 uint64_t       ClutKey;
 short          pageid;
 short          textureMode;
 short          Opaque;
 short          used;
 EXLong         pos;
 GLuint         texname;
 unsigned int   textureType;
} textureWndCacheEntry;

// "standard texture" cache entry (16 bytes per entry; keep this compact)

typedef struct textureSubCacheEntryTagS
{
 uint64_t         ClutKey;
 EXLong          pos;
 unsigned char   posTX;
 unsigned char   posTY;
 unsigned char   cTexID;
 unsigned char   drawInfo;
} textureSubCacheEntryS;

typedef char textureSubCacheEntryMustRemain16Bytes[
 sizeof(textureSubCacheEntryS)==16 ? 1 : -1];

#define SUBCACHE_COUNT_PTR(entry) ((char *)&((entry)->pos))


//---------------------------------------------

#define MAXWNDTEXCACHE 64
#define MAXTPAGES_MAX  64
#ifndef GX_TEXPATH_ATLAS_COUNT
#define GX_TEXPATH_ATLAS_COUNT 96
#endif
#if GX_TEXPATH_ATLAS_COUNT < 8 || GX_TEXPATH_ATLAS_COUNT > 255
#error GX_TEXPATH_ATLAS_COUNT must be between 8 and 255
#endif
#define MAXSORTTEX_MAX GX_TEXPATH_ATLAS_COUNT

//---------------------------------------------

textureWndCacheEntry     wcWndtexStore[MAXWNDTEXCACHE];    // only 64 entries
textureSubCacheEntryS *  pscSubtexStore[3][MAXTPAGES_MAX]; // 3 * (64 / 2) * 4096 * 16 = 6M MB
EXLong *                 pxSsubtexLeft [MAXSORTTEX_MAX];   // 196 * 2048 * 4 = 1568 KB
GLuint                   uiStexturePage[MAXSORTTEX_MAX];   // 196 * 4 = 160 B
static unsigned char     atlasHasSemiBacking[MAXSORTTEX_MAX];
static unsigned short    atlasSemiBackingCount;

unsigned short           usLRUTexPage=0;

int                      iMaxTexWnds=0;
int                      iTexWndTurn=0;
int                      iTexWndLimit=MAXWNDTEXCACHE/2;

GLubyte                  texturepart[256 * 256 *4];
//GLubyte *                texturebuffer=NULL;
unsigned int             g_x1,g_y1,g_x2,g_y2;
unsigned char            ubOpaqueDraw=0;

unsigned short MAXTPAGES     = MAXTPAGES_MAX / 2;
unsigned short CLUTMASK      = 0x7fff;
unsigned short CLUTYMASK     = 0x1ff;
unsigned short MAXSORTTEX    = MAXSORTTEX_MAX;

#ifdef GX_TEXPATH_PROFILE
typedef struct GXTexturePathProfileTag
{
 unsigned int subLookups;
 unsigned int subHits;
 unsigned int subMisses;
 unsigned int subMissNoKey;
 unsigned int subMissUv;
 unsigned int subMissClut;
 unsigned int windowLookups;
 unsigned int windowHits;
 unsigned int windowMisses;
 unsigned int windowReplacements;
 unsigned int invalidationCalls;
 unsigned int invalidatedEntries;
 unsigned int invalidationCallsByReason[TEX_INVALIDATE_REASON_COUNT];
 unsigned int invalidatedEntriesByReason[TEX_INVALIDATE_REASON_COUNT];
 unsigned long long invalidatedAreaByReason[TEX_INVALIDATE_REASON_COUNT];
 unsigned int invalidatedEntriesByReasonMode[TEX_INVALIDATE_REASON_COUNT][3];
 unsigned int missesAfterInvalidation[TEX_INVALIDATE_REASON_COUNT];
 unsigned int maxInvalidatedEntries;
 unsigned int maxInvalidatedReason;
 unsigned int maxInvalidatedX;
 unsigned int maxInvalidatedY;
 unsigned int maxInvalidatedW;
 unsigned int maxInvalidatedH;
 unsigned int garbageCollectCalls;
 unsigned int garbageCollectEntries;
 unsigned int bucketFullEvents;
 unsigned int atlasLocalEvictions;
 unsigned int atlasLocalEvictedEntries;
 unsigned int atlasFullClears;
 unsigned int compressCalls;
 unsigned int compressUploads;
 unsigned int compressFullClears;
 unsigned int semiBackingAllocations;
 unsigned int semiGrowthDenied;
 unsigned int atlasClearPreUsed;
 unsigned int atlasClearPreFreeRects;
 unsigned int atlasClearPreDeadRects;
 unsigned int atlasClearPreMaxW;
 unsigned int atlasClearPreMaxH;
 unsigned int atlasClearRequestW;
 unsigned int atlasClearRequestH;
 unsigned long long decodedTexels[3];
} GXTexturePathProfile;

#define GX_INVALIDATION_RECORD_MAX 128
typedef struct GXInvalidationRecordTag
{
 uint64_t clutKey;
 EXLong pos;
 unsigned char mode;
 unsigned char page;
 unsigned char reason;
} GXInvalidationRecord;

static GXTexturePathProfile gxTexturePathProfile;
static GXInvalidationRecord gxInvalidationRecords[GX_INVALIDATION_RECORD_MAX];
static unsigned int gxInvalidationRecordCount;
static unsigned int gxTextureProfileMinMem2Free=0xffffffffU;
static unsigned char gxTextureProfileLastMissReason;
#define GX_TEX_MISS_NOKEY 0
#define GX_TEX_MISS_UV    1
#define GX_TEX_MISS_CLUT  2
#define GX_TEXPATH_INC(field) (++gxTexturePathProfile.field)
#define GX_TEXPATH_ADD_DECODED(mode,count) \
 (gxTexturePathProfile.decodedTexels[(mode)] += (unsigned long long)(count))
#define GX_TEXPATH_INVALIDATE_CALL(reason,area) do { \
 gxTexturePathProfile.invalidationCalls++; \
 gxTexturePathProfile.invalidationCallsByReason[(reason)]++; \
 gxTexturePathProfile.invalidatedAreaByReason[(reason)]+=(area); \
} while(0)
#define GX_TEXPATH_INVALIDATED(reason,mode,page,entry) \
 ProfileRecordInvalidation((reason),(mode),(page),(entry))
#else
#define GX_TEXPATH_INC(field) ((void)0)
#define GX_TEXPATH_ADD_DECODED(mode,count) ((void)0)
#define GX_TEXPATH_INVALIDATE_CALL(reason,area) ((void)0)
#define GX_TEXPATH_INVALIDATED(reason,mode,page,entry) ((void)0)
#endif

/* A failed rectangle allocation used to discard the metadata for every
 * 256x256 atlas.  Keep the eviction cursor separate from usLRUTexPage:
 * usLRUTexPage is the atlas used by the most recent texture lookup, while
 * this cursor only rotates equally cheap eviction candidates. */
static unsigned short usAtlasEvictCursor;

static int CanGrowSemiBacking(void)
{
 return gx_mem2_free() >= GX_MEM2_MIN_FREE_BYTES + 256U * 256U * 2U;
}

static void RefreshAtlasSemiBacking(int atlas)
{
 GLuint texture;

 if(atlas<0 || atlas>=iSortTexCnt) return;
 texture=uiStexturePage[atlas];
 if(!atlasHasSemiBacking[atlas] && texture &&
    glTextureHasSemiTransData(texture))
  {
   atlasHasSemiBacking[atlas]=1;
   atlasSemiBackingCount++;
   GX_TEXPATH_INC(semiBackingAllocations);
  }
}

static int BuildAtlasPlacementOrder(unsigned char *order,int semiRequest,
                                    int allowSemiGrowth)
{
 int pass,index,count=0,atlas;

 /* Keep both backing planes on the same atlas/GL texture ID.  Only the
  * rectangle-placement order changes: semi-transparent requests fill an
  * atlas which already owns semiTransData before growing another plane. */
 for(pass=0;pass<2;pass++)
  for(index=0;index<iSortTexCnt;index++)
   {
    atlas=(usLRUTexPage+index)%iSortTexCnt;
    if(semiRequest)
     {
      if((pass==0 && atlasHasSemiBacking[atlas]) ||
         (pass==1 && allowSemiGrowth && !atlasHasSemiBacking[atlas]))
       order[count++]=(unsigned char)atlas;
     }
    else
     {
      if((pass==0 && !atlasHasSemiBacking[atlas]) ||
         (pass==1 && atlasHasSemiBacking[atlas]))
       order[count++]=(unsigned char)atlas;
     }
   }

 return count;
}

#ifdef GX_TEXPATH_PROFILE
static void ProfileRecordInvalidation(TextureInvalidateReason reason,int mode,
                                      int page,
                                      const textureSubCacheEntryS *entry)
{
 GXInvalidationRecord *record;

 gxTexturePathProfile.invalidatedEntries++;
 gxTexturePathProfile.invalidatedEntriesByReason[reason]++;
 if(mode>=0 && mode<3)
  gxTexturePathProfile.invalidatedEntriesByReasonMode[reason][mode]++;

 if(gxInvalidationRecordCount>=GX_INVALIDATION_RECORD_MAX) return;
 record=&gxInvalidationRecords[gxInvalidationRecordCount++];
 record->clutKey=entry->ClutKey;
 record->pos=entry->pos;
 record->mode=(unsigned char)mode;
 record->page=(unsigned char)page;
 record->reason=(unsigned char)reason;
}

static void ProfileClassifyPostInvalidationMiss(int mode,int page,
                                                uint64_t clutKey,EXLong pos)
{
 unsigned int i;

 for(i=0;i<gxInvalidationRecordCount;i++)
  {
   GXInvalidationRecord *record=&gxInvalidationRecords[i];
   if(record->mode==(unsigned char)mode &&
      record->page==(unsigned char)page &&
      record->clutKey==clutKey && XCHECK(record->pos,pos))
    {
     gxTexturePathProfile.missesAfterInvalidation[record->reason]++;
     return;
    }
  }
}

static void ProfileScanAtlas(unsigned int *used,unsigned int *freeRects,
                             unsigned int *deadRects,unsigned int *maxW,
                             unsigned int *maxH)
{
 unsigned int bestArea=0;
 int i,j;

 *used=*freeRects=*deadRects=*maxW=*maxH=0;
 for(i=0;i<iSortTexCnt;i++)
  {
   EXLong *base=pxSsubtexLeft[i];
   unsigned int count;

   if(!base) continue;
   count=GETLE32((unsigned long *)base);
   if(!count) continue;
   (*used)++;
   if(count>CSUBSIZE-2) count=CSUBSIZE-2;
   for(j=1;j<=(int)count;j++)
    {
     EXLong *rect=base+j;
     unsigned int area;

     if(rect->l==0xffffffff)
      {
       (*deadRects)++;
       continue;
      }
     (*freeRects)++;
     area=(unsigned int)rect->c.x2*(unsigned int)rect->c.y2;
     if(area>bestArea)
      {
       bestArea=area;
       *maxW=rect->c.x2;
       *maxH=rect->c.y2;
      }
    }
  }
}

static void ProfileCaptureAtlasClear(unsigned int requestW,
                                     unsigned int requestH)
{
 ProfileScanAtlas(&gxTexturePathProfile.atlasClearPreUsed,
                  &gxTexturePathProfile.atlasClearPreFreeRects,
                  &gxTexturePathProfile.atlasClearPreDeadRects,
                  &gxTexturePathProfile.atlasClearPreMaxW,
                  &gxTexturePathProfile.atlasClearPreMaxH);
 gxTexturePathProfile.atlasClearRequestW=requestW;
 gxTexturePathProfile.atlasClearRequestH=requestH;
}
#endif

#if defined(GX_TEXPATH_PROFILE) && defined(DISP_DEBUG)
extern char CdromId[10];
extern char CdromLabel[33];

static unsigned int gxTextureProfileSession;
static char gxTextureProfileCdromId[10];
static char gxTextureProfileCdromLabel[33];

static void DumpGXMemoryDiagnostics(void)
{
 struct mallinfo heap=mallinfo();
 uintptr_t arena1Lo=(uintptr_t)SYS_GetArena1Lo();
 uintptr_t arena1Hi=(uintptr_t)SYS_GetArena1Hi();
 uintptr_t arena2Lo=(uintptr_t)SYS_GetArena2Lo();
 uintptr_t arena2Hi=(uintptr_t)SYS_GetArena2Hi();
 unsigned int mem2Used=gx_mem2_used();
 unsigned int mem2Total=gx_mem2_total();
 unsigned int mem2Free=mem2Total>=mem2Used ? mem2Total-mem2Used : 0;

 sprintf(txtbuffer,
         "TDI MEM frame=%u a1=%08lx-%08lx/%uKB a2=%08lx-%08lx/%uKB "
         "heap=%u/%u/%u/%uKB,%u lr=%u/%u/%u/%u/%uKB "
         "mem2=%u/%uKB fail=%u/%u/%u@%08lx\r\n",
         g_textureDiagFrame,
         (unsigned long)arena1Lo,(unsigned long)arena1Hi,
         (unsigned int)((arena1Hi>=arena1Lo ? arena1Hi-arena1Lo : 0)>>10),
         (unsigned long)arena2Lo,(unsigned long)arena2Hi,
         (unsigned int)((arena2Hi>=arena2Lo ? arena2Hi-arena2Lo : 0)>>10),
         (unsigned int)(heap.arena>>10),
         (unsigned int)(heap.uordblks>>10),
         (unsigned int)(heap.fordblks>>10),
         (unsigned int)(heap.keepcost>>10),
         (unsigned int)heap.ordblks,
         lightrec_get_mem_usage(MEM_FOR_IR)>>10,
         lightrec_get_mem_usage(MEM_FOR_CODE)>>10,
         lightrec_get_mem_usage(MEM_FOR_MIPS_CODE)>>10,
         lightrec_get_mem_usage(MEM_FOR_LIGHTREC)>>10,
         lightrec_get_total_mem_usage()>>10,
         mem2Used>>10,mem2Free>>10,
         gx_mem2_allocation_failures(),
         gx_mem2_last_failed_size(),
         gx_mem2_largest_failed_size(),
         (unsigned long)gx_mem2_last_failed_caller());
 TextureDiagAppend(txtbuffer);
}

static void ResetGXTexturePathProfileSession(void)
{
 memset(&gxTexturePathProfile,0,sizeof(gxTexturePathProfile));
 gxInvalidationRecordCount=0;
 glResetTextureProfileStats();
 gxTextureProfileMinMem2Free=0xffffffffU;
 gxTextureProfileCdromId[0]='\0';
 gxTextureProfileCdromLabel[0]='\0';
}

static void DumpAndResetGXTexturePathProfile(void)
{
 OGXTextureProfileStats ogxStats;
 unsigned int atlasUsed=0,atlasBacked=0;
 unsigned int atlasFreeRects=0,atlasDeadRects=0;
 unsigned int atlasMaxW=0,atlasMaxH=0;
 unsigned int metaValid=0,metaSlots=0;
 unsigned int metaMaxBucket=0,metaNonEmptyBuckets=0,metaHoleBuckets=0;
 unsigned int metaCapacity;
 unsigned int mem2Used,mem2Total,mem2Free;
 int i,j,k,n;
 const char *currentId=CdromId[0] ? CdromId : "UNKNOWN";
 const char *currentLabel=CdromLabel[0] ? CdromLabel : "UNKNOWN";

 if(strcmp(gxTextureProfileCdromId,CdromId)!=0 ||
    strcmp(gxTextureProfileCdromLabel,CdromLabel)!=0)
  {
   gxTextureProfileSession++;
   gxTextureProfileMinMem2Free=0xffffffffU;
   strncpy(gxTextureProfileCdromId,CdromId,
           sizeof(gxTextureProfileCdromId)-1);
   gxTextureProfileCdromId[sizeof(gxTextureProfileCdromId)-1]='\0';
   strncpy(gxTextureProfileCdromLabel,CdromLabel,
           sizeof(gxTextureProfileCdromLabel)-1);
   gxTextureProfileCdromLabel[sizeof(gxTextureProfileCdromLabel)-1]='\0';
   sprintf(txtbuffer,
           "TDI GXTEX SESSION seq=%u cdid=%s label=\"%s\"\r\n",
           gxTextureProfileSession,currentId,currentLabel);
   TextureDiagAppend(txtbuffer);
  }

 glGetTextureProfileStats(&ogxStats);
 sprintf(txtbuffer,
         "TDI GXTEX seq=%u cdid=%s frame=%u sub=%u/%u/%u wnd=%u/%u/%u repl=%u "
         "decode=%llu,%llu,%llu image=%u subimage=%u upload=%llu/%llu "
         "direct=%u/%llu flush=%u/%llu drawdone=%u uploadsync=%u/%u tmem=%u/%u/%u/%u "
         "evict=%u/%u/%u restore=%u\r\n",
         gxTextureProfileSession,currentId,
         g_textureDiagFrame,
         gxTexturePathProfile.subLookups,
         gxTexturePathProfile.subHits,
         gxTexturePathProfile.subMisses,
         gxTexturePathProfile.windowLookups,
         gxTexturePathProfile.windowHits,
         gxTexturePathProfile.windowMisses,
         gxTexturePathProfile.windowReplacements,
         gxTexturePathProfile.decodedTexels[0],
         gxTexturePathProfile.decodedTexels[1],
         gxTexturePathProfile.decodedTexels[2],
         ogxStats.texImageCalls,
         ogxStats.texSubImageCalls,
         ogxStats.uploadedTexels,
         ogxStats.uploadedBytes,
         ogxStats.directSubImageCalls,
         ogxStats.directDecodedTexels,
         ogxStats.textureFlushRangeCalls,
         ogxStats.textureFlushedBytes,
         ogxStats.textureUploadDrawDoneCalls,
         ogxStats.textureUploadSyncSkips,
         ogxStats.textureUploadResidentWaits,
         ogxStats.tmemHits,
         ogxStats.tmemChangedReloads,
         ogxStats.tmemFreeLoads,
         ogxStats.tmemFullResets,
         ogxStats.tmemSingleEvictions,
         ogxStats.tmemEvictionFences,
         ogxStats.tmemEvictionWaits,
         ogxStats.tmemExternalRestores);
 TextureDiagAppend(txtbuffer);

 ProfileScanAtlas(&atlasUsed,&atlasFreeRects,&atlasDeadRects,
                  &atlasMaxW,&atlasMaxH);
 for(i=0;i<iSortTexCnt;i++)
  if(uiStexturePage[i]) atlasBacked++;
 for(i=0;i<3;i++)
  for(j=0;j<MAXTPAGES;j++)
   {
    textureSubCacheEntryS *base=pscSubtexStore[i][j];
    if(!base) continue;
    for(k=0;k<4;k++)
     {
      textureSubCacheEntryS *bucket=base+k*SOFFB;
      unsigned int count=GETLE32(SUBCACHE_COUNT_PTR(bucket));
      unsigned int validInBucket=0;
      if(count>SOFFB-2) count=SOFFB-2;
      if(count)
       {
        metaNonEmptyBuckets++;
        if(count>metaMaxBucket) metaMaxBucket=count;
       }
      metaSlots+=count;
      for(n=1;n<=(int)count;n++)
       if(bucket[n].ClutKey) {metaValid++;validInBucket++;}
      if(validInBucket<count) metaHoleBuckets++;
     }
   }
 metaCapacity=3U*(unsigned int)MAXTPAGES*4U*(SOFFB-2U);
 mem2Used=gx_mem2_used();
 mem2Total=gx_mem2_total();
 mem2Free=mem2Total>=mem2Used ? mem2Total-mem2Used : 0;
 if(mem2Free<gxTextureProfileMinMem2Free)
  gxTextureProfileMinMem2Free=mem2Free;
 sprintf(txtbuffer,
         "TDI GXMISS frame=%u cause=%u/%u/%u inval=%u/%u gc=%u/%u "
         "bucket=%u atlasclear=%u atlaslocal=%u/%u comp=%u/%u/%u atlas=%u/%u/%u "
         "semi=%u/%u/%u pages=%u/%u meta=%u/%u/%u mem2=%u/%u/%uKB\r\n",
         g_textureDiagFrame,
         gxTexturePathProfile.subMissNoKey,
         gxTexturePathProfile.subMissUv,
         gxTexturePathProfile.subMissClut,
         gxTexturePathProfile.invalidationCalls,
         gxTexturePathProfile.invalidatedEntries,
         gxTexturePathProfile.garbageCollectCalls,
         gxTexturePathProfile.garbageCollectEntries,
         gxTexturePathProfile.bucketFullEvents,
         gxTexturePathProfile.atlasFullClears,
         gxTexturePathProfile.atlasLocalEvictions,
         gxTexturePathProfile.atlasLocalEvictedEntries,
         gxTexturePathProfile.compressCalls,
         gxTexturePathProfile.compressUploads,
         gxTexturePathProfile.compressFullClears,
         atlasUsed,atlasBacked,(unsigned int)iSortTexCnt,
         (unsigned int)atlasSemiBackingCount,
         gxTexturePathProfile.semiBackingAllocations,
         gxTexturePathProfile.semiGrowthDenied,
         (unsigned int)MAXTPAGES,(unsigned int)MAXTPAGES_MAX,
         metaValid,metaSlots,metaCapacity,
         mem2Used>>10,mem2Free>>10,gxTextureProfileMinMem2Free>>10);
 TextureDiagAppend(txtbuffer);
 sprintf(txtbuffer,
         "TDI GXINVAL frame=%u order=other,upload,move,blockfill,titlefill,display,readback,draw "
         "calls=%u,%u,%u,%u,%u,%u,%u,%u entries=%u,%u,%u,%u,%u,%u,%u,%u "
         "area=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu "
         "reuse=%u,%u,%u,%u,%u,%u,%u,%u blockmode=%u,%u,%u titlemode=%u,%u,%u "
         "max=%u:%u@%u,%u+%ux%u\r\n",
         g_textureDiagFrame,
         gxTexturePathProfile.invalidationCallsByReason[0],
         gxTexturePathProfile.invalidationCallsByReason[1],
         gxTexturePathProfile.invalidationCallsByReason[2],
         gxTexturePathProfile.invalidationCallsByReason[3],
         gxTexturePathProfile.invalidationCallsByReason[4],
         gxTexturePathProfile.invalidationCallsByReason[5],
         gxTexturePathProfile.invalidationCallsByReason[6],
         gxTexturePathProfile.invalidationCallsByReason[7],
         gxTexturePathProfile.invalidatedEntriesByReason[0],
         gxTexturePathProfile.invalidatedEntriesByReason[1],
         gxTexturePathProfile.invalidatedEntriesByReason[2],
         gxTexturePathProfile.invalidatedEntriesByReason[3],
         gxTexturePathProfile.invalidatedEntriesByReason[4],
         gxTexturePathProfile.invalidatedEntriesByReason[5],
         gxTexturePathProfile.invalidatedEntriesByReason[6],
         gxTexturePathProfile.invalidatedEntriesByReason[7],
         gxTexturePathProfile.invalidatedAreaByReason[0],
         gxTexturePathProfile.invalidatedAreaByReason[1],
         gxTexturePathProfile.invalidatedAreaByReason[2],
         gxTexturePathProfile.invalidatedAreaByReason[3],
         gxTexturePathProfile.invalidatedAreaByReason[4],
         gxTexturePathProfile.invalidatedAreaByReason[5],
         gxTexturePathProfile.invalidatedAreaByReason[6],
         gxTexturePathProfile.invalidatedAreaByReason[7],
         gxTexturePathProfile.missesAfterInvalidation[0],
         gxTexturePathProfile.missesAfterInvalidation[1],
         gxTexturePathProfile.missesAfterInvalidation[2],
         gxTexturePathProfile.missesAfterInvalidation[3],
         gxTexturePathProfile.missesAfterInvalidation[4],
         gxTexturePathProfile.missesAfterInvalidation[5],
         gxTexturePathProfile.missesAfterInvalidation[6],
         gxTexturePathProfile.missesAfterInvalidation[7],
         gxTexturePathProfile.invalidatedEntriesByReasonMode[TEX_INVALIDATE_BLOCK_FILL][0],
         gxTexturePathProfile.invalidatedEntriesByReasonMode[TEX_INVALIDATE_BLOCK_FILL][1],
         gxTexturePathProfile.invalidatedEntriesByReasonMode[TEX_INVALIDATE_BLOCK_FILL][2],
         gxTexturePathProfile.invalidatedEntriesByReasonMode[TEX_INVALIDATE_TITLE_FILL][0],
         gxTexturePathProfile.invalidatedEntriesByReasonMode[TEX_INVALIDATE_TITLE_FILL][1],
         gxTexturePathProfile.invalidatedEntriesByReasonMode[TEX_INVALIDATE_TITLE_FILL][2],
         gxTexturePathProfile.maxInvalidatedReason,
         gxTexturePathProfile.maxInvalidatedEntries,
         gxTexturePathProfile.maxInvalidatedX,
         gxTexturePathProfile.maxInvalidatedY,
         gxTexturePathProfile.maxInvalidatedW,
         gxTexturePathProfile.maxInvalidatedH);
 TextureDiagAppend(txtbuffer);
 sprintf(txtbuffer,
         "TDI GXALLOC frame=%u bucket=%u/%u/%u free=%u/%u/%ux%u "
         "pressurepre=%u/%u/%u/%ux%u request=%ux%u localevict=%u/%u\r\n",
         g_textureDiagFrame,metaMaxBucket,metaNonEmptyBuckets,metaHoleBuckets,
         atlasFreeRects,atlasDeadRects,atlasMaxW,atlasMaxH,
         gxTexturePathProfile.atlasClearPreUsed,
         gxTexturePathProfile.atlasClearPreFreeRects,
         gxTexturePathProfile.atlasClearPreDeadRects,
         gxTexturePathProfile.atlasClearPreMaxW,
         gxTexturePathProfile.atlasClearPreMaxH,
         gxTexturePathProfile.atlasClearRequestW,
         gxTexturePathProfile.atlasClearRequestH,
         gxTexturePathProfile.atlasLocalEvictions,
         gxTexturePathProfile.atlasLocalEvictedEntries);
 TextureDiagAppend(txtbuffer);
 DumpGXMemoryDiagnostics();
 memset(&gxTexturePathProfile,0,sizeof(gxTexturePathProfile));
 gxInvalidationRecordCount=0;
 glResetTextureProfileStats();
}
#endif

static uint64_t BuildClutCacheKey(unsigned int rawClutId,int textureMode,
                                  int drawSemiTrans)
{
 const void *palette=NULL;

 if(textureMode!=2)
  {
   short cx=((rawClutId<<4)&0x3F0);
   short cy=((rawClutId>>6)&CLUTYMASK);

   palette=psxVuw+cx+(cy*1024);
  }
 return ClutKeyBuild(rawClutId,textureMode,drawSemiTrans,palette);
}

#ifdef GX_TEXPATH_PROFILE
static unsigned char ProfileClassifySubMiss(int textureMode,int page,
                                             uint64_t clutKey,
                                             int exactKeySeen)
{
 int bucket,i,count;
 uint64_t identityMask=CLUT_KEY_ADDRESS_MASK|CLUT_KEY_VALID|
                       CLUT_KEY_SEMITRANS;
 textureSubCacheEntryS *base;

 if(exactKeySeen) return GX_TEX_MISS_UV;
 if(textureMode==2) return GX_TEX_MISS_NOKEY;

 base=pscSubtexStore[textureMode][page];
 if(!base) return GX_TEX_MISS_NOKEY;
 for(bucket=0;bucket<4;bucket++)
  {
   textureSubCacheEntryS *entry=base+bucket*SOFFB;
   count=GETLE32(SUBCACHE_COUNT_PTR(entry));
   if(count>SOFFB-2) count=SOFFB-2;
   for(i=1;i<=count;i++)
    if(entry[i].ClutKey &&
       (entry[i].ClutKey&identityMask)==(clutKey&identityMask))
     return GX_TEX_MISS_CLUT;
  }
 return GX_TEX_MISS_NOKEY;
}
#endif

#ifdef DISP_DEBUG
/* Sample the source texels and their resolved CLUT colors.  Full-screen
 * textures use a 4x4 stride so this stays cheap enough for timing-sensitive
 * logging; small sprites are inspected exactly. */
static unsigned int DebugTextureSourceStats(int pageid,int textureMode,
                                            unsigned int rawClutId,
                                            int u0,int v0,int w,int h,
                                            unsigned int *transparent,
                                            unsigned int *stp,
                                            unsigned int *samples)
{
 unsigned int hash=2166136261u;
 int pageX=(pageid&15)*64;
 int pageY=(pageid>>4)*256;
 int clutX=((rawClutId<<4)&0x3f0);
 int clutY=((rawClutId>>6)&CLUTYMASK);
 int stride=(w*h>4096) ? 4 : 1;
 int y,x;

 *transparent=0;
 *stp=0;
 *samples=0;
 if(w<=0 || h<=0) return hash;
 if(w>256) w=256;
 if(h>256) h=256;

 for(y=0;y<h;y+=stride)
  {
   int texY=(v0+y)&255;
   for(x=0;x<w;x+=stride)
    {
     int texX=(u0+x)&255;
     unsigned short packed;
     unsigned short color;

     if(textureMode==2)
      color=GETLE16(psxVuw+
       (((pageY+texY)&iGPUHeightMask)*1024)+
       ((pageX+texX)&0x3ff));
     else
      {
       int pixelsPerWord=textureMode==0 ? 4 : 2;
       int shift=textureMode==0 ? ((texX&3)*4) : ((texX&1)*8);
       unsigned int indexMask=textureMode==0 ? 0x0f : 0xff;
       unsigned int index;

       packed=GETLE16(psxVuw+
        (((pageY+texY)&iGPUHeightMask)*1024)+
        ((pageX+texX/pixelsPerWord)&0x3ff));
       index=(packed>>shift)&indexMask;
       color=GETLE16(psxVuw+
        ((clutY&iGPUHeightMask)*1024)+((clutX+index)&0x3ff));
      }
     hash=(hash^(unsigned int)color)*16777619u;
     if((color&0x7fff)==0) (*transparent)++;
     else if(color&0x8000) (*stp)++;
     (*samples)++;
    }
  }
 return hash;
}
#endif

static unsigned char PackDrawInfo(unsigned char opaque,
                                  unsigned int textureType)
{
 return (opaque&0x3fU)|((textureType&3U)<<6);
}

static unsigned char DrawInfoOpaque(unsigned char drawInfo)
{
 return drawInfo&0x3fU;
}

static unsigned char DrawInfoTextureType(unsigned char drawInfo)
{
 return drawInfo>>6;
}

////////////////////////////////////////////////////////////////////////
// Texture color conversions... all my ASM funcs are removed for easier
// porting... and honestly: nowadays the speed gain would be pointless
////////////////////////////////////////////////////////////////////////

// BGR => big endian(argb) (for movie PSXDisplay.RGB24 = false)
// bgr555 => bgr5A3(In order to be consistent with [PSXDisplay.RGB24=true], 4 bytes were used)
// Non-Transparent
unsigned int XP8RGBA_0(unsigned int BGR)
{
    if (!(BGR & 0x7fff)) return 0xffff0000;
    //return (((BGR&0x1f)<<19)|((BGR&0x3E0)<<6)|((BGR&0x7C00)>>7))|0xff000000;
    return BGR | 0xffff0000;
}
// Semi-Transparent
unsigned int XP8RGBA_1(unsigned int BGR)
{
    if (!(BGR & 0xffff))
    {
        return 0xffff0000;
    }

//    if (sSetMask)
//    {
//        //return BGR | 0xffff8000;
//        return (0x6 << 12) | 0xffff0000 | ((BGR & 0x1f) >> 1) | ((BGR & 0x3C0) >> 2) | ((BGR & 0x7800) >> 3);
//    }
//
//    if (!(BGR & 0x8000))
//    {
//        return (0x6 << 12) | 0xffff0000 | ((BGR & 0x1f) >> 1) | ((BGR & 0x3C0) >> 2) | ((BGR & 0x7800) >> 3);
//    }
    return BGR | 0xffff0000;
}

// BGR => big endian(argb)
unsigned int CP8RGBA_0(unsigned int BGR)
{
 unsigned int l;

 if(!(BGR&0xffff)) return 0x50000000;
 l=(((BGR&0x1f)<<19)|((BGR&0x3E0)<<6)|((BGR&0x7C00)>>7))|0xff000000;
 if(l==0xff00f8f8) l=0xff000000;
 return l;
}

//// BGR => big endian(argb)
//unsigned int XP8RGBA_1(unsigned int BGR)
//{
// if(!(BGR&0xffff)) return 0x50000000;
// if(!(BGR&0x8000)) {ubOpaqueDraw=1;return (((BGR&0x1f)<<19)|((BGR&0x3E0)<<6)|((BGR&0x7C00)>>7));}
// return (((BGR&0x1f)<<19)|((BGR&0x3E0)<<6)|((BGR&0x7C00)>>7))|0xff000000;
//}

// BGR => big endian(argb) (for other texture)
// bgr555 => bgr5A3(2 bytes were used, Can save half of the memory)
// Semi-Transparent
unsigned int P8RGBA_1(unsigned int BGR)
{
    if (!(BGR & 0x7fff))
    {
        return 0;
    }

//    if (sSetMask)
//    {
//        return BGR & 0x7fff;
//        //return (0x6 << 12) | ((BGR & 0x1f) >> 1) | ((BGR & 0x3C0) >> 2) | ((BGR & 0x7800) >> 3);
//    }

//    if (!(BGR & 0x8000))
//    {
//        //return (0x6 << 12) | ((BGR & 0x1f) >> 1) | ((BGR & 0x3C0) >> 2) | ((BGR & 0x7800) >> 3);
//        return (((BGR&0x1f)<<19)|((BGR&0x3E0)<<6)|((BGR&0x7C00)>>7))|0xf0000000;
//    }
//    return (((BGR&0x1f)<<19)|((BGR&0x3E0)<<6)|((BGR&0x7C00)>>7))|0xff000000;
    return BGR;
}

// Non-Transparent (BGR => ARGB)
unsigned int P8RGBA_0(unsigned int BGR)
{
    if (!(BGR&0xffff)) return 0;
    //return (((BGR&0x1f)<<19)|((BGR&0x3E0)<<6)|((BGR&0x7C00)>>7))|0xff000000;
    return BGR;
}

static inline unsigned short MOV24to16 (unsigned char *BGR)
{
    return (((uint16_t)(*(BGR + 2)>>3) << 10)|((uint16_t)(*(BGR + 1)>>3) << 5)|((uint16_t)(*(BGR)>>3)));
}

////////////////////////////////////////////////////////////////////////
// CHECK TEXTURE MEM (on plugin startup)
////////////////////////////////////////////////////////////////////////

//int iFTexA=512;
//int iFTexB=512;

void CheckTextureMemory(void)
{
 GLboolean b;GLboolean * bDetail;
 int i,iCnt,iRam=iVRamSize*1024*1024;
 int iTSize;char * p;


// if(iVRamSize)
//  {
//   int ts;
//
//   iRam-=(iResX*iResY*8);
//   iRam-=(iResX*iResY*(iZBufferDepth/8));
//
//	   ts=4;
//	   iSortTexCnt=iRam/(256*256*ts);
//
//   if(iSortTexCnt>MAXSORTTEX)
//    {
//     iSortTexCnt=MAXSORTTEX-min(1,0);
//    }
//   else
//    {
//     iSortTexCnt-=3+min(1,0);
//     if(iSortTexCnt<8) iSortTexCnt=8;
//    }
//
//   for(i=0;i<MAXSORTTEX;i++)
//    uiStexturePage[i]=0;
//
//   return;
//  }

#if 1
 iSortTexCnt=MAXSORTTEX;
#else // below vram detector supposedly crashes some drivers
#endif
}

////////////////////////////////////////////////////////////////////////
// Main init of textures
////////////////////////////////////////////////////////////////////////

void InitializeTextureStore()
{
 int i,j;

// if(iGPUHeight==1024)
//  {
//   MAXTPAGES     = MAXTPAGES_MAX;
//   CLUTMASK      = 0xffff;
//   CLUTYMASK     = 0x3ff;
//   MAXSORTTEX    = MAXSORTTEX_MAX;
//   iTexGarbageCollection=0;
//  }
// else
  {
   MAXTPAGES     = MAXTPAGES_MAX / 2;
   CLUTMASK      = 0x7fff;
   CLUTYMASK     = 0x1ff;
   MAXSORTTEX    = MAXSORTTEX_MAX;
  }

 memset(vertex,0,4*sizeof(OGLVertex));                 // init vertices

#ifdef GX_TEXPATH_PROFILE
#if defined(DISP_DEBUG)
 ResetGXTexturePathProfileSession();
#else
 memset(&gxTexturePathProfile,0,sizeof(gxTexturePathProfile));
 gxInvalidationRecordCount=0;
 glResetTextureProfileStats();
#endif
#endif

 gTexName=0;                                           // init main tex name
 usLRUTexPage=0;
 usAtlasEvictCursor=0;
 atlasSemiBackingCount=0;
 memset(atlasHasSemiBacking,0,sizeof(atlasHasSemiBacking));

 iTexWndLimit=MAXWNDTEXCACHE;
/* if(!iUsePalTextures) */iTexWndLimit/=2;

 memset(wcWndtexStore,0,sizeof(textureWndCacheEntry)*
                        MAXWNDTEXCACHE);
 //texturepart=(GLubyte *)_mem2_malloc(256*256*4);
 memset(texturepart,0,256*256*4);
	 //texturebuffer=NULL;

 for(i=0;i<3;i++)                                    // -> info for 32*3
  for(j=0;j<MAXTPAGES;j++)
   {
    pscSubtexStore[i][j]=(textureSubCacheEntryS *)_mem2_malloc(CSUBSIZES*sizeof(textureSubCacheEntryS));
    memset(pscSubtexStore[i][j],0,CSUBSIZES*sizeof(textureSubCacheEntryS));
   }
 for(i=0;i<MAXSORTTEX;i++)                           // -> info 0..511
  {
   pxSsubtexLeft[i]=(EXLong *)_mem2_malloc(CSUBSIZE*sizeof(EXLong));
   memset(pxSsubtexLeft[i],0,CSUBSIZE*sizeof(EXLong));
   uiStexturePage[i]=0;
  }
}

////////////////////////////////////////////////////////////////////////
// Clean up on exit
////////////////////////////////////////////////////////////////////////

void CleanupTextureStore()
{
 int i,j;textureWndCacheEntry * tsx;
 //----------------------------------------------------//
 glBindTextureBef(GL_TEXTURE_2D,0);
 glError();
 //----------------------------------------------------//
 //_mem2_free(texturepart);                                    // free tex part
 //texturepart=0;
// if(texturebuffer)
//  {
//   _mem2_free(texturebuffer);
//   texturebuffer=0;
//  }
 //----------------------------------------------------//
 tsx=wcWndtexStore;                                    // loop tex window cache
 for(i=0;i<MAXWNDTEXCACHE;i++,tsx++)
  {
   if(tsx->texname)                                    // -> some tex?
    glDeleteTextures(1,&tsx->texname);                 // --> delete it
    glError();
  }
 iMaxTexWnds=0;                                        // no more tex wnds
 //----------------------------------------------------//
 if(gTexMovieName!=0)                                  // some movie tex?
  glDeleteTextures(1, &gTexMovieName);                 // -> delete it
  glError();
 gTexMovieName=0;                                      // no more movie tex
 //----------------------------------------------------//
 if(gTexFrameName!=0)                                  // some 15bit framebuffer tex?
  glDeleteTextures(1, &gTexFrameName);                 // -> delete it
  glError();
 gTexFrameName=0;                                      // no more movie tex
 gFramebufferTextureCaptured=FALSE;
 gFramebufferTextureWidth=0;
 gFramebufferTextureHeight=0;
 gFramebufferTextureDisplayX=0;
 gFramebufferTextureDisplayY=0;
 gFramebufferTextureAbr=0;
 //----------------------------------------------------//
 if(gTexBlurName!=0)                                   // some 15bit framebuffer tex?
  glDeleteTextures(1, &gTexBlurName);                  // -> delete it
  glError();
 gTexBlurName=0;                                       // no more movie tex
 //----------------------------------------------------//
 for(i=0;i<3;i++)                                    // -> loop
  for(j=0;j<MAXTPAGES;j++)                           // loop tex pages
   {
    _mem2_free(pscSubtexStore[i][j]);                      // -> clean mem
    pscSubtexStore[i][j]=0;
   }
 for(i=0;i<MAXSORTTEX;i++)
  {
   if(uiStexturePage[i])                             // --> tex used ?
    {
     glDeleteTextures(1,&uiStexturePage[i]);
     glError();
     uiStexturePage[i]=0;                            // --> delete it
    }
   _mem2_free(pxSsubtexLeft[i]);                           // -> clean mem
   pxSsubtexLeft[i]=0;
   atlasHasSemiBacking[i]=0;
  }
 atlasSemiBackingCount=0;
 //----------------------------------------------------//
}

////////////////////////////////////////////////////////////////////////
// Reset textures in game...
////////////////////////////////////////////////////////////////////////

void ResetTextureArea(BOOL bDelTex)
{
 int i,j;textureSubCacheEntryS * tss;EXLong * lu;
 textureWndCacheEntry * tsx;
 //----------------------------------------------------//

 dwTexPageComp=0;
 usLRUTexPage=0;
 usAtlasEvictCursor=0;

 //----------------------------------------------------//
 if(bDelTex) {glBindTextureBef(GL_TEXTURE_2D,0); glError();gTexName=0;}
 //----------------------------------------------------//
 tsx=wcWndtexStore;
 for(i=0;i<MAXWNDTEXCACHE;i++,tsx++)
  {
   tsx->used=0;
   if(bDelTex && tsx->texname)
    {
     glDeleteTextures(1,&tsx->texname); glError();
     tsx->texname=0;
    }
  }
 iMaxTexWnds=0;
 //----------------------------------------------------//

 for(i=0;i<3;i++)
  for(j=0;j<MAXTPAGES;j++)
   {
    tss=pscSubtexStore[i][j];
    (tss+SOFFA)->pos.l=0;
    (tss+SOFFB)->pos.l=0;
    (tss+SOFFC)->pos.l=0;
    (tss+SOFFD)->pos.l=0;
   }

 for(i=0;i<iSortTexCnt;i++)
  {
   lu=pxSsubtexLeft[i];
   lu->l=0;
   if(bDelTex && uiStexturePage[i])
    {
     glDeleteTextures(1,&uiStexturePage[i]); glError();uiStexturePage[i]=0;
    }
   if(bDelTex)
    {
     if(atlasHasSemiBacking[i] && atlasSemiBackingCount)
      atlasSemiBackingCount--;
     atlasHasSemiBacking[i]=0;
    }
  }
}


////////////////////////////////////////////////////////////////////////
// Invalidate tex windows
////////////////////////////////////////////////////////////////////////

void InvalidateWndTextureArea(int X,int Y,int W, int H)
{
 int i,px1,px2,py1,py2,iYM=1;
 textureWndCacheEntry * tsw=wcWndtexStore;

 W+=X-1;
 H+=Y-1;
 if(X<0) X=0;if(X>1023) X=1023;
 if(W<0) W=0;if(W>1023) W=1023;
 if(Y<0) Y=0;if(Y>iGPUHeightMask)  Y=iGPUHeightMask;
 if(H<0) H=0;if(H>iGPUHeightMask)  H=iGPUHeightMask;
 W++;H++;

 if(iGPUHeight==1024) iYM=3;

 py1=min(iYM,Y>>8);
 py2=min(iYM,H>>8);                                    // y: 0 or 1

 px1=max(0,(X>>6));
 px2=min(15,(W>>6));

 if(py1==py2)
  {
   py1=py1<<4;px1+=py1;px2+=py1;                       // change to 0-31
   for(i=0;i<iMaxTexWnds;i++,tsw++)
    {
     if(tsw->used)
      {
       if(tsw->pageid>=px1 && tsw->pageid<=px2)
        {
         tsw->used=0;
        }
      }
    }
  }
 else
  {
   py1=px1+16;py2=px2+16;
   for(i=0;i<iMaxTexWnds;i++,tsw++)
    {
     if(tsw->used)
      {
       if((tsw->pageid>=px1 && tsw->pageid<=px2) ||
          (tsw->pageid>=py1 && tsw->pageid<=py2))
        {
         tsw->used=0;
        }
      }
    }
  }

 // adjust tex window count
 tsw=wcWndtexStore+iMaxTexWnds-1;
 while(iMaxTexWnds && !tsw->used) {iMaxTexWnds--;tsw--;}
}



////////////////////////////////////////////////////////////////////////
// same for sort textures
////////////////////////////////////////////////////////////////////////

void MarkFree(textureSubCacheEntryS * tsx)
{
 EXLong * ul, * uls;
 int j,iMax;unsigned char x1,y1,dx,dy;

 uls=pxSsubtexLeft[tsx->cTexID];
 iMax=GETLE32((unsigned long *)(uls));ul=uls+1;

 if(!iMax) return;

 for(j=0;j<iMax;j++,ul++)
  if(ul->l==0xffffffff) break;

 if(j<CSUBSIZE-2)
  {
   if(j==iMax) PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);

   x1=tsx->posTX;dx=tsx->pos.c.x2-tsx->pos.c.x1;
   if(tsx->posTX) {x1--;dx+=3;}
   y1=tsx->posTY;dy=tsx->pos.c.y2-tsx->pos.c.y1;
   if(tsx->posTY) {y1--;dy+=3;}

   ul->c.x1=x1;
   ul->c.x2=dx;
   ul->c.y1=y1;
   ul->c.y2=dy;
  }
}

static void InvalidateSubSTextureArea(int X,int Y,int W,int H,
                                      TextureInvalidateReason reason)
{
 int i,j,k,iMax,px,py,px1,px2,py1,py2,iYM=1;
 EXLong npos;textureSubCacheEntryS * tsb;
 int x1,x2,y1,y2,xa,sw;
 unsigned long long invalidatedArea;
#ifdef GX_TEXPATH_PROFILE
 unsigned int invalidatedBefore;
#endif

 if(reason<0 || reason>=TEX_INVALIDATE_REASON_COUNT)
  reason=TEX_INVALIDATE_OTHER;

 W+=X-1;
 H+=Y-1;
 if(X<0) X=0;if(X>1023) X=1023;
 if(W<0) W=0;if(W>1023) W=1023;
 if(Y<0) Y=0;if(Y>iGPUHeightMask)  Y=iGPUHeightMask;
 if(H<0) H=0;if(H>iGPUHeightMask)  H=iGPUHeightMask;
 W++;H++;

 invalidatedArea=(unsigned long long)(W>X ? W-X : 0)*
                 (unsigned long long)(H>Y ? H-Y : 0);
 GX_TEXPATH_INVALIDATE_CALL(reason,invalidatedArea);
#ifdef GX_TEXPATH_PROFILE
 invalidatedBefore=gxTexturePathProfile.invalidatedEntries;
#endif

 if(iGPUHeight==1024) iYM=3;

 py1=min(iYM,Y>>8);
 py2=min(iYM,H>>8);                                    // y: 0 or 1
 px1=max(0,(X>>6)-3);
 px2=min(15,(W>>6)+3);                                 // x: 0-15

 for(py=py1;py<=py2;py++)
  {
   j=(py<<4)+px1;                                      // get page

   y1=py*256;y2=y1+255;

   if(H<y1)  continue;
   if(Y>y2)  continue;

   if(Y>y1)  y1=Y;
   if(H<y2)  y2=H;
   if(y2<y1) {sw=y1;y1=y2;y2=sw;}
   y1=((y1%256)<<8);
   y2=(y2%256);

   for(px=px1;px<=px2;px++,j++)
    {
     for(k=0;k<3;k++)
      {
       xa=x1=px<<6;
       if(W<x1) continue;
       x2=x1+(64<<k)-1;
       if(X>x2) continue;

       if(X>x1)  x1=X;
       if(W<x2)  x2=W;
       if(x2<x1) {sw=x1;x1=x2;x2=sw;}

       if (dwGPUVersion == 2)
        npos.l=SWAP32_C(0x00ff00ff);
       else
        PUTLE32(&npos.l, ((x1-xa)<<(26-k))|((x2-xa)<<(18-k))|y1|y2);

        {
         tsb=pscSubtexStore[k][j]+SOFFA;iMax=GETLE32(SUBCACHE_COUNT_PTR(tsb));tsb++;
         for(i=0;i<iMax;i++,tsb++)
          if(tsb->ClutKey && XCHECK(tsb->pos,npos))
           {GX_TEXPATH_INVALIDATED(reason,k,j,tsb);tsb->ClutKey=0;MarkFree(tsb);}

//         if(npos.l & 0x00800000)
          {
           tsb=pscSubtexStore[k][j]+SOFFB;iMax=GETLE32(SUBCACHE_COUNT_PTR(tsb));tsb++;
           for(i=0;i<iMax;i++,tsb++)
            if(tsb->ClutKey && XCHECK(tsb->pos,npos))
             {GX_TEXPATH_INVALIDATED(reason,k,j,tsb);tsb->ClutKey=0;MarkFree(tsb);}
          }

//         if(npos.l & 0x00000080)
          {
           tsb=pscSubtexStore[k][j]+SOFFC;iMax=GETLE32(SUBCACHE_COUNT_PTR(tsb));tsb++;
           for(i=0;i<iMax;i++,tsb++)
            if(tsb->ClutKey && XCHECK(tsb->pos,npos))
             {GX_TEXPATH_INVALIDATED(reason,k,j,tsb);tsb->ClutKey=0;MarkFree(tsb);}
          }

//         if(npos.l & 0x00800080)
          {
           tsb=pscSubtexStore[k][j]+SOFFD;iMax=GETLE32(SUBCACHE_COUNT_PTR(tsb));tsb++;
           for(i=0;i<iMax;i++,tsb++)
            if(tsb->ClutKey && XCHECK(tsb->pos,npos))
             {GX_TEXPATH_INVALIDATED(reason,k,j,tsb);tsb->ClutKey=0;MarkFree(tsb);}
          }
        }
      }
    }
  }
#ifdef GX_TEXPATH_PROFILE
 if(gxTexturePathProfile.invalidatedEntries-invalidatedBefore>
    gxTexturePathProfile.maxInvalidatedEntries)
  {
   gxTexturePathProfile.maxInvalidatedEntries=
    gxTexturePathProfile.invalidatedEntries-invalidatedBefore;
   gxTexturePathProfile.maxInvalidatedReason=(unsigned int)reason;
   gxTexturePathProfile.maxInvalidatedX=(unsigned int)X;
   gxTexturePathProfile.maxInvalidatedY=(unsigned int)Y;
   gxTexturePathProfile.maxInvalidatedW=(unsigned int)(W-X);
   gxTexturePathProfile.maxInvalidatedH=(unsigned int)(H-Y);
  }
#endif
}

////////////////////////////////////////////////////////////////////////
// Invalidate some parts of cache: main routine
////////////////////////////////////////////////////////////////////////

void InvalidateTextureAreaEx(void)
{
 short W=sxmax-sxmin;
 short H=symax-symin;

 if(W==0 && H==0) return;

 if(iMaxTexWnds)
  InvalidateWndTextureArea(sxmin,symin,W,H);

 InvalidateSubSTextureArea(sxmin,symin,W,H,TEX_INVALIDATE_DRAW);
}

////////////////////////////////////////////////////////////////////////

void InvalidateTextureArea(int X,int Y,int W, int H)
{
 InvalidateTextureAreaTagged(X,Y,W,H,TEX_INVALIDATE_OTHER);
}

////////////////////////////////////////////////////////////////////////

void InvalidateTextureAreaTagged(int X,int Y,int W,int H,
                                 TextureInvalidateReason reason)
{
 if(W==0 && H==0) return;

 if(iMaxTexWnds) InvalidateWndTextureArea(X,Y,W,H);

 InvalidateSubSTextureArea(X,Y,W,H,reason);
}


////////////////////////////////////////////////////////////////////////
// tex window: define
////////////////////////////////////////////////////////////////////////

void DefineTextureWnd(void)
{
    glSetRGB24( 0 );

    if (gTexName==0)
    {
        glGenTextures(1, &gTexName);
        glError();
        glBindTextureBef(GL_TEXTURE_2D, gTexName);glError();
        glError();
    }
    else
    {
        glBindTextureBef(GL_TEXTURE_2D, gTexName);glError();
    }

    int textureType;
    textureType = glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB,
        TWin.Position.x1,
        TWin.Position.y1,
        0, GL_RGB, GL_UNSIGNED_BYTE, texturepart); glError();
    gl_ux[8] = textureType;
    #ifdef DISP_DEBUG
    sprintf ( txtbuffer, "DefineTextureWnd %d %d %d\r\n", TWin.Position.x1, TWin.Position.y1, textureType);
    writeLogFile ( txtbuffer );
    #endif // DISP_DEBUG

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, iFilter);
    glError();
}

////////////////////////////////////////////////////////////////////////
// tex window: load stretched
////////////////////////////////////////////////////////////////////////

void LoadStretchWndTexturePage(int pageid, int mode, short cx, short cy)
{
 unsigned int start,row,column,j,sxh,sxm,ldx,ldy,ldxo,s;
 unsigned int   palstart;
 unsigned int  *px,*pa,*ta;
 unsigned char  *cSRCPtr,*cOSRCPtr;
 unsigned short *wSRCPtr,*wOSRCPtr;
 unsigned int  LineOffset;
 int pmult=pageid/16;
 unsigned int (*LTCOL)(unsigned int);

 LTCOL=TCF[DrawSemiTrans];

 GX_TEXPATH_ADD_DECODED(mode,
                        (unsigned int)TWin.Position.x1 *
                        (unsigned int)TWin.Position.y1);

 ldxo=TWin.Position.x1-TWin.OPosition.x1;
 ldy =TWin.Position.y1-TWin.OPosition.y1;

 pa=px=(unsigned int *)ubPaletteBuffer;
 ta=(unsigned int *)texturepart;
 palstart=cx+(cy*1024);

 ubOpaqueDraw=0;

 switch(mode)
  {
   //--------------------------------------------------//
   // 4bit texture load ..
   case 0:
    //------------------- ZN STUFF

    if(GlobalTextIL)
     {
      unsigned int TXV,TXU,n_xi,n_yi;

      wSRCPtr=psxVuw+palstart;

      row=4;do
       {
        *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
        *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
        *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
        *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
        row--;px+=4;wSRCPtr+=4;
       }
      while (row);

      column=g_y2-ldy;
      for(TXV=g_y1;TXV<=column;TXV++)
       {
        ldx=ldxo;
        for(TXU=g_x1;TXU<=g_x2-ldxo;TXU++)
         {
  		  n_xi = ( ( TXU >> 2 ) & ~0x3c ) + ( ( TXV << 2 ) & 0x3c );
		  n_yi = ( TXV & ~0xf ) + ( ( TXU >> 4 ) & 0xf );

          s = *(pa + ((GETLE16((unsigned short *)(psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi)) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
          *ta++ = s;

          if(ldx) {
            *ta++ = s;
            ldx--;
          }
         }

        if(ldy)
         {ldy--;
          for(TXU=g_x1;TXU<=g_x2;TXU++)
           *ta++=*(ta-(g_x2-g_x1));
         }
       }

      DefineTextureWnd();

      break;
     }

    //-------------------

    start=((pageid-16*pmult)*128)+256*2048*pmult;
    // convert CLUT to 32bits .. and then use THAT as a lookup table

    wSRCPtr=psxVuw+palstart;
    for(row=0;row<16;row++)
     //*px++=LTCOL(*wSRCPtr++);
     *px++=LTCOL(GETLE16((unsigned short *)(wSRCPtr++)));

    sxm=g_x1&1;sxh=g_x1>>1;
    if(sxm) j=g_x1+1; else j=g_x1;
    cSRCPtr = psxVub + start + (2048*g_y1) + sxh;
    for(column=g_y1;column<=g_y2;column++)
     {
      cOSRCPtr=cSRCPtr;ldx=ldxo;
      if(sxm) *ta++=*(pa+((*cSRCPtr++ >> 4) & 0xF));

      for(row=j;row<=g_x2-ldxo;row++)
       {
        s=*(pa+(*cSRCPtr & 0xF));
        *ta++=s;
        if(ldx) {*ta++=s;ldx--;}
        row++;
        if(row<=g_x2-ldxo)
         {
          s=*(pa+((*cSRCPtr >> 4) & 0xF));
          *ta++=s;
          if(ldx) {*ta++=s;ldx--;}
         }
        cSRCPtr++;
       }
      if(ldy && column&1)
           {ldy--;cSRCPtr = cOSRCPtr;}
      else cSRCPtr = psxVub + start + (2048*(column+1)) + sxh;
     }

    DefineTextureWnd();
    break;
   //--------------------------------------------------//
   // 8bit texture load ..
   case 1:
    //------------ ZN STUFF
    if(GlobalTextIL)
     {
      unsigned int TXV,TXU,n_xi,n_yi;

      wSRCPtr=psxVuw+palstart;

      row=64;do
       {
        *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
        *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
        *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
        *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
        row--;px+=4;wSRCPtr+=4;
       }
      while (row);

      column=g_y2-ldy;
      for(TXV=g_y1;TXV<=column;TXV++)
       {
        ldx=ldxo;
        for(TXU=g_x1;TXU<=g_x2-ldxo;TXU++)
         {
  		  n_xi = ( ( TXU >> 1 ) & ~0x78 ) + ( ( TXU << 2 ) & 0x40 ) + ( ( TXV << 3 ) & 0x38 );
		  n_yi = ( TXV & ~0x7 ) + ( ( TXU >> 5 ) & 0x7 );

          //s=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff));
          //*ta++=s;
          s = *(pa + ((GETLE16((unsigned short *)(psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi)) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff ));
          *ta++ = s;
          if(ldx) {
            *ta++ = s;
            ldx--;
          }
         }

        if(ldy)
         {ldy--;
          for(TXU=g_x1;TXU<=g_x2;TXU++)
           *ta++=*(ta-(g_x2-g_x1));
         }

       }

      DefineTextureWnd();

      break;
     }
    //------------

    start=((pageid-16*pmult)*128)+256*2048*pmult;

    // not using a lookup table here... speeds up smaller texture areas
    cSRCPtr = psxVub + start + (2048*g_y1) + g_x1;
    LineOffset = 2048 - (g_x2-g_x1+1) +ldxo;

    for(column=g_y1;column<=g_y2;column++)
     {
      cOSRCPtr=cSRCPtr;ldx=ldxo;
      for(row=g_x1;row<=g_x2-ldxo;row++)
       {
        s=LTCOL(GETLE16(&psxVuw[palstart+ *cSRCPtr++]));
        *ta++=s;
        if(ldx) {*ta++=s;ldx--;}
       }
      if(ldy && column&1) {ldy--;cSRCPtr=cOSRCPtr;}
      else                cSRCPtr+=LineOffset;
     }

    DefineTextureWnd();
    break;
   //--------------------------------------------------//
   // 16bit texture load ..
   case 2:
    start=((pageid-16*pmult)*64)+256*1024*pmult;

    wSRCPtr = psxVuw + start + (1024*g_y1) + g_x1;
    LineOffset = 1024 - (g_x2-g_x1+1) +ldxo;

    for(column=g_y1;column<=g_y2;column++)
     {
      wOSRCPtr=wSRCPtr;ldx=ldxo;
      for(row=g_x1;row<=g_x2-ldxo;row++)
       {
        s=LTCOL(GETLE16((unsigned short *)(wSRCPtr++)));
        *ta++=s;

        if(ldx) {
            *ta++=s;
            ldx--;
        }
       }
      if(ldy && column&1) {ldy--;wSRCPtr=wOSRCPtr;}
      else                 wSRCPtr+=LineOffset;
     }

    DefineTextureWnd();
    break;
   //--------------------------------------------------//
   // others are not possible !
  }
}

////////////////////////////////////////////////////////////////////////
// tex window: load simple
////////////////////////////////////////////////////////////////////////

void LoadWndTexturePage(int pageid, int mode, short cx, short cy)
{
 unsigned int start,row,column,j,sxh,sxm;
 unsigned int   palstart;
 unsigned int  *px,*pa,*ta;
 unsigned char  *cSRCPtr;
 unsigned short *wSRCPtr;
 unsigned int  LineOffset;
 int pmult=pageid/16;
 unsigned int (*LTCOL)(unsigned int);

 LTCOL=TCF[DrawSemiTrans];

 GX_TEXPATH_ADD_DECODED(mode,
                        (unsigned int)TWin.Position.x1 *
                        (unsigned int)TWin.Position.y1);

 pa=px=(unsigned int *)ubPaletteBuffer;
 ta=(unsigned int *)texturepart;
 palstart=cx+(cy*1024);

 ubOpaqueDraw=0;

 switch(mode)
  {
   //--------------------------------------------------//
   // 4bit texture load ..
   case 0:
    if(GlobalTextIL)
     {
      unsigned int TXV,TXU,n_xi,n_yi;

      wSRCPtr=psxVuw+palstart;

      row=4;do
       {
        *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
        *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
        *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
        *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
        row--;px+=4;wSRCPtr+=4;
       }
      while (row);

      for(TXV=g_y1;TXV<=g_y2;TXV++)
       {
        for(TXU=g_x1;TXU<=g_x2;TXU++)
         {
  		  n_xi = ( ( TXU >> 2 ) & ~0x3c ) + ( ( TXV << 2 ) & 0x3c );
		  n_yi = ( TXV & ~0xf ) + ( ( TXU >> 4 ) & 0xf );

          //*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
          *ta++ = *(pa + ((GETLE16((unsigned short *)(psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi)) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
         }
       }

      DefineTextureWnd();

      break;
     }

    start=((pageid-16*pmult)*128)+256*2048*pmult;

    // convert CLUT to 32bits .. and then use THAT as a lookup table

    wSRCPtr=psxVuw+palstart;
    for(row=0;row<16;row++)
     *px++=LTCOL(GETLE16((unsigned short *)(wSRCPtr++)));

    sxm=g_x1&1;sxh=g_x1>>1;
    if(sxm) j=g_x1+1; else j=g_x1;
    cSRCPtr = psxVub + start + (2048*g_y1) + sxh;
    for(column=g_y1;column<=g_y2;column++)
     {
      cSRCPtr = psxVub + start + (2048*column) + sxh;

      if(sxm) *ta++=*(pa+((*cSRCPtr++ >> 4) & 0xF));

      for(row=j;row<=g_x2;row++)
       {
        *ta++=*(pa+(*cSRCPtr & 0xF)); row++;
        if(row<=g_x2) *ta++=*(pa+((*cSRCPtr >> 4) & 0xF));
        cSRCPtr++;
       }
     }

    DefineTextureWnd();
    break;
   //--------------------------------------------------//
   // 8bit texture load ..
   case 1:
    if(GlobalTextIL)
     {
      unsigned int TXV,TXU,n_xi,n_yi;

      wSRCPtr=psxVuw+palstart;

      row=64;do
       {
        *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
        *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
        *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
        *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
        row--;px+=4;wSRCPtr+=4;
       }
      while (row);

      for(TXV=g_y1;TXV<=g_y2;TXV++)
       {
        for(TXU=g_x1;TXU<=g_x2;TXU++)
         {
  		  n_xi = ( ( TXU >> 1 ) & ~0x78 ) + ( ( TXU << 2 ) & 0x40 ) + ( ( TXV << 3 ) & 0x38 );
		  n_yi = ( TXV & ~0x7 ) + ( ( TXU >> 5 ) & 0x7 );

          //*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff));
          *ta++ = *(pa + ((GETLE16((unsigned short *)(psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi)) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff ));
         }
       }

      DefineTextureWnd();

      break;
     }

    start=((pageid-16*pmult)*128)+256*2048*pmult;

    // not using a lookup table here... speeds up smaller texture areas
    cSRCPtr = psxVub + start + (2048*g_y1) + g_x1;
    LineOffset = 2048 - (g_x2-g_x1+1);

    for(column=g_y1;column<=g_y2;column++)
     {
      for(row=g_x1;row<=g_x2;row++)
       *ta++=LTCOL(GETLE16(&psxVuw[palstart+ *cSRCPtr++]));
      cSRCPtr+=LineOffset;
     }

    DefineTextureWnd();
    break;
   //--------------------------------------------------//
   // 16bit texture load ..
   case 2:
    start=((pageid-16*pmult)*64)+256*1024*pmult;

    wSRCPtr = psxVuw + start + (1024*g_y1) + g_x1;
    LineOffset = 1024 - (g_x2-g_x1+1);

    for(column=g_y1;column<=g_y2;column++)
     {
      for(row=g_x1;row<=g_x2;row++)
       *ta++=LTCOL(GETLE16((unsigned short *)(wSRCPtr++)));
      wSRCPtr+=LineOffset;
     }

    DefineTextureWnd();
    break;
   //--------------------------------------------------//
   // others are not possible !
  }
}

////////////////////////////////////////////////////////////////////////
// tex window: main selecting, cache handler included
////////////////////////////////////////////////////////////////////////

GLuint LoadTextureWnd(int pageid,int TextureMode,unsigned int GivenClutId)
{
 textureWndCacheEntry * ts, * tsx=NULL;
 uint64_t clutKey;
 int i;short cx,cy;
 EXLong npos;

 GX_TEXPATH_INC(windowLookups);

 npos.c.x1=TWin.Position.x0;
 npos.c.x2=TWin.OPosition.x1;
 npos.c.y1=TWin.Position.y0;
 npos.c.y2=TWin.OPosition.y1;

 g_x1=TWin.Position.x0;g_x2=g_x1+TWin.Position.x1-1;
 g_y1=TWin.Position.y0;g_y2=g_y1+TWin.Position.y1-1;

 if(TextureMode==2) {cx=cy=0;}
 else
  {
   cx=((GivenClutId << 4) & 0x3F0);cy=((GivenClutId >> 6) & CLUTYMASK);
  }
 clutKey=BuildClutCacheKey(GivenClutId,TextureMode,DrawSemiTrans);

 ts=wcWndtexStore;

 for(i=0;i<iMaxTexWnds;i++,ts++)
  {
   if(ts->used)
    {
     if(ts->pos.l==npos.l &&
        ts->pageid==pageid &&
        ts->textureMode==TextureMode)
      {
       if(ts->ClutKey==clutKey)
        {
         GX_TEXPATH_INC(windowHits);
         ubOpaqueDraw=ts->Opaque;
         gl_ux[8]=ts->textureType;
         return ts->texname;
        }
      }
    }
   else tsx=ts;
  }

 GX_TEXPATH_INC(windowMisses);

 if(!tsx)
  {
   if(iMaxTexWnds==iTexWndLimit)
    {
     GX_TEXPATH_INC(windowReplacements);
     tsx=wcWndtexStore+iTexWndTurn;
     iTexWndTurn++;
     if(iTexWndTurn==iTexWndLimit) iTexWndTurn=0;
    }
   else
    {
     tsx=wcWndtexStore+iMaxTexWnds;
     iMaxTexWnds++;
    }
  }

 gTexName=tsx->texname;
 texChgType = 1;
 //GX_Flush();
 //GX_SetDrawDone();

 if(TWin.OPosition.y1==TWin.Position.y1 &&
    TWin.OPosition.x1==TWin.Position.x1)
  {
    LoadWndTexturePage(pageid,TextureMode,cx,cy);
  }
 else
  {
    LoadStretchWndTexturePage(pageid,TextureMode,cx,cy);
  }

 tsx->Opaque=ubOpaqueDraw;
 tsx->pos.l=npos.l;
 tsx->ClutKey=clutKey;
 tsx->pageid=pageid;
 tsx->textureMode=TextureMode;
 tsx->texname=gTexName;
 tsx->used=1;
 tsx->textureType=gl_ux[8];

 return gTexName;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// movie texture: define
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////

static inline void DefineTextureMovie(void)
{
 if(gTexMovieName==0)
  {
   glGenTextures(1, &gTexMovieName); glError();
   gTexName=gTexMovieName;
   glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();

    glResetMovieTexPtr();
    gl_ux[8] = glInitMovieTextures((xrMovieArea.x1-xrMovieArea.x0), (xrMovieArea.y1-xrMovieArea.y0), texturepart);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iClampType); glError();
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, iClampType); glError();

//   if(!bUseFastMdec)
//    {
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glError();
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glError();
//    }
//   else
//    {
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iFilter); glError();
//     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, iFilter); glError();
//    }
   //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, texturepart); glError();

  }
 else
  {
   gTexName=gTexMovieName;glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();
   gl_ux[8] = glInitMovieTextures((xrMovieArea.x1-xrMovieArea.x0), (xrMovieArea.y1-xrMovieArea.y0), texturepart);
  }

//  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
//                 (xrMovieArea.x1-xrMovieArea.x0),
//                 (xrMovieArea.y1-xrMovieArea.y0),
//                 GL_RGBA, GL_UNSIGNED_BYTE, texturepart); glError();


 //LOGE("DefineTextureMovie x:%d y:%d",(xrMovieArea.x1-xrMovieArea.x0),(xrMovieArea.y1-xrMovieArea.y0));
}

////////////////////////////////////////////////////////////////////////
// movie texture: load
////////////////////////////////////////////////////////////////////////

#define MRED(x)   ((x>>3) & 0x1f)
#define MGREEN(x) ((x>>6) & 0x3e0)
#define MBLUE(x)  ((x>>9) & 0x7c00)

#define XMGREEN(x) ((x>>5)  & 0x07c0)
#define XMRED(x)   ((x<<8)  & 0xf800)
#define XMBLUE(x)  ((x>>18) & 0x003e)

////////////////////////////////////////////////////////////////////////
// movie texture: load
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////

GLuint LoadTextureMovie(void)
{
    texChgType = 2;
    //GX_Flush();
    //GX_SetDrawDone();

 short row,column,dx;
 unsigned int startxy;
 BOOL b_X,b_Y;

 //if(bUseFastMdec) return LoadTextureMovieFast();

// b_X=FALSE;b_Y=FALSE;
//
// if((xrMovieArea.x1-xrMovieArea.x0)<255)  b_X=TRUE;
// if((xrMovieArea.y1-xrMovieArea.y0)<255)  b_Y=TRUE;

{
   if(PSXDisplay.RGB24)
    {
        #ifdef DISP_DEBUG
        //sprintf(txtbuffer, "LoadMovie1 %d %d %d %d %d %d\r\n",   xrMovieArea.x0, xrMovieArea.y0, xrMovieArea.x1, xrMovieArea.y1, b_X, b_Y);
        //DEBUG_print(txtbuffer, DBG_SPU2);
        //writeLogFile(txtbuffer);
        #endif // DISP_DEBUG

     unsigned char * pD;
     unsigned int * ta=(unsigned int *)texturepart;

//     if(b_X)
//      {
//       for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
//        {
//         startxy=((1024)*column)+xrMovieArea.x0;
//         pD=(unsigned char *)&psxVuw[startxy];
//         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
//          {
//           PUTLE32(ta++, *((unsigned int *)pD)|SWAP32_C(0xff000000));
//           pD+=3;
//          }
//         *ta++=*(ta-1);
//        }
//       if(b_Y)
//        {
//         dx=xrMovieArea.x1-xrMovieArea.x0+1;
//         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
//          *ta++=*(ta-dx);
//         *ta++=*(ta-1);
//        }
//      }
//     else
      {
       for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
        {
         startxy=((1024)*column)+xrMovieArea.x0;
         pD=(unsigned char *)&psxVuw[startxy];
         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
          {
           //PUTLE32(ta++, *((unsigned int *)pD)|SWAP32_C(0xff000000)); // BGR24 => ARGB
           *ta++ = MOV24to16(pD)  | 0x8000;
           pD+=3;
          }
        }
//       if(b_Y)
//        {
//         dx=xrMovieArea.x1-xrMovieArea.x0;
//         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
//          *ta++=*(ta-dx);
//        }
      }
    }
   else
    {
        #ifdef DISP_DEBUG
        //sprintf(txtbuffer, "LoadMovie2 %d %d %d %d %d %d\r\n", xrMovieArea.x0, xrMovieArea.y0, xrMovieArea.x1, xrMovieArea.y1, b_X, b_Y);
        //DEBUG_print(txtbuffer, DBG_SPU2);
        //writeLogFile(txtbuffer);
        #endif // DISP_DEBUG

     unsigned int (*LTCOL)(unsigned int);
     unsigned int *ta;

     if (DrawSemiTrans == 0)
     {
         LTCOL=P8RGBA_0;//TCF[0];
     }
     else
     {
         LTCOL=P8RGBA_1;
     }

     ubOpaqueDraw=0;
     ta=(unsigned int *)texturepart;

//     if(b_X)
//      {
//       for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
//        {
//         startxy=((1024)*column)+xrMovieArea.x0;
//         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
//         {
//             *ta++ = LTCOL(GETLE16(&psxVuw[startxy++]) | 0x8000);
//         }
//
//         *ta++=*(ta-1);
//        }
//
//       if(b_Y)
//        {
//         dx=xrMovieArea.x1-xrMovieArea.x0+1;
//         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
//          *ta++=*(ta-dx);
//         *ta++=*(ta-1);
//        }
//      }
//     else
      {
       for(column=xrMovieArea.y0;column<xrMovieArea.y1;column++)
        {
         startxy=((1024)*column)+xrMovieArea.x0;
         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
         {
             unsigned int uploadPixel =
                 LTCOL(GETLE16(&psxVuw[startxy++]));

             /* A MoveImage display upload must overwrite retained EFB pixels
              * even where the copied PS1 value is 0000.  Ordinary display
              * uploads retain their established transparency rules. */
             if (g_forceOpaqueMoveUpload)
                 uploadPixel |= 0x8000;
             *ta++ = uploadPixel;
         }
        }

//       if(b_Y)
//        {
//         dx=xrMovieArea.x1-xrMovieArea.x0;
//         for(row=xrMovieArea.x0;row<xrMovieArea.x1;row++)
//          *ta++=*(ta-dx);
//        }
      }
    }

   //xrMovieArea.x1+=b_X;xrMovieArea.y1+=b_Y;
   DefineTextureMovie();
   //xrMovieArea.x1-=b_X;xrMovieArea.y1-=b_Y;
  }
 return gTexName;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

GLuint BlackFake15BitTexture(void)
{
 int pmult;short x1,x2,y1,y2;

 if(PSXDisplay.InterlacedTest) return 0;

 pmult=GlobalTexturePage/16;
 x1=gl_ux[7];
 x2=gl_ux[6]-gl_ux[7];
 y1=gl_ux[5];
 y2=gl_ux[4]-gl_ux[5];

 if(iSpriteTex)
  {
   if(x2<255) x2++;
   if(y2<255) y2++;
  }

 y1+=pmult*256;
 x1+=((GlobalTexturePage-16*pmult)<<6);

 if(   FastCheckAgainstFrontScreen(x1,y1,x2,y2)
    || FastCheckAgainstScreen(x1,y1,x2,y2))
  {
   if(!gTexFrameName)
    {
     glGenTextures(1, &gTexFrameName); glError();
     gTexName=gTexFrameName;
     glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();

     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iClampType); glError();
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, iClampType); glError();
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iFilter); glError();
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, iFilter); glError();

     {
       unsigned int * ta=(unsigned int *)texturepart;
       for(y1=0;y1<=4;y1++)
        for(x1=0;x1<=4;x1++)
         *ta++=0xff000000;
      }
      #ifdef DISP_DEBUG
//sprintf(txtbuffer, "Fake15BitTexture 4 4\r\n");
//DEBUG_print(txtbuffer, DBG_SPU1);
#endif // DISP_DEBUG
     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, texturepart); glError();

    }
   else
    {
     gTexName=gTexFrameName;
     glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();
    }
      //LOGE("BlackFake15BitTexture x:%d y:%d",4,4);
   ubOpaqueDraw=0;

   return (GLuint)gTexName;
  }
 return 0;
}

/////////////////////////////////////////////////////////////////////////////

BOOL bFakeFrontBuffer=FALSE;
BOOL bIgnoreNextTile =FALSE;

int iFTex=512;

static void ResetFramebufferTextureCapture(void)
{
 gFramebufferTextureCaptured=FALSE;
 gFramebufferTextureCoordsValid=FALSE;
}

/* AITD4's framebuffer feedback sprites also appear while their VRAM source is
 * completely transparent.  Capturing and drawing the EFB for those sprites
 * feeds the whole scene back over itself and softens it for no visual benefit.
 * Check every 16-bit source texel: sampling would risk missing the small stale
 * door/UI fragments which this workaround is intended to replace. */
static BOOL FramebufferTextureSourceHasData(int x0,int y0,int x1,int y1)
{
 int x,y;

 for(y=y0;y<y1;y++)
  {
   unsigned short *source=psxVuw+((y&iGPUHeightMask)*1024);

   for(x=x0;x<x1;x++)
    {
     if(GETLE16(source+(x&0x3ff))!=0) return TRUE;
    }
  }

 return FALSE;
}

/* Some games build pixels in the EFB and immediately sample the same PSX VRAM
 * area as a 16-bit texture.  The GLES renderer deliberately does not mirror
 * ordinary GX draws into psxVuw, so normal decoding sees stale or empty data.
 * Recognize only the proven game-specific operations and capture the EFB. */
static GLuint CaptureFramebufferTexture(void)
{
 int didCapture=FALSE;
 int mgsCapture=FALSE;
 int pageX, pageY;
 int srcX0, srcY0, srcX1, srcY1;
 int dstX0, dstY0, dstX1, dstY1;
 int displayX, displayY, displayW, displayH;
 int viewportX, viewportY, viewportW, viewportH;
 int copyW, copyH;

 if(!DrawSemiTrans || PSXDisplay.RGB24) return 0;
 if(GlobalTextTP!=2) return 0;
 if(PSXDisplay.InterlacedTest) return 0;

 pageX=(GlobalTexturePage&15)<<6;
 pageY=(GlobalTexturePage>>4)<<8;
 srcX0=pageX+gl_ux[7];
 srcY0=pageY+gl_ux[5];
 srcX1=pageX+gl_ux[6]+1;
 srcY1=pageY+gl_ux[4]+1;

 displayW=PSXDisplay.DisplayMode.x;
 displayH=PSXDisplay.DisplayMode.y+PreviousPSXDisplay.DisplayModeNew.y;

 if(displayW<=0 || displayH<=0) return 0;

 dstX0=dstX1=lx0;
 dstY0=dstY1=ly0;
 if(lx1<dstX0) dstX0=lx1; else if(lx1>dstX1) dstX1=lx1;
 if(ly1<dstY0) dstY0=ly1; else if(ly1>dstY1) dstY1=ly1;
 if(lx2<dstX0) dstX0=lx2; else if(lx2>dstX1) dstX1=lx2;
 if(ly2<dstY0) dstY0=ly2; else if(ly2>dstY1) dstY1=ly2;
 if(lx3<dstX0) dstX0=lx3; else if(lx3>dstX1) dstX1=lx3;
 if(ly3<dstY0) dstY0=ly3; else if(ly3>dstY1) dstY1=ly3;

 /* Do not depend solely on a regional disc-ID list.  MGS's operation has a
  * much narrower runtime signature: a flat semi-transparent FT4 copies an
  * all-zero CPU-VRAM 211x19 strip from the page being built onto the fixed
  * save-entry rectangle.  The pixels are zero only because they exist in the
  * EFB, so this signature also excludes ordinary resident VRAM textures. */
 displayX=PreviousPSXDisplay.DisplayPosition.x;
 displayY=PreviousPSXDisplay.DisplayPosition.y;
 if(!iSpriteTex && !bDrawSmoothShaded &&
    displayW==320 && displayH>=200 && displayH<=256 &&
    pageX==displayX && pageY==displayY &&
    srcX0==pageX && srcX1==pageX+211 &&
    srcY0==pageY+200 && srcY1==pageY+219 &&
    dstX0==55 && dstX1==265 && dstY0==80 && dstY1==98 &&
    !FramebufferTextureSourceHasData(srcX0,srcY0,srcX1,srcY1))
  mgsCapture=TRUE;

 if(!mgsCapture &&
    (!(dwActFixes&AUTO_FIX_FRAMEBUFFER_TEXTURE) || !iSpriteTex))
  return 0;

 if(!mgsCapture)
  {
   /* Restrict the AITD4 workaround to its tall display-page strips.  The
    * page can be split horizontally, so width alone is not a discriminator. */
   displayX=PSXDisplay.DisplayPosition.x;
   displayY=PSXDisplay.DisplayPosition.y;
   if(srcX1<=displayX || srcX0>=displayX+displayW ||
      srcY1<=displayY || srcY0>=displayY+displayH ||
      srcY0>displayY+2 || srcY1<displayY+displayH-2)
    {
     displayX=PreviousPSXDisplay.DisplayPosition.x;
     displayY=PreviousPSXDisplay.DisplayPosition.y;
     if(srcX1<=displayX || srcX0>=displayX+displayW ||
        srcY1<=displayY || srcY0>=displayY+displayH ||
        srcY0>displayY+2 || srcY1<displayY+displayH-2) return 0;
    }

   /* A zero 16-bit source normally decodes as a fully transparent texture.
    * MGS is the opposite proven case: its CPU copy is zero because the source
    * has just been produced in the EFB. */
   if(!FramebufferTextureSourceHasData(srcX0,srcY0,srcX1,srcY1)) return 0;
  }

 viewportX=rRatioRect.left;
 viewportY=iResY-(rRatioRect.top+rRatioRect.bottom);
 viewportW=rRatioRect.right;
 viewportH=rRatioRect.bottom;
 if(viewportW<=0 || viewportH<=0) return 0;

 copyW=(iResX+15)&~15;
 copyH=(iResY+3)&~3;
 if(copyW<=0 || copyH<=0 || copyW>1024 || copyH>1024) return 0;

 if(!gTexFrameName)
  {
   glGenTextures(1,&gTexFrameName); glError();
   if(!gTexFrameName) return 0;
  }

 gTexName=gTexFrameName;
 glBindTextureBef(GL_TEXTURE_2D,gTexName); glError();
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE); glError();
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE); glError();

 if(gFramebufferTextureWidth!=copyW || gFramebufferTextureHeight!=copyH)
  {
   glInitRGBATextures(copyW,copyH); glError();
   gFramebufferTextureWidth=copyW;
   gFramebufferTextureHeight=copyH;
   gFramebufferTextureCaptured=FALSE;
  }

 if(!gFramebufferTextureCaptured ||
    gFramebufferTextureDisplayX!=displayX ||
    gFramebufferTextureDisplayY!=displayY ||
    gFramebufferTextureAbr!=GlobalTextABR)
  {
   if(mgsCapture)
    {
     if(!glCaptureFramebufferIntensityTexture(iResX,iResY)) return 0;
    }
   else if(!glCaptureFramebufferTexture(iResX,iResY)) return 0;
   RestoreDispCopyInfo();
   gFramebufferTextureCaptured=TRUE;
   gFramebufferTextureDisplayX=displayX;
   gFramebufferTextureDisplayY=displayY;
   gFramebufferTextureAbr=GlobalTextABR;
   didCapture=TRUE;
   texChgType=3;
  }

 /* Preserve the EFB coordinates as normalized floats.  MGS needs all four
  * polygon UVs; AITD4 uses the rectangular sprite edges. */
 if(mgsCapture)
  {
   int i;

   for(i=0;i<4;i++)
    {
     float u=((float)viewportX+
              (float)(pageX+gl_ux[i]-displayX)*(float)viewportW/
              (float)displayW)/(float)copyW;
     float v=((float)viewportY+
              (float)(pageY+gl_vy[i]-displayY)*(float)viewportH/
              (float)displayH)/(float)copyH;

     if(u<0.0f) u=0.0f; else if(u>1.0f) u=1.0f;
     if(v<0.0f) v=0.0f; else if(v>1.0f) v=1.0f;
     gFramebufferTextureCoords[i][0]=u;
     gFramebufferTextureCoords[i][1]=v;
    }
   gFramebufferTextureCoordsValid=TRUE;
  }
 else
  {
   int sourceLeft=pageX+gl_ux[0];
   int sourceTop=pageY+gl_vy[0];
   int sourceRight=sourceLeft+sprtW;
   int sourceBottom=sourceTop+sprtH;
   float u0=((float)viewportX+
             (float)(sourceLeft-displayX)*(float)viewportW/(float)displayW)/
            (float)copyW;
   float v0=((float)viewportY+
             (float)(sourceTop-displayY)*(float)viewportH/(float)displayH)/
            (float)copyH;
   float u1=((float)viewportX+
             (float)(sourceRight-displayX)*(float)viewportW/(float)displayW)/
            (float)copyW;
   float v1=((float)viewportY+
             (float)(sourceBottom-displayY)*(float)viewportH/(float)displayH)/
            (float)copyH;

   if(u0<0.0f) u0=0.0f; else if(u0>1.0f) u0=1.0f;
   if(v0<0.0f) v0=0.0f; else if(v0>1.0f) v0=1.0f;
   if(u1<0.0f) u1=0.0f; else if(u1>1.0f) u1=1.0f;
   if(v1<0.0f) v1=0.0f; else if(v1>1.0f) v1=1.0f;

   gFramebufferTextureCoords[0][0]=gFramebufferTextureCoords[3][0]=u0;
   gFramebufferTextureCoords[1][0]=gFramebufferTextureCoords[2][0]=u1;
   gFramebufferTextureCoords[0][1]=gFramebufferTextureCoords[1][1]=v0;
   gFramebufferTextureCoords[2][1]=gFramebufferTextureCoords[3][1]=v1;
   gFramebufferTextureCoordsValid=TRUE;
  }

 /* Type 2 retains the PSX semi-transparency pass used by both feedback paths. */
 gl_ux[8]=2;
 ubOpaqueDraw=0;

#ifdef DISP_DEBUG
 sprintf(txtbuffer,
         "TDI FRAME_TEX frame=%u capture=%d src=%d,%d-%d,%d "
         "display=%d,%d+%d,%d copy=%d,%d uv=%.6f,%.6f-%.6f,%.6f\r\n",
         g_textureDiagFrame,didCapture,
         srcX0,srcY0,srcX1,srcY1,displayX,displayY,displayW,displayH,
         copyW,copyH,gFramebufferTextureCoords[0][0],
         gFramebufferTextureCoords[0][1],
         gFramebufferTextureCoords[2][0],
         gFramebufferTextureCoords[2][1]);
 TextureDiagAppend(txtbuffer);
#endif

 return gTexName;
}

GLuint Fake15BitTexture(void)
{

    #ifdef DISP_DEBUG
//sprintf(txtbuffer, "Fake15BitTexture 0\r\n");
//DEBUG_print(txtbuffer, DBG_SPU1);
//writeLogFile(txtbuffer);
#endif // DISP_DEBUG

 int pmult;short x1,x2,y1,y2;int iYAdjust;
 float ScaleX,ScaleY;RECT rSrc;

 if(iFrameTexType==1) return BlackFake15BitTexture();
 if(PSXDisplay.InterlacedTest) return 0;

 pmult=GlobalTexturePage/16;
 x1=gl_ux[7];
 x2=gl_ux[6]-gl_ux[7];
 y1=gl_ux[5];
 y2=gl_ux[4]-gl_ux[5];

 y1+=pmult*256;
 x1+=((GlobalTexturePage-16*pmult)<<6);

 if(iFrameTexType==3)
  {
   if(iFrameReadType==4) return 0;

   if(!FastCheckAgainstFrontScreen(x1,y1,x2,y2) &&
      !FastCheckAgainstScreen(x1,y1,x2,y2))
    return 0;

   if(bFakeFrontBuffer) bIgnoreNextTile=TRUE;
   CheckVRamReadEx(x1,y1,x1+x2,y1+y2);
   return 0;
  }

 /////////////////////////

 if(FastCheckAgainstFrontScreen(x1,y1,x2,y2))
  {
   x1-=PSXDisplay.DisplayPosition.x;
   y1-=PSXDisplay.DisplayPosition.y;
  }
 else
 if(FastCheckAgainstScreen(x1,y1,x2,y2))
  {
   x1-=PreviousPSXDisplay.DisplayPosition.x;
   y1-=PreviousPSXDisplay.DisplayPosition.y;
  }
 else return 0;

 if(!gTexFrameName)
  {
   char * p;

   if(iResX>1280 || iResY>1024) iFTex=2048;
   else
   if(iResX>640  || iResY>480)  iFTex=1024;
   else                         iFTex=512;

   glGenTextures(1, &gTexFrameName); glError();
   gTexName=gTexFrameName;
   glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iClampType); glError();
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, iClampType); glError();
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iFilter); glError();
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, iFilter); glError();

   p=(char *)_mem2_malloc(iFTex*iFTex*4);
   memset(p,0,iFTex*iFTex*4);
   #ifdef DISP_DEBUG
//sprintf(txtbuffer, "Fake15BitTexture            %d %d\r\n", iFTex, iFTex);
//DEBUG_print(txtbuffer, DBG_SPU1);
#endif // DISP_DEBUG
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iFTex, iFTex, 0, GL_RGBA, GL_UNSIGNED_BYTE, p); glError();
   _mem2_free(p);

   glGetError();
  }
 else
  {
   gTexName=gTexFrameName;
   glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();
  }
      //LOGE("Fake15BitTexture x:%d y:%d",iFTex,iFTex);
 x1+=PreviousPSXDisplay.Range.x0;
 y1+=PreviousPSXDisplay.Range.y0;

 if(PSXDisplay.DisplayMode.x)
      ScaleX=(float)rRatioRect.right/(float)PSXDisplay.DisplayMode.x;
 else ScaleX=1.0f;
 if(PSXDisplay.DisplayMode.y)
      ScaleY=(float)rRatioRect.bottom/(float)PSXDisplay.DisplayMode.y;
 else ScaleY=1.0f;

 rSrc.left  =max(x1*ScaleX,0);
 rSrc.right =min((x1+x2)*ScaleX+0.99f,iResX-1);
 rSrc.top   =max(y1*ScaleY,0);
 rSrc.bottom=min((y1+y2)*ScaleY+0.99f,iResY-1);

 iYAdjust=(y1+y2)-PSXDisplay.DisplayMode.y;
 if(iYAdjust>0)
      iYAdjust=(int)((float)iYAdjust*ScaleY)+1;
 else iYAdjust=0;

 gl_vy[0]=255-gl_vy[0];
 gl_vy[1]=255-gl_vy[1];
 gl_vy[2]=255-gl_vy[2];
 gl_vy[3]=255-gl_vy[3];

 y1=min(gl_vy[0],min(gl_vy[1],min(gl_vy[2],gl_vy[3])));

 gl_vy[0]-=y1;
 gl_vy[1]-=y1;
 gl_vy[2]-=y1;
 gl_vy[3]-=y1;
 gl_ux[0]-=gl_ux[7];
 gl_ux[1]-=gl_ux[7];
 gl_ux[2]-=gl_ux[7];
 gl_ux[3]-=gl_ux[7];

 ScaleX*=256.0f/((float)(iFTex));
 ScaleY*=256.0f/((float)(iFTex));

 y1=((float)gl_vy[0]*ScaleY); if(y1>255) y1=255;
 gl_vy[0]=y1;
 y1=((float)gl_vy[1]*ScaleY); if(y1>255) y1=255;
 gl_vy[1]=y1;
 y1=((float)gl_vy[2]*ScaleY); if(y1>255) y1=255;
 gl_vy[2]=y1;
 y1=((float)gl_vy[3]*ScaleY); if(y1>255) y1=255;
 gl_vy[3]=y1;

 x1=((float)gl_ux[0]*ScaleX); if(x1>255) x1=255;
 gl_ux[0]=x1;
 x1=((float)gl_ux[1]*ScaleX); if(x1>255) x1=255;
 gl_ux[1]=x1;
 x1=((float)gl_ux[2]*ScaleX); if(x1>255) x1=255;
 gl_ux[2]=x1;
 x1=((float)gl_ux[3]*ScaleX); if(x1>255) x1=255;
 gl_ux[3]=x1;

 x1=rSrc.right-rSrc.left;
 if(x1<=0)             x1=1;
 if(x1>iFTex)          x1=iFTex;

 y1=rSrc.bottom-rSrc.top;
 if(y1<=0)             y1=1;
 if(y1+iYAdjust>iFTex) y1=iFTex-iYAdjust;


 #ifdef DISP_DEBUG
//sprintf(txtbuffer, "glCopyTexSubImage2D\r\n");
//DEBUG_print(txtbuffer, DBG_CDR4);
#endif // DISP_DEBUG
 glCopyTexSubImage2D( GL_TEXTURE_2D, 0,
                      0,
                      iYAdjust,
                      rSrc.left+rRatioRect.left,
                      iResY-rSrc.bottom-rRatioRect.top,
                      x1,y1); glError();

// if(glGetError()) // never error
//  {
//   char * p=(char *)_mem2_malloc(iFTex*iFTex*4);
//   memset(p,0,iFTex*iFTex*4);
//   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, iFTex, iFTex,
//                   GL_RGBA, GL_UNSIGNED_BYTE, p); glError();
//   _mem2_free(p);
//  }


 ubOpaqueDraw=0;

 if(iSpriteTex)
  {
   sprtW=gl_ux[1]-gl_ux[0];
   sprtH=-(gl_vy[0]-gl_vy[2]);
  }

 return (GLuint)gTexName;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// load texture part (unpacked)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#ifdef GX_TEXPATH_PHASE1E
#define GX_DIRECT_TEX_TYPE_SEMI    0x1
#define GX_DIRECT_TEX_TYPE_OPAQUE  0x2

static inline unsigned short *DirectRGB5A3Pixel(void *data,
                                                unsigned int blockWidth,
                                                unsigned int x,
                                                unsigned int y)
{
 unsigned int tileOffset=(((y>>2)*blockWidth)+(x>>2))*32;
 unsigned int pixelOffset=((((y&3)*4)+(x&3))*2);
 return (unsigned short *)((unsigned char *)data+tileOffset+pixelOffset);
}

static inline void PutDirectRGB5A3Pixel(OGXRGB5A3DirectMap *map,
                                        unsigned int x,unsigned int y,
                                        unsigned short pixel,
                                        int *textureType)
{
 unsigned short *opaque=DirectRGB5A3Pixel(map->opaqueData,map->blockWidth,x,y);
 unsigned short *semi=DirectRGB5A3Pixel(map->semiTransData,map->blockWidth,x,y);

 if(!pixel)
  {
   *opaque=0;
   *semi=0;
  }
 else if(map->blendEnabled && !(pixel&0x8000))
  {
   *opaque=0;
   *semi=pixel|0x8000;
   *textureType|=GX_DIRECT_TEX_TYPE_SEMI;
  }
 else
  {
   *opaque=pixel|0x8000;
   *semi=0;
   *textureType|=GX_DIRECT_TEX_TYPE_OPAQUE;
  }
}

static inline void CopyDirectRGB5A3Pixel(OGXRGB5A3DirectMap *map,
                                         unsigned int dstX,unsigned int dstY,
                                         unsigned int srcX,unsigned int srcY)
{
 *DirectRGB5A3Pixel(map->opaqueData,map->blockWidth,dstX,dstY)=
  *DirectRGB5A3Pixel(map->opaqueData,map->blockWidth,srcX,srcY);
 *DirectRGB5A3Pixel(map->semiTransData,map->blockWidth,dstX,dstY)=
  *DirectRGB5A3Pixel(map->semiTransData,map->blockWidth,srcX,srcY);
}

static int CanLoadSubTexturePageSortDirect(void)
{
 if(!iFilterType) return 1;
 if(iFilterType!=2 && iFilterType!=4 && iFilterType!=6) return 1;
 if((iFilterType==4 || iFilterType==6) &&
    ly0==ly1 && ly2==ly3 && lx0==lx3 && lx1==lx2) return 1;
 return 0;
}

/* Decode a normal cached PSX texture directly into the MEM2 GX_TF_RGB5A3
 * atlas.  This removes texturepart's 32-bit linear staging write and the
 * second linear-to-tiled conversion pass. */
static int LoadSubTexturePageSortDirect(int pageid,int mode,short cx,short cy)
{
 OGXRGB5A3DirectMap map;
 unsigned short palette[256];
 unsigned int x1=gl_ux[7],x2=gl_ux[6],y1=gl_ux[5],y2=gl_ux[4];
 unsigned int dx=x2-x1+1,dy=y2-y1+1;
 unsigned int hasX=(XTexS!=0),hasY=(YTexS!=0);
 unsigned int finalX=XTexS-hasX,finalY=YTexS-hasY;
 unsigned int finalW=dx+(hasX?2:0),finalH=dy+(hasY?2:0);
 unsigned int dataX=finalX+hasX,dataY=finalY+hasY;
 unsigned int palstart=(unsigned int)cx+((unsigned int)cy<<10);
 unsigned int x,y,index,raw,start,n_xi,n_yi;
 int pmult=pageid/16;
 int textureType=0;
 unsigned int (*LTCOL)(unsigned int)=TCF[DrawSemiTrans];

 if(mode<0 || mode>2) return 0;

 PrepareSubTextureSortTexture();
 if(!glBeginRGB5A3DirectSubImage(finalX,finalY,finalW,finalH,&map))
  return 0;

 ubOpaqueDraw=0;
 GX_TEXPATH_ADD_DECODED(mode,dx*dy);

 if(mode==0 || (mode==1 && (GlobalTextIL || dx*dy>384)))
  {
   unsigned int colors=(mode==0)?16:256;
   for(index=0;index<colors;index++)
    palette[index]=(unsigned short)LTCOL(GETLE16(psxVuw+palstart+index));
  }

 if(mode==0)
  {
   start=((pageid-16*pmult)<<7)+524288*pmult;
   for(y=y1;y<=y2;y++)
    for(x=x1;x<=x2;x++)
     {
      if(GlobalTextIL)
       {
        n_xi=((x>>2)&~0x3c)+((y<<2)&0x3c);
        n_yi=(y&~0xf)+((x>>4)&0xf);
        raw=GETLE16(psxVuw+((GlobalTextAddrY+n_yi)*1024)+
                    GlobalTextAddrX+n_xi);
        index=(raw>>((x&3)<<2))&0xf;
       }
      else
       {
        raw=*(psxVub+start+(y<<11)+(x>>1));
        index=(raw>>((x&1)<<2))&0xf;
       }
      PutDirectRGB5A3Pixel(&map,dataX+(x-x1),dataY+(y-y1),
                           palette[index],&textureType);
     }
  }
 else if(mode==1)
  {
   start=((pageid-16*pmult)<<7)+524288*pmult;
   for(y=y1;y<=y2;y++)
    for(x=x1;x<=x2;x++)
     {
      if(GlobalTextIL)
       {
        n_xi=((x>>1)&~0x78)+((x<<2)&0x40)+((y<<3)&0x38);
        n_yi=(y&~7)+((x>>5)&7);
        raw=GETLE16(psxVuw+((GlobalTextAddrY+n_yi)*1024)+
                    GlobalTextAddrX+n_xi);
        index=(raw>>((x&1)<<3))&0xff;
       }
      else
       index=*(psxVub+start+(y<<11)+x);

      if(GlobalTextIL || dx*dy>384)
       raw=palette[index];
      else
       raw=(unsigned short)LTCOL(GETLE16(psxVuw+palstart+index));
      PutDirectRGB5A3Pixel(&map,dataX+(x-x1),dataY+(y-y1),
                           (unsigned short)raw,&textureType);
     }
  }
 else
  {
   start=((pageid-16*pmult)<<6)+262144*pmult;
   for(y=y1;y<=y2;y++)
    for(x=x1;x<=x2;x++)
     {
      raw=(unsigned short)LTCOL(GETLE16(psxVuw+start+(y<<10)+x));
      PutDirectRGB5A3Pixel(&map,dataX+(x-x1),dataY+(y-y1),
                           (unsigned short)raw,&textureType);
     }
  }

 if(hasY)
  for(x=finalX;x<finalX+finalW;x++)
   {
    CopyDirectRGB5A3Pixel(&map,x,finalY,x,dataY);
    CopyDirectRGB5A3Pixel(&map,x,finalY+finalH-1,x,dataY+dy-1);
   }
 if(hasX)
  for(y=finalY;y<finalY+finalH;y++)
   {
    CopyDirectRGB5A3Pixel(&map,finalX,y,dataX,y);
    CopyDirectRGB5A3Pixel(&map,finalX+finalW-1,y,dataX+dx-1,y);
   }

 XTexS=finalX;YTexS=finalY;DXTexS=finalW;DYTexS=finalH;
 gl_ux[8]=(GLubyte)glEndRGB5A3DirectSubImage(&map,textureType);
 glError();
 return 1;
}
#endif

void LoadSubTexturePageSort(int pageid, int mode, short cx, short cy)
{
 unsigned int  start,row,column,j,sxh,sxm;
 unsigned int   palstart;
 unsigned int  *px,*pa,*ta;
 unsigned char  *cSRCPtr;
 unsigned short *wSRCPtr;
 unsigned int  LineOffset;
 unsigned int  x2a,xalign=0;
 unsigned int  x1=gl_ux[7];
 unsigned int  x2=gl_ux[6];
 unsigned int  y1=gl_ux[5];
 unsigned int  y2=gl_ux[4];
 unsigned int  dx=x2-x1+1;
 unsigned int  dy=y2-y1+1;
 int pmult=pageid/16;
 unsigned int (*LTCOL)(unsigned int);
 unsigned int a,r,g,b,cnt,h;
 unsigned int scol[8];

#ifdef GX_TEXPATH_PHASE1E
 if(CanLoadSubTexturePageSortDirect() &&
    LoadSubTexturePageSortDirect(pageid,mode,cx,cy)) return;
#endif

 LTCOL=TCF[DrawSemiTrans];

 GX_TEXPATH_ADD_DECODED(mode,
                        (unsigned int)dx * (unsigned int)dy);

 pa=px=(unsigned int *)ubPaletteBuffer;
 ta=(unsigned int *)texturepart;
 palstart=cx+(cy<<10);

 ubOpaqueDraw=0;

 if(YTexS) {ta+=dx;if(XTexS) ta+=2;}
 if(XTexS) {ta+=1;xalign=2;}

 switch(mode)
  {
   //--------------------------------------------------//
   // 4bit texture load ..
   case 0:
    if(GlobalTextIL)
     {
      unsigned int TXV,TXU,n_xi,n_yi;

      wSRCPtr=psxVuw+palstart;

      row=4;do
       {
        *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
        *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
        *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
        *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
        row--;px+=4;wSRCPtr+=4;
       }
      while (row);

      for(TXV=y1;TXV<=y2;TXV++)
       {
        for(TXU=x1;TXU<=x2;TXU++)
         {
  		  n_xi = ( ( TXU >> 2 ) & ~0x3c ) + ( ( TXV << 2 ) & 0x3c );
		  n_yi = ( TXV & ~0xf ) + ( ( TXU >> 4 ) & 0xf );

          //*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
          *ta++ = *(pa + ((GETLE16((unsigned short *)(psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi)) >> ( ( TXU & 0x03 ) << 2 ) ) & 0x0f ));
         }
        ta+=xalign;
       }
      break;
     }

    start=((pageid-16*pmult)<<7)+524288*pmult;
    // convert CLUT to 32bits .. and then use THAT as a lookup table

    wSRCPtr=psxVuw+palstart;

    row=4;do
     {
      *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
      *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
      *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
      *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
      row--;px+=4;wSRCPtr+=4;
     }
    while (row);

    x2a=x2?(x2-1):0;//if(x2) x2a=x2-1; else x2a=0;
    sxm=x1&1;sxh=x1>>1;
    j=sxm?(x1+1):x1;//if(sxm) j=x1+1; else j=x1;
    for(column=y1;column<=y2;column++)
     {
      cSRCPtr = psxVub + start + (column<<11) + sxh;

      if(sxm) *ta++=*(pa+((*cSRCPtr++ >> 4) & 0xF));

      for(row=j;row<x2a;row+=2)
       {
        *ta    =*(pa+(*cSRCPtr & 0xF));
        *(ta+1)=*(pa+((*cSRCPtr >> 4) & 0xF));
        cSRCPtr++;ta+=2;
       }

      if(row<=x2)
       {
        *ta++=*(pa+(*cSRCPtr & 0xF)); row++;
        if(row<=x2) *ta++=*(pa+((*cSRCPtr >> 4) & 0xF));
       }

      ta+=xalign;
     }

    break;
   //--------------------------------------------------//
   // 8bit texture load ..
   case 1:
    if(GlobalTextIL)
     {
      unsigned int TXV,TXU,n_xi,n_yi;

      wSRCPtr=psxVuw+palstart;

      row=64;do
       {
        *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
        *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
        *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
        *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
        row--;px+=4;wSRCPtr+=4;
       }
      while (row);

      for(TXV=y1;TXV<=y2;TXV++)
       {
        for(TXU=x1;TXU<=x2;TXU++)
         {
  		  n_xi = ( ( TXU >> 1 ) & ~0x78 ) + ( ( TXU << 2 ) & 0x40 ) + ( ( TXV << 3 ) & 0x38 );
		  n_yi = ( TXV & ~0x7 ) + ( ( TXU >> 5 ) & 0x7 );

          //*ta++=*(pa+((*( psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi ) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff));
          *ta++ = *(pa + ((GETLE16((unsigned short *)(psxVuw + ((GlobalTextAddrY + n_yi)*1024) + GlobalTextAddrX + n_xi)) >> ( ( TXU & 0x01 ) << 3 ) ) & 0xff ));
         }
        ta+=xalign;
       }

      break;
     }

    start=((pageid-16*pmult)<<7)+524288*pmult;

    cSRCPtr = psxVub + start + (y1<<11) + x1;
    LineOffset = 2048 - dx;

    if(dy*dx>384)
     {
      wSRCPtr=psxVuw+palstart;

      row=64;do
       {
        *px    =LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
        *(px+1)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 1)));
        *(px+2)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 2)));
        *(px+3)=LTCOL(GETLE16((unsigned short *)(wSRCPtr + 3)));
        row--;px+=4;wSRCPtr+=4;
       }
      while (row);

      column=dy;do
       {
        row=dx;
        do {*ta++=*(pa+(*cSRCPtr++));row--;} while(row);
        ta+=xalign;
        cSRCPtr+=LineOffset;column--;
       }
      while(column);
     }
    else
     {
      wSRCPtr=psxVuw+palstart;

      column=dy;do
       {
        row=dx;
        //do {*ta++=LTCOL(GETLE16(wSRCPtr+*cSRCPtr++));row--;} while(row);
        do {
            *ta++ = LTCOL(GETLE16((unsigned short *)(wSRCPtr + *cSRCPtr++)));
            row--;
        }
        while(row);
        ta+=xalign;
        cSRCPtr+=LineOffset;column--;
       }
      while(column);
     }

    break;
   //--------------------------------------------------//
   // 16bit texture load ..
   case 2:
    start=((pageid-16*pmult)<<6)+262144*pmult;

    wSRCPtr = psxVuw + start + (y1<<10) + x1;
    LineOffset = 1024 - dx;

    column=dy;do
     {
      row=dx;
      //do {*ta++=LTCOL(*wSRCPtr++);row--;} while(row);
      do {
            *ta++ = LTCOL(GETLE16((unsigned short *)(wSRCPtr)));
			wSRCPtr++;
            row--;
      }
      while(row);
      ta+=xalign;
      wSRCPtr+=LineOffset;column--;
     }
    while(column);

    break;
   //--------------------------------------------------//
   // others are not possible !
  }

 x2a=dx+xalign;

 if(YTexS)
  {
   ta=(unsigned int *)texturepart;
   pa=(unsigned int *)texturepart+x2a;
   row=x2a;do {*ta++=*pa++;row--;} while(row);
   pa=(unsigned int *)texturepart+dy*x2a;
   ta=pa+x2a;
   row=x2a;do {*ta++=*pa++;row--;} while(row);
   YTexS--;
   dy+=2;
  }

 if(XTexS)
  {
   ta=(unsigned int *)texturepart;
   pa=ta+1;
   row=dy;do {*ta=*pa;ta+=x2a;pa+=x2a;row--;} while(row);
   pa=(unsigned int *)texturepart+dx;
   ta=pa+1;
   row=dy;do {*ta=*pa;ta+=x2a;pa+=x2a;row--;} while(row);
   XTexS--;
   dx+=2;
  }

 DXTexS=dx;DYTexS=dy;

 if(!iFilterType) {DefineSubTextureSort();return;}
 if(iFilterType!=2 && iFilterType!=4 && iFilterType!=6) {DefineSubTextureSort();return;}
 if((iFilterType==4 || iFilterType==6) && ly0==ly1 && ly2==ly3 && lx0==lx3 && lx1==lx2)
  {DefineSubTextureSort();return;}

// ta=(unsigned int *)texturepart;
// x1=dx-1;
// y1=dy-1;
//
// if(bOpaquePass)
//  {
//{
//     for(column=0;column<dy;column++)
//      {
//       for(row=0;row<dx;row++)
//        {
//         if(*ta==0x50000000)
//          {
//           cnt=0;
//
//           if(           column     && *(ta-dx)  !=0x50000000 && *(ta-dx)>>24!=1) scol[cnt++]=*(ta-dx);
//           if(row                   && *(ta-1)   !=0x50000000 && *(ta-1)>>24!=1) scol[cnt++]=*(ta-1);
//           if(row!=x1               && *(ta+1)   !=0x50000000 && *(ta+1)>>24!=1) scol[cnt++]=*(ta+1);
//           if(           column!=y1 && *(ta+dx)  !=0x50000000 && *(ta+dx)>>24!=1) scol[cnt++]=*(ta+dx);
//
//           if(row     && column     && *(ta-dx-1)!=0x50000000 && *(ta-dx-1)>>24!=1) scol[cnt++]=*(ta-dx-1);
//           if(row!=x1 && column     && *(ta-dx+1)!=0x50000000 && *(ta-dx+1)>>24!=1) scol[cnt++]=*(ta-dx+1);
//           if(row     && column!=y1 && *(ta+dx-1)!=0x50000000 && *(ta+dx-1)>>24!=1) scol[cnt++]=*(ta+dx-1);
//           if(row!=x1 && column!=y1 && *(ta+dx+1)!=0x50000000 && *(ta+dx+1)>>24!=1) scol[cnt++]=*(ta+dx+1);
//
//           if(cnt)
//            {
//             r=g=b=a=0;
//             for(h=0;h<cnt;h++)
//              {
//               a+=(scol[h]>>24);
//               r+=(scol[h]>>16)&0xff;
//               g+=(scol[h]>>8)&0xff;
//               b+=scol[h]&0xff;
//              }
//             r/=cnt;b/=cnt;g/=cnt;
//
//             *ta=(r<<16)|(g<<8)|b;
//             if(a) *ta|=0x50000000;
//             else  *ta|=0x01000000;
//            }
//          }
//         ta++;
//        }
//      }
//    }
//  }
// else
// for(column=0;column<dy;column++)
//  {
//   for(row=0;row<dx;row++)
//    {
//     if(*ta==0x00000000)
//      {
//       cnt=0;
//
//       if(row!=x1               && *(ta+1)   !=0x00000000) scol[cnt++]=*(ta+1);
//       if(           column!=y1 && *(ta+dx)  !=0x00000000) scol[cnt++]=*(ta+dx);
//
//       if(cnt)
//        {
//         r=g=b=0;
//         for(h=0;h<cnt;h++)
//          {
//           r+=(scol[h]>>16)&0xff;
//           g+=(scol[h]>>8)&0xff;
//           b+=scol[h]&0xff;
//          }
//         r/=cnt;b/=cnt;g/=cnt;
//         *ta=(r<<16)|(g<<8)|b;
//        }
//      }
//     ta++;
//    }
//  }
//
// DefineSubTextureSort();
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// load texture part (packed)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// hires texture funcs
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


#define GET_RESULT(A, B, C, D) ((A != C || A != D) - (B != C || B != D))

////////////////////////////////////////////////////////////////////////

//#define colorMask8     0x00FEFEFE
//#define lowPixelMask8  0x00010101
//#define qcolorMask8    0x00FCFCFC
//#define qlowpixelMask8 0x00030303
//
//
//#define INTERPOLATE8_02(A, B) (((((A & colorMask8) >> 1) + ((B & colorMask8) >> 1) + (A & B & lowPixelMask8))|((((A&0xFF000000)==0x03000000)?0x03000000:(((B&0xFF000000)==0x03000000)?0x03000000:(((A&0xFF000000)==0x00000000)?0x00000000:(((B&0xFF000000)==0x00000000)?0x00000000:0xFF000000)))))))
//
//#define Q_INTERPOLATE8_02(A, B, C, D) (((((A & qcolorMask8) >> 2) + ((B & qcolorMask8) >> 2) + ((C & qcolorMask8) >> 2) + ((D & qcolorMask8) >> 2) + ((((A & qlowpixelMask8) + (B & qlowpixelMask8) + (C & qlowpixelMask8) + (D & qlowpixelMask8)) >> 2) & qlowpixelMask8))|((((A&0xFF000000)==0x03000000)?0x03000000:(((B&0xFF000000)==0x03000000)?0x03000000:(((C&0xFF000000)==0x03000000)?0x03000000:(((D&0xFF000000)==0x03000000)?0x03000000:(((A&0xFF000000)==0x00000000)?0x00000000:(((B&0xFF000000)==0x00000000)?0x00000000:(((C&0xFF000000)==0x00000000)?0x00000000:(((D&0xFF000000)==0x00000000)?0x00000000:0xFF000000)))))))))))
//
//#define INTERPOLATE8(A, B) (((((A & colorMask8) >> 1) + ((B & colorMask8) >> 1) + (A & B & lowPixelMask8))|((((A&0xFF000000)==0x50000000)?0x50000000:(((B&0xFF000000)==0x50000000)?0x50000000:(((A&0xFF000000)==0x00000000)?0x00000000:(((B&0xFF000000)==0x00000000)?0x00000000:0xFF000000)))))))
//
//#define Q_INTERPOLATE8(A, B, C, D) (((((A & qcolorMask8) >> 2) + ((B & qcolorMask8) >> 2) + ((C & qcolorMask8) >> 2) + ((D & qcolorMask8) >> 2) + ((((A & qlowpixelMask8) + (B & qlowpixelMask8) + (C & qlowpixelMask8) + (D & qlowpixelMask8)) >> 2) & qlowpixelMask8))|((((A&0xFF000000)==0x50000000)?0x50000000:(((B&0xFF000000)==0x50000000)?0x50000000:(((C&0xFF000000)==0x50000000)?0x50000000:(((D&0xFF000000)==0x50000000)?0x50000000:(((A&0xFF000000)==0x00000000)?0x00000000:(((B&0xFF000000)==0x00000000)?0x00000000:(((C&0xFF000000)==0x00000000)?0x00000000:(((D&0xFF000000)==0x00000000)?0x00000000:0xFF000000)))))))))))
//
//void Super2xSaI_ex8_Ex(unsigned char *srcPtr, DWORD srcPitch,
//	            unsigned char  *dstBitmap, int width, int height)
//{
// DWORD dstPitch = srcPitch * 2;
// DWORD line;
// DWORD *dP;
// DWORD *bP;
// int   width2 = width*2;
// int iXA,iXB,iXC,iYA,iYB,iYC,finish;
// DWORD color4, color5, color6;
// DWORD color1, color2, color3;
// DWORD colorA0, colorA1, colorA2, colorA3,
//       colorB0, colorB1, colorB2, colorB3,
//       colorS1, colorS2;
// DWORD product1a, product1b,
//       product2a, product2b;
//
// line = 0;
//
//  {
//   for (; height; height-=1)
//	{
//     bP = (DWORD *)srcPtr;
//	 dP = (DWORD *)(dstBitmap + line*dstPitch);
//     for (finish = width; finish; finish -= 1 )
//      {
////---------------------------------------    B1 B2
////                                         4  5  6 S2
////                                         1  2  3 S1
////                                           A1 A2
//       if(finish==width) iXA=0;
//       else              iXA=1;
//       if(finish>4) {iXB=1;iXC=2;}
//       else
//       if(finish>3) {iXB=1;iXC=1;}
//       else         {iXB=0;iXC=0;}
//       if(line==0) iYA=0;
//       else        iYA=width;
//       if(height>4) {iYB=width;iYC=width2;}
//       else
//       if(height>3) {iYB=width;iYC=width;}
//       else         {iYB=0;iYC=0;}
//
//
//       colorB0 = *(bP- iYA - iXA);
//       colorB1 = *(bP- iYA);
//       colorB2 = *(bP- iYA + iXB);
//       colorB3 = *(bP- iYA + iXC);
//
//       color4 = *(bP  - iXA);
//       color5 = *(bP);
//       color6 = *(bP  + iXB);
//       colorS2 = *(bP + iXC);
//
//       color1 = *(bP  + iYB  - iXA);
//       color2 = *(bP  + iYB);
//       color3 = *(bP  + iYB  + iXB);
//       colorS1= *(bP  + iYB  + iXC);
//
//       colorA0 = *(bP + iYC - iXA);
//       colorA1 = *(bP + iYC);
//       colorA2 = *(bP + iYC + iXB);
//       colorA3 = *(bP + iYC + iXC);
//
////--------------------------------------
//       if (color2 == color6 && color5 != color3)
//        {
//         product2b = product1b = color2;
//        }
//       else
//       if (color5 == color3 && color2 != color6)
//        {
//         product2b = product1b = color5;
//        }
//       else
//       if (color5 == color3 && color2 == color6)
//        {
//         register int r = 0;
//
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color1&0x00ffffff),  (colorA1&0x00ffffff));
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color4&0x00ffffff),  (colorB1&0x00ffffff));
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorA2&0x00ffffff), (colorS1&0x00ffffff));
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorB2&0x00ffffff), (colorS2&0x00ffffff));
//
//         if (r > 0)
//          product2b = product1b = color6;
//         else
//         if (r < 0)
//          product2b = product1b = color5;
//         else
//          {
//           product2b = product1b = INTERPOLATE8_02(color5, color6);
//          }
//        }
//       else
//        {
//         if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
//             product2b = Q_INTERPOLATE8_02 (color3, color3, color3, color2);
//         else
//         if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
//             product2b = Q_INTERPOLATE8_02 (color2, color2, color2, color3);
//         else
//             product2b = INTERPOLATE8_02 (color2, color3);
//
//         if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
//             product1b = Q_INTERPOLATE8_02 (color6, color6, color6, color5);
//         else
//         if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
//             product1b = Q_INTERPOLATE8_02 (color6, color5, color5, color5);
//         else
//             product1b = INTERPOLATE8_02 (color5, color6);
//        }
//
//       if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
//        product2a = INTERPOLATE8_02(color2, color5);
//       else
//       if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
//        product2a = INTERPOLATE8_02(color2, color5);
//       else
//        product2a = color2;
//
//       if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
//        product1a = INTERPOLATE8_02(color2, color5);
//       else
//       if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
//        product1a = INTERPOLATE8_02(color2, color5);
//       else
//        product1a = color5;
//
//       *dP=product1a;
//       *(dP+1)=product1b;
//       *(dP+(width2))=product2a;
//       *(dP+1+(width2))=product2b;
//
//       bP += 1;
//       dP += 2;
//      }//end of for ( finish= width etc..)
//
//     line += 2;
//     srcPtr += srcPitch;
//	}; //endof: for (; height; height--)
//  }
//}
//
//
//void Super2xSaI_ex8(unsigned char *srcPtr, DWORD srcPitch,
//	            unsigned char  *dstBitmap, int width, int height)
//{
// DWORD dstPitch = srcPitch * 2;
// DWORD line;
// DWORD *dP;
// DWORD *bP;
// int   width2 = width*2;
// int iXA,iXB,iXC,iYA,iYB,iYC,finish;
// DWORD color4, color5, color6;
// DWORD color1, color2, color3;
// DWORD colorA0, colorA1, colorA2, colorA3,
//       colorB0, colorB1, colorB2, colorB3,
//       colorS1, colorS2;
// DWORD product1a, product1b,
//       product2a, product2b;
//
// line = 0;
//
//  {
//   for (; height; height-=1)
//	{
//     bP = (DWORD *)srcPtr;
//	 dP = (DWORD *)(dstBitmap + line*dstPitch);
//     for (finish = width; finish; finish -= 1 )
//      {
////---------------------------------------    B1 B2
////                                         4  5  6 S2
////                                         1  2  3 S1
////                                           A1 A2
//       if(finish==width) iXA=0;
//       else              iXA=1;
//       if(finish>4) {iXB=1;iXC=2;}
//       else
//       if(finish>3) {iXB=1;iXC=1;}
//       else         {iXB=0;iXC=0;}
//       if(line==0) iYA=0;
//       else        iYA=width;
//       if(height>4) {iYB=width;iYC=width2;}
//       else
//       if(height>3) {iYB=width;iYC=width;}
//       else         {iYB=0;iYC=0;}
//
//
//       colorB0 = *(bP- iYA - iXA);
//       colorB1 = *(bP- iYA);
//       colorB2 = *(bP- iYA + iXB);
//       colorB3 = *(bP- iYA + iXC);
//
//       color4 = *(bP  - iXA);
//       color5 = *(bP);
//       color6 = *(bP  + iXB);
//       colorS2 = *(bP + iXC);
//
//       color1 = *(bP  + iYB  - iXA);
//       color2 = *(bP  + iYB);
//       color3 = *(bP  + iYB  + iXB);
//       colorS1= *(bP  + iYB  + iXC);
//
//       colorA0 = *(bP + iYC - iXA);
//       colorA1 = *(bP + iYC);
//       colorA2 = *(bP + iYC + iXB);
//       colorA3 = *(bP + iYC + iXC);
//
////--------------------------------------
//       if (color2 == color6 && color5 != color3)
//        {
//         product2b = product1b = color2;
//        }
//       else
//       if (color5 == color3 && color2 != color6)
//        {
//         product2b = product1b = color5;
//        }
//       else
//       if (color5 == color3 && color2 == color6)
//        {
//         register int r = 0;
//
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color1&0x00ffffff),  (colorA1&0x00ffffff));
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (color4&0x00ffffff),  (colorB1&0x00ffffff));
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorA2&0x00ffffff), (colorS1&0x00ffffff));
//         r += GET_RESULT ((color6&0x00ffffff), (color5&0x00ffffff), (colorB2&0x00ffffff), (colorS2&0x00ffffff));
//
//         if (r > 0)
//          product2b = product1b = color6;
//         else
//         if (r < 0)
//          product2b = product1b = color5;
//         else
//          {
//           product2b = product1b = INTERPOLATE8(color5, color6);
//          }
//        }
//       else
//        {
//         if (color6 == color3 && color3 == colorA1 && color2 != colorA2 && color3 != colorA0)
//             product2b = Q_INTERPOLATE8 (color3, color3, color3, color2);
//         else
//         if (color5 == color2 && color2 == colorA2 && colorA1 != color3 && color2 != colorA3)
//             product2b = Q_INTERPOLATE8 (color2, color2, color2, color3);
//         else
//             product2b = INTERPOLATE8 (color2, color3);
//
//         if (color6 == color3 && color6 == colorB1 && color5 != colorB2 && color6 != colorB0)
//             product1b = Q_INTERPOLATE8 (color6, color6, color6, color5);
//         else
//         if (color5 == color2 && color5 == colorB2 && colorB1 != color6 && color5 != colorB3)
//             product1b = Q_INTERPOLATE8 (color6, color5, color5, color5);
//         else
//             product1b = INTERPOLATE8 (color5, color6);
//        }
//
//       if (color5 == color3 && color2 != color6 && color4 == color5 && color5 != colorA2)
//        product2a = INTERPOLATE8(color2, color5);
//       else
//       if (color5 == color1 && color6 == color5 && color4 != color2 && color5 != colorA0)
//        product2a = INTERPOLATE8(color2, color5);
//       else
//        product2a = color2;
//
//       if (color2 == color6 && color5 != color3 && color1 == color2 && color2 != colorB2)
//        product1a = INTERPOLATE8(color2, color5);
//       else
//       if (color4 == color2 && color3 == color2 && color1 != color5 && color2 != colorB0)
//        product1a = INTERPOLATE8(color2, color5);
//       else
//        product1a = color5;
//
//       *dP=product1a;
//       *(dP+1)=product1b;
//       *(dP+(width2))=product2a;
//       *(dP+1+(width2))=product2b;
//
//       bP += 1;
//       dP += 2;
//      }//end of for ( finish= width etc..)
//
//     line += 2;
//     srcPtr += srcPitch;
//	}; //endof: for (; height; height--)
//  }
//}
///////////////////////////////////////////////////////////////////////////////
//
//#define colorMask4     0x0000EEE0
//#define lowPixelMask4  0x00001110
//#define qcolorMask4    0x0000CCC0
//#define qlowpixelMask4 0x00003330
//
//#define INTERPOLATE4(A, B) ((((A & colorMask4) >> 1) + ((B & colorMask4) >> 1) + (A & B & lowPixelMask4))|((((A&0x0000000F)==0x00000006)?0x00000006:(((B&0x0000000F)==0x00000006)?0x00000006:(((A&0x0000000F)==0x00000000)?0x00000000:(((B&0x0000000F)==0x00000000)?0x00000000:0x0000000F))))))
//
//#define Q_INTERPOLATE4(A, B, C, D) ((((A & qcolorMask4) >> 2) + ((B & qcolorMask4) >> 2) + ((C & qcolorMask4) >> 2) + ((D & qcolorMask4) >> 2) + ((((A & qlowpixelMask4) + (B & qlowpixelMask4) + (C & qlowpixelMask4) + (D & qlowpixelMask4)) >> 2) & qlowpixelMask4))| ((((A&0x0000000F)==0x00000006)?0x00000006:(((B&0x0000000F)==0x00000006)?0x00000006:(((C&0x0000000F)==0x00000006)?0x00000006:(((D&0x0000000F)==0x00000006)?0x00000006:(((A&0x0000000F)==0x00000000)?0x00000000:(((B&0x0000000F)==0x00000000)?0x00000000:(((C&0x0000000F)==0x00000000)?0x00000000:(((D&0x0000000F)==0x00000000)?0x00000000:0x0000000F))))))))))
//
//
//#define colorMask5     0x0000F7BC
//#define lowPixelMask5  0x00000842
//#define qcolorMask5    0x0000E738
//#define qlowpixelMask5 0x000018C6
//
//#define INTERPOLATE5(A, B) ((((A & colorMask5) >> 1) + ((B & colorMask5) >> 1) + (A & B & lowPixelMask5))|((((A&0x00000001)==0x00000000)?0x00000000:(((B&0x00000001)==0x00000000)?0x00000000:0x00000001))))
//
//#define Q_INTERPOLATE5(A, B, C, D) ((((A & qcolorMask5) >> 2) + ((B & qcolorMask5) >> 2) + ((C & qcolorMask5) >> 2) + ((D & qcolorMask5) >> 2) + ((((A & qlowpixelMask5) + (B & qlowpixelMask5) + (C & qlowpixelMask5) + (D & qlowpixelMask5)) >> 2) & qlowpixelMask5))| ((((A&0x00000001)==0x00000000)?0x00000000:(((B&0x00000001)==0x00000000)?0x00000000:(((C&0x00000001)==0x00000000)?0x00000000:(((D&0x00000001)==0x00000000)?0x00000000:0x00000001))))))
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// ogl texture defines
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////

static void PrepareSubTextureSortTexture(void)
{
 glSetRGB24( 0 );

 if(!gTexName)
  {
   glGenTextures(1, &gTexName); glError();
   glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();

   glInitRGBATextures(256, 256);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, iClampType); glError();
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, iClampType); glError();

   if(iFilterType)
    {
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glError();
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); glError();
    }
   else
    {
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, iFilter); glError();
     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, iFilter); glError();
    }
   //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0,GL_RGBA, GL_UNSIGNED_BYTE, texturepart); glError();
  }
 else
 {
     glBindTextureBef(GL_TEXTURE_2D, gTexName); glError();
 }
}

void DefineSubTextureSort(void)
{
  PrepareSubTextureSortTexture();

  #ifdef DISP_DEBUG
  //int oldWidth, oldHeight;
  //glGetTextureInfo(gTexName, &oldWidth, &oldHeight);
  //sprintf(txtbuffer, "DefineSubTextureSort %d %d %d %d %d %d\r\n", oldWidth, oldHeight, XTexS, YTexS, DXTexS, DYTexS);
  sprintf(txtbuffer, "DefineSubTextureSort %d %d %d %d\r\n", XTexS, YTexS, DXTexS, DYTexS);
  DEBUG_print(txtbuffer, DBG_CDR2);
  writeLogFile(txtbuffer);
  #endif // DISP_DEBUG

  int textureType;
  textureType = glTexSubImage2D(GL_TEXTURE_2D, 0, XTexS, YTexS,
                 DXTexS, DYTexS,
                 GL_RGBA, GL_UNSIGNED_BYTE, texturepart); glError();
    gl_ux[8] = (GLubyte)(textureType);
}

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// texture cache garbage collection
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

static int SelectTextureAtlasVictim(int semiRequest)
{
 unsigned int entryCount[MAXSORTTEX_MAX];
 unsigned int bestCount=0xffffffffU;
 int victim=-1;
 int mode,page,bucket,index,count,candidate,pass;
 textureSubCacheEntryS *base,*entry;

 memset(entryCount,0,sizeof(entryCount));
 for(mode=0;mode<3;mode++)
  for(page=0;page<MAXTPAGES;page++)
   for(bucket=0;bucket<4;bucket++)
    {
     base=pscSubtexStore[mode][page]+bucket*SOFFB;
     count=GETLE32(SUBCACHE_COUNT_PTR(base));
     if(count>SOFFB-2) count=SOFFB-2;
     entry=base+1;
     for(index=0;index<count;index++,entry++)
      if(entry->ClutKey && entry->cTexID<iSortTexCnt)
       entryCount[entry->cTexID]++;
    }

 /* Prefer the atlas which destroys the fewest still-valid entries.  A
  * semi-transparent request first reuses an atlas which already owns the
  * optional semi plane; an opaque request first avoids those atlases.  The
  * atlas number itself never changes, so data and semiTransData retain the
  * original one-to-one texture-index relationship. */
 for(pass=0;pass<2 && victim<0;pass++)
  for(index=0;index<iSortTexCnt;index++)
   {
    candidate=(usAtlasEvictCursor+index)%iSortTexCnt;
    if(pass==0 &&
       ((semiRequest && !atlasHasSemiBacking[candidate]) ||
        (!semiRequest && atlasHasSemiBacking[candidate])))
     continue;
    if(entryCount[candidate]<bestCount ||
       (entryCount[candidate]==bestCount &&
        victim==(int)usLRUTexPage && candidate!=(int)usLRUTexPage))
     {
      victim=candidate;
      bestCount=entryCount[candidate];
     }
   }

 return victim;
}

static unsigned int EvictTextureAtlas(int victim)
{
 unsigned int evicted=0;
 int mode,page,bucket,index,count;
 textureSubCacheEntryS *base,*entry;

 if(victim<0 || victim>=iSortTexCnt) return 0;

 for(mode=0;mode<3;mode++)
  for(page=0;page<MAXTPAGES;page++)
   for(bucket=0;bucket<4;bucket++)
    {
     base=pscSubtexStore[mode][page]+bucket*SOFFB;
     count=GETLE32(SUBCACHE_COUNT_PTR(base));
     if(count>SOFFB-2) count=SOFFB-2;
     entry=base+1;
     for(index=0;index<count;index++,entry++)
      if(entry->ClutKey && entry->cTexID==victim)
       {
        entry->ClutKey=0;
        evicted++;
       }
    }

 /* Count zero means the whole 256x256 backing is available again.  The next
  * upload is already synchronized by the OpenGX texture upload path, so the
  * other atlas backings and all eight GX TMEM regions remain untouched. */
 pxSsubtexLeft[victim]->l=0;
 usAtlasEvictCursor=(unsigned short)((victim+1)%iSortTexCnt);
 return evicted;
}

void DoTexGarbageCollection(void)
{
 static unsigned short LRUCleaned=0;
 unsigned short iC,iC1,iC2;
 int i,j,iMax;textureSubCacheEntryS * tsb;

 GX_TEXPATH_INC(garbageCollectCalls);

 iC=4;//=iSortTexCnt/2,
 LRUCleaned+=iC;                                       // we clean different textures each time
 if((LRUCleaned+iC)>=iSortTexCnt) LRUCleaned=0;        // wrap? wrap!
 iC1=LRUCleaned;                                       // range of textures to clean
 iC2=LRUCleaned+iC;

 for(iC=iC1;iC<iC2;iC++)                               // make some textures available
  {
   pxSsubtexLeft[iC]->l=0;
  }

 for(i=0;i<3;i++)                                      // remove all references to that textures
  for(j=0;j<MAXTPAGES;j++)
   for(iC=0;iC<4;iC++)                                 // loop all texture rect info areas
    {
     tsb=pscSubtexStore[i][j]+(iC*SOFFB);
     iMax=GETLE32(SUBCACHE_COUNT_PTR(tsb));
     if(iMax)
      do
       {
        tsb++;
        if(tsb->cTexID>=iC1 && tsb->cTexID<iC2)        // info uses the cleaned textures? remove info
         {
          if(tsb->ClutKey) GX_TEXPATH_INC(garbageCollectEntries);
          tsb->ClutKey=0;
         }
       }
      while(--iMax);
     }

 usLRUTexPage=LRUCleaned;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// search cache for existing (already used) parts
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

textureSubCacheEntryS *CheckTextureInSubSCache(
 int TextureMode,uint64_t clutKey,unsigned short *pCache)
{
 textureSubCacheEntryS * tsx, * tsb, *tsg;//, *tse=NULL;
 int i,iMax;EXLong npos;
 unsigned char cx,cy;
 unsigned char atlasOrder[MAXSORTTEX_MAX];
 int iC,j,k,atlasOrderCount,semiRequest,allowSemiGrowth;
 unsigned int rx,ry,mx,my;
 EXLong * ul=0, * uls;
 EXLong rfree;
 unsigned char cXAdj,cYAdj;
#ifdef GX_TEXPATH_PROFILE
 int profileExactKeySeen=0;
#endif

 npos.l=*((unsigned int *)&gl_ux[4]);

 //--------------------------------------------------------------//
 // find matching texturepart first... speed up...
 //--------------------------------------------------------------//

 tsg=pscSubtexStore[TextureMode][GlobalTexturePage];
 tsg+=ClutKeyBucket(clutKey)*SOFFB;

 iMax=GETLE32(SUBCACHE_COUNT_PTR(tsg));
 if(iMax)
  {
   i=iMax;
   tsb=tsg+1;
   do
    {
     if(clutKey==tsb->ClutKey)
      {
#ifdef GX_TEXPATH_PROFILE
       profileExactKeySeen=1;
#endif
       if(INCHECK(tsb->pos,npos))
        {
         cx=tsb->pos.c.x1-tsb->posTX;
         cy=tsb->pos.c.y1-tsb->posTY;

         gl_ux[0]-=cx;
         gl_ux[1]-=cx;
         gl_ux[2]-=cx;
         gl_ux[3]-=cx;
         gl_vy[0]-=cy;
         gl_vy[1]-=cy;
         gl_vy[2]-=cy;
         gl_vy[3]-=cy;

         gl_ux[8]=DrawInfoTextureType(tsb->drawInfo);

         ubOpaqueDraw=DrawInfoOpaque(tsb->drawInfo);
         *pCache=tsb->cTexID;
         return NULL;
        }
      }
     tsb++;
    }
  while(--i);
  }

#ifdef GX_TEXPATH_PROFILE
 gxTextureProfileLastMissReason=ProfileClassifySubMiss(
  TextureMode,GlobalTexturePage,clutKey,profileExactKeySeen);
 ProfileClassifyPostInvalidationMiss(TextureMode,GlobalTexturePage,
                                     clutKey,npos);
#endif

 //----------------------------------------------------//

 cXAdj=1;cYAdj=1;

 rx=(int)gl_ux[6]-(int)gl_ux[7];
 ry=(int)gl_ux[4]-(int)gl_ux[5];

 tsx=NULL;tsb=tsg+1;
 for(i=0;i<iMax;i++,tsb++)
  {
   if(!tsb->ClutKey) {tsx=tsb;break;}
  }

 if(!tsx)
  {
   iMax++;
   if(iMax>=SOFFB-2)
    {
     GX_TEXPATH_INC(bucketFullEvents);
     if(iTexGarbageCollection)                         // gc mode?
      {
       if(*pCache==0)
        {
         dwTexPageComp|=(1<<GlobalTexturePage);
         *pCache=0xffff;
         return 0;
        }

       iMax--;
       tsb=tsg+1;

       for(i=0;i<iMax;i++,tsb++)                       // 1. search other slots with same cluts, and unite the area
        if(clutKey==tsb->ClutKey)
         {
          if(!tsx) {tsx=tsb;rfree.l=npos.l;}           //
          else      tsb->ClutKey=0;
          rfree.c.x1=min(rfree.c.x1,tsb->pos.c.x1);
          rfree.c.x2=max(rfree.c.x2,tsb->pos.c.x2);
          rfree.c.y1=min(rfree.c.y1,tsb->pos.c.y1);
          rfree.c.y2=max(rfree.c.y2,tsb->pos.c.y2);
          MarkFree(tsb);
         }

       if(tsx)                                         // 3. if one or more found, create a new rect with bigger size
        {
         *((unsigned int *)&gl_ux[4])=npos.l=rfree.l;
         rx=(int)rfree.c.x2-(int)rfree.c.x1;
         ry=(int)rfree.c.y2-(int)rfree.c.y1;
         DoTexGarbageCollection();

         goto ENDLOOP3;
        }
      }

     iMax=1;
    }
   tsx=tsg+iMax;
   PUTLE32(SUBCACHE_COUNT_PTR(tsg),iMax);
  }

 //----------------------------------------------------//
 // now get a free texture space
 //----------------------------------------------------//

 if(iTexGarbageCollection) usLRUTexPage=0;

ENDLOOP3:

 rx+=3;if(rx>255) {cXAdj=0;rx=255;}
 ry+=3;if(ry>255) {cYAdj=0;ry=255;}

 semiRequest=DrawSemiTrans ? 1 : 0;
 allowSemiGrowth=semiRequest ? CanGrowSemiBacking() : 0;
 if(semiRequest && !allowSemiGrowth)
  GX_TEXPATH_INC(semiGrowthDenied);
 atlasOrderCount=BuildAtlasPlacementOrder(atlasOrder,semiRequest,
                                          allowSemiGrowth);
 if(!atlasOrderCount)
  {
   /* This can only happen if the safety band was reached before the first
    * semi plane existed.  Let OpenGX reject that optional allocation safely
    * rather than indexing an empty placement list. */
   for(k=0;k<iSortTexCnt;k++)
    atlasOrder[k]=(unsigned char)((usLRUTexPage+k)%iSortTexCnt);
   atlasOrderCount=iSortTexCnt;
  }

 for(k=0;k<atlasOrderCount;k++)
  {
   iC=atlasOrder[k];
   uls=pxSsubtexLeft[iC];
   iMax=GETLE32((unsigned long *)(uls));ul=uls+1;

   //--------------------------------------------------//
   // first time

   if(!iMax)
    {
     rfree.l=0;

     if(rx>252 && ry>252)
      {PUTLE32((unsigned long *)(uls), 1);ul->l=0xffffffff;ul=0;goto ENDLOOP;}

     if(rx<253)
      {
       PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);
       ul->c.x1=rx;
       ul->c.x2=255-rx;
       ul->c.y1=0;
       ul->c.y2=ry;
       ul++;
      }

     if(ry<253)
      {
       PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);
       ul->c.x1=0;
       ul->c.x2=255;
       ul->c.y1=ry;
       ul->c.y2=255-ry;
      }
     ul=0;
     goto ENDLOOP;
    }

   //--------------------------------------------------//
   for(i=0;i<iMax;i++,ul++)
    {
     if(ul->l!=0xffffffff &&
        ry<=ul->c.y2      &&
        rx<=ul->c.x2)
      {
       rfree=*ul;
       mx=ul->c.x2-2;
       my=ul->c.y2-2;
       if(rx<mx && ry<my)
        {
         ul->c.x1+=rx;
         ul->c.x2-=rx;
         ul->c.y2=ry;

         for(ul=uls+1,j=0;j<iMax;j++,ul++)
          if(ul->l==0xffffffff) break;

         if(j<CSUBSIZE-2)
          {
           if(j==iMax) PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);

           ul->c.x1=rfree.c.x1;
           ul->c.x2=rfree.c.x2;
           ul->c.y1=rfree.c.y1+ry;
           ul->c.y2=rfree.c.y2-ry;
          }
        }
       else if(rx<mx)
        {
         ul->c.x1+=rx;
         ul->c.x2-=rx;
        }
       else if(ry<my)
        {
         ul->c.y1+=ry;
         ul->c.y2-=ry;
        }
       else
        {
         ul->l=0xffffffff;
        }
       ul=0;
       goto ENDLOOP;
      }
    }

   //--------------------------------------------------//
  }

 //----------------------------------------------------//
 // check, if free space got
 //----------------------------------------------------//

ENDLOOP:
 if(ul)
  {
   unsigned int evictedEntries;
   int victim;

#ifdef GX_TEXPATH_PROFILE
   ProfileCaptureAtlasClear(rx,ry);
#endif
   victim=SelectTextureAtlasVictim(semiRequest);
   if(victim<0)
    {
     /* Defensive fallback; normal configurations always have at least one
      * atlas.  Preserve the old counter so an impossible configuration is
      * visible in the log instead of silently corrupting memory. */
     GX_TEXPATH_INC(atlasFullClears);
     victim=0;
    }
   evictedEntries=EvictTextureAtlas(victim);
#ifdef GX_TEXPATH_PROFILE
   gxTexturePathProfile.atlasLocalEvictions++;
   gxTexturePathProfile.atlasLocalEvictedEntries+=evictedEntries;
#endif

   iC=victim;
   usLRUTexPage=(unsigned short)victim;
   uls=pxSsubtexLeft[victim];
   uls->l=0;ul=uls+1;
   rfree.l=0;

   if(rx>252 && ry>252)
    {PUTLE32((unsigned long *)(uls), 1);ul->l=0xffffffff;}
   else
    {
     if(rx<253)
      {
       PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);
       ul->c.x1=rx;
       ul->c.x2=255-rx;
       ul->c.y1=0;
       ul->c.y2=ry;
       ul++;
      }
     if(ry<253)
      {
       PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);
       ul->c.x1=0;
       ul->c.x2=255;
       ul->c.y1=ry;
       ul->c.y2=255-ry;
      }
    }
  }

 rfree.c.x1+=cXAdj;
 rfree.c.y1+=cYAdj;

 tsx->cTexID   =*pCache=iC;
 tsx->pos      = npos;
 tsx->ClutKey  = clutKey;
 tsx->posTX    = rfree.c.x1;
 tsx->posTY    = rfree.c.y1;

 cx=gl_ux[7]-rfree.c.x1;
 cy=gl_ux[5]-rfree.c.y1;

 gl_ux[0]-=cx;
 gl_ux[1]-=cx;
 gl_ux[2]-=cx;
 gl_ux[3]-=cx;
 gl_vy[0]-=cy;
 gl_vy[1]-=cy;
 gl_vy[2]-=cy;
 gl_vy[3]-=cy;

 XTexS=rfree.c.x1;
 YTexS=rfree.c.y1;

 return tsx;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// search cache for free place (on compress)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

BOOL GetCompressTexturePlace(textureSubCacheEntryS * tsx)
{
 unsigned char atlasOrder[MAXSORTTEX_MAX];
 int i,j,k,iMax,iC,atlasOrderCount,semiRequest,allowSemiGrowth;
 unsigned int rx,ry,mx,my;
 EXLong * ul=0, * uls, rfree;
 unsigned char cXAdj=1,cYAdj=1;

 rx=(int)tsx->pos.c.x2-(int)tsx->pos.c.x1;
 ry=(int)tsx->pos.c.y2-(int)tsx->pos.c.y1;

 rx+=3;if(rx>255) {cXAdj=0;rx=255;}
 ry+=3;if(ry>255) {cYAdj=0;ry=255;}

 semiRequest=ClutKeyDrawSemiTrans(tsx->ClutKey) ? 1 : 0;
 allowSemiGrowth=semiRequest ? CanGrowSemiBacking() : 0;
 atlasOrderCount=BuildAtlasPlacementOrder(atlasOrder,semiRequest,
                                          allowSemiGrowth);
 if(!atlasOrderCount)
  {
   for(k=0;k<iSortTexCnt;k++)
    atlasOrder[k]=(unsigned char)((usLRUTexPage+k)%iSortTexCnt);
   atlasOrderCount=iSortTexCnt;
  }

 for(k=0;k<atlasOrderCount;k++)
  {
   iC=atlasOrder[k];
   uls=pxSsubtexLeft[iC];
   iMax=GETLE32((unsigned long *)(uls));ul=uls+1;

   //--------------------------------------------------//
   // first time

   if(!iMax)
    {
     rfree.l=0;

     if(rx>252 && ry>252)
      {PUTLE32((unsigned long *)(uls), 1);ul->l=0xffffffff;ul=0;goto TENDLOOP;}

     if(rx<253)
      {
       PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);
       ul->c.x1=rx;
       ul->c.x2=255-rx;
       ul->c.y1=0;
       ul->c.y2=ry;
       ul++;
      }

     if(ry<253)
      {
       PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);
       ul->c.x1=0;
       ul->c.x2=255;
       ul->c.y1=ry;
       ul->c.y2=255-ry;
      }
     ul=0;
     goto TENDLOOP;
    }

   //--------------------------------------------------//
   for(i=0;i<iMax;i++,ul++)
    {
     if(ul->l!=0xffffffff &&
        ry<=ul->c.y2      &&
        rx<=ul->c.x2)
      {
       rfree=*ul;
       mx=ul->c.x2-2;
       my=ul->c.y2-2;

       if(rx<mx && ry<my)
        {
         ul->c.x1+=rx;
         ul->c.x2-=rx;
         ul->c.y2=ry;

         for(ul=uls+1,j=0;j<iMax;j++,ul++)
          if(ul->l==0xffffffff) break;

         if(j<CSUBSIZE-2)
          {
           if(j==iMax) PUTLE32((unsigned long *)(uls), GETLE32((unsigned long *)(uls)) + 1);

           ul->c.x1=rfree.c.x1;
           ul->c.x2=rfree.c.x2;
           ul->c.y1=rfree.c.y1+ry;
           ul->c.y2=rfree.c.y2-ry;
          }
        }
       else if(rx<mx)
        {
         ul->c.x1+=rx;
         ul->c.x2-=rx;
        }
       else if(ry<my)
        {
         ul->c.y1+=ry;
         ul->c.y2-=ry;
        }
       else
        {
         ul->l=0xffffffff;
        }
       ul=0;
       goto TENDLOOP;
      }
    }

   //--------------------------------------------------//
  }

 //----------------------------------------------------//
 // check, if free space got
 //----------------------------------------------------//

TENDLOOP:
 if(ul) return FALSE;

 rfree.c.x1+=cXAdj;
 rfree.c.y1+=cYAdj;

 tsx->cTexID   = iC;
 tsx->posTX    = rfree.c.x1;
 tsx->posTY    = rfree.c.y1;

 XTexS=rfree.c.x1;
 YTexS=rfree.c.y1;

 return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// compress texture cache (to make place for new texture part, if needed)
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void CompressTextureSpace(void)
{
 textureSubCacheEntryS * tsx, * tsg, * tsb;
 int i,j,k,m,n,iMax;EXLong * ul, r,opos;
 short sOldDST=DrawSemiTrans,cx,cy;
 int  lOGTP=GlobalTexturePage;

 GX_TEXPATH_INC(compressCalls);

 opos.l=*((unsigned int *)&gl_ux[4]);

 // 1. mark all textures as free
 for(i=0;i<iSortTexCnt;i++)
  {ul=pxSsubtexLeft[i];ul->l=0;}
 usLRUTexPage=0;

 // 2. compress
 for(j=0;j<3;j++)
  {
   for(k=0;k<MAXTPAGES;k++)
    {
     tsg=pscSubtexStore[j][k];

     if((!(dwTexPageComp&(1<<k))))
      {
       (tsg+SOFFA)->pos.l=0;
       (tsg+SOFFB)->pos.l=0;
       (tsg+SOFFC)->pos.l=0;
       (tsg+SOFFD)->pos.l=0;
       continue;
      }

     for(m=0;m<4;m++,tsg+=SOFFB)
      {
       iMax=GETLE32(SUBCACHE_COUNT_PTR(tsg));

       tsx=tsg+1;
       for(i=0;i<iMax;i++,tsx++)
        {
         if(tsx->ClutKey)
          {
           r.l=tsx->pos.l;
           for(n=i+1,tsb=tsx+1;n<iMax;n++,tsb++)
            {
             if(tsx->ClutKey==tsb->ClutKey)
              {
               r.c.x1=min(r.c.x1,tsb->pos.c.x1);
               r.c.x2=max(r.c.x2,tsb->pos.c.x2);
               r.c.y1=min(r.c.y1,tsb->pos.c.y1);
               r.c.y2=max(r.c.y2,tsb->pos.c.y2);
               tsb->ClutKey=0;
              }
            }

//           if(r.l!=tsx->pos.l)
            {
             cx=(short)((tsx->ClutKey<<4)&0x3F0);
             cy=(short)((tsx->ClutKey>>6)&CLUTYMASK);

             if(j!=2 &&
                BuildClutCacheKey((unsigned int)tsx->ClutKey,j,
                                  ClutKeyDrawSemiTrans(tsx->ClutKey))!=
                tsx->ClutKey)
              {tsx->ClutKey=0;continue;}

             tsx->pos.l=r.l;
             if(!GetCompressTexturePlace(tsx))         // no place?
              {
               GX_TEXPATH_INC(compressFullClears);
               for(i=0;i<3;i++)                        // -> clean up everything
                for(j=0;j<MAXTPAGES;j++)
                 {
                  tsb=pscSubtexStore[i][j];
                  (tsb+SOFFA)->pos.l=0;
                  (tsb+SOFFB)->pos.l=0;
                  (tsb+SOFFC)->pos.l=0;
                  (tsb+SOFFD)->pos.l=0;
                 }
               for(i=0;i<iSortTexCnt;i++)
                {ul=pxSsubtexLeft[i];ul->l=0;}
               usLRUTexPage=0;
               DrawSemiTrans=sOldDST;
               GlobalTexturePage=lOGTP;
               *((unsigned int *)&gl_ux[4])=opos.l;
               dwTexPageComp=0;

               return;
              }

             DrawSemiTrans=ClutKeyDrawSemiTrans(tsx->ClutKey);
             *((unsigned int *)&gl_ux[4])=r.l;

             gTexName=uiStexturePage[tsx->cTexID];
             GX_TEXPATH_INC(compressUploads);
             LoadSubTexFn(k,j,cx,cy);
             uiStexturePage[tsx->cTexID]=gTexName;
             RefreshAtlasSemiBacking(tsx->cTexID);
             tsx->drawInfo=PackDrawInfo(ubOpaqueDraw,gl_ux[8]);
            }
          }
        }

       if(iMax)
        {
         tsx=tsg+iMax;
         while(!tsx->ClutKey && iMax) {tsx--;iMax--;}
         PUTLE32(SUBCACHE_COUNT_PTR(tsg),iMax);
        }

      }
    }
  }

 if(dwTexPageComp==0xffffffff) dwTexPageComp=0;

 *((unsigned int *)&gl_ux[4])=opos.l;
 GlobalTexturePage=lOGTP;
 DrawSemiTrans=sOldDST;
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// main entry for searching/creating textures, called from prim.c
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

GLuint SelectSubTextureS(int TextureMode, unsigned int GivenClutId)
{
 textureSubCacheEntryS *newEntry;
 uint64_t clutKey;
 unsigned short iCache;short cx,cy;

 GX_TEXPATH_INC(subLookups);

 // sort sow/tow infos for fast access

 unsigned char ma1,ma2,mi1,mi2;
 if(gl_ux[0]>gl_ux[1]) {mi1=gl_ux[1];ma1=gl_ux[0];}
 else                  {mi1=gl_ux[0];ma1=gl_ux[1];}
 if(gl_ux[2]>gl_ux[3]) {mi2=gl_ux[3];ma2=gl_ux[2];}
 else                  {mi2=gl_ux[2];ma2=gl_ux[3];}
 if(mi1>mi2) gl_ux[7]=mi2;
 else        gl_ux[7]=mi1;
 if(ma1>ma2) gl_ux[6]=ma1;
 else        gl_ux[6]=ma2;

 if(gl_vy[0]>gl_vy[1]) {mi1=gl_vy[1];ma1=gl_vy[0];}
 else                  {mi1=gl_vy[0];ma1=gl_vy[1];}
 if(gl_vy[2]>gl_vy[3]) {mi2=gl_vy[3];ma2=gl_vy[2];}
 else                  {mi2=gl_vy[2];ma2=gl_vy[3];}
 if(mi1>mi2) gl_ux[5]=mi2;
 else        gl_ux[5]=mi1;
 if(ma1>ma2) gl_ux[4]=ma1;
 else        gl_ux[4]=ma2;

 // get clut infos in one 32 bit val

 if(TextureMode==2)                                    // no clut here
  {
   cx=cy=0;

   if(CaptureFramebufferTexture())
   {
       return (GLuint)gTexName;
   }

   if(iFrameTexType && Fake15BitTexture())
   {

       return (GLuint)gTexName;
   }
  }
 else
  {
   cx=((GivenClutId << 4) & 0x3F0);                    // but here
   cy=((GivenClutId >> 6) & CLUTYMASK);
  }
 clutKey=BuildClutCacheKey(GivenClutId,TextureMode,DrawSemiTrans);

 // search cache
 iCache=0;
 newEntry=CheckTextureInSubSCache(TextureMode,clutKey,&iCache);

 // cache full? compress and try again
 if(iCache==0xffff)
  {
   CompressTextureSpace();
   newEntry=CheckTextureInSubSCache(TextureMode,clutKey,&iCache);
  }

 // found? fine
 usLRUTexPage=iCache;
 #ifdef DISP_DEBUG
// sprintf(txtbuffer, "SelectSubTextureS 1 %d %d\r\n", *OPtr, iCache);
// DEBUG_print(txtbuffer,  DBG_CDR3);
//writeLogFile(txtbuffer);
 #endif // DISP_DEBUG
 if(!newEntry)
  {
   GX_TEXPATH_INC(subHits);
   return uiStexturePage[iCache];
  }

 GX_TEXPATH_INC(subMisses);
#ifdef GX_TEXPATH_PROFILE
 if(gxTextureProfileLastMissReason==GX_TEX_MISS_UV)
  GX_TEXPATH_INC(subMissUv);
 else if(gxTextureProfileLastMissReason==GX_TEX_MISS_CLUT)
  GX_TEXPATH_INC(subMissClut);
 else
  GX_TEXPATH_INC(subMissNoKey);
#endif

  texChgType = 3;
  //GX_Flush();
  //GX_SetDrawDone();

 // not found? upload texture and store infos in cache
 gTexName=uiStexturePage[iCache];
 #ifdef DISP_DEBUG
// sprintf(txtbuffer, "SelectSubTextureS 2 %d %d\r\n", iCache, gTexName);
// DEBUG_print(txtbuffer,  DBG_CDR3);
 //writeLogFile(txtbuffer);
 #endif // DISP_DEBUG
 LoadSubTexFn(GlobalTexturePage,TextureMode,cx,cy);
 uiStexturePage[iCache]=gTexName;
 RefreshAtlasSemiBacking(iCache);
 newEntry->drawInfo=PackDrawInfo(ubOpaqueDraw,gl_ux[8]);
 #ifdef DISP_DEBUG
// sprintf(txtbuffer, "SelectSubTextureS 3 %d\r\n", gTexName);
// DEBUG_print(txtbuffer,  DBG_CDR3);
 //writeLogFile(txtbuffer);
 #endif // DISP_DEBUG
 return (GLuint) gTexName;
}

#endif // _IN_GPU_LIB

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

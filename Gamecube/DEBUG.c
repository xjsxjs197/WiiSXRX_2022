/* DEBUG.c - DEBUG interface
   by Mike Slegeir for Mupen64-GC
 */

#include <gccore.h>
#include <string.h>
#include <stdio.h>
#include <fat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/dir.h>
#include <aesndlib.h>
#include <stdbool.h>
#include "DEBUG.h"
#include "TEXT.h"
#include "../mem2_manager.h"
//#include "usb.h"

char text[DEBUG_TEXT_HEIGHT][DEBUG_TEXT_WIDTH];
char printToSD = 1;
extern u32 dyna_used;
extern u32 dyna_total;

#ifdef SHOW_DEBUG
char txtbuffer[1024];
long long texttimes[DEBUG_TEXT_HEIGHT];
extern u32 dyna_used;
extern u32 dyna_total;
extern long long gettime();
extern unsigned int diff_sec(long long start,long long end);
static void check_heap_space(void){
    sprintf(txtbuffer,"%dKB MEM1 (%dKB / %dKB MEM2)", SYS_GetArena1Size() >> 10, gx_mem2_used() >> 10, gx_mem2_total() >> 10);
    DEBUG_print(txtbuffer, DBG_MEMFREEINFO);

    //sprintf(txtbuffer,"Dynarec (KB) %05d/%05d", dyna_used, dyna_total >> 10);
    //DEBUG_print(txtbuffer, DBG_CORE1);

    //sprintf(txtbuffer,"DSP is at %f%%",AESND_GetDSPProcessUsage());
    //DEBUG_print(txtbuffer,DBG_CORE2);
}
#endif

void DEBUG_update() {
    #ifdef SHOW_DEBUG
    int i;
    long long nowTick = gettime();
    for(i=0; i<DEBUG_TEXT_HEIGHT; i++){
        if(diff_sec(texttimes[i],nowTick)>=DEBUG_STRING_LIFE)
        {
            memset(text[i],0,DEBUG_TEXT_WIDTH);
        }
    }
    check_heap_space();
    #endif
}

int flushed = 0;
int writtenbefore = 0;
int amountwritten = 0;
//char *dump_filename = "dev0:\\PSXISOS\\debug.txt";
char *dump_filename = "/PSXISOS/debug.txt";
FILE* fdebug = NULL;

static FILE* fdebugLog = NULL;
#ifdef VRAM_TILING_DIAG_ONLY
/* This dedicated build exists only to capture the S5 trace, so do not depend
 * on the legacy hidden CDDA-menu toggle to enable its output. */
bool canWriteLog = true;
static const char *debugLogFile;
static const char *const debugLogPaths[] = {
    "sd:/wiisxrx/debugLog.txt",
    "usb:/wiisxrx/debugLog.txt",
    "sd:/debugLog.txt",
    "usb:/debugLog.txt"
};
#else
bool canWriteLog = false;
static const char *debugLogFile = "sd:/wiisxrx/debugLog.txt";
#endif

void openLogFile() {
#ifdef VRAM_TILING_DIAG_ONLY
    unsigned int path;

    /* A settings callback in old Debug builds toggles this flag as a side
     * effect of changing CDDA.  The dedicated trace must stay enabled. */
    canWriteLog = true;
    if (fdebugLog) return;

    if (debugLogFile)
        fdebugLog = fopen(debugLogFile, "a+");
    if (fdebugLog) return;

    debugLogFile = NULL;
    for (path = 0; path < sizeof(debugLogPaths) / sizeof(debugLogPaths[0]);
         path++) {
        fdebugLog = fopen(debugLogPaths[path], "a+");
        if (fdebugLog) {
            debugLogFile = debugLogPaths[path];
            break;
        }
    }
#else
    if (!canWriteLog) return;
    if (!fdebugLog) {
        fdebugLog = fopen(debugLogFile, "a+");
    }
#endif
}

void closeLogFile() {
    if (fdebugLog) {
        fclose(fdebugLog);
        fdebugLog = NULL;
    }
}

void writeLogFile(char* string) {
#ifdef VRAM_TILING_DIAG_ONLY
    canWriteLog = true;
#else
    if (!canWriteLog) return;
#endif

#if defined(EFB_512_HEIGHT_TEST)
    (void)string;
    return;
#elif defined(VRAM_APERTURE_DIAG_ONLY)
    if (!string || strncmp(string, "VAP ", 4) != 0) return;
#elif defined(VRAM_TILING_DIAG_ONLY)
    if (!string || (strncmp(string, "VTL ", 4) != 0 &&
                    strncmp(string, "TDI ", 4) != 0)) return;
#elif defined(TEXTURE_DIAG_ONLY)
    if (!string || strncmp(string, "TDI ", 4) != 0) return;
#endif

    closeLogFile();

    openLogFile();

    if (!fdebugLog) return;
    fputs(string, fdebugLog);

    closeLogFile();
}

void printFunctionName() {
    DEBUG_print(txtbuffer, DBG_CORE2);
    //writeLogFile(txtbuffer);
}

void DEBUG_print(char* string,int pos){

#if defined(TEXTURE_DIAG_ONLY) || defined(VRAM_APERTURE_DIAG_ONLY) || \
    defined(VRAM_TILING_DIAG_ONLY) || defined(EFB_512_HEIGHT_TEST)
    /* File diagnostics remain available through writeLogFile().  Avoid the
     * legacy on-screen Debug queue while measuring this timing-sensitive bug. */
    (void)string;
    (void)pos;
    return;
#endif

    #ifdef SHOW_DEBUG
        if(pos == DBG_USBGECKO) {
            #ifdef PRINTGECKO
            if(!flushed){
                usb_flush(1);
                flushed = 1;
            }
            int size = strlen(string);
            usb_sendbuffer_safe(1, &size,4);
            usb_sendbuffer_safe(1, string,size);
            #endif
        }
        else if(pos == DBG_SDGECKOOPEN) {
#ifdef SDPRINT
            if(!f && printToSD)
                fdebug = fopen( dump_filename, "wb" );
#endif
        }
        else if(pos == DBG_SDGECKOAPPEND) {
#ifdef SDPRINT
            if(!fdebug && printToSD)
                fdebug = fopen( dump_filename, "ab" );
#endif
        }
        else if(pos == DBG_SDGECKOCLOSE) {
#ifdef SDPRINT
            if(fdebug)
            {
                fclose(fdebug);
                fdebug = NULL;
            }
#endif
        }
        else if(pos == DBG_SDGECKOPRINT) {
#ifdef SDPRINT
            if(!f || (printToSD == 0))
                return;
            fwrite(string, 1, strlen(string), f);
#endif
        }
        else {
            memset(text[pos],0,DEBUG_TEXT_WIDTH);
            strncpy(text[pos], string, DEBUG_TEXT_WIDTH);
            memset(text[DEBUG_TEXT_WIDTH-1],0,1);
            texttimes[pos] = gettime();
        }
    #endif

}


#define MAX_STATS 20
unsigned int stats_buffer[MAX_STATS];
unsigned int avge_counter[MAX_STATS];
void DEBUG_stats(int stats_id, char *info, unsigned int stats_type, unsigned int adjustment_value)
{
    #ifdef SHOW_DEBUG
    switch(stats_type)
    {
        case STAT_TYPE_ACCUM:    //accumulate
            stats_buffer[stats_id] += adjustment_value;
            break;
        case STAT_TYPE_AVGE:    //average
            avge_counter[stats_id] += 1;
            stats_buffer[stats_id] += adjustment_value;
            break;
        case STAT_TYPE_CLEAR:
            if(stats_type & STAT_TYPE_AVGE)
                avge_counter[stats_id] = 0;
            stats_buffer[stats_id] = 0;
            break;

    }
    unsigned int value = stats_buffer[stats_id];
    if(stats_type == STAT_TYPE_AVGE) value /= avge_counter[stats_id];

    sprintf(txtbuffer,"%s [ %u ]", info, value);
    DEBUG_print(txtbuffer,DBG_STATSBASE+stats_id);
    #endif
}

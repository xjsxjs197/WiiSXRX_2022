#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <fat.h>

#include "../MessageBox.h"
#include "../../wiiSXconfig.h"
#include "../../DEBUG.h"
#include "../../../psxcommon.h"

char biosFileName[16];
static char tstLine[150];

static bool isBiosExists(char * biosPath, char *biosName)
{
    sprintf(tstLine, "%s/%s", biosPath, biosName);
    //writeLogFile(tstLine);
    FILE* f = fopen(tstLine, "rb" );  //attempt to open file
    if (f) {
        fclose(f);
        return true;
    }
    else
    {
        return false;
    }
}

char * GetGameBios(char * biosPath, char * fileName, int isoFileNameLen)
{
    // Assuming that SCPH1001.BIN must exist.
    char * retVal = "/SCPH1001.BIN";
    sprintf(biosFileName, "SCPH1001.BIN");
    memset(tstLine, 0, 150);

    char * tmpPtr = tstLine;
    memcpy(tstLine, biosPath, strlen(biosPath));
    tmpPtr += strlen(biosPath);
    *tmpPtr = '//';
    *(tmpPtr + 1) = '\0';

    // Find Bios with a name equal to the file name
    strncat(tstLine, fileName, strlen(fileName) - 4);
    strcat(tstLine, ".bin");

    FILE* f = fopen(tstLine, "rb" );  //attempt to open file
    if (f) {

        fclose(f);

        memset(tstLine, 0, strlen(tstLine));
        *tstLine = '/';
        *(tstLine + 1) = '\0';
        strncat(tstLine, fileName, strlen(fileName) - 4);
        strcat(tstLine, ".bin");

        // Return the Bios with a name equal to the file name
        return tstLine;
    }
    else
    {
        memset(tstLine, 0, 150);
        // Return different versions of Bios based on settings
        if (Config.PsxType == PSX_TYPE_PAL)
        {
            if (isBiosExists(biosPath, "SCPH1002.BIN"))
            {
                sprintf(biosFileName, "SCPH1002.BIN");
                return "/SCPH1002.BIN";
            }
            else if (isBiosExists(biosPath, "SCPH5502.BIN"))
            {
                sprintf(biosFileName, "SCPH5502.BIN");
                return "/SCPH5502.BIN";
            }
        }
        else
        {
            if (isBiosExists(biosPath, "SCPH1001.BIN"))
            {
                sprintf(biosFileName, "SCPH1001.BIN");
                return "/SCPH1001.BIN";
            }
            else if (isBiosExists(biosPath, "SCPH5501.BIN"))
            {
                sprintf(biosFileName, "SCPH5501.BIN");
                return "/SCPH5501.BIN";
            }
            else if (isBiosExists(biosPath, "SCPH1000.BIN"))
            {
                sprintf(biosFileName, "SCPH1000.BIN");
                return "/SCPH1000.BIN";
            }
            else if (isBiosExists(biosPath, "SCPH5500.BIN"))
            {
                sprintf(biosFileName, "SCPH5500.BIN");
                return "/SCPH5500.BIN";
            }
        }
        return retVal;
    }
}

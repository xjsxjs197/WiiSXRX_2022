#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../GlesGpu/gpuClutKey.h"

static int failures;

typedef union
{
    uint32_t value;
    unsigned char byte[4];
} TestPos;

typedef struct
{
    uint64_t clutKey;
    TestPos pos;
    unsigned char posTX;
    unsigned char posTY;
    unsigned char textureId;
    unsigned char drawInfo;
} TestSubCacheEntry;

static unsigned int legacy_palette_checksum14(const uint16_t *palette,
                                               int textureMode)
{
    unsigned int checksum=0;
    int pairs=textureMode==1 ? 128 : 8;
    int pair;

    for(pair=1;pair<=pairs;pair++)
    {
        uint32_t value=(uint32_t)palette[2*(pair-1)] |
                       ((uint32_t)palette[2*(pair-1)+1]<<16);
        if(textureMode==1)
            checksum+=(value-1U)*(unsigned int)pair;
        else
            checksum+=(value-1U)<<pair;
    }
    return (checksum+(checksum>>16))&0x3fffU;
}

static void expect(int condition,const char *message)
{
    if(!condition)
    {
        printf("FAIL %s\n",message);
        failures++;
    }
}

int main(void)
{
    uint16_t palette4a[16]={0};
    uint16_t palette4b[16]={0};
    uint16_t palette8a[256]={0};
    uint16_t palette8b[256]={0};
    uint64_t keyA,keyB;

    palette4b[15]=64;
    expect(legacy_palette_checksum14(palette4a,0)==
           legacy_palette_checksum14(palette4b,0),
           "known 4-bit legacy checksum collision");
    keyA=ClutKeyBuild(0x1234,0,0,palette4a);
    keyB=ClutKeyBuild(0x1234,0,0,palette4b);
    expect(keyA!=keyB,"4-bit collision gets distinct 64-bit keys");

    palette8b[255]=128;
    expect(legacy_palette_checksum14(palette8a,1)==
           legacy_palette_checksum14(palette8b,1),
           "known 8-bit legacy checksum collision");
    keyA=ClutKeyBuild(0x2345,1,0,palette8a);
    keyB=ClutKeyBuild(0x2345,1,0,palette8b);
    expect(keyA!=keyB,"8-bit collision gets distinct 64-bit keys");

    keyA=ClutKeyBuild(0x1234,0,0,palette4a);
    keyB=ClutKeyBuild(0x1234,0,0,palette4a);
    expect(keyA==keyB,"same palette keeps the same key");
    expect(ClutKeyBuild(0x1234,0,1,palette4a)!=keyA,
           "semi-transparency is part of the key");
    expect(ClutKeyBuild(0x1235,0,0,palette4a)!=keyA,
           "CLUT address is part of the key");
    expect(sizeof(TestSubCacheEntry)==16,
           "standard subtexture cache entry remains 16 bytes");

    if(failures==0)
        printf("PASS clut_key\n");
    return failures!=0;
}

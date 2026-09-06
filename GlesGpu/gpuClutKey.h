#ifndef GPU_CLUT_KEY_H
#define GPU_CLUT_KEY_H

#include <stdint.h>

#define CLUT_KEY_ADDRESS_MASK UINT64_C(0x7fff)
#define CLUT_KEY_VALID        (UINT64_C(1) << 15)
#define CLUT_KEY_SEMITRANS    (UINT64_C(1) << 16)
#define CLUT_KEY_HASH_SHIFT   17

static inline uint32_t ClutKeyRotateLeft32(uint32_t value,unsigned int shift)
{
    return (value<<shift)|(value>>(32-shift));
}

/* Two independent 32-bit accumulators are cheaper than 64-bit multiply on
 * Broadway.  Their 32+15 output bits form the 47-bit palette fingerprint. */
static inline uint64_t ClutPaletteFingerprint47(const void *paletteBytes,
                                                int textureMode)
{
    const uint8_t *palette=(const uint8_t *)paletteBytes;
    uint32_t hashA=UINT32_C(2166136261);
    uint32_t hashB=UINT32_C(0x9e3779b9);
    unsigned int pairs=textureMode==1 ? 128U : 8U;
    unsigned int i;

    for(i=0;i<pairs;i++,palette+=4)
    {
        uint32_t value=(uint32_t)palette[0] |
                       ((uint32_t)palette[1]<<8) |
                       ((uint32_t)palette[2]<<16) |
                       ((uint32_t)palette[3]<<24);

        hashA=(hashA^value)*UINT32_C(16777619);
        hashB+=value+UINT32_C(0x9e3779b9)+(hashB<<6)+(hashB>>2);
        hashB^=ClutKeyRotateLeft32(value,(i&15U)+1U);
    }

    hashA^=hashA>>16;
    hashA*=UINT32_C(0x7feb352d);
    hashA^=hashA>>15;
    hashB^=hashB>>16;
    hashB*=UINT32_C(0x846ca68b);
    hashB^=hashB>>16;
    return ((uint64_t)hashA<<15)|(hashB&UINT32_C(0x7fff));
}

static inline uint64_t ClutKeyBuild(unsigned int rawClutId,int textureMode,
                                    int drawSemiTrans,
                                    const void *paletteBytes)
{
    uint64_t key=CLUT_KEY_VALID;

    if(drawSemiTrans)
        key|=CLUT_KEY_SEMITRANS;
    if(textureMode!=2)
    {
        key|=(uint64_t)(rawClutId&0x7fffU);
        key|=ClutPaletteFingerprint47(paletteBytes,textureMode)<<
             CLUT_KEY_HASH_SHIFT;
    }
    return key;
}

static inline int ClutKeyDrawSemiTrans(uint64_t key)
{
    return (key&CLUT_KEY_SEMITRANS)!=0;
}

static inline unsigned int ClutKeyBucket(uint64_t key)
{
    return (unsigned int)((key>>CLUT_KEY_HASH_SHIFT)&3U);
}

#endif /* GPU_CLUT_KEY_H */

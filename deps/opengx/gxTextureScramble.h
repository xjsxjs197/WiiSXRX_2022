/***************************************************************************
                          gxTextureScramble.h
                             -------------------
    Pure PS1 texel -> GX RGB5A3 conversion helpers shared by OpenGX and
    host-side tests.  The semi-transparent plane is written into the caller's
    correctly-sized target, never into a fixed-size global scratch buffer.
 ***************************************************************************/

#ifndef GX_TEXTURE_SCRAMBLE_H
#define GX_TEXTURE_SCRAMBLE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TEX_TYPE_1
#define TEX_TYPE_1 0x1
#endif
#ifndef TEX_TYPE_2
#define TEX_TYPE_2 0x2
#endif
#ifndef W_BLOCK
#define W_BLOCK(w) (((w + 3) & ~(unsigned int)3) >> 2)
#endif

/*
 * Byte size of one RGB5A3 plane in GX's 4x4-block tiled layout.  Width and
 * height are rounded up to the next 4-texel boundary; each block holds 32
 * bytes.  Both the primary plane and the semi-transparent plane use this
 * size.
 */
static inline unsigned int GxRgb5a3TiledSize(unsigned int width,
                                             unsigned int height)
{
    return W_BLOCK(width) * W_BLOCK(height) * 32u;
}

/*
 * Scan the 4-byte-per-texel source for any semi-transparent texel using the
 * same classification as GxScramble4b5a3Full()/GxScramble4b5a3Sub().
 */
static inline int GxScramble4b5a3HasSemi(const unsigned char *src,
                                         unsigned int width,
                                         unsigned int height,
                                         unsigned short semiTransFlg)
{
    unsigned int y, x;
    unsigned short tmpPixel;

    if (!semiTransFlg)
        return 0;

    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
        {
            tmpPixel = *(const unsigned short *)(src + (x + y * width) * 4 + 2);
            if (tmpPixel != 0 && (tmpPixel & 0x8000) == 0)
                return 1;
        }

    return 0;
}

/*
 * Full-image scramble.  dstPrimary receives the RGB5A3 texture plane.
 * dstSemi, when non-NULL, receives the semi-transparent plane sized
 * GxRgb5a3TiledSize(width, height); when NULL, all semi-plane writes are
 * skipped.
 */
static inline int GxScramble4b5a3Full(unsigned char *src, void *dstPrimary,
                                      void *dstSemi, unsigned short semiTransFlg,
                                      unsigned int width, unsigned int height)
{
    unsigned int block;
    unsigned int i;
    unsigned char c;
    unsigned char argb;
    unsigned char *p = (unsigned char *)dstPrimary;
    unsigned char *semiTransP = (unsigned char *)dstSemi;
    unsigned short tmpPixel;
    int textureType = 0;

    for (block = 0; block < height; block += 4) {
        for (i = 0; i < width; i += 4) {
            for (c = 0; c < 4; c++) {
                for (argb = 0; argb < 4; argb++) {
                    if ((i + argb) >= width || (block + c) >= height)
                    {
                        *(unsigned short *)p = 0;
                        if (semiTransP)
                            *(unsigned short *)semiTransP = 0;
                    }
                    else
                    {
                        tmpPixel = *(unsigned short *)(src + ((i + argb) + ((block + c) * width)) * 4 + 2);
                        if (tmpPixel == 0)
                        {
                            if (semiTransP)
                                *(unsigned short *)semiTransP = 0;
                            *(unsigned short *)p = 0;
                        }
                        else if (semiTransFlg && (tmpPixel & 0x8000) == 0)
                        {
                            if (semiTransP)
                                *(unsigned short *)semiTransP = tmpPixel | 0x8000;
                            *(unsigned short *)p = 0;
                            textureType |= TEX_TYPE_1;
                        }
                        else
                        {
                            if (semiTransP)
                                *(unsigned short *)semiTransP = 0;
                            *(unsigned short *)p = tmpPixel | 0x8000;
                            textureType |= TEX_TYPE_2;
                        }
                    }
                    p += 2;
                    if (semiTransP)
                        semiTransP += 2;
                }
            }
        }
    }

    return textureType;
}

/*
 * Sub-image scramble.  dstPrimary/dstSemi point at the first block of the
 * destination texture; oldWidth is the full destination texture width.
 */
static inline int GxScramble4b5a3Sub(unsigned char *src, void *dstPrimary,
                                     void *dstSemi, unsigned short semiTransFlg,
                                     unsigned int width, unsigned int height,
                                     unsigned int oldWidth)
{
    unsigned int he;
    unsigned int wi;
    unsigned char blockHe;
    unsigned char blockWi;
    unsigned char *p = (unsigned char *)dstPrimary;
    unsigned char *semiTransP = (unsigned char *)dstSemi;
    unsigned short tmpPixel;
    int oldWidthBlock = W_BLOCK(oldWidth);
    int newWidthBlock = W_BLOCK(width);
    int textureType = 0;

    for (he = 0; he < height; he += 4) {
        for (wi = 0; wi < width; wi += 4) {
            for (blockHe = 0; blockHe < 4; blockHe++) {
                for (blockWi = 0; blockWi < 4; blockWi++) {
                    if ((wi + blockWi) >= width || (he + blockHe) >= height)
                    {
                        /* padding texels are left untouched */
                    }
                    else
                    {
                        tmpPixel = *(unsigned short *)(src + ((wi + blockWi) + ((he + blockHe) * width)) * 4 + 2);
                        if (tmpPixel == 0)
                        {
                            if (semiTransP)
                                *(unsigned short *)semiTransP = 0;
                            *(unsigned short *)p = 0;
                        }
                        else if (semiTransFlg && (tmpPixel & 0x8000) == 0)
                        {
                            if (semiTransP)
                                *(unsigned short *)semiTransP = tmpPixel | 0x8000;
                            *(unsigned short *)p = 0;
                            textureType |= TEX_TYPE_1;
                        }
                        else
                        {
                            if (semiTransP)
                                *(unsigned short *)semiTransP = 0;
                            *(unsigned short *)p = tmpPixel | 0x8000;
                            textureType |= TEX_TYPE_2;
                        }
                    }
                    p += 2;
                    if (semiTransP)
                        semiTransP += 2;
                }
            }
        }
        p += (oldWidthBlock - newWidthBlock) * 32;
        if (semiTransP)
            semiTransP += (oldWidthBlock - newWidthBlock) * 32;
    }

    return textureType;
}

#ifdef __cplusplus
}
#endif

#endif /* GX_TEXTURE_SCRAMBLE_H */

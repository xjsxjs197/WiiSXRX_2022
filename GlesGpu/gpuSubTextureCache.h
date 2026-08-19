#ifndef GPU_SUB_TEXTURE_CACHE_H
#define GPU_SUB_TEXTURE_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gpuVramRect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SubTextureInvalidateFn)(void *user, void *entry);

/*
 * Production 14-bit palette checksum.  mode 0 reads 16 words, mode 1 reads
 * 256 words.  The two-word read is expressed explicitly so host and Wii use
 * the same value.
 */
static inline unsigned int SubTexturePaletteChecksum14(
    const uint16_t *palette, int textureMode)
{
    unsigned int l = 0;
    int words = (textureMode == 1) ? 128 : 8;
    int row;

    for (row = 1; row <= words; row++)
    {
        uint32_t v = (uint32_t)palette[2 * (row - 1)] |
                     ((uint32_t)palette[2 * (row - 1) + 1] << 16);
        if (textureMode == 1)
            l += (v - 1) * (unsigned int)row;
        else
            l += (v - 1) << row;
    }

    return (l + (l >> 16)) & 0x3fffU;
}

/*
 * Entry-level palette invalidation shared by production and tests.  Entries
 * are addressed as opaque memory so the caller keeps the production struct
 * type; ClutID is read/cleared through memcpy to avoid strict-aliasing UB.
 */
static inline int SubTexturePaletteInvalidateEntries(
    void *entries, int count, size_t stride, int textureMode,
    unsigned int clutMask, int clutYMask, int vramWidth, int vramHeight,
    const VramRect *rects, int rectCount,
    SubTextureInvalidateFn fn, void *user)
{
    unsigned char *base = (unsigned char *)entries;
    int invalidated = 0;
    int i, r;

    if (entries == NULL || rects == NULL || rectCount <= 0 ||
        count <= 0 || stride < sizeof(unsigned int))
        return 0;

    for (i = 0; i < count; i++)
    {
        unsigned char *entry = base + (size_t)i * stride;
        unsigned int clutId;
        unsigned int zero = 0;

        memcpy(&clutId, entry, sizeof(clutId));
        if (!clutId)
            continue;

        for (r = 0; r < rectCount; r++)
        {
            if (StandardSubTexturePaletteDependsOnRect(
                    clutId, textureMode, clutMask, clutYMask,
                    vramWidth, vramHeight, &rects[r]))
            {
                memcpy(entry, &zero, sizeof(zero));
                if (fn != NULL)
                    fn(user, entry);
                invalidated++;
                break;
            }
        }
    }

    return invalidated;
}

#ifdef __cplusplus
}
#endif

#endif /* GPU_SUB_TEXTURE_CACHE_H */

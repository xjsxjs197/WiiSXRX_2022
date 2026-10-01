#ifndef GPU_VRAM_COMMAND_TRANSFER_H
#define GPU_VRAM_COMMAND_TRANSFER_H

#include <stdint.h>

uint16_t GlesVramCommandReadPixel(const uint16_t *vram, int x, int y);

int GlesVramCommandReadTransferWord(const uint16_t *vram,
                                    int x, int y,
                                    int width, int height,
                                    int pixelOffset,
                                    uint32_t *value);

int GlesVramCommandWritePixel(uint16_t *vram, int x, int y,
                              uint16_t value, int setMask, int checkMask);

int GlesVramCommandFill(uint16_t *vram, int x, int y,
                        int width, int height, uint16_t value);

int GlesVramCommandCopy(uint16_t *vram,
                        int sourceX, int sourceY,
                        int destinationX, int destinationY,
                        int width, int height,
                        int setMask, int checkMask);

#endif

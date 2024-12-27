/*****************************************************************************
Copyright (c) 2011  David Guillen Fandos (david@davidgf.net)
Copyright (c) 2024  Alberto Mardegan (mardy@users.sourceforge.net)
All rights reserved.

Attention! Contains pieces of code from others such as Mesa and GRRLib

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of copyright holders nor the names of its
   contributors may be used to endorse or promote products derived
   from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL COPYRIGHT HOLDERS OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include "pixels.h"

#include "debug.h"
#include <math.h>
#include <string.h>

#define FLOAT_TO_BYTE(f) ((GLbyte)(f * 255.0) & 0xff)

void _ogx_swap_rgba(unsigned char *pixels, int num_pixels)
{
    while (num_pixels--) {
        unsigned char temp;
        temp = pixels[0];
        pixels[0] = pixels[3];
        pixels[3] = temp;
        pixels += 4;
    }
}

void _ogx_swap_rgb565(unsigned short *pixels, int num_pixels)
{
    while (num_pixels--) {
        unsigned int b = *pixels & 0x1F;
        unsigned int r = (*pixels >> 11) & 0x1F;
        unsigned int g = (*pixels >> 5) & 0x3F;
        *pixels++ = (b << 11) | (g << 5) | r;
    }
}

static void conv_rgba32_to_rgb565(const int8_t *src, void *dst, int numpixels)
{
    unsigned short *out = dst;
    while (numpixels--) {
        *out++ = ((src[0] & 0xF8) << 8) | ((src[1] & 0xFC) << 3) | ((src[2] >> 3));
        src += 4;
    }
}

static void conv_rgbaf_to_rgb565(const float *src, void *dst, int numpixels)
{
    unsigned short *out = dst;
    while (numpixels--) {
        *out++ = ((FLOAT_TO_BYTE(src[0]) & 0xF8) << 8) |
            ((FLOAT_TO_BYTE(src[1]) & 0xFC) << 3) |
            ((FLOAT_TO_BYTE(src[2]) >> 3));
        src += 4;
    }
}

// Discards alpha and fits the texture in 16 bits
void _ogx_conv_rgba_to_rgb565(const void *data, GLenum type,
                              void *dst, int width, int height)
{
    int numpixels = width * height;
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        conv_rgba32_to_rgb565(data, dst, numpixels);
        break;
    case GL_FLOAT:
        conv_rgbaf_to_rgb565(data, dst, numpixels);
        break;
    default:
        warning("Unsupported texture format %d", type);
    }
}

static void conv_rgb24_to_rgb565(const uint8_t *src, void *dst, int numpixels)
{
    unsigned short *out = dst;
    while (numpixels--) {
        *out++ = ((src[0] & 0xF8) << 8) | ((src[1] & 0xFC) << 3) | ((src[2] >> 3));
        src += 3;
    }
}

static void conv_rgbf_to_rgb565(const float *src, void *dst, int numpixels)
{
    unsigned short *out = dst;
    while (numpixels--) {
        *out++ = ((FLOAT_TO_BYTE(src[0]) & 0xF8) << 8) |
            ((FLOAT_TO_BYTE(src[1]) & 0xFC) << 3) |
            ((FLOAT_TO_BYTE(src[2]) >> 3));
        src += 3;
    }
}

// Fits the texture in 16 bits
void _ogx_conv_rgb_to_rgb565(const void *data, GLenum type,
                             void *dst, int width, int height)
{
    int numpixels = width * height;
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        conv_rgb24_to_rgb565(data, dst, numpixels);
        break;
    case GL_FLOAT:
        conv_rgbf_to_rgb565(data, dst, numpixels);
        break;
    default:
        warning("Unsupported texture format %d", type);
    }
}

static void conv_rgbaf_to_rgba32(const float *src, void *dst, int numpixels)
{
    uint32_t *out = dst;
    while (numpixels--) {
        *out++ = (FLOAT_TO_BYTE(src[0]) << 24) |
            (FLOAT_TO_BYTE(src[1]) << 16) |
            (FLOAT_TO_BYTE(src[2]) << 8) |
            FLOAT_TO_BYTE(src[3]);
        src += 4;
    }
}

void _ogx_conv_rgba_to_rgba32(const void *data, GLenum type,
                              void *dest, int width, int height)
{
    int numpixels = width * height;
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        memcpy(dest, data, numpixels * 4);
        break;
    case GL_FLOAT:
        conv_rgbaf_to_rgba32(data, dest, numpixels);
        break;
    default:
        warning("Unsupported texture format %d", type);
    }
}

static void conv_laf_to_ia8(const float *src, void *dst, int numpixels)
{
    uint16_t *out = dst;
    while (numpixels--) {
        *out++ = (FLOAT_TO_BYTE(src[1]) << 8) | FLOAT_TO_BYTE(src[0]);
        src += 2;
    }
}

void _ogx_conv_luminance_alpha_to_ia8(const void *data, GLenum type,
                                      void *dest, int width, int height)
{
    int numpixels = width * height;
    switch (type) {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:
        memcpy(dest, data, numpixels * 2);
        break;
    case GL_FLOAT:
        conv_laf_to_ia8(data, dest, numpixels);
        break;
    default:
        warning("Unsupported texture format %d", type);
    }
}

// Converts color into luminance and saves alpha
void _ogx_conv_rgba_to_luminance_alpha(unsigned char *src, void *dst,
                                       const unsigned int width, const unsigned int height)
{
    int numpixels = width * height;
    unsigned char *out = dst;
    while (numpixels--) {
        int lum = ((int)src[0]) + ((int)src[1]) + ((int)src[2]);
        lum = lum / 3;
        *out++ = src[3];
        *out++ = lum;
        src += 4;
    }
}

// 4x4 tile scrambling
// 2b texel scrambling
void _ogx_scramble_2b(unsigned short *src, void *dst,
                      const unsigned int width, const unsigned int height)
{
    unsigned int block;
    unsigned int i;
    unsigned char c;
    unsigned short *p = (unsigned short *)dst;

    for (block = 0; block < height; block += 4) {
        for (i = 0; i < width; i += 4) {
            for (c = 0; c < 4; c++) {
                *p++ = src[(block + c) * width + i];
                *p++ = src[(block + c) * width + i + 1];
                *p++ = src[(block + c) * width + i + 2];
                *p++ = src[(block + c) * width + i + 3];
            }
        }
    }
}

// The position happens to be the integer position of the Block
void _ogx_scramble_4b_sub(unsigned char *src, void *dst,
                      const unsigned int width, const unsigned int height, const unsigned int oldWidth)
{
    unsigned int he;
    unsigned int wi;
    unsigned char blockHe;
    unsigned char blockWi;
    unsigned char *p = (unsigned char *)dst;
    int oldWidthBlock = W_BLOCK(oldWidth);
    int newWidthBlock = W_BLOCK(width);

    for (he = 0; he < height; he += 4) {
        for (wi = 0; wi < width; wi += 4) {
            for (blockHe = 0; blockHe < 4; blockHe++) {
                for (blockWi = 0; blockWi < 4; blockWi++) {
                    if ((wi + blockWi) >= width || (he + blockHe) >= height)
                    {
                        //*(unsigned short*)p = 0;
                    }
                    else
                    {
                        // RGB5A3(Actually, it's the original BGR555 of PSX, Can be efficiently converted to Wii RGB5A3)
                        *(unsigned short*)(p) = *(unsigned short*)(src + ((wi + blockWi) + ((he + blockHe) * width)) * 4 + 2);
                    }
                    p += 2;
                }
            }
        }
        p += (oldWidthBlock - newWidthBlock) * 32;
    }
}

// 4b texel scrambling, opengx conversion: src(argb) -> dst(ar...gb)
// for movie
void _ogx_scramble_4b(unsigned char *src, void *dst,
                      const unsigned int width, const unsigned int height)
{
    unsigned int block;
    unsigned int i;
    unsigned char c;
    unsigned char argb;
    unsigned char *p = (unsigned char *)dst;

    for (block = 0; block < height; block += 4) {
        for (i = 0; i < width; i += 4) {
            for (c = 0; c < 4; c++) {
                for (argb = 0; argb < 4; argb++) {
                    if ((i + argb) >= width || (block + c) >= height)
                    {
                        *(unsigned short*)p = 0;
                        *(unsigned short*)(p + 32) = 0;
                    }
                    else
                    {
                        *(unsigned short*)p = *(unsigned short*)(src + ((i + argb) + ((block + c) * width)) * 4);             // AR
                        *(unsigned short*)(p + 32) = *(unsigned short*)(src + ((i + argb) + ((block + c) * width)) * 4 + 2);  // GB
                    }
                    p += 2;
                }
            }
            p += 32;
        }
    }
}

// 4b texel scrambling, opengx conversion: src(4 bytes bgr555) -> dst(2 bytes bgr5a3)
void _ogx_scramble_4b_5a3(unsigned char *src, void *dst,
                      const unsigned int width, const unsigned int height)
{
    unsigned int block;
    unsigned int i;
    unsigned char c;
    unsigned char argb;
    unsigned char *p = (unsigned char *)dst;

    for (block = 0; block < height; block += 4) {
        for (i = 0; i < width; i += 4) {
            for (c = 0; c < 4; c++) {
                for (argb = 0; argb < 4; argb++) {
                    if ((i + argb) >= width || (block + c) >= height)
                    {
                        *(unsigned short*)p = 0;
                    }
                    else
                    {
                        *(unsigned short*)(p) = *(unsigned short*)(src + ((i + argb) + ((block + c) * width)) * 4 + 2);
                    }
                    p += 2;
                }
            }
        }
    }
}

//// Image scaling for arbitrary size taken from Mesa 3D and adapted by davidgf ////

void _ogx_scale_internal(int components, int widthin, int heightin,
                         const unsigned char *datain,
                         int widthout, int heightout, unsigned char *dataout)
{
    float x, lowx, highx, convx, halfconvx;
    float y, lowy, highy, convy, halfconvy;
    float xpercent, ypercent;
    float percent;
    /* Max components in a format is 4, so... */
    float totals[4];
    float area;
    int i, j, k, yint, xint, xindex, yindex;
    int temp;

    convy = (float)heightin / heightout;
    convx = (float)widthin / widthout;
    halfconvx = convx / 2;
    halfconvy = convy / 2;
    for (i = 0; i < heightout; i++) {
        y = convy * (i + 0.5);
        if (heightin > heightout) {
            highy = y + halfconvy;
            lowy = y - halfconvy;
        } else {
            highy = y + 0.5;
            lowy = y - 0.5;
        }
        for (j = 0; j < widthout; j++) {
            x = convx * (j + 0.5);
            if (widthin > widthout) {
                highx = x + halfconvx;
                lowx = x - halfconvx;
            } else {
                highx = x + 0.5;
                lowx = x - 0.5;
            }

            /*
            ** Ok, now apply box filter to box that goes from (lowx, lowy)
            ** to (highx, highy) on input data into this pixel on output
            ** data.
            */
            totals[0] = totals[1] = totals[2] = totals[3] = 0.0;
            area = 0.0;

            y = lowy;
            yint = floor(y);
            while (y < highy) {
                yindex = (yint + heightin) % heightin;
                if (highy < yint + 1) {
                    ypercent = highy - y;
                } else {
                    ypercent = yint + 1 - y;
                }

                x = lowx;
                xint = floor(x);

                while (x < highx) {
                    xindex = (xint + widthin) % widthin;
                    if (highx < xint + 1) {
                        xpercent = highx - x;
                    } else {
                        xpercent = xint + 1 - x;
                    }

                    percent = xpercent * ypercent;
                    area += percent;
                    temp = (xindex + (yindex * widthin)) * components;
                    for (k = 0; k < components; k++) {
                        totals[k] += datain[temp + k] * percent;
                    }

                    xint++;
                    x = xint;
                }
                yint++;
                y = yint;
            }

            temp = (j + (i * widthout)) * components;
            for (k = 0; k < components; k++) {
                /* totals[] should be rounded in the case of enlarging an RGB
                 * ramp when the type is 332 or 4444
                 */
                dataout[temp + k] = (totals[k] + 0.5) / area;
            }
        }
    }
}

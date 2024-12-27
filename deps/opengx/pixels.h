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

#ifndef OPENGX_PIXELS_H
#define OPENGX_PIXELS_H

#include "GL/gl.h"

#define W_BLOCK(w) (((w + 3) & ~(unsigned int)3) >> 2)

void _ogx_swap_rgba(unsigned char *pixels, int num_pixels);
void _ogx_swap_rgb565(unsigned short *pixels, int num_pixels);
void _ogx_conv_rgba_to_rgb565(const void *data, GLenum type,
                              void *dest, int width, int height);
void _ogx_conv_rgb_to_rgb565(const void *data, GLenum type,
                             void *dest, int width, int height);
void _ogx_conv_rgba_to_rgba32(const void *data, GLenum type,
                              void *dest, int width, int height);
void _ogx_conv_luminance_alpha_to_ia8(const void *data, GLenum type,
                                      void *dest, int width, int height);
void _ogx_conv_rgba_to_luminance_alpha(unsigned char *src, void *dst,
                                       const unsigned int width, const unsigned int height);
void _ogx_scramble_2b(unsigned short *src, void *dst,
                      const unsigned int width, const unsigned int height);
void _ogx_scramble_4b(unsigned char *src, void *dst,
                      const unsigned int width, const unsigned int height);
void _ogx_scramble_4b_5a3(unsigned char *src, void *dst,
                      const unsigned int width, const unsigned int height);

// The position happens to be the integer position of the Block
void _ogx_scramble_4b_sub(unsigned char *src, void *dst,
                      const unsigned int width, const unsigned int height, const unsigned int oldWidth);

void _ogx_scale_internal(int components, int widthin, int heightin,
                         const unsigned char *datain,
                         int widthout, int heightout, unsigned char *dataout);

#endif /* OPENGX_PIXELS_H */

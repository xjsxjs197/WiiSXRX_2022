/*****************************************************************************
Copyright (c) 2011  David Guillen Fandos (david@davidgf.net)
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

#ifndef OGX_UTILS_H
#define OGX_UTILS_H

#include "state.h"

#include <gctypes.h>
#include <math.h>
#include <string.h>

static inline float clampf_01(float n)
{
    if (n > 1.0f)
        return 1.0f;
    else if (n < 0.0f)
        return 0.0f;
    else
        return n;
}

static inline float clampf_11(float n)
{
    if (n > 1.0f)
        return 1.0f;
    else if (n < -1.0f)
        return -1.0f;
    else
        return n;
}

static inline void floatcpy(float *dest, const float *src, size_t count)
{
    memcpy(dest, src, count * sizeof(float));
}

static inline void normalize(GLfloat v[3])
{
    GLfloat r;
    r = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (r == 0.0f)
        return;

    v[0] /= r;
    v[1] /= r;
    v[2] /= r;
}

static inline void cross(GLfloat v1[3], GLfloat v2[3], GLfloat result[3])
{
    result[0] = v1[1] * v2[2] - v1[2] * v2[1];
    result[1] = v1[2] * v2[0] - v1[0] * v2[2];
    result[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

// We have to reverse the product, as we store the matrices as colmajor (otherwise said trasposed)
// we need to compute C = A*B (as rowmajor, "regular") then C = B^T * A^T = (A*B)^T
// so we compute the product and transpose the result (for storage) in one go.
//                                        Reversed operands a and b
static inline void gl_matrix_multiply(float *dst, float *b, float *a)
{
    dst[0] = a[0] * b[0] + a[1] * b[4] + a[2] * b[8] + a[3] * b[12];
    dst[1] = a[0] * b[1] + a[1] * b[5] + a[2] * b[9] + a[3] * b[13];
    dst[2] = a[0] * b[2] + a[1] * b[6] + a[2] * b[10] + a[3] * b[14];
    dst[3] = a[0] * b[3] + a[1] * b[7] + a[2] * b[11] + a[3] * b[15];
    dst[4] = a[4] * b[0] + a[5] * b[4] + a[6] * b[8] + a[7] * b[12];
    dst[5] = a[4] * b[1] + a[5] * b[5] + a[6] * b[9] + a[7] * b[13];
    dst[6] = a[4] * b[2] + a[5] * b[6] + a[6] * b[10] + a[7] * b[14];
    dst[7] = a[4] * b[3] + a[5] * b[7] + a[6] * b[11] + a[7] * b[15];
    dst[8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[8] + a[11] * b[12];
    dst[9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[9] + a[11] * b[13];
    dst[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10] + a[11] * b[14];
    dst[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11] * b[15];
    dst[12] = a[12] * b[0] + a[13] * b[4] + a[14] * b[8] + a[15] * b[12];
    dst[13] = a[12] * b[1] + a[13] * b[5] + a[14] * b[9] + a[15] * b[13];
    dst[14] = a[12] * b[2] + a[13] * b[6] + a[14] * b[10] + a[15] * b[14];
    dst[15] = a[12] * b[3] + a[13] * b[7] + a[14] * b[11] + a[15] * b[15];
}

static inline GXColor gxcol_new_fv(float *components)
{
    GXColor c = {
        components[0] * 255.0f,
        components[1] * 255.0f,
        components[2] * 255.0f,
        components[3] * 255.0f
    };
    return c;
}

static inline void gxcol_mulfv(GXColor *color, float *components)
{
    color->r *= components[0];
    color->g *= components[1];
    color->b *= components[2];
    color->a *= components[3];
}

static inline GXColor gxcol_cpy_mulfv(GXColor color, float *components)
{
    color.r *= components[0];
    color.g *= components[1];
    color.b *= components[2];
    color.a *= components[3];
    return color;
}

static inline void set_error(GLenum code)
{
    /* OpenGL mandates that the oldest unretrieved error must be preserved. */
    if (!glparamstate.error) {
        glparamstate.error = code;
    }
}

#endif /* OGX_UTILS_H */

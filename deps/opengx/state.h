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

#ifndef OGX_STATE_H
#define OGX_STATE_H

#include <GL/gl.h>
#include <gccore.h>

// Constant definition. Here are the limits of this implementation.
// Can be changed with care.

#define _MAX_GL_TEX    128 // Maximum number of textures
#define MAX_PROJ_STACK 4   // Proj. matrix stack depth
#define MAX_MODV_STACK 16  // Modelview matrix stack depth
#define NUM_VERTS_IM   64  // Maximum number of vertices that can be inside a glBegin/End
#define MAX_LIGHTS     4   // Max num lights
#define MAX_GX_LIGHTS  8

typedef float VertexData[12];

typedef struct gltexture_
{
    void *data;
    unsigned short w, h;
    GXTexObj texobj;
    char used;
    int bytespp;
    char maxlevel, minlevel;
    char onelevel;
    unsigned char wraps, wrapt;
} gltexture_;

typedef struct glparams_
{
    Mtx44 modelview_matrix;
    Mtx44 projection_matrix;
    Mtx44 modelview_stack[MAX_MODV_STACK];
    Mtx44 projection_stack[MAX_PROJ_STACK];
    int cur_modv_mat, cur_proj_mat;

    unsigned char srcblend, dstblend;
    unsigned char blendenabled;
    unsigned char zwrite, ztest, zfunc;
    unsigned char matrixmode;
    unsigned char frontcw, cullenabled;
    uint16_t texture_env_mode;
    GLenum glcullmode;
    int glcurtex;
    GXColor clear_color;
    float clearz;

    void *index_array;
    float *vertex_array, *texcoord_array, *normal_array;
    unsigned char *color_array;
    int vertex_stride, color_stride, index_stride, texcoord_stride, normal_stride;
    char vertex_enabled, normal_enabled, texcoord_enabled, index_enabled, color_enabled;

    char texture_enabled;

    struct imm_mode
    {
        float current_color[4];
        float current_texcoord[2];
        float current_normal[3];
        int current_numverts;
        int current_vertices_size;
        VertexData *current_vertices;
        GLenum prim_type;
    } imm_mode;

    union dirty_union
    {
        struct dirty_struct
        {
            unsigned dirty_blend : 1;
            unsigned dirty_z : 1;
            unsigned dirty_matrices : 1;
            unsigned dirty_lighting : 1;
            unsigned dirty_material : 1;
        } bits;
        unsigned int all;
    } dirty;

    struct _lighting
    {
        struct alight
        {
            float position[4];
            float direction[3];
            float spot_direction[3];
            float ambient_color[4];
            float diffuse_color[4];
            float specular_color[4];
            float atten[3];
            float spot_cutoff;
            int spot_exponent;
            char enabled;
            int8_t gx_ambient;
            int8_t gx_diffuse;
            int8_t gx_specular;
        } lights[MAX_LIGHTS];
        GXLightObj lightobj[MAX_LIGHTS * 2];
        float globalambient[4];
        float matambient[4];
        float matdiffuse[4];
        float matemission[4];
        float matspecular[4];
        float matshininess;
        char enabled;

        char color_material_enabled;
        uint16_t color_material_mode;

        GXColor cached_ambient;
    } lighting;

    struct _fog
    {
        u8 enabled;
        uint16_t mode;
        float color[4];
        float density;
        float start;
        float end;
    } fog;

    gltexture_ textures[_MAX_GL_TEX];

    GLenum error;
    short globalTextABR;
    short RGB24;
} glparams_;

extern glparams_ _ogx_state;

/* To avoid renaming all the variables */
#define glparamstate _ogx_state
#define texture_list _ogx_state.textures

#endif /* OGX_STATE_H */

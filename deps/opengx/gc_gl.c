
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

*****************************************************************************

             BASIC WII/GC OPENGL-LIKE IMPLEMENTATION

     This is a very basic OGL-like implementation. Don't expect any
     advanced (or maybe basic) features from the OGL spec.
     The support is very limited in some cases, you shoud read the
     README file which comes with the source to have an idea of the
     limits and how you can tune or modify this file to adapt the
     source to your neeeds.
     Take in mind this is not very fast. The code is intended to be
     tiny and much as portable as possible and easy to compile so
     there's lot of room for improvement.

*****************************************************************************/

#include "debug.h"
#include "image_DXT.h"
#include "opengx.h"
#include "state.h"
#include "utils.h"

#include "GL/gl.h"
#include <gctypes.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../../mem2_manager.h"
#include "../../Gamecube/wiiSXconfig.h"

#define ROUND_32B(x) (((x) + 31) & (~31))
#define min(a,b)     (((a) < (b)) ? (a) : (b))

#define TXT_TYPE_1  0x1
#define TXT_TYPE_2  0x2

//#define DISP_DEBUG

#ifdef DISP_DEBUG

static FILE* fdebugLog = NULL;
static char *debugLogFile = "sd:/wiisxrx/debugLog.txt";
static char txtbuffer[1024];

static void openLogFile() {
    if (!fdebugLog) {
        fdebugLog = fopen(debugLogFile, "a+");
    }
}

static void closeLogFile() {
    if (fdebugLog) {
        fclose(fdebugLog);
        fdebugLog = NULL;
    }
}

extern void writeLogFile(char* string);

#endif // DISP_DEBUG

glparams_ _ogx_state;

typedef struct
{
    uint8_t ambient_mask;
    uint8_t diffuse_mask;
    uint8_t specular_mask;
} LightMasks;

static const GLubyte gl_null_string[1] = { 0 };
char _ogx_log_level = 0;

static void draw_arrays_pos_normal_texc(float *ptr_pos, float *ptr_texc, float *ptr_normal,
                                        int count, bool loop);
static void draw_arrays_pos_normal(float *ptr_pos, float *ptr_normal, int count, bool loop);
static void draw_arrays_general(float *ptr_pos, float *ptr_normal, float *ptr_texc, unsigned char *ptr_color,
                                int count, int ne, int color_provide, int texen, bool loop);

static Mtx44 GXprojection2D;

#define MODELVIEW_UPDATE                                           \
    {                                                              \
        float trans[3][4];                                         \
        int i;                                                     \
        int j;                                                     \
        for (i = 0; i < 3; i++)                                    \
            for (j = 0; j < 4; j++)                                \
                trans[i][j] = glparamstate.modelview_matrix[j][i]; \
                                                                   \
        GX_LoadPosMtxImm(trans, GX_PNMTX0);                        \
        GX_SetCurrentMtx(GX_PNMTX0);                               \
    }

/* OpenGL's projection matrix transform the scene into a clip space where all
 * the coordinates lie in the range [-1, 1]. Nintendo's GX, however, for the z
 * coordinates expects a range of [-1, 0], so we need to transform the z
 * coordinates accordingly: they must be halved (which gives us a range of
 * [-0.5, 0.5] and translated back by 0.5 (to get [-1, 0]). The adjustment
 * matrix below does exactly that: it's a matrix that scales and translates the
 * z coordinate as we just described. */
static const Mtx44 s_projection_adj = {
    { 1.0, 0.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0, 0.0 },
    { 0.0, 0.0, 0.5, -0.5 },
    { 0.0, 0.0, 0.0, 1.0 },
};

#define PROJECTION_UPDATE                                           \
    {                                                               \
        Mtx44 proj;                                                 \
        guMtx44Concat(s_projection_adj,                             \
                      glparamstate.projection_matrix,               \
                      proj);                                        \
        if (glparamstate.projection_matrix[3][3] != 0)              \
            GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);            \
        else                                                        \
            GX_LoadProjectionMtx(proj, GX_PERSPECTIVE);             \
    }

#define NORMAL_UPDATE                                                  \
    {                                                                  \
        int i, j;                                                      \
        Mtx mvinverse, normalm, modelview;                             \
        for (i = 0; i < 3; i++)                                        \
            for (j = 0; j < 4; j++)                                    \
                modelview[i][j] = glparamstate.modelview_matrix[j][i]; \
                                                                       \
        guMtxInverse(modelview, mvinverse);                            \
        guMtxTranspose(mvinverse, normalm);                            \
        GX_LoadNrmMtxImm(normalm, GX_PNMTX0);                          \
    }

/* Deduce the projection type (perspective vs orthogonal) and the values of the
 * near and far clipping plane from the projection matrix.
 * Note that the formulas for computing "near" and "far" are only valid for
 * matrices created by opengx or by the gu* family of GX functions. OpenGL
 * books use different formulas.
 */
static void get_projection_info(u8 *type, float *near, float *far)
{
    Mtx44 proj;
    float A, B;

    guMtx44Concat(s_projection_adj,
                  glparamstate.projection_matrix,
                  proj);
    A = proj[2][2];
    B = proj[2][3];

    if (proj[3][3] == 0) {
        *type = GX_PERSPECTIVE;
        *near = B / (A - 1.0);
    } else {
        *type = GX_ORTHOGRAPHIC;
        *near = (B + 1.0) / A;
    }
    *far = B / A;
}

void ogx_initialize()
{
    const char *log_env = getenv("OPENGX_DEBUG");
    if (log_env) {
        _ogx_log_level = log_env[0] - '0';
    }

    GX_SetDispCopyGamma(GX_GM_1_0);
    int i;
    for (i = 0; i < _MAX_GL_TEX; i++) {
        texture_list[i].used = 0;
        texture_list[i].data = 0;
        texture_list[i].semiTransData = 0;
    }

    glparamstate.blendenabled = 0;
    glparamstate.srcblend = GX_BL_ONE;
    glparamstate.dstblend = GX_BL_ZERO;

    glparamstate.clear_color.r = 0; // Black as default
    glparamstate.clear_color.g = 0;
    glparamstate.clear_color.b = 0;
    glparamstate.clear_color.a = 1;
    glparamstate.clearz = 1.0f;

    glparamstate.ztest = GX_FALSE; // Depth test disabled but z write enabled
    glparamstate.zfunc = GX_LESS;  // Although write is efectively disabled
    glparamstate.zwrite = GX_TRUE; // unless test is enabled

    glparamstate.matrixmode = 1; // Modelview default mode
    glparamstate.glcurtex = 0;   // Default texture is 0 (nonstardard)
    GX_SetNumChans(1);           // One modulation color (as glColor)
    glDisable(GL_TEXTURE_2D);

    glparamstate.glcullmode = GL_BACK;
    glparamstate.cullenabled = 0;
    glparamstate.frontcw = 0; // By default front is CCW
    glDisable(GL_CULL_FACE);
    glparamstate.texture_env_mode = GL_MODULATE;

    glparamstate.cur_proj_mat = -1;
    glparamstate.cur_modv_mat = -1;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glparamstate.imm_mode.current_color[0] = 1.0f; // Default imm data, could be wrong
    glparamstate.imm_mode.current_color[1] = 1.0f;
    glparamstate.imm_mode.current_color[2] = 1.0f;
    glparamstate.imm_mode.current_color[3] = 1.0f;
    glparamstate.imm_mode.current_texcoord[0] = 0;
    glparamstate.imm_mode.current_texcoord[1] = 0;
    glparamstate.imm_mode.current_normal[0] = 0;
    glparamstate.imm_mode.current_normal[1] = 0;
    glparamstate.imm_mode.current_normal[2] = 1.0f;
    glparamstate.imm_mode.current_numverts = 0;

    glparamstate.vertex_enabled = 0; // DisableClientState on everything
    glparamstate.normal_enabled = 0;
    glparamstate.texcoord_enabled = 0;
    glparamstate.index_enabled = 0;
    glparamstate.color_enabled = 0;

    glparamstate.texture_enabled = 0;

    // Set up lights default states
    glparamstate.lighting.enabled = 0;
    for (i = 0; i < MAX_LIGHTS; i++) {
        glparamstate.lighting.lights[i].enabled = false;

        glparamstate.lighting.lights[i].atten[0] = 1;
        glparamstate.lighting.lights[i].atten[1] = 0;
        glparamstate.lighting.lights[i].atten[2] = 0;

        /* The default value for light position is (0, 0, 1), but since it's a
         * directional light we need to transform it to 100000. */
        glparamstate.lighting.lights[i].position[0] = 0;
        glparamstate.lighting.lights[i].position[1] = 0;
        glparamstate.lighting.lights[i].position[2] = 100000;
        glparamstate.lighting.lights[i].position[3] = 0;

        glparamstate.lighting.lights[i].direction[0] = 0;
        glparamstate.lighting.lights[i].direction[1] = 0;
        glparamstate.lighting.lights[i].direction[2] = -1;

        glparamstate.lighting.lights[i].spot_direction[0] = 0;
        glparamstate.lighting.lights[i].spot_direction[1] = 0;
        glparamstate.lighting.lights[i].spot_direction[2] = -1;

        glparamstate.lighting.lights[i].ambient_color[0] = 0;
        glparamstate.lighting.lights[i].ambient_color[1] = 0;
        glparamstate.lighting.lights[i].ambient_color[2] = 0;
        glparamstate.lighting.lights[i].ambient_color[3] = 1;

        if (i == 0) {
            glparamstate.lighting.lights[i].diffuse_color[0] = 1;
            glparamstate.lighting.lights[i].diffuse_color[1] = 1;
            glparamstate.lighting.lights[i].diffuse_color[2] = 1;

            glparamstate.lighting.lights[i].specular_color[0] = 1;
            glparamstate.lighting.lights[i].specular_color[1] = 1;
            glparamstate.lighting.lights[i].specular_color[2] = 1;
        } else {
            glparamstate.lighting.lights[i].diffuse_color[0] = 0;
            glparamstate.lighting.lights[i].diffuse_color[1] = 0;
            glparamstate.lighting.lights[i].diffuse_color[2] = 0;

            glparamstate.lighting.lights[i].specular_color[0] = 0;
            glparamstate.lighting.lights[i].specular_color[1] = 0;
            glparamstate.lighting.lights[i].specular_color[2] = 0;
        }
        glparamstate.lighting.lights[i].diffuse_color[3] = 1;
        glparamstate.lighting.lights[i].specular_color[3] = 1;

        glparamstate.lighting.lights[i].spot_cutoff = 180.0f;
        glparamstate.lighting.lights[i].spot_exponent = 0;
    }

    glparamstate.lighting.globalambient[0] = 0.2f;
    glparamstate.lighting.globalambient[1] = 0.2f;
    glparamstate.lighting.globalambient[2] = 0.2f;
    glparamstate.lighting.globalambient[3] = 1.0f;

    glparamstate.lighting.matambient[0] = 0.2f;
    glparamstate.lighting.matambient[1] = 0.2f;
    glparamstate.lighting.matambient[2] = 0.2f;
    glparamstate.lighting.matambient[3] = 1.0f;

    glparamstate.lighting.matdiffuse[0] = 0.8f;
    glparamstate.lighting.matdiffuse[1] = 0.8f;
    glparamstate.lighting.matdiffuse[2] = 0.8f;
    glparamstate.lighting.matdiffuse[3] = 1.0f;

    glparamstate.lighting.matemission[0] = 0.0f;
    glparamstate.lighting.matemission[1] = 0.0f;
    glparamstate.lighting.matemission[2] = 0.0f;
    glparamstate.lighting.matemission[3] = 1.0f;

    glparamstate.lighting.matspecular[0] = 0.0f;
    glparamstate.lighting.matspecular[1] = 0.0f;
    glparamstate.lighting.matspecular[2] = 0.0f;
    glparamstate.lighting.matspecular[3] = 1.0f;
    glparamstate.lighting.matshininess = 0.0f;

    glparamstate.lighting.color_material_enabled = 0;
    glparamstate.lighting.color_material_mode = GL_AMBIENT_AND_DIFFUSE;

    glparamstate.fog.enabled = false;
    glparamstate.fog.mode = GL_EXP;
    glparamstate.fog.color[0] = 0.0f;
    glparamstate.fog.color[1] = 0.0f;
    glparamstate.fog.color[2] = 0.0f;
    glparamstate.fog.color[3] = 0.0f;
    glparamstate.fog.density = 1.0f;
    glparamstate.fog.start = 0.0f;
    glparamstate.fog.end = 1.0f;

    glparamstate.error = GL_NO_ERROR;

    // Setup data types for every possible attribute

    // Typical straight float
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    // Mark all the hardware data as dirty, so it will be recalculated
    // and uploaded again to the hardware
    glparamstate.dirty.all = ~0;

    GX_SetTevColor(GX_TEVREG2, (GXColor){0x3F, 0x3F, 0x3F, 0xFF}); // 1/4 color
}

void glEnable(GLenum cap)
{ // TODO
    switch (cap) {
    case GL_TEXTURE_2D:
        glparamstate.texture_enabled = 1;
        break;
    case GL_COLOR_MATERIAL:
        glparamstate.lighting.color_material_enabled = 1;
        break;
    case GL_CULL_FACE:
        switch (glparamstate.glcullmode) {
        case GL_FRONT:
            if (glparamstate.frontcw)
                GX_SetCullMode(GX_CULL_FRONT);
            else
                GX_SetCullMode(GX_CULL_BACK);
            break;
        case GL_BACK:
            if (glparamstate.frontcw)
                GX_SetCullMode(GX_CULL_BACK);
            else
                GX_SetCullMode(GX_CULL_FRONT);
            break;
        case GL_FRONT_AND_BACK:
            GX_SetCullMode(GX_CULL_ALL);
            break;
        };
        glparamstate.cullenabled = 1;
        break;
    case GL_BLEND:
        glparamstate.blendenabled = 1;
        glparamstate.dirty.bits.dirty_blend = 1;
        break;
    case GL_DEPTH_TEST:
        glparamstate.ztest = GX_TRUE;
        glparamstate.dirty.bits.dirty_z = 1;
        break;
    case GL_FOG:
        glparamstate.fog.enabled = 1;
        break;
    case GL_LIGHTING:
        glparamstate.lighting.enabled = 1;
        glparamstate.dirty.bits.dirty_lighting = 1;
        break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
        glparamstate.lighting.lights[cap - GL_LIGHT0].enabled = 1;
        glparamstate.dirty.bits.dirty_lighting = 1;
        break;
    default:
        break;
    }
}

void glDisable(GLenum cap)
{ // TODO
    switch (cap) {
    case GL_TEXTURE_2D:
        glparamstate.texture_enabled = 0;
        break;
    case GL_COLOR_MATERIAL:
        glparamstate.lighting.color_material_enabled = 0;
        break;
    case GL_CULL_FACE:
        GX_SetCullMode(GX_CULL_NONE);
        glparamstate.cullenabled = 0;
        break;
    case GL_BLEND:
        glparamstate.blendenabled = 0;
        glparamstate.dirty.bits.dirty_blend = 1;
        break;
    case GL_DEPTH_TEST:
        glparamstate.ztest = GX_FALSE;
        glparamstate.dirty.bits.dirty_z = 1;
        break;
    case GL_LIGHTING:
        glparamstate.lighting.enabled = 0;
        glparamstate.dirty.bits.dirty_lighting = 1;
        break;
    case GL_LIGHT0:
    case GL_LIGHT1:
    case GL_LIGHT2:
    case GL_LIGHT3:
        glparamstate.lighting.lights[cap - GL_LIGHT0].enabled = 0;
        glparamstate.dirty.bits.dirty_lighting = 1;
        break;
    default:
        break;
    }
}

void glFogf(GLenum pname, GLfloat param)
{
    switch (pname) {
    case GL_FOG_MODE:
        glFogi(pname, (int)param);
        break;
    case GL_FOG_DENSITY:
        glparamstate.fog.density = param;
        break;
    case GL_FOG_START:
        glparamstate.fog.start = param;
        break;
    case GL_FOG_END:
        glparamstate.fog.end = param;
        break;
    }
}

void glFogi(GLenum pname, GLint param)
{
    switch (pname) {
    case GL_FOG_MODE:
        glparamstate.fog.mode = param;
        break;
    case GL_FOG_DENSITY:
    case GL_FOG_START:
    case GL_FOG_END:
        glFogf(pname, param);
        break;
    }
}

void glFogfv(GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_FOG_MODE:
    case GL_FOG_DENSITY:
    case GL_FOG_START:
    case GL_FOG_END:
        glFogf(pname, params[0]);
        break;
    case GL_FOG_COLOR:
        floatcpy(glparamstate.fog.color, params, 4);
        break;
    }
}

void glLightf(GLenum light, GLenum pname, GLfloat param)
{
    int lnum = light - GL_LIGHT0;

    switch (pname) {
    case GL_CONSTANT_ATTENUATION:
        glparamstate.lighting.lights[lnum].atten[0] = param;
        break;
    case GL_LINEAR_ATTENUATION:
        glparamstate.lighting.lights[lnum].atten[1] = param;
        break;
    case GL_QUADRATIC_ATTENUATION:
        glparamstate.lighting.lights[lnum].atten[2] = param;
        break;
    case GL_SPOT_CUTOFF:
        glparamstate.lighting.lights[lnum].spot_cutoff = param;
        break;
    case GL_SPOT_EXPONENT:
        glparamstate.lighting.lights[lnum].spot_exponent = (int)param;
        break;
    default:
        break;
    }
    glparamstate.dirty.bits.dirty_lighting = 1;
}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
    int lnum = light - GL_LIGHT0;
    switch (pname) {
    case GL_SPOT_DIRECTION:
        floatcpy(glparamstate.lighting.lights[lnum].spot_direction, params, 3);
        break;
    case GL_POSITION:
        if (params[3] == 0) {
            // Push the light far away, calculate the direction and normalize it
            glparamstate.lighting.lights[lnum].position[0] = params[0] * 100000;
            glparamstate.lighting.lights[lnum].position[1] = params[1] * 100000;
            glparamstate.lighting.lights[lnum].position[2] = params[2] * 100000;
        } else {
            glparamstate.lighting.lights[lnum].position[0] = params[0];
            glparamstate.lighting.lights[lnum].position[1] = params[1];
            glparamstate.lighting.lights[lnum].position[2] = params[2];
        }
        glparamstate.lighting.lights[lnum].position[3] = params[3];
        {
            float modv[3][4];
            int i;
            int j;
            for (i = 0; i < 3; i++)
                for (j = 0; j < 4; j++)
                    modv[i][j] = glparamstate.modelview_matrix[j][i];
            guVecMultiply(modv, (guVector *)glparamstate.lighting.lights[lnum].position, (guVector *)glparamstate.lighting.lights[lnum].position);
        }
        break;
    case GL_DIFFUSE:
        floatcpy(glparamstate.lighting.lights[lnum].diffuse_color, params, 4);
        break;
    case GL_AMBIENT:
        floatcpy(glparamstate.lighting.lights[lnum].ambient_color, params, 4);
        break;
    case GL_SPECULAR:
        floatcpy(glparamstate.lighting.lights[lnum].specular_color, params, 4);
        break;
    }
    glparamstate.dirty.bits.dirty_lighting = 1;
}

void glLightModelfv(GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_LIGHT_MODEL_AMBIENT:
        floatcpy(glparamstate.lighting.globalambient, params, 4);
        break;
    }
    glparamstate.dirty.bits.dirty_material = 1;
};

void glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    glMaterialfv(face, pname, &param);
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
    switch (pname) {
    case GL_DIFFUSE:
        floatcpy(glparamstate.lighting.matdiffuse, params, 4);
        break;
    case GL_AMBIENT:
        floatcpy(glparamstate.lighting.matambient, params, 4);
        break;
    case GL_AMBIENT_AND_DIFFUSE:
        floatcpy(glparamstate.lighting.matambient, params, 4);
        floatcpy(glparamstate.lighting.matdiffuse, params, 4);
        break;
    case GL_EMISSION:
        floatcpy(glparamstate.lighting.matemission, params, 4);
        break;
    case GL_SPECULAR:
        floatcpy(glparamstate.lighting.matspecular, params, 4);
        break;
    case GL_SHININESS:
        glparamstate.lighting.matshininess = params[0];
        break;
    default:
        break;
    }
    glparamstate.dirty.bits.dirty_material = 1;
};

void glColorMaterial(GLenum face, GLenum mode)
{
    /* TODO: support the face parameter */
    glparamstate.lighting.color_material_mode = mode;
}

void glCullFace(GLenum mode)
{
    glparamstate.glcullmode = mode;
    if (glparamstate.cullenabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
}

void glBindTexture(GLenum target, GLuint texture)
{
    if (texture < 0 || texture >= _MAX_GL_TEX)
        return;

    // If the texture has been initialized (data!=0) then load it to GX reg 0
    if (texture_list[texture].used) {
        glparamstate.glcurtex = texture;

        if (texture_list[texture].data != 0)
            GX_LoadTexObj(&texture_list[glparamstate.glcurtex].texobj, GX_TEXMAP0);
    }
}

void glSetGlobalTextABR( short globalTextABR )
{
    glparamstate.globalTextABR = globalTextABR;
}

void glSetRGB24( short rgb24 )
{
    glparamstate.RGB24 = rgb24;
}

static short semiTransFlg = 0;
void glSetSemiTransFlg( short semiTrans_Flg )
{
    semiTransFlg = semiTrans_Flg;
}

static short setTextureMask = 0;
static short tmpTextureMask = 0;
void glSetTextureMask( short mask )
{
    tmpTextureMask = mask;
    setTextureMask = tmpTextureMask;
}

static short vramClearedFlg = 0;
void glSetVramClearedFlg( void )
{
    vramClearedFlg = 1;
}

static short texturyType = 0;
void glSetTextureType( short texType )
{
    texturyType = texType;
}

static short needLoadMtx = 1;
void glSetLoadMtxFlg( void )
{
    needLoadMtx = 1;
}

void glBindTextureBef(GLenum target, GLuint texture)
{
    if (texture < 0 || texture >= _MAX_GL_TEX)
        return;

    glparamstate.glcurtex = texture;
}

static short doubleColor = 0;

void glSetDoubleCol( void )
{
    doubleColor = 1;
}

void glChgTextureFilter( void )
{
    int i;
    for (i = 1; i < _MAX_GL_TEX; i++) {
        gltexture_ *currtex = &texture_list[i];
        if (currtex->used == 1) {
            if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
            {
                if (currtex->data)
                {
                    GX_InitTexObjFilterMode(&currtex->texobj, GX_NEAR, GX_NEAR);
                }
                if (currtex->semiTransData)
                {
                    GX_InitTexObjFilterMode(&currtex->semiTransTexobj, GX_NEAR, GX_NEAR);
                }
            }
            else
            {
                if (currtex->data)
                {
                    GX_InitTexObjFilterMode(&currtex->texobj, GX_LINEAR, GX_LINEAR);
                }
                if (currtex->semiTransData)
                {
                    GX_InitTexObjFilterMode(&currtex->semiTransTexobj, GX_LINEAR, GX_LINEAR);
                }
            }
        }
        else
        {
            break;
        }
    }
}

#define TEX_TYPE_DEFAULT      0
#define TEX_TYPE_MOVIE        1
#define TEX_TYPE_UPLOAD       2
#define TEX_TYPE_WIN          3
#define TEX_TYPE_SUB          4
#define TEX_TYPE_UI           5
#define TEX_TYPE_SEMI         6

#define GX_TEXMAP_MOV         GX_TEXMAP0
#define GX_TEXMAP_WIN         GX_TEXMAP1
#define GX_TEXMAP_SUB         GX_TEXMAP2
#define GX_TEXMAP_SEMI        GX_TEXMAP3
#define GX_TEXMAP_UI          GX_TEXMAP4

extern GXTexRegion movieUploadTexRegion;
extern GXTexRegion semiTransTexRegion;
extern GXTexRegion subTexRegion;
extern GXTexRegion winTexRegion;

static short curTextureType;
void glCheckLoadTextureObj( int loadTextureType )
{
    curTextureType = loadTextureType;
    gltexture_ *currtex = &texture_list[glparamstate.glcurtex];

    if (texturyType & TXT_TYPE_1)
    {
        if (glparamstate.blendenabled)
        {
            GX_InvalidateTexRegion(&semiTransTexRegion);
            GX_LoadTexObjPreloaded(&currtex->semiTransTexobj, &semiTransTexRegion, GX_TEXMAP_SEMI);
            GX_SetDrawDone();
        }
    }
    if (texturyType & TXT_TYPE_2)
    {
        switch (loadTextureType)
        {
            case TEX_TYPE_MOVIE:
                GX_InvalidateTexRegion(&movieUploadTexRegion);
                GX_LoadTexObjPreloaded(&currtex->texobj, &movieUploadTexRegion, GX_TEXMAP_MOV);
                break;
            case TEX_TYPE_WIN:
                GX_InvalidateTexRegion(&winTexRegion);
                GX_LoadTexObjPreloaded(&currtex->texobj, &winTexRegion, GX_TEXMAP_WIN);
                break;
            case TEX_TYPE_SUB:
                if ((texturyType & TXT_TYPE_1) && glparamstate.blendenabled)
                {
                    GX_WaitDrawDone();
                }
                GX_InvalidateTexRegion(&subTexRegion);
                GX_LoadTexObjPreloaded(&currtex->texobj, &subTexRegion, GX_TEXMAP_SUB);
                break;
        }
    }
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
    const GLuint *texlist = textures;
    GX_DrawDone();
    while (n-- > 0) {
        int i = *texlist++;
        if (!(i < 0 || i >= _MAX_GL_TEX)) {
            if (texture_list[i].data != 0)
                _mem2_free(texture_list[i].data);
            if (texture_list[i].semiTransData != 0)
                _mem2_free(texture_list[i].semiTransData);
            texture_list[i].data = 0;
            texture_list[i].semiTransData = 0;
            texture_list[i].used = 0;
        }
    }
}

void glGenTextures(GLsizei n, GLuint *textures)
{
    GLuint *texlist = textures;
    int i;
    for (i = 1; i < _MAX_GL_TEX && n > 0; i++) {
        if (texture_list[i].used == 0) {
            texture_list[i].used = 1;
            texture_list[i].data = 0;
            texture_list[i].semiTransData = 0;
            texture_list[i].w = 0;
            texture_list[i].h = 0;
            texture_list[i].wraps = GX_REPEAT;
            texture_list[i].wrapt = GX_REPEAT;
            texture_list[i].bytespp = 0;
            texture_list[i].maxlevel = -1;
            texture_list[i].minlevel = 20;
            *texlist++ = i;
            n--;
        }
    }
}
void glBegin(GLenum mode)
{
    // Just discard all the data!
    glparamstate.imm_mode.current_numverts = 0;
    glparamstate.imm_mode.prim_type = mode;
    if (!glparamstate.imm_mode.current_vertices) {
        int count = 64;
        void *buffer = _mem2_malloc(count * sizeof(VertexData));
        if (buffer) {
            glparamstate.imm_mode.current_vertices = buffer;
            glparamstate.imm_mode.current_vertices_size = count;
        } else {
            set_error(GL_OUT_OF_MEMORY);
        }
    }
}

void glEnd()
{
    glInterleavedArrays(GL_T2F_C4F_N3F_V3F, 0, glparamstate.imm_mode.current_vertices);
    glDrawArrays(glparamstate.imm_mode.prim_type, 0, glparamstate.imm_mode.current_numverts);
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    GX_SetViewport(x, y, width, height, 0.0f, 1.0f);
    GX_SetScissor(x, y, width, height);
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    GX_SetScissor(x, y, width, height);
}

// To improve efficiency, reduce multiplication operations when the color value is 255,255,255,255
static short noNeedMulConstColor = 0;

void glNoNeedMulConstColor( short noNeedMulConstColorFlg )
{
    noNeedMulConstColor = noNeedMulConstColorFlg;
}

void glColor4Lcol( unsigned int  lcol )
{
    glparamstate.imm_mode.c.lcol = lcol;
}

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
    glparamstate.imm_mode.current_color[0] = r / 255.0f;
    glparamstate.imm_mode.current_color[1] = g / 255.0f;
    glparamstate.imm_mode.current_color[2] = b / 255.0f;
    glparamstate.imm_mode.current_color[3] = a / 255.0f;
}
void glColor4ubv(const GLubyte *color)
{
    glparamstate.imm_mode.current_color[0] = color[0] / 255.0f;
    glparamstate.imm_mode.current_color[1] = color[1] / 255.0f;
    glparamstate.imm_mode.current_color[2] = color[2] / 255.0f;
    glparamstate.imm_mode.current_color[3] = color[3] / 255.0f;
}
void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glparamstate.imm_mode.current_color[0] = clampf_01(red);
    glparamstate.imm_mode.current_color[1] = clampf_01(green);
    glparamstate.imm_mode.current_color[2] = clampf_01(blue);
    glparamstate.imm_mode.current_color[3] = clampf_01(alpha);
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
    glparamstate.imm_mode.current_color[0] = clampf_01(red);
    glparamstate.imm_mode.current_color[1] = clampf_01(green);
    glparamstate.imm_mode.current_color[2] = clampf_01(blue);
    glparamstate.imm_mode.current_color[3] = 1.0f;
}

void glColor4fv(const GLfloat *v)
{
    glparamstate.imm_mode.current_color[0] = clampf_01(v[0]);
    glparamstate.imm_mode.current_color[1] = clampf_01(v[1]);
    glparamstate.imm_mode.current_color[2] = clampf_01(v[2]);
    glparamstate.imm_mode.current_color[3] = clampf_01(v[3]);
}

void glColor3fv(const GLfloat *v)
{
    glColor3f(v[0], v[1], v[2]);
}

void glTexCoord2d(GLdouble u, GLdouble v)
{
    glTexCoord2f(u, v);
}

void glTexCoord2f(GLfloat u, GLfloat v)
{
    glparamstate.imm_mode.current_texcoord[0] = u;
    glparamstate.imm_mode.current_texcoord[1] = v;
}

void glTexCoord3f(GLfloat s, GLfloat t, GLfloat r)
{
    glTexCoord2f(s, t);
    if (r != 0.0) {
        warning("glTexCoord3f not supported");
    }
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    glparamstate.imm_mode.current_normal[0] = nx;
    glparamstate.imm_mode.current_normal[1] = ny;
    glparamstate.imm_mode.current_normal[2] = nz;
}

void glNormal3fv(const GLfloat *v)
{
    glparamstate.imm_mode.current_normal[0] = v[0];
    glparamstate.imm_mode.current_normal[1] = v[1];
    glparamstate.imm_mode.current_normal[2] = v[2];
}

void glVertex2d(GLdouble x, GLdouble y)
{
    glVertex3f(x, y, 0.0f);
}

void glVertex2i(GLint x, GLint y)
{
    glVertex3f(x, y, 0.0f);
}

void glVertex2f(GLfloat x, GLfloat y)
{
    glVertex3f(x, y, 0.0f);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (glparamstate.imm_mode.current_numverts >= glparamstate.imm_mode.current_vertices_size) {
        if (!glparamstate.imm_mode.current_vertices) return;
        int current_size = glparamstate.imm_mode.current_vertices_size;
        int new_size = current_size < 256 ? (current_size * 2) : (current_size + 256);
        void *new_buffer = realloc(glparamstate.imm_mode.current_vertices,
                                   new_size * sizeof(VertexData));
        if (!new_buffer) {
            set_error(GL_OUT_OF_MEMORY);
            return;
        }
        glparamstate.imm_mode.current_vertices_size = new_size;
        glparamstate.imm_mode.current_vertices = new_buffer;
    }

    // GL_T2F_C4F_N3F_V3F
    float *vert = glparamstate.imm_mode.current_vertices[glparamstate.imm_mode.current_numverts++];
    vert[0] = glparamstate.imm_mode.current_texcoord[0];
    vert[1] = glparamstate.imm_mode.current_texcoord[1];

    floatcpy(vert + 2, glparamstate.imm_mode.current_color, 4);

    floatcpy(vert + 6, glparamstate.imm_mode.current_normal, 3);

    vert[9] = x;
    vert[10] = y;
    vert[11] = z;
}

void glVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    glVertex3f(x / w, y / w, z / w);
}

void glMatrixMode(GLenum mode)
{
    switch (mode) {
    case GL_MODELVIEW:
        glparamstate.matrixmode = 1;
        break;
    case GL_PROJECTION:
        glparamstate.matrixmode = 0;
        break;
    default:
        glparamstate.matrixmode = -1;
        break;
    }
}
void glPopMatrix(void)
{
    switch (glparamstate.matrixmode) {
    case 0:
        memcpy(glparamstate.projection_matrix, glparamstate.projection_stack[glparamstate.cur_proj_mat], sizeof(Mtx44));
        glparamstate.cur_proj_mat--;
    case 1:
        memcpy(glparamstate.modelview_matrix, glparamstate.modelview_stack[glparamstate.cur_modv_mat], sizeof(Mtx44));
        glparamstate.cur_modv_mat--;
    default:
        break;
    }
    glparamstate.dirty.bits.dirty_matrices = 1;
}
void glPushMatrix(void)
{
    switch (glparamstate.matrixmode) {
    case 0:
        glparamstate.cur_proj_mat++;
        memcpy(glparamstate.projection_stack[glparamstate.cur_proj_mat], glparamstate.projection_matrix, sizeof(Mtx44));
        break;
    case 1:
        glparamstate.cur_modv_mat++;
        memcpy(glparamstate.modelview_stack[glparamstate.cur_modv_mat], glparamstate.modelview_matrix, sizeof(Mtx44));
        break;
    default:
        break;
    }
}
void glLoadMatrixf(const GLfloat *m)
{
    switch (glparamstate.matrixmode) {
    case 0:
        memcpy(glparamstate.projection_matrix, m, sizeof(Mtx44));
        break;
    case 1:
        memcpy(glparamstate.modelview_matrix, m, sizeof(Mtx44));
        break;
    default:
        return;
    }
    glparamstate.dirty.bits.dirty_matrices = 1;
}

void glMultMatrixd(const GLdouble *m)
{
    GLfloat mf[16];
    for (int i = 0; i < 16; i++) {
        mf[i] = m[i];
    }
    glMultMatrixf(mf);
}

void glMultMatrixf(const GLfloat *m)
{
    Mtx44 curr;

    switch (glparamstate.matrixmode) {
    case 0:
        memcpy((float *)curr, &glparamstate.projection_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.projection_matrix[0][0], (float *)curr, (float *)m);
        break;
    case 1:
        memcpy((float *)curr, &glparamstate.modelview_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.modelview_matrix[0][0], (float *)curr, (float *)m);
        break;
    default:
        break;
    }
    glparamstate.dirty.bits.dirty_matrices = 1;
}
void glLoadIdentity()
{
    float *mtrx;
    switch (glparamstate.matrixmode) {
    case 0:
        mtrx = &glparamstate.projection_matrix[0][0];
        break;
    case 1:
        mtrx = &glparamstate.modelview_matrix[0][0];
        break;
    default:
        return;
    }

    mtrx[0] = 1.0f;
    mtrx[1] = 0.0f;
    mtrx[2] = 0.0f;
    mtrx[3] = 0.0f;
    mtrx[4] = 0.0f;
    mtrx[5] = 1.0f;
    mtrx[6] = 0.0f;
    mtrx[7] = 0.0f;
    mtrx[8] = 0.0f;
    mtrx[9] = 0.0f;
    mtrx[10] = 1.0f;
    mtrx[11] = 0.0f;
    mtrx[12] = 0.0f;
    mtrx[13] = 0.0f;
    mtrx[14] = 0.0f;
    mtrx[15] = 1.0f;

    glparamstate.dirty.bits.dirty_matrices = 1;
}
void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
    Mtx44 newmat;
    Mtx44 curr;
    newmat[0][0] = x;
    newmat[0][1] = 0.0f;
    newmat[0][2] = 0.0f;
    newmat[0][3] = 0.0f;
    newmat[1][0] = 0.0f;
    newmat[1][1] = y;
    newmat[1][2] = 0.0f;
    newmat[1][3] = 0.0f;
    newmat[2][0] = 0.0f;
    newmat[2][1] = 0.0f;
    newmat[2][2] = z;
    newmat[2][3] = 0.0f;
    newmat[3][0] = 0.0f;
    newmat[3][1] = 0.0f;
    newmat[3][2] = 0.0f;
    newmat[3][3] = 1.0f;

    switch (glparamstate.matrixmode) {
    case 0:
        memcpy((float *)curr, &glparamstate.projection_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.projection_matrix[0][0], (float *)curr, (float *)newmat);
        break;
    case 1:
        memcpy((float *)curr, &glparamstate.modelview_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.modelview_matrix[0][0], (float *)curr, (float *)newmat);
        break;
    default:
        break;
    }

    glparamstate.dirty.bits.dirty_matrices = 1;
}

void glTranslated(GLdouble x, GLdouble y, GLdouble z)
{
    glTranslatef(x, y, z);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    Mtx44 newmat;
    Mtx44 curr;
    newmat[0][0] = 1.0f;
    newmat[0][1] = 0.0f;
    newmat[0][2] = 0.0f;
    newmat[0][3] = 0.0f;
    newmat[1][0] = 0.0f;
    newmat[1][1] = 1.0f;
    newmat[1][2] = 0.0f;
    newmat[1][3] = 0.0f;
    newmat[2][0] = 0.0f;
    newmat[2][1] = 0.0f;
    newmat[2][2] = 1.0f;
    newmat[2][3] = 0.0f;
    newmat[3][0] = x;
    newmat[3][1] = y;
    newmat[3][2] = z;
    newmat[3][3] = 1.0f;

    switch (glparamstate.matrixmode) {
    case 0:
        memcpy((float *)curr, &glparamstate.projection_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.projection_matrix[0][0], (float *)curr, (float *)newmat);
        break;
    case 1:
        memcpy((float *)curr, &glparamstate.modelview_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.modelview_matrix[0][0], (float *)curr, (float *)newmat);
        break;
    default:
        break;
    }

    glparamstate.dirty.bits.dirty_matrices = 1;
}
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    angle *= (M_PI / 180.0f);
    float c = cosf(angle);
    float s = sinf(angle);
    float t = 1.0f - c;
    Mtx44 newmat;
    Mtx44 curr;

    float imod = 1.0f / sqrtf(x * x + y * y + z * z);
    x *= imod;
    y *= imod;
    z *= imod;

    newmat[0][0] = t * x * x + c;
    newmat[0][1] = t * x * y + s * z;
    newmat[0][2] = t * x * z - s * y;
    newmat[0][3] = 0;
    newmat[1][0] = t * x * y - s * z;
    newmat[1][1] = t * y * y + c;
    newmat[1][2] = t * y * z + s * x;
    newmat[1][3] = 0;
    newmat[2][0] = t * x * z + s * y;
    newmat[2][1] = t * y * z - s * x;
    newmat[2][2] = t * z * z + c;
    newmat[2][3] = 0;
    newmat[3][0] = 0;
    newmat[3][1] = 0;
    newmat[3][2] = 0;
    newmat[3][3] = 1;

    switch (glparamstate.matrixmode) {
    case 0:
        memcpy((float *)curr, &glparamstate.projection_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.projection_matrix[0][0], (float *)curr, (float *)newmat);
        break;
    case 1:
        memcpy((float *)curr, &glparamstate.modelview_matrix[0][0], sizeof(Mtx44));
        gl_matrix_multiply(&glparamstate.modelview_matrix[0][0], (float *)curr, (float *)newmat);
        break;
    default:
        break;
    }

    glparamstate.dirty.bits.dirty_matrices = 1;
}

void glClearColor2(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
{
    glparamstate.clear_color.r = (red);
    glparamstate.clear_color.g = (green);
    glparamstate.clear_color.b = (blue);
    glparamstate.clear_color.a = (alpha);
}
void glClearDepth(GLclampd depth)
{
    glparamstate.clearz = clampf_01(depth);
}

// Clearing is simulated by rendering a big square with the depth value
// and the desired color
void glClear(GLbitfield mask)
{
    // Tweak the Z value to avoid floating point errors. dpeth goes from 0.001 to 0.998
    float depth = (0.998f * glparamstate.clearz) + 0.001f;
    if (mask & GL_DEPTH_BUFFER_BIT)
        GX_SetZMode(GX_TRUE, GX_ALWAYS, GX_TRUE);
    else
        GX_SetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);

    if (mask & GL_COLOR_BUFFER_BIT)
        GX_SetColorUpdate(GX_TRUE);
    else
        GX_SetColorUpdate(GX_TRUE);

    GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    GX_SetCullMode(GX_CULL_NONE);

    static float modl[3][4];
    modl[0][0] = 1.0f;
    modl[0][1] = 0.0f;
    modl[0][2] = 0.0f;
    modl[0][3] = 0.0f;
    modl[1][0] = 0.0f;
    modl[1][1] = 1.0f;
    modl[1][2] = 0.0f;
    modl[1][3] = 0.0f;
    modl[2][0] = 0.0f;
    modl[2][1] = 0.0f;
    modl[2][2] = 1.0f;
    modl[2][3] = 0.0f;
    GX_LoadPosMtxImm(modl, GX_PNMTX0);
    GX_SetCurrentMtx(GX_PNMTX0);

    Mtx44 proj;
    guOrtho(proj, -1, 1, -1, 1, -1, 1);
    GX_LoadProjectionMtx(proj, GX_ORTHOGRAPHIC);

    GX_SetNumChans(1);
    GX_SetNumTevStages(1);
    GX_SetNumTexGens(0);

    GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);

    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_InvVtxCache();

    if (glparamstate.fog.enabled) {
        /* Disable fog while clearing */
        GX_SetFog(GX_FOG_NONE, 0.0, 0.0, 0.0, 0.0, glparamstate.clear_color);
    }

    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(-1, -1, -depth);
    GX_Color4u8(glparamstate.clear_color.r, glparamstate.clear_color.g, glparamstate.clear_color.b, glparamstate.clear_color.a);
    GX_Position3f32(1, -1, -depth);
    GX_Color4u8(glparamstate.clear_color.r, glparamstate.clear_color.g, glparamstate.clear_color.b, glparamstate.clear_color.a);
    GX_Position3f32(1, 1, -depth);
    GX_Color4u8(glparamstate.clear_color.r, glparamstate.clear_color.g, glparamstate.clear_color.b, glparamstate.clear_color.a);
    GX_Position3f32(-1, 1, -depth);
    GX_Color4u8(glparamstate.clear_color.r, glparamstate.clear_color.g, glparamstate.clear_color.b, glparamstate.clear_color.a);
    GX_End();

    glparamstate.dirty.all = ~0;
}

void glDepthFunc(GLenum func)
{
    switch (func) {
    case GL_NEVER:
        glparamstate.zfunc = GX_NEVER;
        break;
    case GL_LESS:
        glparamstate.zfunc = GX_LESS;
        break;
    case GL_EQUAL:
        glparamstate.zfunc = GX_EQUAL;
        break;
    case GL_LEQUAL:
        glparamstate.zfunc = GX_LEQUAL;
        break;
    case GL_GREATER:
        glparamstate.zfunc = GX_GREATER;
        break;
    case GL_NOTEQUAL:
        glparamstate.zfunc = GX_NEQUAL;
        break;
    case GL_GEQUAL:
        glparamstate.zfunc = GX_GEQUAL;
        break;
    case GL_ALWAYS:
        glparamstate.zfunc = GX_ALWAYS;
        break;
    default:
        break;
    }
    glparamstate.dirty.bits.dirty_z = 1;
}
void glDepthMask(GLboolean flag)
{
    if (flag == GL_FALSE || flag == 0)
        glparamstate.zwrite = GX_FALSE;
    else
        glparamstate.zwrite = GX_TRUE;
    glparamstate.dirty.bits.dirty_z = 1;
}

GLenum glGetError(void)
{
    GLenum error = glparamstate.error;
    glparamstate.error = GL_NO_ERROR;
    return error;
}

void glFlush() {} // All commands are sent immediately to draw, no queue, so pointless

// Waits for all the commands to be successfully executed
void glFinish()
{
    GX_DrawDone(); // Be careful, WaitDrawDone waits for the DD command, this sends AND waits for it
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
    switch (sfactor) {
    case GL_ZERO:
        glparamstate.srcblend = GX_BL_ZERO;
        break;
    case GL_ONE:
        glparamstate.srcblend = GX_BL_ONE;
        break;
    case GL_SRC_COLOR:
        glparamstate.srcblend = GX_BL_SRCCLR;
        break;
    case GL_ONE_MINUS_SRC_COLOR:
        glparamstate.srcblend = GX_BL_INVSRCCLR;
        break;
    case GL_DST_COLOR:
        glparamstate.srcblend = GX_BL_DSTCLR;
        break;
    case GL_ONE_MINUS_DST_COLOR:
        glparamstate.srcblend = GX_BL_INVDSTCLR;
        break;
    case GL_SRC_ALPHA:
        glparamstate.srcblend = GX_BL_SRCALPHA;
        break;
    case GL_ONE_MINUS_SRC_ALPHA:
        glparamstate.srcblend = GX_BL_INVSRCALPHA;
        break;
    case GL_DST_ALPHA:
        glparamstate.srcblend = GX_BL_DSTALPHA;
        break;
    case GL_ONE_MINUS_DST_ALPHA:
        glparamstate.srcblend = GX_BL_INVDSTALPHA;
        break;
    case GL_CONSTANT_COLOR:
    case GL_ONE_MINUS_CONSTANT_COLOR:
    case GL_CONSTANT_ALPHA:
    case GL_ONE_MINUS_CONSTANT_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
        break; // Not supported
    }

    switch (dfactor) {
    case GL_ZERO:
        glparamstate.dstblend = GX_BL_ZERO;
        break;
    case GL_ONE:
        glparamstate.dstblend = GX_BL_ONE;
        break;
    case GL_SRC_COLOR:
        glparamstate.dstblend = GX_BL_SRCCLR;
        break;
    case GL_ONE_MINUS_SRC_COLOR:
        glparamstate.dstblend = GX_BL_INVSRCCLR;
        break;
    case GL_DST_COLOR:
        glparamstate.dstblend = GX_BL_DSTCLR;
        break;
    case GL_ONE_MINUS_DST_COLOR:
        glparamstate.dstblend = GX_BL_INVDSTCLR;
        break;
    case GL_SRC_ALPHA:
        glparamstate.dstblend = GX_BL_SRCALPHA;
        break;
    case GL_ONE_MINUS_SRC_ALPHA:
        glparamstate.dstblend = GX_BL_INVSRCALPHA;
        break;
    case GL_DST_ALPHA:
        glparamstate.dstblend = GX_BL_DSTALPHA;
        break;
    case GL_ONE_MINUS_DST_ALPHA:
        glparamstate.dstblend = GX_BL_INVDSTALPHA;
        break;
    case GL_CONSTANT_COLOR:
    case GL_ONE_MINUS_CONSTANT_COLOR:
    case GL_CONSTANT_ALPHA:
    case GL_ONE_MINUS_CONSTANT_ALPHA:
    case GL_SRC_ALPHA_SATURATE:
        break; // Not supported
    }

    glparamstate.dirty.bits.dirty_blend = 1;
}

void glLineWidth(GLfloat width)
{
    GX_SetLineWidth((unsigned int)(width * 16), GX_TO_ZERO);
}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    /* For the time being, all the parameters we support take integer values */
    glTexEnvi(target, pname, param);
}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
{
    switch (pname) {
    case GL_TEXTURE_ENV_MODE:
        glparamstate.texture_env_mode = param;
        break;
    }
}

// If bytes per pixel is decimal (0,5 0,25 ...) we encode the number
// as the divisor in a negative way
static int calc_memory(int w, int h, int bytespp)
{
    if (bytespp > 0) {
        return w * h * bytespp;
    } else {
        return w * h / (-bytespp);
    }
}
// Returns the number of bytes required to store a texture with all its bitmaps
static int calc_tex_size(int w, int h, int bytespp)
{
    int size = 0;
    while (w > 1 || h > 1) {
        int mipsize = calc_memory(w, h, bytespp);
        if ((mipsize % 32) != 0)
            mipsize += (32 - (mipsize % 32)); // Alignment
        size += mipsize;
        if (w != 1)
            w = w / 2;
        if (h != 1)
            h = h / 2;
    }
    return size;
}
// Returns the number of bytes required to store a texture with all its bitmaps
static int calc_original_size(int level, int s, GLint internalFormat)
{
    while (level > 0) {
        s = 2 * s;
        level--;
    }

    // update by xjsxjs197
    s = (s + 3) & ~(unsigned int)3;

    return s;
}
// Given w,h,level,and bpp, returns the offset to the mipmap at level "level"
static int calc_mipmap_offset(int level, int w, int h, int b)
{
    int size = 0;
    while (level > 0) {
        int mipsize = calc_memory(w, h, b);
        if ((mipsize % 32) != 0)
            mipsize += (32 - (mipsize % 32)); // Alignment
        size += mipsize;
        if (w != 1)
            w = w / 2;
        if (h != 1)
            h = h / 2;
        level--;
    }
    return size;
}

// Create a Blank Texture
void glInitRGBATextures( GLsizei width, GLsizei height )
{
    //GX_WaitDrawDone();
    //GX_Flush();
    gltexture_ *currtex = &texture_list[glparamstate.glcurtex];

    int wi = width; //(width + 3) & ~(unsigned int)3;
    int he = height; //(height + 3) & ~(unsigned int)3;

    if (currtex->data != 0)
        _mem2_free(currtex->data);
    if (currtex->semiTransData != 0)
    {
        _mem2_free(currtex->semiTransData);
        currtex->semiTransData = 0;
    }

    int required_size = wi * he * 2;
    int tex_size_rnd = ROUND_32B(required_size);
    currtex->data = _mem2_memalign(32, tex_size_rnd);
    memset(currtex->data, 0, tex_size_rnd);
    DCFlushRange(currtex->data, tex_size_rnd);

    currtex->w = wi;
    currtex->h = he;

    GX_InitTexObj(&currtex->texobj, currtex->data,
                  currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
    if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
    {
        GX_InitTexObjFilterMode(&currtex->texobj, GX_NEAR, GX_NEAR);
    }
    // For Non transparent colors in transparent mode
//    GX_InitTexObj(&currtex->semiTransTexobj, currtex->semiTransData,
//                  currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
    //GX_InitTexObjFilterMode(&currtex->texobj, GX_LINEAR, GX_LINEAR);
}

#define RESY_MAX 512	//Vmem height
#define GXRESX_MAX 1366	//1024 * 1.33 for ARGB
#define MOVIE_BUF_SIZE (GXRESX_MAX*RESY_MAX*2)
#define W_BLOCK(w) (((w + 3) & ~(unsigned int)3) >> 2)

extern unsigned char GXtexture[MOVIE_BUF_SIZE];
static unsigned char *movieTexPtr;
static int movieUsedSize = 0;
static unsigned char semiTransBuf[256 * 256 * 2];

// 4b texel scrambling, opengx conversion: src(argb) -> dst(ar...gb)
// for movie
static inline void _ogx_scramble_4b(unsigned char *src, void *dst,
                      const unsigned int width, const unsigned int height)
{
    unsigned int block;
    unsigned int i;
    unsigned char c;
    unsigned char argb;
    unsigned char *p = (unsigned char *)dst;
    int tmp1, tmp2, tmp3;

    for (block = 0; block < height; block += 4) {
        for (i = 0; i < width; i += 4) {
            for (c = 0; c < 4; c++) {
                tmp1 = (block + c);
                tmp2 = tmp1 * width;
                for (argb = 0; argb < 4; argb++) {
                    tmp3 = (i + argb);
                    if (tmp3 >= width || tmp1 >= height)
                    {
                        *(unsigned short*)p = 0;
                        //*(unsigned short*)(p + 32) = 0;
                    }
                    else
                    {
                        //*(unsigned short*)p = *(unsigned short*)(src + ((tmp3 + tmp2) << 2));             // AR
                        //*(unsigned short*)(p + 32) = *(unsigned short*)(src + ((tmp3 + tmp2) << 2) + 2);  // GB
                        *(unsigned short*)p = *(unsigned short*)(src + ((tmp3 + tmp2) << 2) + 2);  // BGR
                    }
                    p += 2;
                }
            }
            //p += 32;
        }
    }
}

// The position happens to be the integer position of the Block
static inline int _ogx_scramble_4b_sub(unsigned char *src, void *dst, void *semiTransDst, unsigned short semiTransFlg,
                      const unsigned int width, const unsigned int height, const unsigned int oldWidth)
{
    unsigned int he;
    unsigned int wi;
    unsigned char blockHe;
    unsigned char blockWi;
    unsigned char *p = (unsigned char *)dst;
    unsigned char *semiTransP = (unsigned char *)semiTransDst;
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
                        //*(unsigned short*)p = 0;
                    }
                    else
                    {
                        // RGB5A3(Actually, it's the original BGR555 of PSX, Can be efficiently converted to Wii RGB5A3)
                        tmpPixel = *(unsigned short*)(src + ((wi + blockWi) + ((he + blockHe) * width)) * 4 + 2);
                        if (tmpPixel == 0)
                        {
                            *(unsigned short*)(semiTransP) = 0;
                            *(unsigned short*)(p) = 0;
                        }
                        else if (semiTransFlg && (tmpPixel & 0x8000) == 0)
                        {
                            *(unsigned short*)(semiTransP) = tmpPixel | 0x8000;
                            *(unsigned short*)(p) = 0;
                            textureType |= TXT_TYPE_1;
                        }
                        else
                        {
                            *(unsigned short*)(semiTransP) = 0;
                            *(unsigned short*)(p) = tmpPixel | 0x8000;
                            textureType |= TXT_TYPE_2;
                        }
                    }
                    p += 2;
                    semiTransP += 2;
                }
            }
        }
        p += (oldWidthBlock - newWidthBlock) * 32;
        semiTransP += (oldWidthBlock - newWidthBlock) * 32;
    }

    return textureType;
}

// 4b texel scrambling, opengx conversion: src(4 bytes bgr555) -> dst(2 bytes bgr5a3)
static inline int _ogx_scramble_4b_5a3(unsigned char *src, void *dst, unsigned short semiTransFlg,
                      const unsigned int width, const unsigned int height)
{
    unsigned int block;
    unsigned int i;
    unsigned char c;
    unsigned char argb;
    unsigned char *p = (unsigned char *)dst;
    unsigned char *semiTransP = semiTransBuf;
    unsigned short tmpPixel;
    int textureType = 0;

    for (block = 0; block < height; block += 4) {
        for (i = 0; i < width; i += 4) {
            for (c = 0; c < 4; c++) {
                for (argb = 0; argb < 4; argb++) {
                    if ((i + argb) >= width || (block + c) >= height)
                    {
                        *(unsigned short*)p = 0;
                        *(unsigned short*)semiTransP = 0;
                    }
                    else
                    {
                        tmpPixel = *(unsigned short*)(src + ((i + argb) + ((block + c) * width)) * 4 + 2);
                        if (tmpPixel == 0)
                        {
                            *(unsigned short*)semiTransP = 0;
                            *(unsigned short*)p = 0;
                        }
                        else if (semiTransFlg && (tmpPixel & 0x8000) == 0)
                        {
                            *(unsigned short*)(semiTransP) = tmpPixel | 0x8000;
                            *(unsigned short*)p = 0;
                            textureType |= TXT_TYPE_1;
                        }
                        else
                        {
                            *(unsigned short*)semiTransP = 0;
                            *(unsigned short*)(p) = tmpPixel | 0x8000;
                            textureType |= TXT_TYPE_2;
                        }
                    }
                    p += 2;
                    semiTransP += 2;
                }
            }
        }
    }

    return textureType;
}

void glResetMovieTexPtr( void )
{
    movieUsedSize = 0;
    movieTexPtr = (unsigned char *)GXtexture;
}

// Create a Movie Texture
int glInitMovieTextures( GLsizei width, GLsizei height, void * texData )
{
    int textureType = 0;
    GX_WaitDrawDone();
    //GX_SetDrawDone();
    //GX_Flush();
    //GX_InvalidateTexAll();
    gltexture_ *currtex = &texture_list[glparamstate.glcurtex];

    int wi = width; //(width + 3) & ~(unsigned int)3;
    int he = height; //(height + 3) & ~(unsigned int)3;

    int required_size = wi * he * 4;
    int tex_size_rnd = ROUND_32B(required_size);
    if ((movieUsedSize + tex_size_rnd) > MOVIE_BUF_SIZE)
    {
        glResetMovieTexPtr();
    }
    currtex->data = (unsigned char *)movieTexPtr;
    currtex->semiTransData = currtex->data;
    if (!glparamstate.RGB24)
    {
        currtex->semiTransData = currtex->data + (tex_size_rnd / 2);
    }
    memset(currtex->data, 0, tex_size_rnd);
    movieTexPtr += tex_size_rnd;
    movieUsedSize += tex_size_rnd;

    currtex->w = wi;
    currtex->h = he;

    if (glparamstate.RGB24)
    {
        textureType = TXT_TYPE_2;
        _ogx_scramble_4b((unsigned char *)texData, currtex->data, width, height);
//        GX_InitTexObj(&currtex->texobj, currtex->data,
//                      currtex->w, currtex->h, GX_TF_RGBA8, currtex->wraps, currtex->wrapt, GX_FALSE);
        GX_InitTexObj(&currtex->texobj, currtex->data,
                      currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
        if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
        {
            GX_InitTexObjFilterMode(&currtex->texobj, GX_NEAR, GX_NEAR);
        }
    }
    else
    {
        textureType = _ogx_scramble_4b_5a3((unsigned char *)texData, currtex->data, glparamstate.blendenabled, width, height);
        GX_InitTexObj(&currtex->texobj, currtex->data,
                      currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
        if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
        {
            GX_InitTexObjFilterMode(&currtex->texobj, GX_NEAR, GX_NEAR);
        }
        // For Non transparent colors in transparent mode
        if (textureType & TXT_TYPE_1)
        {
            memcpy(currtex->semiTransData, semiTransBuf, currtex->w * currtex->h * 2);
            GX_InitTexObj(&currtex->semiTransTexobj, currtex->semiTransData,
                          currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
            if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
            {
                GX_InitTexObjFilterMode(&currtex->semiTransTexobj, GX_NEAR, GX_NEAR);
            }
        }
    }
    DCFlushRange(currtex->data, tex_size_rnd);

    return textureType;
}

// Update a Texture
int glTexSubImage2D(GLenum target, GLint level,
                   GLint xoffset, GLint yoffset,
                   GLsizei width, GLsizei height,
                   GLenum format, GLenum type,
                   const GLvoid *data )
{
    int textureType = 0;
    GX_WaitDrawDone();
    //GX_Flush();
    //GX_InvalidateTexAll();

    gltexture_ *currtex = &texture_list[glparamstate.glcurtex];
    unsigned char * semiTransBufPtr = (currtex->semiTransData == 0 ? semiTransBuf : currtex->semiTransData);

    if ((xoffset & 3) == 0 && (yoffset & 3) == 0)
    {
        // The position happens to be the integer position of the Block
        int startOffset = ((yoffset >> 2) * W_BLOCK(currtex->w) + (xoffset >> 2)) * 32;
        textureType = _ogx_scramble_4b_sub((unsigned char *)data, currtex->data + startOffset, semiTransBufPtr + startOffset, glparamstate.blendenabled, width, height, currtex->w);
        DCFlushRange(currtex->data , currtex->w * currtex->h * 2);
    }
    else
    {
        // It is not the position of the integer Block, so we need to write data to different blocks
        unsigned char * dstBlock = currtex->data;
        unsigned char * semiTransDstBlock = semiTransBufPtr;
        unsigned char * src = (unsigned char *)data;
        int totalWi = min(xoffset + width, currtex->w);
        int totalHe = min(yoffset + height, currtex->h);
        int oldXoffset = xoffset;
        int oldYoffset = yoffset;
        unsigned short tmpPixel;

        int y, x;
        unsigned char copyHe;
        unsigned char copyWi;
        int tmp;
        for (y = 0; y < height;)
        {
            tmp = yoffset + (4 - (yoffset & 3));
            if (tmp <= totalHe)
            {
                copyHe = tmp - yoffset;
            }
            else
            {
                copyHe = totalHe - yoffset;
            }

            for (x = 0; x < width;)
            {
                tmp = xoffset + (4 - (xoffset & 3));
                if (tmp <= totalWi)
                {
                    copyWi = tmp - xoffset;
                }
                else
                {
                    copyWi = totalWi - xoffset;
                }

                unsigned char he;
                unsigned char wi;
                unsigned char blockHe;
                unsigned char blockWi;
                src = (unsigned char *)data + (y * width + x) * 4;
                dstBlock = currtex->data + ((yoffset >> 2) * W_BLOCK(currtex->w) + (xoffset >> 2)) * 32;
                semiTransDstBlock = semiTransBufPtr + ((yoffset >> 2) * W_BLOCK(currtex->w) + (xoffset >> 2)) * 32;
                for (he = 0, blockHe = (yoffset & 3); he < copyHe; he++, blockHe++)
                {
                    for (wi = 0, blockWi = (xoffset & 3); wi < copyWi; wi++, blockWi++)
                    {
                        tmpPixel = *(unsigned short*)(src + (he * width + wi) * 4 + 2); // RGB5A3
                        if (tmpPixel == 0)
                        {
                            *(unsigned short*)(semiTransDstBlock + (blockHe * 4 + blockWi) * 2) = 0;
                            *(unsigned short*)(dstBlock + (blockHe * 4 + blockWi) * 2) = 0;
                        }
                        else if (glparamstate.blendenabled && (tmpPixel & 0x8000) == 0)
                        {
                            *(unsigned short*)(semiTransDstBlock + (blockHe * 4 + blockWi) * 2) = tmpPixel | 0x8000;
                            *(unsigned short*)(dstBlock + (blockHe * 4 + blockWi) * 2) = 0;
                            textureType |= TXT_TYPE_1;
                        }
                        else
                        {
                            *(unsigned short*)(semiTransDstBlock + (blockHe * 4 + blockWi) * 2) = 0;
                            *(unsigned short*)(dstBlock + (blockHe * 4 + blockWi) * 2) = tmpPixel | 0x8000;
                            textureType |= TXT_TYPE_2;
                        }
                    }
                }

                x += copyWi;
                xoffset += copyWi;

            }
            y += copyHe;
            yoffset += copyHe;
            xoffset = oldXoffset;
        }

        DCFlushRange(currtex->data , currtex->w * currtex->h * 2);
    }

    if (textureType & TXT_TYPE_1)
    {
        if (currtex->semiTransData == 0)
        {
            currtex->semiTransData = _mem2_memalign(32, currtex->w * currtex->h * 2);
            GX_InitTexObj(&currtex->semiTransTexobj, currtex->semiTransData,
                        currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
            if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
            {
                GX_InitTexObjFilterMode(&currtex->semiTransTexobj, GX_NEAR, GX_NEAR);
            }
            memcpy(currtex->semiTransData, semiTransBuf, currtex->w * currtex->h * 2);
        }
        DCFlushRange(currtex->semiTransData , currtex->w * currtex->h * 2);
    }

    return textureType;
}

// Create a Image Texture
int glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const GLvoid *data)
{

    // Initial checks
    if (texture_list[glparamstate.glcurtex].used == 0)
        return 0;
    if (target != GL_TEXTURE_2D)
        return 0; // FIXME Implement non 2D textures

    int textureType = 0;
    //GX_DrawDone(); // Very ugly, we should have a list of used textures and only wait if we are using the curr tex.
                   // This way we are sure that we are not modifying a texture which is being drawn
    GX_WaitDrawDone();
    //GX_Flush();
    //GX_InvalidateTexAll();

    gltexture_ *currtex = &texture_list[glparamstate.glcurtex];

    // RGB5A3 format fixed
    int wi = width; //calc_original_size(level, width, internalFormat);
    int he = height; //calc_original_size(level, height, internalFormat);

    // Check if the texture has changed its geometry and proceed to delete it
    // If the specified level is zero, create a onelevel texture to save memory
    if (wi != currtex->w || he != currtex->h) {
        if (currtex->data != 0)
        {
            _mem2_free(currtex->data);
        }
        if (currtex->semiTransData != 0)
        {
            _mem2_free(currtex->semiTransData);
            currtex->semiTransData = 0;
        }

        int required_size = wi * he * 2;
        int tex_size_rnd = ROUND_32B(required_size);
        currtex->data = _mem2_memalign(32, tex_size_rnd);
        memset(currtex->data, 0, tex_size_rnd);
    }

    currtex->w = wi;
    currtex->h = he;
    currtex->bytespp = 2;

    textureType = _ogx_scramble_4b_5a3((unsigned char *)data, currtex->data, glparamstate.blendenabled, width, height);
    DCFlushRange(currtex->data, currtex->w * currtex->h * 2);

    // Slow but necessary! The new textures may be in the same region of some old cached textures
    //GX_InvalidateTexAll();

    GX_InitTexObj(&currtex->texobj, currtex->data,
                currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
    if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
    {
        GX_InitTexObjFilterMode(&currtex->texobj, GX_NEAR, GX_NEAR);
    }
    // For Non transparent colors in transparent mode
    if (textureType & TXT_TYPE_1)
    {
        if (currtex->semiTransData == 0)
        {
            currtex->semiTransData = _mem2_memalign(32, currtex->w * currtex->h * 2);
            memcpy(currtex->semiTransData, semiTransBuf, currtex->w * currtex->h * 2);

            GX_InitTexObj(&currtex->semiTransTexobj, currtex->semiTransData,
                        currtex->w, currtex->h, GX_TF_RGB5A3, currtex->wraps, currtex->wrapt, GX_FALSE);
            if (originalMode == ORIGINALMODE_ENABLE || bilinearFilter == BILINEARFILTER_DISABLE)
            {
                GX_InitTexObjFilterMode(&currtex->semiTransTexobj, GX_NEAR, GX_NEAR);
            }
        }
    }
    //GX_InitTexObjFilterMode(&currtex->texobj, GX_LINEAR, GX_LINEAR);
    //GX_InitTexObjLOD(&currtex->texobj, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN, currtex->minlevel, currtex->maxlevel, 0, GX_ENABLE, GX_ENABLE, GX_ANISO_1);

    return textureType;
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    if ((red | green | blue | alpha) != 0)
        GX_SetColorUpdate(GX_TRUE);
    else
        GX_SetColorUpdate(GX_FALSE);
}

/*

  Render setup code.

*/

void glDisableClientState(GLenum cap)
{
    switch (cap) {
    case GL_COLOR_ARRAY:
        glparamstate.color_enabled = 0;
        break;
    case GL_INDEX_ARRAY:
        glparamstate.index_enabled = 0;
        break;
    case GL_NORMAL_ARRAY:
        glparamstate.normal_enabled = 0;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        glparamstate.texcoord_enabled = 0;
        break;
    case GL_VERTEX_ARRAY:
        glparamstate.vertex_enabled = 0;
        break;
    case GL_EDGE_FLAG_ARRAY:
    case GL_FOG_COORD_ARRAY:
    case GL_SECONDARY_COLOR_ARRAY:
        return;
    }
}
void glEnableClientState(GLenum cap)
{
    switch (cap) {
    case GL_COLOR_ARRAY:
        glparamstate.color_enabled = 1;
        break;
    case GL_INDEX_ARRAY:
        glparamstate.index_enabled = 1;
        break;
    case GL_NORMAL_ARRAY:
        glparamstate.normal_enabled = 1;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        glparamstate.texcoord_enabled = 1;
        break;
    case GL_VERTEX_ARRAY:
        glparamstate.vertex_enabled = 1;
        break;
    case GL_EDGE_FLAG_ARRAY:
    case GL_FOG_COORD_ARRAY:
    case GL_SECONDARY_COLOR_ARRAY:
        return;
    }
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glparamstate.vertex_array = (float *)pointer;
    glparamstate.vertex_stride = stride;
    if (stride == 0)
        glparamstate.vertex_stride = size;
}
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glparamstate.normal_array = (float *)pointer;
    glparamstate.normal_stride = stride;
    if (stride == 0)
        glparamstate.normal_stride = 3;
}

void glColorPointer(GLint size, GLenum type,
                    GLsizei stride, const GLvoid *pointer)
{
    glparamstate.color_array = (unsigned char *)pointer;
    glparamstate.color_stride = stride;
    if (stride == 0)
        glparamstate.color_stride = size;
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
{
    glparamstate.texcoord_array = (float *)pointer;
    glparamstate.texcoord_stride = stride;
    if (stride == 0)
        glparamstate.texcoord_stride = size;
}

void glInterleavedArrays(GLenum format, GLsizei stride, const GLvoid *pointer)
{
    glparamstate.vertex_array = (float *)pointer;
    glparamstate.normal_array = (float *)pointer;
    glparamstate.texcoord_array = (float *)pointer;
    glparamstate.color_array = (unsigned char *)pointer;

    glparamstate.index_enabled = 0;
    glparamstate.normal_enabled = 0;
    glparamstate.texcoord_enabled = 0;
    glparamstate.vertex_enabled = 0;
    glparamstate.color_enabled = 0;

    int cstride = 0;
    switch (format) {
    case GL_V2F:
        glparamstate.vertex_enabled = 1;
        cstride = 2;
        break;
    case GL_V3F:
        glparamstate.vertex_enabled = 1;
        cstride = 3;
        break;
    case GL_N3F_V3F:
        glparamstate.vertex_enabled = 1;
        glparamstate.normal_enabled = 1;
        cstride = 6;
        glparamstate.vertex_array += 3;
        break;
    case GL_T2F_V3F:
        glparamstate.vertex_enabled = 1;
        glparamstate.texcoord_enabled = 1;
        cstride = 5;
        glparamstate.vertex_array += 2;
        break;
    case GL_T2F_N3F_V3F:
        glparamstate.vertex_enabled = 1;
        glparamstate.normal_enabled = 1;
        glparamstate.texcoord_enabled = 1;
        cstride = 8;

        glparamstate.vertex_array += 5;
        glparamstate.normal_array += 2;
        break;

    case GL_C4F_N3F_V3F:
        glparamstate.vertex_enabled = 1;
        glparamstate.normal_enabled = 1;
        glparamstate.color_enabled = 1;
        cstride = 10;

        glparamstate.vertex_array += 7;
        glparamstate.normal_array += 4;
        break;
    case GL_C3F_V3F:
        glparamstate.vertex_enabled = 1;
        glparamstate.color_enabled = 1;
        cstride = 6;

        glparamstate.vertex_array += 3;
        break;
    case GL_T2F_C3F_V3F:
        glparamstate.vertex_enabled = 1;
        glparamstate.color_enabled = 1;
        glparamstate.texcoord_enabled = 1;
        cstride = 8;

        glparamstate.vertex_array += 5;
        glparamstate.color_array += 2;
        break;
    case GL_T2F_C4F_N3F_V3F: // Complete type
        glparamstate.vertex_enabled = 1;
        glparamstate.normal_enabled = 1;
        glparamstate.color_enabled = 1;
        glparamstate.texcoord_enabled = 1;
        cstride = 12;

        glparamstate.vertex_array += 9;
        glparamstate.normal_array += 6;
        glparamstate.color_array += 2;
        break;

    case GL_C4UB_V2F:
    case GL_C4UB_V3F:
    case GL_T2F_C4UB_V3F:
    case GL_T4F_C4F_N3F_V4F:
    case GL_T4F_V4F:
        // TODO: Implement T4F! And UB color!
        return;
    }

    // If the stride is 0, they're tighly packed
    if (stride != 0)
        cstride = stride;

    glparamstate.color_stride = cstride;
    glparamstate.normal_stride = cstride;
    glparamstate.vertex_stride = cstride;
    glparamstate.texcoord_stride = cstride;
}

/*

  Render code. All the renderer calls should end calling this one.

*/

/*****************************************************

        LIGHTING IMPLEMENTATION EXPLAINED

   GX differs in some aspects from OGL lighting.
    - It shares the same material for ambient
      and diffuse components
    - Lights can be specular or diffuse, not both
    - The ambient component is NOT attenuated by
      distance

   GX hardware can do lights with:
    - Distance based attenuation
    - Angle based attenuation (for diffuse lights)

   We simulate each light this way:

    - Ambient: Using distance based attenuation, disabling
      angle-based attenuation (GX_DF_NONE).
    - Diffuse: Using distance based attenuation, enabling
      angle-based attenuation in clamp mode (GX_DF_CLAMP)
    - Specular: Specular based attenuation (GX_AF_SPEC)

   As each channel is configured for all the TEV stages
   we CANNOT emulate the three types of light at once.
   So we emulate two types only.

   For unlit scenes the setup is:
     - TEV 0: Modulate vertex color with texture
              Speed hack: use constant register
              If no tex, just pass color
   For ambient+diffuse lights:
     - TEV 0: Pass RAS0 color with material color
          set to vertex color (to modulate vert color).
          Set the ambient value for this channel to 0.
         Speed hack: Use material register for constant
          color
     - TEV 1: Sum RAS1 color with material color
          set to vertex color (to modulate vert color)
          to the previous value. Also set the ambient
          value to the global ambient value.
         Speed hack: Use material register for constant
          color
     - TEV 2: If texture is enabled multiply the texture
          rasterized color with the previous value.
      The result is:

     Color = TexC * (VertColor*AmbientLightColor*Atten
      + VertColor*DiffuseLightColor*Atten*DifAtten)

     As we use the material register for vertex color
     the material colors will be multiplied with the
     light color and uploaded as light color.

     We'll be using 0-3 lights for ambient and 4-7 lights
     for diffuse

******************************************************/

static inline bool is_black(const float *color)
{
    return color[0] == 0.0f && color[1] == 0.0f && color[2] == 0.0f;
}

static void allocate_lights()
{
    /* For the time being, just allocate the lights using a first come, first
     * served algorithm.
     * TODO: take the light impact into account: privilege stronger lights, and
     * light types in this order (probably): directional, ambient, diffuse,
     * specular. */
    char lights_needed = 0;
    bool global_ambient_off = is_black(glparamstate.lighting.globalambient);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (!glparamstate.lighting.lights[i].enabled)
            continue;

        if (!is_black(glparamstate.lighting.lights[i].ambient_color) &&
            !global_ambient_off) {
            /* This ambient light is needed, allocate it */
            char gx_light = lights_needed++;
            glparamstate.lighting.lights[i].gx_ambient =
                gx_light < MAX_GX_LIGHTS ? gx_light : -1;
        } else {
            glparamstate.lighting.lights[i].gx_ambient = -1;
        }

        if (!is_black(glparamstate.lighting.lights[i].diffuse_color)) {
            /* This diffuse light is needed, allocate it */
            char gx_light = lights_needed++;
            glparamstate.lighting.lights[i].gx_diffuse =
                gx_light < MAX_GX_LIGHTS ? gx_light : -1;
        } else {
            glparamstate.lighting.lights[i].gx_diffuse = -1;
        }

        /* GX support specular light only for directional light sources. For
         * this reason we enable the specular light only if the "w" component
         * of the position is 0. */
        if (!is_black(glparamstate.lighting.lights[i].specular_color) &&
            !is_black(glparamstate.lighting.matspecular) &&
            glparamstate.lighting.matshininess > 0.0 &&
            glparamstate.lighting.lights[i].position[3] == 0.0f) {
            /* This specular light is needed, allocate it */
            char gx_light = lights_needed++;
            glparamstate.lighting.lights[i].gx_specular =
                gx_light < MAX_GX_LIGHTS ? gx_light : -1;
        } else {
            glparamstate.lighting.lights[i].gx_specular = -1;
        }
    }

    if (lights_needed > MAX_GX_LIGHTS) {
        warning("Excluded %d lights since max is 8", lights_needed - MAX_GX_LIGHTS);
    }
}

static LightMasks prepare_lighting()
{
    LightMasks masks = { 0, 0 };
    int i;

    allocate_lights();

    for (i = 0; i < MAX_LIGHTS; i++) {
        if (!glparamstate.lighting.lights[i].enabled)
            continue;

        int8_t gx_ambient_idx = glparamstate.lighting.lights[i].gx_ambient;
        int8_t gx_diffuse_idx = glparamstate.lighting.lights[i].gx_diffuse;
        int8_t gx_specular_idx = glparamstate.lighting.lights[i].gx_specular;
        GXLightObj *gx_ambient = gx_ambient_idx >= 0 ?
            &glparamstate.lighting.lightobj[gx_ambient_idx] : NULL;
        GXLightObj *gx_diffuse = gx_diffuse_idx >= 0 ?
            &glparamstate.lighting.lightobj[gx_diffuse_idx] : NULL;
        GXLightObj *gx_specular = gx_specular_idx >= 0 ?
            &glparamstate.lighting.lightobj[gx_specular_idx] : NULL;

        if (gx_ambient) {
            // Multiply the light color by the material color and set as light color
            GXColor amb_col = gxcol_new_fv(glparamstate.lighting.lights[i].ambient_color);
            GX_InitLightColor(gx_ambient, amb_col);
            GX_InitLightPosv(gx_ambient, &glparamstate.lighting.lights[i].position[0]);
        }

        if (gx_diffuse) {
            GXColor diff_col = gxcol_new_fv(glparamstate.lighting.lights[i].diffuse_color);
            GX_InitLightColor(gx_diffuse, diff_col);
            GX_InitLightPosv(gx_diffuse, &glparamstate.lighting.lights[i].position[0]);
        }

        // FIXME: Need to consider spotlights
        if (glparamstate.lighting.lights[i].position[3] == 0) {
            // Directional light, it's a point light very far without attenuation
            if (gx_ambient) {
                GX_InitLightAttn(gx_ambient, 1, 0, 0, 1, 0, 0);
            }
            if (gx_diffuse) {
                GX_InitLightAttn(gx_diffuse, 1, 0, 0, 1, 0, 0);
            }
            if (gx_specular) {
                GXColor spec_col = gxcol_new_fv(glparamstate.lighting.lights[i].specular_color);

                /* We need to compute the normals of the direction */
                float normal[3] = {
                    -glparamstate.lighting.lights[i].position[0],
                    -glparamstate.lighting.lights[i].position[1],
                    -glparamstate.lighting.lights[i].position[2],
                };
                normalize(normal);
                GX_InitSpecularDirv(gx_specular, normal);
                GX_InitLightShininess(gx_specular, glparamstate.lighting.matshininess);
                GX_InitLightColor(gx_specular, spec_col);
            }
        } else {
            // Point light
            if (gx_ambient) {
                GX_InitLightAttn(gx_ambient, 1, 0, 0,
                                 glparamstate.lighting.lights[i].atten[0],
                                 glparamstate.lighting.lights[i].atten[1],
                                 glparamstate.lighting.lights[i].atten[2]);
                GX_InitLightDir(gx_ambient, 0, -1, 0);
            }
            if (gx_diffuse) {
                GX_InitLightAttn(gx_diffuse, 1, 0, 0,
                                 glparamstate.lighting.lights[i].atten[0],
                                 glparamstate.lighting.lights[i].atten[1],
                                 glparamstate.lighting.lights[i].atten[2]);
                GX_InitLightDir(gx_diffuse, 0, -1, 0);
            }
        }

        if (gx_ambient) {
            GX_LoadLightObj(gx_ambient, 1 << gx_ambient_idx);
            masks.ambient_mask |= (1 << gx_ambient_idx);
        }
        if (gx_diffuse) {
            GX_LoadLightObj(gx_diffuse, 1 << gx_diffuse_idx);
            masks.diffuse_mask |= (1 << gx_diffuse_idx);
        }
        if (gx_specular) {
            GX_LoadLightObj(gx_specular, 1 << gx_specular_idx);
            masks.specular_mask |= (1 << gx_specular_idx);
        }
    }
    debug("Ambient mask 0x%02x, diffuse 0x%02x, specular 0x%02x",
          masks.ambient_mask, masks.diffuse_mask, masks.specular_mask);
    return masks;
}

static unsigned char draw_mode(GLenum mode)
{
    unsigned char gxmode;
    switch (mode) {
    case GL_POINTS:
        gxmode = GX_POINTS;
        break;
    case GL_LINE_LOOP:
    case GL_LINE_STRIP:
        gxmode = GX_LINESTRIP;
        break;
    case GL_LINES:
        gxmode = GX_LINES;
        break;
    case GL_TRIANGLE_STRIP:
    case GL_QUAD_STRIP:
        gxmode = GX_TRIANGLESTRIP;
        break;
    case GL_TRIANGLE_FAN:
        gxmode = GX_TRIANGLEFAN;
        break;
    case GL_TRIANGLES:
        gxmode = GX_TRIANGLES;
        break;
    case GL_QUADS:
        gxmode = GX_QUADS;
        break;

    case GL_POLYGON:
    default:
        return 0xff; // FIXME: Emulate these modes
    }
    return gxmode;
}

static short glDrawArraysFlg = 0;

static void setup_texture_stage(u8 stage, u8 raster_color, u8 raster_alpha,
                                u8 channel)
{
//    if (glparamstate.color_enabled)
//    {
//        // During texture and vertex processing, texture filtering should first be applied to discard useless pixels
//        // otherwise invalid vertex information will be displayed.
//        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_DISABLE, channel);
//        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, raster_color);
//        GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CC_ZERO, GX_CC_ZERO, raster_alpha);
//        GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
//        GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
//
//        GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
//        GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_CPREV, GX_CC_ZERO);
//        GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CA_TEXA);
//        GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_TRUE, GX_TEVPREV);
//        GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
//
//        doubleColor = 0;
//    }
//    else
    {
        if (noNeedMulConstColor)
        {
            GX_SetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        }
        else
        {
            GX_SetTevColorIn(stage, GX_CC_ZERO, raster_color, GX_CC_TEXC, GX_CC_ZERO);
        }
        GX_SetTevAlphaIn(stage, GX_CA_ZERO, raster_alpha, GX_CA_TEXA, GX_CA_ZERO);

        if (doubleColor)
        {
            GX_SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_TRUE, GX_TEVPREV);
            doubleColor = 0;
        }
        else
        {
            GX_SetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        }
        GX_SetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        short gxTexMap;
        if (glDrawArraysFlg == 0 && (texturyType & TXT_TYPE_1))
        {
            gxTexMap = GX_TEXMAP_SEMI;
        }
        else
        {
            switch (curTextureType)
            {
                case TEX_TYPE_MOVIE:
                    gxTexMap = GX_TEXMAP_MOV;
                    break;
                case TEX_TYPE_WIN:
                    gxTexMap = GX_TEXMAP_WIN;
                    break;
                case TEX_TYPE_SUB:
                    gxTexMap = GX_TEXMAP_SUB;
                    break;
            }
        }

        GX_SetTevOrder(stage, GX_TEXCOORD0, gxTexMap, channel);
    }

    // Set up the data for the TEXCOORD0 (use a identity matrix, TODO: allow user texture matrices)
    GX_SetNumTexGens(1);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
}

#define COL5TO8(a) (a | ((a >> 5) & 0x7))

static inline GXColor gxImmCol(unsigned char *colAdr, int texen)
{
    if (texen)
    {
//        if (glparamstate.blendenabled && glparamstate.globalTextABR == 4 && glDrawArraysFlg == 1)
//        {
//            // 0.25 * F
//            return (GXColor){ (colAdr[0]) >> 2, (colAdr[1]) >> 2, (colAdr[2]) >> 2, 255};
//        }
//        else
//        {
//            return (GXColor){ (colAdr[0]), (colAdr[1]), (colAdr[2]), 255};
//        }
        return (GXColor){ (colAdr[0]), (colAdr[1]), (colAdr[2]), (colAdr[3])};
    }

    return (GXColor){ COL5TO8(colAdr[0]), COL5TO8(colAdr[1]), COL5TO8(colAdr[2]), colAdr[3]};
}

static void setup_render_stages(int texen)
{
    // Unlit scene
    // TEV STAGE 0: Modulate the vertex color with the texture 0. Outputs to GX_TEVPREV
    // Optimization: If color_enabled is false (constant vertex color) use the constant color register
    // instead of using the rasterizer and emitting a color for each vertex

    // By default use rasterized data and put it a COLOR0A0
    unsigned char vertex_color_register = GX_CC_RASC;
    unsigned char vertex_alpha_register = GX_CA_RASA;
    unsigned char rasterized_color = GX_COLOR0A0;
    if (!glparamstate.color_enabled) { // No need for vertex color raster, it's constant
        // Use constant color
        vertex_color_register = GX_CC_KONST;
        vertex_alpha_register = GX_CA_KONST;
        // Select register 0 for color/alpha
        GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
        GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
        // Load the color (current GL color)
        GXColor ccol = gxImmCol(&glparamstate.imm_mode.c.col, texen);
        GX_SetTevKColor(GX_KCOLOR0, ccol);

        rasterized_color = GX_COLORNULL; // Disable vertex color rasterizer
    }

    GX_SetNumChans(1);
//    if (texen && glparamstate.color_enabled)
//    {
//        GX_SetNumTevStages(2);
//    }
//    else
    {
        GX_SetNumTevStages(1);
    }

    // Disable lighting and output vertex color to the rasterized color
    GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_VTX, 0, 0, 0);
    GX_SetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, 0, 0);

    if (texen) {
        // Select COLOR0A0 for the rasterizer, Texture 0 for texture rasterizer and TEXCOORD0 slot for tex coordinates
        setup_texture_stage(GX_TEVSTAGE0,
                            vertex_color_register, vertex_alpha_register,
                            rasterized_color);

    }
    else
    {
        // Display vertex information only.
        GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, vertex_color_register);
        GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, vertex_alpha_register);

        GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
        // Select COLOR0A0 for the rasterizer, Texture 0 for texture rasterizer and TEXCOORD0 slot for tex coordinates
        GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_DISABLE, rasterized_color);
        GX_SetNumTexGens(0);
    }
}

static int _ogx_apply_state()
{
    int texen = glparamstate.texcoord_enabled & glparamstate.texture_enabled;
    gltexture_ *currtex;
    if (texen)
    {
        currtex = &texture_list[glparamstate.glcurtex];
    }

    setup_render_stages(texen);

    // Set up the OGL state to GX state
    GX_SetZMode(GX_TRUE, glparamstate.zfunc, GX_TRUE);

//    // It appears that the GX_SetDstAlpha function is not functioning.
//    if (glparamstate.color_enabled && texen)
//    {
//        GX_SetAlphaUpdate(GX_TRUE);
//    }
//    else
//    {
//        GX_SetAlphaUpdate(GX_FALSE);
//        GX_SetDstAlpha(GX_ENABLE, 0xFF);
//    }

    #ifdef DISP_DEBUG
    sprintf(txtbuffer, "draw %d %d %d %d %d\r\n", glparamstate.blendenabled, texen, glparamstate.color_enabled, glparamstate.globalTextABR, glparamstate.glcurtex);
    writeLogFile(txtbuffer);
    #endif // DISP_DEBUG

    GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_GREATER, 0);
    if (glparamstate.blendenabled)
    {
        if (glparamstate.globalTextABR == 1)
        {
            if (texen)
            {
                if (glDrawArraysFlg == 0)
                {
                    if ((texturyType & TXT_TYPE_1))
                    {
                        // Non transparent colors in transparent mode
                        //if (loadTexFlg) GX_LoadTexObj(&currtex->semiTransTexobj, GX_TEXMAP0);
                        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
                    }
                    else
                    {
                        return 0;
                    }
                }
                else if (glDrawArraysFlg == 1)
                {
                    // transparent colors in transparent mode(0.5F + 0.5B)
                    if ((texturyType & TXT_TYPE_2))
                    {
                        //if (loadTexFlg) GX_LoadTexObj(&currtex->texobj, GX_TEXMAP0);
                        // 0.5B + 0.5F
                        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_SRCALPHA, GX_LO_CLEAR);
                    }
                    else
                    {
                        return 0;
                    }
                }
            }
            else
            {
                // transparent colors in transparent mode(0.5F + 0.5B)
                GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_SRCALPHA, GX_LO_CLEAR);
            }
        }
        else if (glparamstate.globalTextABR == 2 || glparamstate.globalTextABR == 4)
        {
            if (texen)
            {
                if (vramClearedFlg)
                {
                    // Which game was this fix intended for? I can't recall.
                    vramClearedFlg = 0;
                    return 0;
                }
                else
                {
                    if (glDrawArraysFlg == 0)
                    {
                        if (texturyType & TXT_TYPE_1)
                        {
                            //if (loadTexFlg) GX_LoadTexObj(&currtex->semiTransTexobj, GX_TEXMAP0);
                            // Non transparent colors in transparent mode
                            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
                        }
                        else
                        {
                            return 0;
                        }
                    }
                    else if (glDrawArraysFlg == 1)
                    {
                        if (texturyType & TXT_TYPE_2)
                        {
                            //if (loadTexFlg) GX_LoadTexObj(&currtex->texobj, GX_TEXMAP0);
                            // transparent colors in transparent mode(F + B)
                            // transparent colors in transparent mode(0.25F + B)
                            GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
                        }
                        else
                        {
                            return 0;
                        }
                    }
                }
            }
            else
            {
                // transparent colors in transparent mode(F + B)
                // transparent colors in transparent mode(0.25F + B)
                GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
            }
        }
        else if (glparamstate.globalTextABR == 3)
        {
            if (texen)
            {
                if (glDrawArraysFlg == 0)
                {
                    if (texturyType & TXT_TYPE_1)
                    {
                        //if (loadTexFlg) GX_LoadTexObj(&currtex->semiTransTexobj, GX_TEXMAP0);
                        // Non transparent colors in transparent mode
                        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
                    }
                    else
                    {
                        return 0;
                    }
                }
                else if (glDrawArraysFlg == 1)
                {
                    if (texturyType & TXT_TYPE_2)
                    {
                        //if (loadTexFlg) GX_LoadTexObj(&currtex->texobj, GX_TEXMAP0);
                        // transparent colors in transparent mode(B - F)
                        GX_SetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
                    }
                    else
                    {
                        return 0;
                    }
                }
            }
            else
            {
                // transparent colors in transparent mode(B - F)
                GX_SetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
            }
        }
    }
    else
    {
//        if (texen)
//        {
//            if (loadTexFlg) GX_LoadTexObj(&currtex->texobj, GX_TEXMAP0);
//
//        }
        GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    }

    // Matrix stuff
    if (needLoadMtx)
    {
        MODELVIEW_UPDATE
        //PROJECTION_UPDATE
        GX_LoadProjectionMtx(GXprojection2D, GX_ORTHOGRAPHIC);

        needLoadMtx = 0;
    }
//    if (glparamstate.dirty.bits.dirty_matrices | glparamstate.dirty.bits.dirty_lighting) {
//        NORMAL_UPDATE
//    }

    // All the state has been transferred, no need to update it again next time
    glparamstate.dirty.all = 0;

    return 1;
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    unsigned char gxmode = draw_mode(mode);
    if (gxmode == 0xff)
        return;

    int texen = glparamstate.texcoord_enabled & glparamstate.texture_enabled;
    int color_provide = 0;
    if (glparamstate.color_enabled &&
        (!glparamstate.lighting.enabled || glparamstate.lighting.color_material_enabled)) { // Vertex colouring
        if (glparamstate.lighting.enabled)
            color_provide = 2; // Lighting requires two color channels
        else
            color_provide = 1;
    }

    // Create data pointers
    float *ptr_pos = glparamstate.vertex_array;
    float *ptr_texc = glparamstate.texcoord_array;
    unsigned char *ptr_color = glparamstate.color_array;
    float *ptr_normal = glparamstate.normal_array;

    ptr_pos += (glparamstate.vertex_stride * first);
    ptr_texc += (glparamstate.texcoord_stride * first);
    ptr_color += (glparamstate.color_stride * first);
    ptr_normal += (glparamstate.normal_stride * first);

    // Not using indices
    GX_ClearVtxDesc();
    if (glparamstate.vertex_enabled)
        GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    if (glparamstate.normal_enabled)
        GX_SetVtxDesc(GX_VA_NRM, GX_DIRECT);
    if (color_provide)
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    if (color_provide == 2)
        GX_SetVtxDesc(GX_VA_CLR1, GX_DIRECT);
    if (texen)
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    // Using floats
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR1, GX_CLR_RGBA, GX_RGBA8, 0);

    // Invalidate vertex data as may have been modified by the user
    GX_InvVtxCache();
//    if (texen && loadTexFlg)
//    {
//        GX_DrawDone();
//        GX_InvalidateTexAll();
//    }
    #ifdef DISP_DEBUG
    if (texen)
    {
        // tex debug start
        extern bool canWriteLog;
        static bool isWritedTex = false;
        if (canWriteLog && isWritedTex == false)
        {
            int i;
            for (i = 1; i < _MAX_GL_TEX; i++) {
                if (texture_list[i].used == 0) {
                    break;
                }
                char txtbuffer[1024];
                gltexture_ *curTex = &texture_list[i];
                sprintf(txtbuffer, "sd:/wiisxrx/txtDebug_%d_%d_%02d.bin", curTex->w, curTex->h, i);
                FILE* texDebugLog = fopen(txtbuffer, "wb");
                fwrite(curTex->data, 1, curTex->w * curTex->h * 2, texDebugLog);
                fclose(texDebugLog);

                if (curTex->semiTransData)
                {
                    sprintf(txtbuffer, "sd:/wiisxrx/txtDebugS_%d_%d_%02d.bin", curTex->w, curTex->h, i);
                    texDebugLog = fopen(txtbuffer, "wb");
                    fwrite(curTex->semiTransData, 1, curTex->w * curTex->h * 2, texDebugLog);
                    fclose(texDebugLog);
                }
            }
            isWritedTex = true;
        }
        // tex debug end
    }
    #endif // DISP_DEBUG

    // blendenabled=false, Execute GX_SetBlendMode once
    //   1st GX_SetBlendMode: Non transparent colors
    // blendenabled=true, Possible execution of GX_SetBlendMode three times
    //   1st GX_SetBlendMode: Non transparent colors in transparent mode
    //   2nd GX_SetBlendMode: transparent colors in transparent mode
    glDrawArraysFlg = 0;
    bool loop = (mode == GL_LINE_LOOP);
    int needDraw;

    if (_ogx_apply_state())
    {
        GX_Begin(gxmode, GX_VTXFMT0, count + loop);
        draw_arrays_general(ptr_pos, ptr_normal, ptr_texc, ptr_color,
                            count, glparamstate.normal_enabled, color_provide, texen, loop);
        GX_End();
    }

    if (glparamstate.blendenabled && texen)
    {
        glDrawArraysFlg = 1;

        if (_ogx_apply_state())
        {
            GX_Begin(gxmode, GX_VTXFMT0, count + loop);
            draw_arrays_general(ptr_pos, ptr_normal, ptr_texc, ptr_color,
                                count, glparamstate.normal_enabled, color_provide, texen, loop);
            GX_End();
        }
    }
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{

    unsigned char gxmode = draw_mode(mode);
    if (gxmode == 0xff)
        return;

    _ogx_apply_state();

    int texen = glparamstate.texcoord_enabled & glparamstate.texture_enabled;
    int color_provide = 0;
    if (glparamstate.color_enabled &&
        (!glparamstate.lighting.enabled || glparamstate.lighting.color_material_enabled)) { // Vertex colouring
        if (glparamstate.lighting.enabled)
            color_provide = 2; // Lighting requires two color channels
        else
            color_provide = 1;
    }

    // Create data pointers
    unsigned short *ind = (unsigned short *)indices;

    // Not using indices
    GX_ClearVtxDesc();
    if (glparamstate.vertex_enabled)
        GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
    if (glparamstate.normal_enabled)
        GX_SetVtxDesc(GX_VA_NRM, GX_DIRECT);
    if (color_provide)
        GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    if (color_provide == 2)
        GX_SetVtxDesc(GX_VA_CLR1, GX_DIRECT);
    if (texen)
        GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    // Using floats
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR1, GX_CLR_RGBA, GX_RGBA8, 0);

    // Invalidate vertex data as may have been modified by the user
    GX_InvVtxCache();

    bool loop = (mode == GL_LINE_LOOP);
    GX_Begin(gxmode, GX_VTXFMT0, count + loop);
    int i;
    for (i = 0; i < count + loop; i++) {
        int index = ind[i % count];
        float *ptr_pos = glparamstate.vertex_array + glparamstate.vertex_stride * index;
        float *ptr_texc = glparamstate.texcoord_array + glparamstate.texcoord_stride * index;
        unsigned char *ptr_color = glparamstate.color_array + glparamstate.color_stride * index;
        float *ptr_normal = glparamstate.normal_array + glparamstate.normal_stride * index;

        GX_Position3f32(ptr_pos[0], ptr_pos[1], ptr_pos[2]);

        if (glparamstate.normal_enabled) {
            GX_Normal3f32(ptr_normal[0], ptr_normal[1], ptr_normal[2]);
        }

        // If the data stream doesn't contain any color data just
        // send the current color (the last glColor* call)
        if (color_provide) {
            //unsigned char arr[4] = { ptr_color[0] * 255.0f, ptr_color[1] * 255.0f, ptr_color[2] * 255.0f, ptr_color[3] * 255.0f };
            unsigned char arr[4] = { ptr_color[0], ptr_color[1], ptr_color[2], ptr_color[3] };
            GX_Color4u8(arr[0], arr[1], arr[2], arr[3]);
            if (color_provide == 2)
                GX_Color4u8(arr[0], arr[1], arr[2], arr[3]);
        }

        if (texen) {
            GX_TexCoord2f32(ptr_texc[0], ptr_texc[1]);
        }
    }
    GX_End();
}

static void draw_arrays_pos_normal_texc(float *ptr_pos, float *ptr_texc, float *ptr_normal,
                                        int count, bool loop)
{
    int i;
    float *pos = ptr_pos, *texc = ptr_texc, *normal = ptr_normal;
    for (i = 0; i < count; i++) {
        GX_Position3f32(ptr_pos[0], ptr_pos[1], ptr_pos[2]);
        ptr_pos += glparamstate.vertex_stride;

        GX_Normal3f32(ptr_normal[0], ptr_normal[1], ptr_normal[2]);
        ptr_normal += glparamstate.normal_stride;

        GX_TexCoord2f32(ptr_texc[0], ptr_texc[1]);
        ptr_texc += glparamstate.texcoord_stride;
    }
    if (loop) {
        GX_Position3f32(pos[0], pos[1], pos[2]);
        GX_Normal3f32(normal[0], normal[1], normal[2]);
        GX_TexCoord2f32(texc[0], texc[1]);
    }
}

static void draw_arrays_pos_normal(float *ptr_pos, float *ptr_normal, int count,
                                   bool loop)
{
    int i;
    float *pos = ptr_pos, *normal = ptr_normal;
    for (i = 0; i < count; i++) {
        GX_Position3f32(ptr_pos[0], ptr_pos[1], ptr_pos[2]);
        ptr_pos += glparamstate.vertex_stride;

        GX_Normal3f32(ptr_normal[0], ptr_normal[1], ptr_normal[2]);
        ptr_normal += glparamstate.normal_stride;
    }
    if (loop) {
        GX_Position3f32(pos[0], pos[1], pos[2]);
        GX_Normal3f32(normal[0], normal[1], normal[2]);
    }
}

static void draw_arrays_general(float *ptr_pos, float *ptr_normal, float *ptr_texc, unsigned char *ptr_color,
                                int count, int ne, int color_provide, int texen, bool loop)
{

    int i;
    for (i = 0; i < count + loop; i++) {
        int j = i % count;
        float *pos = ptr_pos + j * glparamstate.vertex_stride;
        GX_Position3f32(pos[0], pos[1], pos[2]);

        if (ne) {
            float *normal = ptr_normal + j * glparamstate.normal_stride;
            GX_Normal3f32(normal[0], normal[1], normal[2]);
        }

        // If the data stream doesn't contain any color data just
        // send the current color (the last glColor* call)
        if (color_provide) {
            unsigned char *color = ptr_color + j * glparamstate.color_stride;
            //unsigned char arr[4] = { color[0] * 255.0f, color[1] * 255.0f, color[2] * 255.0f, color[3] * 255.0f };
            //unsigned char arr[4] = { color[0], color[1], color[2], color[3] };
            GX_Color4u8(color[0], color[1], color[2], color[3]);
            if (color_provide == 2)
                GX_Color4u8(color[0], color[1], color[2], color[3]);
        }

        if (texen) {
            float *texc = ptr_texc + j * glparamstate.texcoord_stride;
            GX_TexCoord2f32(texc[0], texc[1]);
        }
    }
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
               GLdouble near, GLdouble far)
{
    Mtx44 mt;
    f32 tmp;

    tmp = 1.0f / (right - left);
    mt[0][0] = (2 * near) * tmp;
    mt[0][1] = 0.0f;
    mt[0][2] = (right + left) * tmp;
    mt[0][3] = 0.0f;
    tmp = 1.0f / (top - bottom);
    mt[1][0] = 0.0f;
    mt[1][1] = (2 * near) * tmp;
    mt[1][2] = (top + bottom) * tmp;
    mt[1][3] = 0.0f;
    tmp = 1.0f / (far - near);
    mt[2][0] = 0.0f;
    mt[2][1] = 0.0f;
    mt[2][2] = -(far + near) * tmp;
    mt[2][3] = -2.0 * (far * near) * tmp;
    mt[3][0] = 0.0f;
    mt[3][1] = 0.0f;
    mt[3][2] = -1.0f;
    mt[3][3] = 0.0f;

    glMultMatrixf((float *)mt);
}

void glOrtho(int left, int right, int bottom, int top, int near_val, int far_val)
{
    guOrtho(GXprojection2D, top, bottom, left, right, near_val, far_val);
    needLoadMtx = 1;
//    Mtx44 newmat;
//    // Same as GX's guOrtho, but transposed
//    float x = (left + right) / (left - right);
//    float y = (bottom + top) / (bottom - top);
//    float z = (near_val + far_val) / (near_val - far_val);
//    newmat[0][0] = 2.0f / (right - left);
//    newmat[0][1] = 0.0f;
//    newmat[0][2] = 0.0f;
//    newmat[0][3] = x;
//    newmat[1][0] = 0.0f;
//    newmat[1][1] = 2.0f / (top - bottom);
//    newmat[1][2] = 0.0f;
//    newmat[1][3] = y;
//    newmat[2][0] = 0.0f;
//    newmat[2][1] = 0.0f;
//    newmat[2][2] = 2.0f / (near_val - far_val);
//    newmat[2][3] = z;
//    newmat[3][0] = 0.0f;
//    newmat[3][1] = 0.0f;
//    newmat[3][2] = 0.0f;
//    newmat[3][3] = 1.0f;
//
//    glMultMatrixf((float *)newmat);
}

// NOT GOING TO IMPLEMENT

void glBlendEquation(GLenum mode) {}
void glClearStencil(GLint s) {}
void glStencilMask(GLuint mask) {} // Should use Alpha testing to achieve similar results
void glShadeModel(GLenum mode) {}  // In theory we don't have GX equivalent?
void glHint(GLenum target, GLenum mode) {}

static unsigned char gcgl_texwrap_conv(GLint param)
{
    switch (param) {
    case GL_MIRRORED_REPEAT:
        return GX_MIRROR;
    case GL_CLAMP:
    case GL_CLAMP_TO_EDGE:
        return GX_CLAMP;
    case GL_REPEAT:
    default:
        return GX_REPEAT;
    };
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    /* For the time being, all the parameters we support take integer values */
    glTexParameteri(target, pname, param);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    if (target != GL_TEXTURE_2D)
        return;

    gltexture_ *currtex = &texture_list[glparamstate.glcurtex];

    switch (pname) {
    case GL_TEXTURE_WRAP_S:
        currtex->wraps = gcgl_texwrap_conv(param);
        GX_InitTexObjWrapMode(&currtex->texobj, currtex->wraps, currtex->wrapt);
        break;
    case GL_TEXTURE_WRAP_T:
        currtex->wrapt = gcgl_texwrap_conv(param);
        GX_InitTexObjWrapMode(&currtex->texobj, currtex->wraps, currtex->wrapt);
        break;
    };
}

// XXX: Need to finish glGets, important!!!
void glGetIntegerv(GLenum pname, GLint *params)
{
    switch (pname) {
    case GL_MAX_TEXTURE_SIZE:
        *params = 1024;
        return;
    case GL_MODELVIEW_STACK_DEPTH:
        *params = MAX_MODV_STACK;
        return;
    case GL_PROJECTION_STACK_DEPTH:
        *params = MAX_PROJ_STACK;
        return;
    default:
        return;
    };
}
void glGetFloatv(GLenum pname, GLfloat *params)
{
    switch (pname) {
    case GL_MODELVIEW_MATRIX:
        memcpy(params, glparamstate.modelview_matrix, sizeof(float) * 16);
        return;
    case GL_PROJECTION_MATRIX:
        memcpy(params, glparamstate.projection_matrix, sizeof(float) * 16);
        return;
    default:
        return;
    };
}

// TODO STUB IMPLEMENTATION

void glClipPlane(GLenum plane, const GLdouble *equation) {}
const GLubyte *glGetString(GLenum name) { return gl_null_string; }
void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params) {}
void glLightModelf(GLenum pname, GLfloat param) {}
void glLightModeli(GLenum pname, GLint param) {}
void glPushAttrib(GLbitfield mask) {}
void glPopAttrib(void) {}
void glPolygonMode(GLenum face, GLenum mode) {}
void glReadBuffer(GLenum mode) {}
void glPixelStorei(GLenum pname, GLint param) {}
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *data) {}
void glAlphaFunc(GLenum func, GLclampf ref) {} // We need a TEVSTAGE for comparing and discarding pixels by alpha value

/*
 ****** NOTES ******

 Front face definition is reversed. CCW is front for OpenGL
 while front facing is defined CW in GX.

 This implementation ONLY supports floats for vertexs, texcoords
 and normals. Support for different types is not implemented as
 GX does only support floats. Simple conversion would be needed.

*/

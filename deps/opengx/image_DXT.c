/*
        Jonathan Dummer
        2007-07-31-10.32

        simple DXT compression / decompression code

        public domain

        2011-07-19
        David Guillen
        Fixed some big-endian issues, added compatibility
        with Nintendo GX CMPR texture format
*/

#include "image_DXT.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*	set this =1 if you want to use the covarince matrix method...
        which is better than my method of using standard deviations
        overall, except on the infintesimal chance that the power
        method fails for finding the largest eigenvector	*/
#define USE_COV_MAT 1

/*
        Set this constant for reversing the columns in the lookup table
        Nintendo GX uses the following format (taken from GX documentation)

        T00 T01 T02 T03
        T10 T11 T12 T13
        T20 T21 T22 T23
        T30 T31 T32 T33

        MSB         LSB
             Byte 0          Byte 1            Byte 2           Byte 3
        T00 T01 T02 T03  T10 T11 T12 T13  T20 T21 T22 T23  T30 T31 T32 T33

        The original S3TC uses the following bit order
        MSB         LSB
             Byte 0          Byte 1            Byte 2           Byte 3
        T03 T02 T01 T00  T13 T12 T11 T10  T23 T22 T21 T20  T33 T32 T31 T30

*/
#define REVERSE_LOOKUP_TABLE 1

/********* Function Prototypes *********/
/*
        Takes a 4x4 block of pixels and compresses it into 8 bytes
        in DXT1 format (color only, no alpha).  Speed is valued
        over prettyness, at least for now.
*/
static void compress_DDS_color_block(
    int channels,
    const unsigned char *const uncompressed,
    unsigned char compressed[8]);

// Modified and simplified version. It also handles Nintendo GX scrambling
// Forced to 3 channels (RGB)
// Forced to square and power of two textures
void _ogx_convert_rgb_image_to_DXT1(
    const unsigned char *const uncompressed, unsigned char *compressed,
    int width, int height, int red_blue_swap)
{
    int i, j, x, y, jj, ii;
    unsigned char ublock[16 * 3];
    unsigned char cblock[8];
    int index = 0, chan_step = 1;
    if (red_blue_swap)
        red_blue_swap = 2;
    int block_count = 0;
    int channels = 3; // RGB fixed to 3 channels
    /*	error check	*/
    if ((width < 1) || (height < 1) ||
        (NULL == uncompressed) ||
        (NULL == compressed)) {
        return;
    }

    for (j = 0; j < height; j += 8) {
        for (i = 0; i < width; i += 8) {
            for (jj = 0; jj < 8; jj += 4) {
                for (ii = 0; ii < 8; ii += 4) {
                    /*	copy this block into a new one	*/
                    int idx = 0;
                    for (y = 0; y < 4; y++) {
                        for (x = 0; x < 4; x++) {
                            ublock[idx++] = uncompressed[(j + jj + y) * width * channels + (i + ii + x) * channels + red_blue_swap];
                            ublock[idx++] = uncompressed[(j + jj + y) * width * channels + (i + ii + x) * channels + chan_step];
                            ublock[idx++] = uncompressed[(j + jj + y) * width * channels + (i + ii + x) * channels + chan_step + chan_step - red_blue_swap];
                        }
                    }

                    /*	compress the block	*/
                    ++block_count;
                    compress_DDS_color_block(3, ublock, cblock);
                    /*	copy the data from the block into the main block	*/

                    for (x = 0; x < 8; ++x)
                        compressed[index++] = cblock[x];
                }
            }
        }
    }
}

/********* Helper Functions *********/
static int convert_bit_range(int c, int from_bits, int to_bits)
{
    int b = (1 << (from_bits - 1)) + c * ((1 << to_bits) - 1);
    return (b + (b >> from_bits)) >> from_bits;
}

static int rgb_to_565(int r, int g, int b)
{
    return (convert_bit_range(r, 8, 5) << 11) |
           (convert_bit_range(g, 8, 6) << 05) |
           (convert_bit_range(b, 8, 5) << 00);
}

static void rgb_888_from_565(unsigned int c, int *r, int *g, int *b)
{
    *r = convert_bit_range((c >> 11) & 31, 5, 8);
    *g = convert_bit_range((c >> 05) & 63, 6, 8);
    *b = convert_bit_range((c >> 00) & 31, 5, 8);
}

static void compute_color_line_STDEV(
    const unsigned char *const uncompressed,
    int channels,
    float point[3], float direction[3])
{
    const float inv_16 = 1.0f / 16.0f;
    int i;
    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    float sum_rr = 0.0f, sum_gg = 0.0f, sum_bb = 0.0f;
    float sum_rg = 0.0f, sum_rb = 0.0f, sum_gb = 0.0f;
    /*	calculate all data needed for the covariance matrix
            ( to compare with _rygdxt code)	*/
    for (i = 0; i < 16 * channels; i += channels) {
        sum_r += uncompressed[i + 0];
        sum_rr += uncompressed[i + 0] * uncompressed[i + 0];
        sum_g += uncompressed[i + 1];
        sum_gg += uncompressed[i + 1] * uncompressed[i + 1];
        sum_b += uncompressed[i + 2];
        sum_bb += uncompressed[i + 2] * uncompressed[i + 2];
        sum_rg += uncompressed[i + 0] * uncompressed[i + 1];
        sum_rb += uncompressed[i + 0] * uncompressed[i + 2];
        sum_gb += uncompressed[i + 1] * uncompressed[i + 2];
    }
    /*	convert the sums to averages	*/
    sum_r *= inv_16;
    sum_g *= inv_16;
    sum_b *= inv_16;
    /*	and convert the squares to the squares of the value - avg_value	*/
    sum_rr -= 16.0f * sum_r * sum_r;
    sum_gg -= 16.0f * sum_g * sum_g;
    sum_bb -= 16.0f * sum_b * sum_b;
    sum_rg -= 16.0f * sum_r * sum_g;
    sum_rb -= 16.0f * sum_r * sum_b;
    sum_gb -= 16.0f * sum_g * sum_b;
    /*	the point on the color line is the average	*/
    point[0] = sum_r;
    point[1] = sum_g;
    point[2] = sum_b;
#if USE_COV_MAT
    /*
            The following idea was from ryg.
            (https://mollyrocket.com/forums/viewtopic.php?t=392)
            The method worked great (less RMSE than mine) most of
            the time, but had some issues handling some simple
            boundary cases, like full green next to full red,
            which would generate a covariance matrix like this:

            | 1  -1  0 |
            | -1  1  0 |
            | 0   0  0 |

            For a given starting vector, the power method can
            generate all zeros!  So no starting with {1,1,1}
            as I was doing!  This kind of error is still a
            slight posibillity, but will be very rare.
    */
    /*	use the covariance matrix directly
            (1st iteration, don't use all 1.0 values!)	*/
    sum_r = 1.0f;
    sum_g = 2.718281828f;
    sum_b = 3.141592654f;
    direction[0] = sum_r * sum_rr + sum_g * sum_rg + sum_b * sum_rb;
    direction[1] = sum_r * sum_rg + sum_g * sum_gg + sum_b * sum_gb;
    direction[2] = sum_r * sum_rb + sum_g * sum_gb + sum_b * sum_bb;
    /*	2nd iteration, use results from the 1st guy	*/
    sum_r = direction[0];
    sum_g = direction[1];
    sum_b = direction[2];
    direction[0] = sum_r * sum_rr + sum_g * sum_rg + sum_b * sum_rb;
    direction[1] = sum_r * sum_rg + sum_g * sum_gg + sum_b * sum_gb;
    direction[2] = sum_r * sum_rb + sum_g * sum_gb + sum_b * sum_bb;
    /*	3rd iteration, use results from the 2nd guy	*/
    sum_r = direction[0];
    sum_g = direction[1];
    sum_b = direction[2];
    direction[0] = sum_r * sum_rr + sum_g * sum_rg + sum_b * sum_rb;
    direction[1] = sum_r * sum_rg + sum_g * sum_gg + sum_b * sum_gb;
    direction[2] = sum_r * sum_rb + sum_g * sum_gb + sum_b * sum_bb;
#else
    /*	use my standard deviation method
            (very robust, a tiny bit slower and less accurate)	*/
    direction[0] = sqrt(sum_rr);
    direction[1] = sqrt(sum_gg);
    direction[2] = sqrt(sum_bb);
    /*	which has a greater component	*/
    if (sum_gg > sum_rr) {
        /*	green has greater component, so base the other signs off of green	*/
        if (sum_rg < 0.0f) {
            direction[0] = -direction[0];
        }
        if (sum_gb < 0.0f) {
            direction[2] = -direction[2];
        }
    } else {
        /*	red has a greater component	*/
        if (sum_rg < 0.0f) {
            direction[1] = -direction[1];
        }
        if (sum_rb < 0.0f) {
            direction[2] = -direction[2];
        }
    }
#endif
}

static void LSE_master_colors_max_min(
    int *cmax, int *cmin,
    int channels,
    const unsigned char *const uncompressed)
{
    int i, j;
    /*	the master colors	*/
    int c0[3], c1[3];
    /*	used for fitting the line	*/
    float sum_x[] = { 0.0f, 0.0f, 0.0f };
    float sum_x2[] = { 0.0f, 0.0f, 0.0f };
    float dot_max = 1.0f, dot_min = -1.0f;
    float vec_len2 = 0.0f;
    float dot;
    /*	error check	*/
    if ((channels < 3) || (channels > 4)) {
        return;
    }
    compute_color_line_STDEV(uncompressed, channels, sum_x, sum_x2);
    vec_len2 = 1.0f / (0.00001f +
                       sum_x2[0] * sum_x2[0] + sum_x2[1] * sum_x2[1] + sum_x2[2] * sum_x2[2]);
    /*	finding the max and min vector values	*/
    dot_max =
        (sum_x2[0] * uncompressed[0] +
         sum_x2[1] * uncompressed[1] +
         sum_x2[2] * uncompressed[2]);
    dot_min = dot_max;
    for (i = 1; i < 16; ++i) {
        dot =
            (sum_x2[0] * uncompressed[i * channels + 0] +
             sum_x2[1] * uncompressed[i * channels + 1] +
             sum_x2[2] * uncompressed[i * channels + 2]);
        if (dot < dot_min) {
            dot_min = dot;
        } else if (dot > dot_max) {
            dot_max = dot;
        }
    }
    /*	and the offset (from the average location)	*/
    dot = sum_x2[0] * sum_x[0] + sum_x2[1] * sum_x[1] + sum_x2[2] * sum_x[2];
    dot_min -= dot;
    dot_max -= dot;
    /*	post multiply by the scaling factor	*/
    dot_min *= vec_len2;
    dot_max *= vec_len2;
    /*	OK, build the master colors	*/
    for (i = 0; i < 3; ++i) {
        /*	color 0	*/
        c0[i] = (int)(0.5f + sum_x[i] + dot_max * sum_x2[i]);
        if (c0[i] < 0) {
            c0[i] = 0;
        } else if (c0[i] > 255) {
            c0[i] = 255;
        }
        /*	color 1	*/
        c1[i] = (int)(0.5f + sum_x[i] + dot_min * sum_x2[i]);
        if (c1[i] < 0) {
            c1[i] = 0;
        } else if (c1[i] > 255) {
            c1[i] = 255;
        }
    }
    /*	down_sample (with rounding?)	*/
    i = rgb_to_565(c0[0], c0[1], c0[2]);
    j = rgb_to_565(c1[0], c1[1], c1[2]);
    if (i > j) {
        *cmax = i;
        *cmin = j;
    } else {
        *cmax = j;
        *cmin = i;
    }
}

static void
compress_DDS_color_block(
    int channels,
    const unsigned char *const uncompressed,
    unsigned char compressed[8])
{
    /*	variables	*/
    int i;
    int next_bit;
    int enc_c0, enc_c1;
    int c0[4], c1[4];
    float color_line[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float vec_len2 = 0.0f, dot_offset = 0.0f;
    /*	stupid order	*/
    int swizzle4[] = { 0, 2, 3, 1 };
    /*	get the master colors	*/
    LSE_master_colors_max_min(&enc_c0, &enc_c1, channels, uncompressed);
    /*	store the 565 color 0 and color 1	*/
    // Big endian fix (davidgf.net)
    unsigned short *colors = (unsigned short *)compressed;
    colors[0] = enc_c0;
    colors[1] = enc_c1;
    /*	zero out the compressed data	*/
    compressed[4] = 0;
    compressed[5] = 0;
    compressed[6] = 0;
    compressed[7] = 0;
    /*	reconstitute the master color vectors	*/
    rgb_888_from_565(enc_c0, &c0[0], &c0[1], &c0[2]);
    rgb_888_from_565(enc_c1, &c1[0], &c1[1], &c1[2]);
    /*	the new vector	*/
    vec_len2 = 0.0f;
    for (i = 0; i < 3; ++i) {
        color_line[i] = (float)(c1[i] - c0[i]);
        vec_len2 += color_line[i] * color_line[i];
    }
    if (vec_len2 > 0.0f) {
        vec_len2 = 1.0f / vec_len2;
    }
    /*	pre-proform the scaling	*/
    color_line[0] *= vec_len2;
    color_line[1] *= vec_len2;
    color_line[2] *= vec_len2;
    /*	compute the offset (constant) portion of the dot product	*/
    dot_offset = color_line[0] * c0[0] + color_line[1] * c0[1] + color_line[2] * c0[2];
    /*	store the rest of the bits	*/
    next_bit = 8 * 4;
    for (i = 0; i < 16; ++i) {
        /*	find the dot product of this color, to place it on the line
                (should be [-1,1])	*/
        int next_value = 0;
        float dot_product =
            color_line[0] * uncompressed[i * channels + 0] +
            color_line[1] * uncompressed[i * channels + 1] +
            color_line[2] * uncompressed[i * channels + 2] -
            dot_offset;
        /*	map to [0,3]	*/
        next_value = (int)(dot_product * 3.0f + 0.5f);
        if (next_value > 3) {
            next_value = 3;
        } else if (next_value < 0) {
            next_value = 0;
        }
/*	OK, store this value	*/
#ifdef REVERSE_LOOKUP_TABLE
        compressed[next_bit >> 3] |= swizzle4[next_value] << (6 - (next_bit & 7));
#else
        compressed[next_bit >> 3] |= swizzle4[next_value] << (next_bit & 7);
#endif
        next_bit += 2;
    }
    /*	done compressing to DXT1	*/
}

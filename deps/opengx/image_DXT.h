/*
        Jonathan Dummer
        2007-07-31-10.32

        simple DXT compression / decompression code

        public domain
*/

#ifndef OPENGX_IMAGE_DXT_H
#define OPENGX_IMAGE_DXT_H

void _ogx_convert_rgb_image_to_DXT1(
    const unsigned char *const uncompressed, unsigned char *compressed,
    int width, int height, int red_blue_swap);

#endif /* OPENGX_IMAGE_DXT_H */

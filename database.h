#ifndef DATABASE_H
#define DATABASE_H

#ifdef __cplusplus
extern "C" {
#endif

#define AUTO_FIX_GPU_BUSY            0x100
#define AUTO_FIX_QUADS_AS_2TRIANGLES 0x200
#define AUTO_FIX_DINO_CRISIS2        0x400
#define AUTO_FIX_FF9                 0x800
#define AUTO_FIX_NO_SOFT_TITLE       0x1000
#define AUTO_FIX_CHRONO_CROSS        0x2000

void Apply_Hacks_Cdrom(void);
int check_unsatisfied_libcrypt(void);

#ifdef __cplusplus
}
#endif

#endif

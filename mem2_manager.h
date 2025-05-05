#ifndef _MEM2_MANAGER_H
#define _MEM2_MANAGER_H

#include <stdint.h>

bool gx_init_mem2(void);

uint32_t gx_mem2_used(void);

uint32_t gx_mem2_total(void);

void *_mem2_memalign(uint8_t align, uint32_t size);

void _mem2_free(void *ptr);

#endif

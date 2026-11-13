#ifndef _MEM2_MANAGER_H
#define _MEM2_MANAGER_H

#include <stdint.h>

#define GX_MEM2_MIN_FREE_BYTES (512U * 1024U)

bool gx_init_mem2(void);

uint32_t gx_mem2_used(void);

uint32_t gx_mem2_total(void);

uint32_t gx_mem2_free(void);

uint32_t gx_mem2_allocation_failures(void);
uint32_t gx_mem2_last_failed_size(void);
uint32_t gx_mem2_largest_failed_size(void);
uintptr_t gx_mem2_last_failed_caller(void);

void *_mem2_memalign(uint8_t align, uint32_t size);

void *_mem2_malloc(uint32_t size);

void _mem2_free(void *ptr);

#endif

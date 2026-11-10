#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <ogc/machine/asm.h>
#include <ogc/machine/processor.h>
#include <ogc/lwp_heap.h>
#include <ogc/system.h>

#include "mem2_manager.h"
#include "Gamecube/MEM2.h"

/* Forbid the use of MEM2 through malloc */
//uint32_t MALLOC_MEM2 = 0;

/*** from libogc (lwp_heap.inl) ****/

static inline heap_block *__lwp_heap_blockat(heap_block *block, uint32_t offset)
{
   return (heap_block *) ((char *) block + offset);
}

static inline heap_block *__lwp_heap_usrblockat(void *ptr)
{
   uint32_t offset = *(((uint32_t *) ptr) - 1);
   return __lwp_heap_blockat(ptr, -offset + -HEAP_BLOCK_USED_OVERHEAD);
}

static inline bool __lwp_heap_blockin(heap_cntrl *heap, heap_block *block)
{
   return ((uint32_t) block >= (uint32_t) heap->start && (uint32_t) block <= (uint32_t) heap->final);
}

static inline bool __lwp_heap_blockfree(heap_block *block)
{
   return !(block->front_flag & HEAP_BLOCK_USED);
}

static inline uint32_t __lwp_heap_blocksize(heap_block *block)
{
   return (block->front_flag & ~HEAP_BLOCK_USED);
}

/*** end from libogc (lwp_heap.inl)  ****/

static uint32_t __lwp_heap_block_size(heap_cntrl *theheap, void *ptr)
{
   heap_block *block;
   uint32_t dsize, level;
   (void)level;

   _CPU_ISR_Disable(level);
   block = __lwp_heap_usrblockat(ptr);

   if (!__lwp_heap_blockin(theheap, block) || __lwp_heap_blockfree(block))
   {
      _CPU_ISR_Restore(level);
      return 0;
   }

   dsize = __lwp_heap_blocksize(block);
   _CPU_ISR_Restore(level);
   return dsize;
}

#define ROUNDUP32(v) (((uint32_t)(v) + 0x1f) & ~0x1f)

static heap_cntrl gx_mem2_heap;
static uint32_t gx_mem2_failures;
static uint32_t gx_mem2_last_fail_size;
static uint32_t gx_mem2_largest_fail_size;
static uintptr_t gx_mem2_last_fail_caller;

static void *gx_mem2_allocate(uint32_t size, uintptr_t caller)
{
   void *ptr;

   if (size == 0)
      return NULL;

   ptr = __lwp_heap_allocate(&gx_mem2_heap, size);
   if (!ptr)
   {
      gx_mem2_failures++;
      gx_mem2_last_fail_size = size;
      if (size > gx_mem2_largest_fail_size)
         gx_mem2_largest_fail_size = size;
      gx_mem2_last_fail_caller = caller;
   }
   return ptr;
}

bool gx_init_mem2(void)
{
   void *heap_ptr;
   uint32_t level, size;
   _CPU_ISR_Disable(level);

   /* BIG NOTE: MEM2 on the Wii is 64MB, but a portion
    * of that is reserved for IOS.
    *
    * libogc by default defines the "safe" area for MEM2
    * to go from 0x90002000 to 0x933E0000.
    *
    * However, from my testing, I've found I need to
    * reserve about 256KB for stuff like network and USB to work correctly.
    * However, other sources says these functions need at least 0xE0000 bytes,
    * 7/8 of a megabyte, of reserved memory to do this. My initial testing
    * shows that we can work with only 128KB, but we use 256KB becuse testing
    * has shown some stuff being iffy with only 128KB, mainly Wiimote stuff.
    * If some stuff mysteriously stops working, try fiddling with this size.
    */
   // Reset mem2 range
   SYS_SetArena2Lo((void *)NEW_MEM2_LO);

   uint32_t newArena2Hi = NEW_MEM2_LO + (2 * MB);
   uint32_t oldArena2Hi = (uint32_t) SYS_GetArena2Hi();
   heap_ptr = (void *)newArena2Hi;

   SYS_SetArena2Hi(newArena2Hi);
   __lwp_heap_init(&gx_mem2_heap, heap_ptr, oldArena2Hi - newArena2Hi - 1024 * 1024, 32);
   gx_mem2_failures = 0;
   gx_mem2_last_fail_size = 0;
   gx_mem2_largest_fail_size = 0;
   gx_mem2_last_fail_caller = 0;

   _CPU_ISR_Restore(level);

   return true;
}

void *_mem2_memalign(uint8_t align, uint32_t size)
{
   (void)align;
   return gx_mem2_allocate(size, (uintptr_t)__builtin_return_address(0));
}

void *_mem2_malloc(uint32_t size)
{
   return gx_mem2_allocate(size, (uintptr_t)__builtin_return_address(0));
}

void _mem2_free(void *ptr)
{
   if (ptr)
      __lwp_heap_free(&gx_mem2_heap, ptr);
}

void *_mem2_realloc(void *ptr, uint32_t newsize)
{
   uint32_t size;
   void *newptr = NULL;

   if (!ptr)
      return _mem2_malloc(newsize);

   if (newsize == 0)
   {
      _mem2_free(ptr);
      return NULL;
   }

   size = __lwp_heap_block_size(&gx_mem2_heap, ptr);

   if (size > newsize)
      size = newsize;

   newptr = _mem2_malloc(newsize);

   if (!newptr)
      return NULL;

   memcpy(newptr, ptr, size);
   _mem2_free(ptr);

   return newptr;
}

void *_mem2_calloc(uint32_t num, uint32_t size)
{
   void *ptr = _mem2_malloc(num * size);

   if (!ptr)
      return NULL;

   memset(ptr, 0, num * size);
   return ptr;
}

char *_mem2_strdup(const char *s)
{
    char *ptr = NULL;

    if (s)
    {
        size_t len = strlen(s) + 1;
        ptr        = _mem2_calloc(1, len);

        if (ptr)
            memcpy(ptr, s, len);
    }

    return ptr;
}

char *_mem2_strndup(const char *s, size_t n)
{
    char *ptr = NULL;

    if (s)
    {
        int len = n + 1;
        ptr = _mem2_calloc(1, len);

        if (ptr)
            memcpy(ptr, s, len);
    }
    return ptr;
}

uint32_t gx_mem2_used(void)
{
   heap_iblock info;
   __lwp_heap_getinfo(&gx_mem2_heap, &info);
   return info.used_size;
}

uint32_t gx_mem2_total(void)
{
   heap_iblock info;
   __lwp_heap_getinfo(&gx_mem2_heap, &info);
   return info.used_size + info.free_size;
}

uint32_t gx_mem2_allocation_failures(void)
{
   return gx_mem2_failures;
}

uint32_t gx_mem2_last_failed_size(void)
{
   return gx_mem2_last_fail_size;
}

uint32_t gx_mem2_largest_failed_size(void)
{
   return gx_mem2_largest_fail_size;
}

uintptr_t gx_mem2_last_failed_caller(void)
{
   return gx_mem2_last_fail_caller;
}

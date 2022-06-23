/* General Purpose Registers (GPRs) */

#define    r0    0
#define    r1    1
#define    r2    2
#define    r3    3
#define    r4    4
#define    r5    5
#define    r6    6
#define    r7    7
#define    r8    8
#define    r9    9
#define    r10    10
#define    r11    11
#define    r12    12
#define    r13    13
#define    r14    14
#define    r15    15
#define    r16    16
#define    r17    17
#define    r18    18
#define    r19    19
#define    r20    20
#define    r21    21
#define    r22    22
#define    r23    23
#define    r24    24
#define    r25    25
#define    r26    26
#define    r27    27
#define    r28    28
#define    r29    29
#define    r30    30
#define    r31    31

#define L1_CACHE_LINE_SIZE  32
#define LG_L1_CACHE_LINE_SIZE   5
#define MAX_L1_COPY_PREFETCH    4

#define COPY_16_BYTES        \
    lwz    r7,4(r4);    \
    lwz    r8,8(r4);    \
    lwz    r9,12(r4);    \
    lwzu    r10,16(r4);    \
    stw    r7,4(r6);    \
    stw    r8,8(r6);    \
    stw    r9,12(r6);    \
    stwu    r10,16(r6)

    .text

CACHELINE_BYTES = L1_CACHE_LINE_SIZE
LG_CACHELINE_BYTES = LG_L1_CACHE_LINE_SIZE
CACHELINE_MASK = (L1_CACHE_LINE_SIZE-1)

/*
 * This version uses dcbz on the complete cache lines in the
 * destination area to reduce memory traffic.  This requires that
 * the destination area is cacheable.
 * We only use this version if the source and dest don't overlap.
 * -- paulus.
 */
    .global    cacheable_kernel_memcpy
cacheable_kernel_memcpy:
    add    r7,r3,r5        /* test if the src & dst overlap */
    add    r8,r4,r5
    cmplw    0,r4,r7
    cmplw    1,r3,r8
    crand    0,0,4            /* cr0.lt &= cr1.lt */
    blt    kernel_memcpy            /* if regions overlap */

    addi    r4,r4,-4
    addi    r6,r3,-4
    neg    r0,r3
    andi.    r0,r0,CACHELINE_MASK    /* # bytes to start of cache line */
    beq    58f

    cmplw    0,r5,r0            /* is this more than total to do? */
    blt    63f            /* if not much to do */
    andi.    r8,r0,3            /* get it word-aligned first */
    subf    r5,r0,r5
    mtctr    r8
    beq+    61f
70:    lbz    r9,4(r4)        /* do some bytes */
    stb    r9,4(r6)
    addi    r4,r4,1
    addi    r6,r6,1
    bdnz    70b
61:    srwi.    r0,r0,2
    mtctr    r0
    beq    58f
72:    lwzu    r9,4(r4)        /* do some words */
    stwu    r9,4(r6)
    bdnz    72b

58:    srwi.    r0,r5,LG_CACHELINE_BYTES /* # complete cachelines */
    clrlwi    r5,r5,32-LG_CACHELINE_BYTES
    li    r11,4
    mtctr    r0
    beq    63f
53:
#if !defined(CONFIG_8xx)
    dcbz    r11,r6
#endif
    COPY_16_BYTES
#if L1_CACHE_LINE_SIZE >= 32
    COPY_16_BYTES
#if L1_CACHE_LINE_SIZE >= 64
    COPY_16_BYTES
    COPY_16_BYTES
#if L1_CACHE_LINE_SIZE >= 128
    COPY_16_BYTES
    COPY_16_BYTES
    COPY_16_BYTES
    COPY_16_BYTES
#endif
#endif
#endif
    bdnz    53b

63:    srwi.    r0,r5,2
    mtctr    r0
    beq    64f
30:    lwzu    r0,4(r4)
    stwu    r0,4(r6)
    bdnz    30b

64:    andi.    r0,r5,3
    mtctr    r0
    beq+    65f
40:    lbz    r0,4(r4)
    stb    r0,4(r6)
    addi    r4,r4,1
    addi    r6,r6,1
    bdnz    40b
65:    blr

    .globl    kernel_memcpy
kernel_memcpy:
    srwi.    r7,r5,3
    addi    r6,r3,-4
    addi    r4,r4,-4
    beq    2f            /* if less than 8 bytes to do */
    andi.    r0,r6,3            /* get dest word aligned */
    mtctr    r7
    bne    5f
1:    lwz    r7,4(r4)
    lwzu    r8,8(r4)
    stw    r7,4(r6)
    stwu    r8,8(r6)
    bdnz    1b
    andi.    r5,r5,7
2:    cmplwi    0,r5,4
    blt    3f
    lwzu    r0,4(r4)
    addi    r5,r5,-4
    stwu    r0,4(r6)
3:    cmpwi    0,r5,0
    beqlr
    mtctr    r5
    addi    r4,r4,3
    addi    r6,r6,3
4:    lbzu    r0,1(r4)
    stbu    r0,1(r6)
    bdnz    4b
    blr
5:    subfic    r0,r0,4
    mtctr    r0
6:    lbz    r7,4(r4)
    addi    r4,r4,1
    stb    r7,4(r6)
    addi    r6,r6,1
    bdnz    6b
    subf    r5,r0,r5
    rlwinm.    r7,r5,32-3,3,31
    beq    2b
    mtctr    r7
    b    1b

/*    .section __ex_table,"a"
    .align    2
    .long    1b,99b
*/


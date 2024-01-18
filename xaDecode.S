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

#define    lr    8

/* Floating Point Registers (FPRs) */

#define	fr0		0
#define	fr1		1
#define	fr2		2
#define	fr3		3
#define	fr4		4
#define	fr5		5
#define	fr6		6
#define	fr7		7
#define	fr8		8
#define	fr9		9
#define	fr10	10
#define	fr11	11
#define	fr12	12
#define	fr13	13
#define	fr14	14
#define	fr15	15
#define	fr16	16
#define	fr17	17
#define	fr18	18
#define	fr19	19
#define	fr20	20
#define	fr21	21
#define	fr22	22
#define	fr23	23
#define	fr24	24
#define	fr25	25
#define	fr26	26
#define	fr27	27
#define	fr28	28
#define	fr29	29
#define	fr30	30
#define	fr31	31

#define fr14ShiftR12 14
#define fr15ShiftR16 15
#define fr17ShiftR8  17
#define fr18Kmagic   18


.globl psDecodeBlock16
// r3: IK0K1Tmp
// r4: xTmp
// r5: f32
// r6: destp
// r7: inc
psDecodeBlock16:
    // load IK0, IK1
    lfs         fr0, 0(r3)          // fr0 = IK0
    lfs         fr1, 4(r3)          // fr1 = IK1

    // load fy0, fy1
    lfs         fr2, 0(r5)          // fr2 = fy0
    lfs         fr3, 4(r5)          // fr3 = fy1

    // load x0~x3
    psq_l       fr6, 0(r4), 0, 3    // fr6 = x0, x1
    psq_l       fr7, 4(r4), 0, 3    // fr7 = x2, x3

    fmuls       fr4, fr1, fr3       // fr4 = IK1 * fy1
    fmuls       fr9, fr1, fr2       // fr9 = IK1 * new fy1           for x1
    fmadds      fr8, fr0, fr2, fr4  // fr8 = IK0 * fy0 + IK1 * fy1   for x0

    ps_merge10  fr10, fr6, fr6      // fr10 = x1
    fadds       fr8, fr6, fr8       // fr8 = x0 + (IK0 * fy0 + IK1 * fy1)
    ps_merge10  fr11, fr7, fr7      // fr11 = x3

    // save x0
    psq_st      fr8, 0(r6), 1, 3    // destp + 0
    add         r4, r6, r7          // r4 = addr for save x1

    fmadds      fr9, fr0, fr8, fr9  // fr9 = IK0 * fy0 + IK1 * fy1   for x1
    fmuls       fr12, fr1, fr8      // fr12 = IK1 * new fy1          for x2

    fadds       fr10, fr10, fr9     // fr10 = x1 + (IK0 * fy0 + IK1 * fy1)

    // save x1
    add         r3, r4, r7          // r3 = addr for save x2
    psq_st      fr10, 0(r4), 1, 3   // destp + inc

    fmadds      fr9, fr0, fr10, fr12// fr9 = IK0 * fy0 + IK1 * fy1   for x2
    add         r4, r3, r7          // r4 = addr for save x3

    fmuls       fr12, fr1, fr10     // fr12 = IK1 *  new fy1         for x3
    fadds       fr7, fr7, fr9       // fr7 = x2 + (IK0 * fy0 + IK1 * fy1)

    // save x2
    psq_st      fr7, 0(r3), 1, 3    // destp + inc

    fmadds      fr9, fr0, fr7, fr12 // fr9 = IK0 * fy0 + IK1 * fy1   for x3

    // save fy1
    stfs        fr7, 4(r5)

    fadds       fr11, fr11, fr9     // fr11 = x3 + (IK0 * fy0 + IK1 * fy1)

    // save x3
    psq_st      fr11, 0(r4), 1, 3   // destp + inc

    // save fy0
    stfs        fr11, 0(r5)

    blr


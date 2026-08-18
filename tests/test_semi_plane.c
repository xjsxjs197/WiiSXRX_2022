/***************************************************************************
                          test_semi_plane.c
                             -------------------
    Host-side tests for the OpenGX semi-transparent plane reuse decision.
    Not part of the Wii build; compile with a host C compiler:
        cc -std=c99 -Wall -Wextra -Werror -I../deps/opengx \
           test_semi_plane.c -o test_semi_plane
 ***************************************************************************/

#include <stdio.h>

#include "../deps/opengx/gxSemiPlane.h"

static int s_failures = 0;

static void expect_action(GxSemiPlaneAction got, GxSemiPlaneAction expected)
{
    if (got != expected)
    {
        printf("FAIL action got %d expected %d\n", (int)got, (int)expected);
        s_failures++;
    }
}

int main(void)
{
    int hasSemiData = 0;

    /* A: first upload with semi texels -> allocate and copy. */
    expect_action(GxSemiPlaneUpdateAction(hasSemiData, 1),
                  GX_SEMI_ACTION_ALLOC_COPY);
    hasSemiData = 1;

    /* B: same-size reuse with different semi texels -> copy only. */
    expect_action(GxSemiPlaneUpdateAction(hasSemiData, 1),
                  GX_SEMI_ACTION_COPY);

    /* C: same-size reuse without semi texels -> retained plane untouched. */
    expect_action(GxSemiPlaneUpdateAction(hasSemiData, 0),
                  GX_SEMI_ACTION_NONE);

    /* D: semi texels return -> copy into the retained plane. */
    expect_action(GxSemiPlaneUpdateAction(hasSemiData, 1),
                  GX_SEMI_ACTION_COPY);

    if (s_failures == 0)
        printf("PASS semi_plane\n");
    else
        printf("FAIL semi_plane (%d)\n", s_failures);

    return s_failures == 0 ? 0 : 1;
}

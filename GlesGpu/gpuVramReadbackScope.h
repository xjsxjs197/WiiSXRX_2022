#ifndef GPU_VRAM_READBACK_SCOPE_H
#define GPU_VRAM_READBACK_SCOPE_H

/*
 * Dino Crisis 2 only consumes GPU-only framebuffer contents during the
 * underwater 320x240 C0/read + five-strip MoveImage sequence.  Keep the
 * recognition rules pure so they can be verified by the host test suite.
 */
enum
{
    DC2_READBACK_SCOPE_HOLD_FRAMES = 8
};

typedef struct DC2ReadbackScope
{
    unsigned int holdFrames;
    unsigned int generation;
} DC2ReadbackScope;

static inline int DC2ReadbackScopeIsTriggerC0(
    int x, int y, int w, int h)
{
    return x == 0 && y == 256 && w == 320 && h == 240;
}

static inline int DC2ReadbackScopeIsUnderwaterMove(
    int sourceX, int sourceY, int destinationX, int destinationY,
    int w, int h)
{
    return w == 64 && h == 240 &&
           destinationX == 448 && destinationY == 256 &&
           sourceX >= 0 && sourceX <= 256 && (sourceX & 63) == 0 &&
           (sourceY == 0 || sourceY == 256);
}

static inline void DC2ReadbackScopeArm(DC2ReadbackScope *scope)
{
    if (scope == 0)
        return;

    if (scope->holdFrames == 0)
    {
        scope->generation++;
        if (scope->generation == 0)
            scope->generation++;
    }
    scope->holdFrames = DC2_READBACK_SCOPE_HOLD_FRAMES;
}

static inline int DC2ReadbackScopeActive(const DC2ReadbackScope *scope)
{
    return scope != 0 && scope->holdFrames != 0;
}

/* Returns non-zero only on the active-to-inactive transition. */
static inline int DC2ReadbackScopeAdvanceFrame(DC2ReadbackScope *scope)
{
    if (scope == 0 || scope->holdFrames == 0)
        return 0;

    scope->holdFrames--;
    return scope->holdFrames == 0;
}

static inline int DC2ReadbackScopeAllowsWork(
    int isDinoCrisis2, const DC2ReadbackScope *scope)
{
    return !isDinoCrisis2 || DC2ReadbackScopeActive(scope);
}

static inline int DC2ReadbackScopeCaptureIsCurrent(
    int isDinoCrisis2, const DC2ReadbackScope *scope,
    unsigned int captureGeneration)
{
    return DC2ReadbackScopeAllowsWork(isDinoCrisis2, scope) &&
           (!isDinoCrisis2 ||
            (captureGeneration != 0 && scope != 0 &&
             captureGeneration == scope->generation));
}

#endif /* GPU_VRAM_READBACK_SCOPE_H */

/* Deterministic witness for the runtime's 8-byte allocator contract. */
#ifndef TEST_TIMEZONE_ALIGNMENT_ALLOCATOR_H
#define TEST_TIMEZONE_ALIGNMENT_ALLOCATOR_H
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct TzAlignmentAllocator {
    struct {
        void *original;
        void *returned;
    } blocks[8];
    int live, allocations, frees, fail_next, attempts, fail_on_attempt;
} TzAlignmentAllocator;

static void *tz_alignment_allocate(void *opaque, size_t size)
{
    TzAlignmentAllocator *w = opaque;
    unsigned char *original;
    void *returned;
    size_t i;
    w->attempts++;
    if (w->fail_next) { w->fail_next--; return NULL; }
    if (w->fail_on_attempt && w->attempts == w->fail_on_attempt) return NULL;
    if (size > SIZE_MAX - 23) return NULL;
    original = malloc(size + 23);
    if (!original) return NULL;
    returned = (void *)(((uintptr_t)original + 15) & ~(uintptr_t)15);
    returned = (unsigned char *)returned + 8;
    assert(((uintptr_t)returned & 15) == 8);
    for (i = 0; i < sizeof w->blocks / sizeof *w->blocks; i++) {
        if (!w->blocks[i].returned) {
            w->blocks[i].original = original;
            w->blocks[i].returned = returned;
            w->live++; w->allocations++;
            return returned;
        }
    }
    free(original);
    /* A system zone additionally owns its accepted immutable byte copy. */
    assert(0);
    return NULL;
}

static void tz_alignment_deallocate(void *opaque, void *pointer)
{
    TzAlignmentAllocator *w = opaque;
    size_t i;
    assert(pointer);
    for (i = 0; i < sizeof w->blocks / sizeof *w->blocks; i++) {
        if (w->blocks[i].returned == pointer) {
            assert(w->live > 0);
            free(w->blocks[i].original);
            w->blocks[i].original = w->blocks[i].returned = NULL;
            w->live--; w->frees++;
            return;
        }
    }
    assert(0); /* The owner must free the exact allocation result. */
}
#endif

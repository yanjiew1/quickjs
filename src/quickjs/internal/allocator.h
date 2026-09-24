/*
 * QuickJS Allocator Types
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef QUICKJS_INTERNAL_ALLOCATOR_H
#define QUICKJS_INTERNAL_ALLOCATOR_H

#include "base.h"

/* JS malloc */

#define JS_MALLOC_ALIGN 8
#define JS_MALLOC_ARENA_SIZE 4096
#define JS_MALLOC_BLOCK_SIZE_COUNT 31
#define JS_MALLOC_MIN_SMALL_SIZE 16
#define JS_MALLOC_MAX_SMALL_SIZE 512
#if defined(__SANITIZE_ADDRESS__)
/* use the host malloc() for all allocations */
#define JS_MALLOC_LARGE_BLOCKS_ONLY 1
#else
#define JS_MALLOC_LARGE_BLOCKS_ONLY 0
#endif

/* allow iteration among the allocated blocks. Currently not used. May
   be used to suppress the memory overhead of JSGCObjectHeader */
//#define JS_MALLOC_USE_ITER

#define FREE_NIL 0xffff

/* 8 byte header */
/* Notes:
   - the header is necessary at least to recover a pointer to
     JSMallocArena because we don't want to enforce a page
     alignment on the system malloc().
   - could store the block offset instead of (block_idx,
   block_size_idx), but it would require a division to recover the block
   index.
*/
typedef struct JSMallocBlockHeader {
    union {
        uint16_t block_idx; /* FREE_NIL if large block */
        uint16_t free_next; /* FREE_NIL if none */
    } u;
    uint8_t block_size_idx;
    uint8_t gc_obj_type : 7;
    uint8_t mark : 1;
    int ref_count;
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t user_data[];
} JSMallocBlockHeader;

typedef struct JSMallocLargeBlockHeader {
#ifdef JS_MALLOC_USE_ITER
    struct list_head link;
#endif
    JSMallocBlockHeader header;
} JSMallocLargeBlockHeader;

typedef struct {
    struct list_head free_link;
    struct list_head link;
    uint8_t block_size_idx;
    uint16_t n_used_blocks; /* number of allocated blocks */
    uint16_t n_blocks; /* total number of blocks */
    uint16_t first_free_block; /* FREE_NIL if none */
#ifdef JS_MALLOC_USE_ITER
    /* bit set to 1 for allocated block */
    uint32_t bitmap[((JS_MALLOC_ARENA_SIZE / JS_MALLOC_MIN_SMALL_SIZE) + 31) / 32];
#endif
    /* n_blocks memory blocks of identical size */
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t blocks[];
} JSMallocArena;

typedef struct {
    struct list_head arena_list[JS_MALLOC_BLOCK_SIZE_COUNT]; /* list of JSMallocArena.link (all arenas) */
    struct list_head free_arena_list[JS_MALLOC_BLOCK_SIZE_COUNT]; /* list of JSMallocArena.free_link (arenas where n_used_blocks < n_blocks) */
#ifdef JS_MALLOC_USE_ITER
    struct list_head large_block_list; /* list of JSMallocLargeBlockHeader.link */
#endif
    __attribute__((aligned(JS_MALLOC_ALIGN))) uint8_t zero_size_block[sizeof(JSMallocBlockHeader)];

    /* callbacks to the host malloc */
    JSMallocFunctions mf;
    JSMallocState malloc_state;
} JSMallocContext;

/* end JS Malloc */

void js_malloc_init(JSMallocContext *s);
void js_trigger_gc(JSRuntime *rt, size_t size);
void *js_realloc_bytecode_rt(void *opaque, void *ptr, size_t size);

static inline JSMallocBlockHeader *js_rc(void *ptr)
{
    return container_of(ptr, JSMallocBlockHeader, user_data);
}

no_inline int js_realloc_array(JSContext *ctx, void **parray,
                                      int elem_size, int *psize, int req_size);

static inline int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size)
{
    if (unlikely(req_size > *psize))
        return js_realloc_array(ctx, parray, elem_size, psize, req_size);
    else
        return 0;
}

#endif /* QUICKJS_INTERNAL_ALLOCATOR_H */

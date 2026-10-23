/* Persistent Unicode range callbacks, family candidates and physical OOM.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/segment/routing.h"
#include <assert.h>
#include <stdlib.h>

typedef struct Allocation { size_t calls, fail_at, live; } Allocation;

static void *allocate(void *opaque, size_t size)
{
    Allocation *state = opaque;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = malloc(size);
    if (ptr)
        state->live++;
    return ptr;
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    Allocation *state = opaque;
    void *result;
    int fresh = ptr == NULL;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    result = realloc(ptr, size);
    if (result && fresh)
        state->live++;
    return result;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    assert(ptr && state->live);
    state->live--;
    free(ptr);
}

static void check_masks(const QJSIntlSegmentRouting *routing)
{
    static const struct Example {
        uint32_t scalar;
        QJSIntlSegmentDictionaryFamily family;
    } examples[] = {
        {0x4e00, QJS_INTL_DICTIONARY_CJK},
        {0x20000, QJS_INTL_DICTIONARY_CJK},
        {0x3042, QJS_INTL_DICTIONARY_CJK},
        {0x30a2, QJS_INTL_DICTIONARY_CJK},
        {0x1780, QJS_INTL_DICTIONARY_KHMER},
        {0xe81, QJS_INTL_DICTIONARY_LAO},
        {0x1000, QJS_INTL_DICTIONARY_MYANMAR},
        {0xe01, QJS_INTL_DICTIONARY_THAI},
    };
    size_t i;
    for (i = 0; i < sizeof(examples) / sizeof(examples[0]); i++) {
        unsigned int mask = 1u << examples[i].family;
        assert(qjs_intl_segment_routing_mask(routing, examples[i].scalar, 0) == mask);
        assert(qjs_intl_segment_routing_mask(routing, examples[i].scalar, 1) & mask);
    }
    assert(!qjs_intl_segment_routing_mask(routing, 'A', 0));
    assert(!qjs_intl_segment_routing_mask(routing, ' ', 1));
    assert(!qjs_intl_segment_routing_mask(routing, 0x1f600, 1));
    assert(!qjs_intl_segment_routing_mask(routing, 0xd800, 0));
    assert(!qjs_intl_segment_routing_mask(routing, 0x110000, 1));
    assert(!qjs_intl_segment_routing_mask(routing, 0x4e00, -1));
    assert(!qjs_intl_segment_routing_mask(NULL, 0x4e00, 0));
    /* Script_Extensions supplies Japanese shared characters and punctuation;
       a mask is deliberately not a dictionary-core admission decision. */
    assert(!qjs_intl_segment_routing_mask(routing, 0x30fc, 0));
    assert(qjs_intl_segment_routing_mask(routing, 0x30fc, 1) & (1u << QJS_INTL_DICTIONARY_CJK));
    assert(qjs_intl_segment_routing_mask(routing, 0x3001, 1) & (1u << QJS_INTL_DICTIONARY_CJK));
    assert(qjs_intl_segment_routing_inherited(routing, 0x34f));
    assert(!qjs_intl_segment_routing_inherited(routing, 0x93e));
    assert(!qjs_intl_segment_routing_inherited(routing, 0x1037));
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlSegmentRouting *routing;
    size_t calls, i;
    assert(!qjs_intl_segment_routing_open(&allocator, &routing));
    calls = state.calls;
    assert(calls && state.live);
    check_masks(routing);
    assert(state.calls == calls);
    qjs_intl_segment_routing_close(routing);
    assert(!state.live);
    /* Every real malloc/realloc point must release all earlier ranges, not
       merely the cache struct. Retrying after the entire sweep must succeed. */
    for (i = 1; i <= calls; i++) {
        state.calls = 0;
        state.fail_at = i;
        routing = (void *)1;
        assert(qjs_intl_segment_routing_open(&allocator, &routing) == QJS_INTL_NO_MEMORY);
        assert(!routing && !state.live);
    }
    state.fail_at = 0;
    assert(!qjs_intl_segment_routing_open(&allocator, &routing));
    check_masks(routing);
    qjs_intl_segment_routing_close(routing);
    qjs_intl_segment_routing_close(NULL);
    assert(!state.live);
    return 0;
}

/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/plural-native.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct Heap { size_t calls, fail_at, live; } Heap;
static void *allocate(void *opaque, size_t size)
{
    Heap *heap = opaque;
    void *ptr;
    if (heap->calls++ == heap->fail_at) return NULL;
    ptr = malloc(size);
    if (ptr) heap->live++;
    return ptr;
}
static void release(void *opaque, void *ptr)
{
    Heap *heap = opaque;
    if (ptr) { assert(heap->live); heap->live--; free(ptr); }
}

int main(void)
{
    char relation[] = "i % 10 = 1 and v = 0";
    QJSIntlPluralRule rules[] = {
        {QJS_INTL_PLURAL_ONE, {relation, sizeof(relation) - 1}},
        {QJS_INTL_PLURAL_OTHER, {"", 0}}
    };
    QJSIntlPluralRulesData data = {rules, 2, NULL, 0};
    QJSIntlPluralOptions options = {0};
    Heap heap = {0, SIZE_MAX, 0};
    QJSIntlAllocator allocator = {&heap, allocate, NULL, release};
    QJSIntlNativePlural *handle;
    QJSIntlPluralCategory category;
    JSIntlPluralOperands operands = {"90071992547409921", 17, 0};
    size_t calls, failure;
    options.digits.minimum_integer_digits = 1;
    options.digits.maximum_fraction_digits = 3;
    options.digits.minimum_significant_digits = 1;
    options.digits.maximum_significant_digits = 21;
    options.digits.rounding_increment = 1;
    options.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    options.digits.maximum_output_length = 128;
    assert(qjs_intl_native_plural_open(&allocator, &options, &data, &handle) == QJS_INTL_OK);
    calls = heap.calls;
    relation[0] = 'x'; /* Open owns the original rule, independent of input. */
    assert(qjs_intl_native_plural_select_operands(handle, &operands, &category) == QJS_INTL_OK);
    assert(category == QJS_INTL_PLURAL_ONE);
    operands.decimal = "1.0"; operands.length = 3;
    assert(qjs_intl_native_plural_select_operands(handle, &operands, &category) == QJS_INTL_OK);
    assert(category == QJS_INTL_PLURAL_OTHER);
    assert(qjs_intl_native_plural_range_categories(handle, QJS_INTL_PLURAL_ONE,
                QJS_INTL_PLURAL_ONE, &category) == QJS_INTL_OK);
    assert(category == QJS_INTL_PLURAL_OTHER); /* Absent LDML range default. */
    qjs_intl_native_plural_close(handle);
    assert(!heap.live);
    relation[0] = 'i';
    for (failure = 0; failure < calls; failure++) {
        heap = (Heap){0, failure, 0};
        handle = NULL;
        assert(qjs_intl_native_plural_open(&allocator, &options, &data, &handle) == QJS_INTL_NO_MEMORY);
        assert(!handle && !heap.live);
    }
    return 0;
}

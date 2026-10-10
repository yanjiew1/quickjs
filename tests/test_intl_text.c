/* Private UTF16 bridge ownership and failure regression.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include "quickjs.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef CONFIG_INTL
#include "../src/quickjs/builtins/intl/intl-text.h"
#include "../src/quickjs/internal/string.h"
#include "../src/quickjs/internal/error.h"
#include "intl-allocation-probe.h"

static void clear_exception(JSContext *ctx)
{
    JSValue exception;
    assert(JS_HasException(ctx));
    exception = JS_GetException(ctx);
    JS_FreeValue(ctx, exception);
    assert(!JS_HasException(ctx));
}

static JSValue evaluate(JSContext *ctx, const char *source)
{
    JSValue value = JS_Eval(ctx, source, strlen(source), "intl-text-test",
                            JS_EVAL_TYPE_GLOBAL);
    assert(!JS_IsException(value));
    return value;
}

static void check_string(JSValueConst value, const uint16_t *expected, size_t n)
{
    JSString *string;
    size_t i;
    assert(JS_VALUE_GET_TAG(value) == JS_TAG_STRING);
    string = JS_VALUE_GET_STRING(value);
    assert(string->len == n);
    for (i = 0; i < n; i++)
        assert(string_get(string, i) == expected[i]);
}

static void check_copy(JSContext *ctx, JSValueConst value,
                       const uint16_t *expected, size_t n)
{
    uint16_t *text = NULL;
    int32_t length = -1;
    JSValue roundtrip;
    assert(js_intl_to_utf16(ctx, value, &text, &length) == 0);
    assert(!JS_HasException(ctx) && text && length == (int32_t)n);
    assert(!memcmp(text, expected, n * sizeof(*text)));
    assert(text[n] == 0);
    roundtrip = js_intl_from_utf16(ctx, text, length);
    assert(!JS_IsException(roundtrip));
    /* The JS result owns its copy after the bridge buffer is freed. */
    js_free(ctx, text);
    check_string(roundtrip, expected, n);
    JS_FreeValue(ctx, roundtrip);
}

static void check_values(JSContext *ctx)
{
    const uint16_t empty[] = { 0 };
    const uint16_t narrow[] = { 0x41, 0, 0xff, 0x42 };
    const uint16_t wide[] = { 0x41, 0, 0xd800, 0x42, 0xdc00,
                              0xd83d, 0xde00, 0x65, 0x301 };
    JSValue value, marker, exception;
    uint16_t sentinel = 0, *text = &sentinel;
    int32_t length = 99;
    value = JS_NewString(ctx, "");
    assert(!JS_IsException(value));
    check_copy(ctx, value, empty, 0);
    JS_FreeValue(ctx, value);
    value = js_new_string8_len(ctx, "A\0\xff" "B", 4);
    assert(!JS_IsException(value));
    assert(!JS_VALUE_GET_STRING(value)->is_wide_char);
    check_copy(ctx, value, narrow, 4);
    JS_FreeValue(ctx, value);
    value = js_new_string16_len(ctx, wide, sizeof(wide) / sizeof(*wide));
    assert(!JS_IsException(value));
    check_copy(ctx, value, wide, sizeof(wide) / sizeof(*wide));
    JS_FreeValue(ctx, value);

    value = evaluate(ctx,
        "globalThis.textCoercions = 0; ({ [Symbol.toPrimitive](hint) {"
        " if (hint !== 'string') throw Error('hint'); textCoercions++;"
        " return 'A\\0\\ud800B\\udc00\\ud83d\\ude00e\\u0301'; } })");
    check_copy(ctx, value, wide, sizeof(wide) / sizeof(*wide));
    JS_FreeValue(ctx, value);
    value = evaluate(ctx, "textCoercions === 1");
    assert(JS_ToBool(ctx, value) == 1);
    JS_FreeValue(ctx, value);

    marker = evaluate(ctx, "globalThis.textMarker = ({failure: true})");
    value = evaluate(ctx,
        "({ toString() { textCoercions++; throw textMarker; } })");
    assert(js_intl_to_utf16(ctx, value, &text, &length) == -1);
    assert(text == NULL && length == 0 && JS_HasException(ctx));
    exception = JS_GetException(ctx);
    assert(JS_VALUE_GET_TAG(exception) == JS_VALUE_GET_TAG(marker));
    assert(JS_VALUE_GET_PTR(exception) == JS_VALUE_GET_PTR(marker));
    JS_FreeValue(ctx, exception);
    JS_FreeValue(ctx, marker);
    JS_FreeValue(ctx, value);
    value = evaluate(ctx, "textCoercions === 2");
    assert(JS_ToBool(ctx, value) == 1);
    JS_FreeValue(ctx, value);

    assert(js_intl_alloc_utf16(ctx, -1) == NULL);
    clear_exception(ctx);
    assert(js_intl_alloc_utf16(ctx, INT32_MAX) == NULL);
    clear_exception(ctx);
    value = js_intl_from_utf16(ctx, wide, -1);
    assert(JS_IsException(value));
    clear_exception(ctx);
    value = js_intl_from_utf16(ctx, wide, INT32_MAX);
    assert(JS_IsException(value));
    clear_exception(ctx);
}

/* Large strings bypass small-block arenas, so each owned text allocation
 * reaches the existing host allocator probe. A fresh runtime and input for
 * each position also prevents a flattened rope cache from hiding failures. */
enum TextOperation { TEXT_COPY, TEXT_ROPE, TEXT_NEW };
#define TEXT_LENGTH 4096
static size_t probe_operation(enum TextOperation operation, size_t failure_at)
{
    const uint16_t pattern[] = { 0x41, 0, 0xd800, 0x42, 0xdc00, 0xd83d, 0xde00, 0x301 };
    uint16_t expected[TEXT_LENGTH], sentinel = 0, *text = &sentinel;
    AllocationProbe probe = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
    JSContext *ctx;
    JSValue input = JS_UNDEFINED, result = JS_UNDEFINED;
    int32_t length = 99;
    int status = 0;
    size_t i, count;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    for (i = 0; i < TEXT_LENGTH; i++)
        expected[i] = pattern[i % (sizeof(pattern) / sizeof(*pattern))];
    if (operation == TEXT_ROPE) {
        JSValue left = js_new_string16_len(ctx, expected, TEXT_LENGTH / 2);
        JSValue right = js_new_string16_len(ctx, expected + TEXT_LENGTH / 2,
                                           TEXT_LENGTH / 2);
        assert(!JS_IsException(left) && !JS_IsException(right));
        input = JS_ConcatString(ctx, left, right);
        assert(JS_VALUE_GET_TAG(input) == JS_TAG_STRING_ROPE);
    } else if (operation == TEXT_COPY) {
        input = js_new_string16_len(ctx, expected, TEXT_LENGTH);
        assert(!JS_IsException(input));
    }
    JS_ThrowOutOfMemory(ctx);
    clear_exception(ctx);
    probe.failure_at = failure_at ? failure_at : SIZE_MAX;
    probe.attempts = 0;
    if (operation == TEXT_NEW)
        result = js_intl_from_utf16(ctx, expected, TEXT_LENGTH);
    else
        status = js_intl_to_utf16(ctx, input, &text, &length);
    count = probe.attempts;
    probe.failure_at = 0;
    if (failure_at) {
        assert(probe.failed);
        if (operation == TEXT_NEW)
            assert(JS_IsException(result));
        else
            assert(status == -1 && text == NULL && length == 0);
        clear_exception(ctx);
        /* Retry in the same runtime after clearing the exception. */
        if (operation == TEXT_NEW)
            result = js_intl_from_utf16(ctx, expected, TEXT_LENGTH);
        else
            status = js_intl_to_utf16(ctx, input, &text, &length);
    }
    assert(!JS_HasException(ctx));
    if (operation == TEXT_NEW) {
        check_string(result, expected, TEXT_LENGTH);
        JS_FreeValue(ctx, result);
    } else {
        assert(status == 0 && text && length == TEXT_LENGTH);
        assert(!memcmp(text, expected, sizeof(expected)) && text[TEXT_LENGTH] == 0);
        js_free(ctx, text);
    }
    JS_FreeValue(ctx, input);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(probe.live == 0);
    return count;
}
#endif

int main(void)
{
#ifdef CONFIG_INTL
    AllocationProbe probe = { 0 };
    JSRuntime *rt = JS_NewRuntime2(&probe_functions, &probe);
    JSContext *ctx;
    enum TextOperation operation;
    size_t count, position;
    assert(rt);
    ctx = JS_NewContext(rt);
    assert(ctx);
    check_values(ctx);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    assert(probe.live == 0);
    for (operation = TEXT_COPY; operation <= TEXT_NEW; operation++) {
        count = probe_operation(operation, 0);
        assert(count > 0);
        if (operation == TEXT_ROPE)
            assert(count >= 2); /* flatten plus owned UTF16 copy */
        for (position = 1; position <= count; position++)
            probe_operation(operation, position);
    }
    puts("Intl UTF16 text bridge passed");
#endif
    return 0;
}

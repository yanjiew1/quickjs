/* ResolveLocale negotiation, null hc, extensions and independent allocators.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../src/intl/locale/resolve.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

static void *resize(void *opaque, void *old, size_t size)
{
    Allocation *state = opaque;
    int had_old = old != NULL;
    void *ptr;
    assert(size);
    if (++state->calls == state->fail_at)
        return NULL;
    ptr = realloc(old, size);
    if (ptr && !had_old)
        state->live++;
    return ptr;
}

static void release(void *opaque, void *ptr)
{
    Allocation *state = opaque;
    if (!ptr)
        return;
    assert(state->live);
    state->live--;
    free(ptr);
}

static QJSIntlBytes bytes(const char *text) { return (QJSIntlBytes){text, strlen(text)}; }

typedef struct Capability {
    const char *types;
    unsigned int nullable;
    QJSIntlStatus error;
    size_t calls;
} Capability;

static QJSIntlStatus supported(void *opaque, QJSIntlBytes value, int *out)
{
    Capability *capability = opaque;
    const char *at = capability->types;
    capability->calls++;
    *out = 0;
    if (capability->error)
        return capability->error;
    if (!value.data) {
        assert(!value.length);
        *out = capability->nullable;
        return QJS_INTL_OK;
    }
    while (*at) {
        const char *end = strchr(at, ' ');
        size_t length = end ? (size_t)(end - at) : strlen(at);
        if (length == value.length && !memcmp(at, value.data, length))
            *out = 1;
        if (!end)
            break;
        at = end + 1;
    }
    return QJS_INTL_OK;
}

static void expected(const QJSIntlResolvedLocale *result, const char *locale,
                     const char *calendar, const char *hour, const char *number)
{
    assert(result->count == 3 && result->values && !strcmp(result->locale, locale));
    assert(!strcmp(result->values[0], calendar) && !strcmp(result->values[2], number));
    assert(hour ? result->values[1] && !strcmp(result->values[1], hour) : !result->values[1]);
}

int main(void)
{
    Allocation input = {0}, output = {0};
    QJSIntlAllocator a = {&input, allocate, resize, release};
    QJSIntlAllocator b = {&output, allocate, resize, release};
    QJSIntlLocaleService *identity;
    QJSIntlResolvedLocale result, zero = {0};
    Capability calendars = {.types = "gregory buddhist islamic-civil"};
    Capability hours = {.types = "h11 h12 h23 h24", .nullable = 1};
    Capability numbers = {.types = "latn arab mathbold"};
    QJSIntlLocaleResolveKey keys[] = {
        {{'c', 'a'}, {"gregory", 7}, {0}, 0, supported, &calendars},
        {{'h', 'c'}, {0}, {0}, 0, supported, &hours},
        {{'n', 'u'}, {"latn", 4}, {0}, 0, supported, &numbers}
    };
    QJSIntlBytes extension = bytes("-u-ca-buddhist-hc-h23-nu-arab");
    size_t i, calls, baseline;
    assert(!qjs_intl_locale_open(&a, NULL, &identity));
    baseline = input.live;
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
    expected(&result, "en-u-ca-buddhist-hc-h23-nu-arab", "buddhist", "h23", "arab");
    qjs_intl_resolved_locale_clear(&result);
    keys[1].option_is_null = 1;
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
    expected(&result, "en-u-ca-buddhist-nu-arab", "buddhist", NULL, "arab");
    qjs_intl_resolved_locale_clear(&result);
    keys[2].option = bytes("ARAB");
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
    expected(&result, "en-u-ca-buddhist-nu-arab", "buddhist", NULL, "arab");
    qjs_intl_resolved_locale_clear(&result);
    keys[2].option = bytes("finance");
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
    expected(&result, "en-u-ca-buddhist-nu-arab", "buddhist", NULL, "arab");
    qjs_intl_resolved_locale_clear(&result);
    keys[2].option = bytes("LATN");
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
    expected(&result, "en-u-ca-buddhist", "buddhist", NULL, "latn");
    qjs_intl_resolved_locale_clear(&result);
    hours.nullable = 0;
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
    expected(&result, "en-u-ca-buddhist-hc-h23", "buddhist", "h23", "latn");
    qjs_intl_resolved_locale_clear(&result);
    hours.nullable = 1;
    keys[0].option = bytes("islamicc");
    calendars.calls = 0;
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), bytes("-u-ca-islamic-civil"), keys, 1, &result));
    assert(result.count == 1 && !strcmp(result.values[0], "islamic-civil") &&
           !strcmp(result.locale, "en-u-ca-islamic-civil"));
    /* The option aliases the accepted extension, so SameValue avoids a second
       membership query. Treating the raw alias as unsupported would not. */
    assert(calendars.calls == 1);
    qjs_intl_resolved_locale_clear(&result);
    keys[0].option = (QJSIntlBytes){0};
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, NULL, 0, &result));
    assert(!result.count && !result.values && !strcmp(result.locale, "en"));
    qjs_intl_resolved_locale_clear(&result);
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), bytes("-u-ca-unknown-hc-zzzz-nu-zzzz"),
                                         keys, 3, &result));
    expected(&result, "en", "gregory", NULL, "latn");
    qjs_intl_resolved_locale_clear(&result);
    hours.error = QJS_INTL_DATA_ERROR;
    assert(qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result) == QJS_INTL_DATA_ERROR);
    assert(!memcmp(&result, &zero, sizeof(result)) && !output.live && input.live == baseline);
    hours.error = 0;
    {
        Capability boolean = {.types = "true false"};
        QJSIntlLocaleResolveKey key = {{'k', 'n'}, {"false", 5}, {0}, 0, supported, &boolean};
        assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), bytes("-u-kn"), &key, 1, &result));
        assert(result.count == 1 && !strcmp(result.values[0], "true") && !strcmp(result.locale, "en-u-kn"));
        qjs_intl_resolved_locale_clear(&result);
        key.option = bytes("false");
        assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), bytes("-u-kn"), &key, 1, &result));
        assert(!strcmp(result.values[0], "false") && !strcmp(result.locale, "en"));
        qjs_intl_resolved_locale_clear(&result);
    }
    /* Fail each allocator independently; neither may free the other's state. */
    for (i = 0; i < 2; i++) {
        Allocation *state = i ? &input : &output;
        size_t fail;
        state->calls = 0;
        assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
        calls = state->calls;
        qjs_intl_resolved_locale_clear(&result);
        for (fail = 1; fail <= calls; fail++) {
            state->calls = 0;
            state->fail_at = fail;
            assert(qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result) == QJS_INTL_NO_MEMORY);
            assert(!memcmp(&result, &zero, sizeof(result)) && !output.live && input.live == baseline);
        }
        state->calls = state->fail_at = 0;
    }
    keys[1].key[0] = 'c'; keys[1].key[1] = 'a';
    assert(qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(&result, &zero, sizeof(result)) && !output.live);
    keys[1].key[0] = 'h'; keys[1].key[1] = 'c';
    assert(!qjs_intl_resolve_locale_keys(identity, &b, bytes("en"), extension, keys, 3, &result));
    /* Results own their strings independently of the identity service. */
    qjs_intl_locale_close(identity);
    assert(!input.live);
    expected(&result, "en-u-ca-buddhist", "buddhist", NULL, "latn");
    qjs_intl_resolved_locale_clear(&result);
    assert(!output.live);
    return 0;
}

/* Actual single-bundle provider snapshot and result lifetime.
 * Copyright (c) 2026 Yan-Jie Wang. */
#include "../src/intl/provider-native-duration.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef union Header { long double align; void *pointer; size_t size; } Header;
typedef struct AllocationState { size_t calls, live, fail; } AllocationState;
static void *allocate(void *opaque, size_t size)
{
    AllocationState *s = opaque;
    Header *p;
    s->calls++;
    if (s->fail && s->calls == s->fail) return NULL;
    if (size > SIZE_MAX - sizeof(*p)) return NULL;
    p = malloc(sizeof(*p) + size);
    if (!p) return NULL;
    p->size = size; s->live++; return p + 1;
}
static void release(void *opaque, void *pointer)
{
    AllocationState *s = opaque;
    if (pointer) { assert(s->live); s->live--; free((Header *)pointer - 1); }
}
static void *resize(void *opaque, void *pointer, size_t size)
{
    AllocationState *s = opaque;
    Header *p;
    if (!pointer) return allocate(opaque, size);
    if (!size) { release(opaque, pointer); return NULL; }
    s->calls++;
    if (s->fail && s->calls == s->fail) return NULL;
    if (size > SIZE_MAX - sizeof(*p)) return NULL;
    p = realloc((Header *)pointer - 1, sizeof(*p) + size);
    if (!p) return NULL;
    p->size = size; return p + 1;
}
static void options(QJSIntlDurationOptions *o)
{
    int i;
    memset(o, 0, sizeof(*o));
    o->style = QJS_INTL_DURATION_DIGITAL;
    o->fractional_digits = 9;
    o->maximum_output_length = 1048576;
    for (i = 0; i < 10; i++) {
        o->units[i].style = i < 4 ? QJS_INTL_DURATION_UNIT_SHORT :
            i == 4 ? QJS_INTL_DURATION_NUMERIC :
            i < 7 ? QJS_INTL_DURATION_TWO_DIGIT : QJS_INTL_DURATION_FRACTIONAL;
        o->units[i].always = i >= 4 && i <= 6;
    }
}
static int text_is(const QJSIntlFormatted *f, const char *text)
{
    size_t i;
    if (f->length != strlen(text)) return 0;
    for (i = 0; i < f->length; i++) if (f->text[i] != (unsigned char)text[i]) return 0;
    return 1;
}
int main(void)
{
    AllocationState state = {0};
    QJSIntlProviderConfig config;
    QJSIntlProvider *provider = NULL;
    QJSIntlNativeDuration *duration = NULL;
    QJSIntlNativeDuration *unsupported = NULL;
    QJSIntlDurationOptions o;
    QJSIntlDurationRecord record;
    QJSIntlFormatted output = {0};
    QJSIntlAllocator allocator = {&state, allocate, resize, release};
    QJSIntlTagList systems = {0};
    QJSIntlBytes locale = {"en-US",5}, numbering = {"latn",4};
    size_t i, position, calls, baseline;
    uint8_t two;
    memset(&config, 0, sizeof(config));
    config.backend = QJS_INTL_BACKEND_NATIVE;
    config.allocator = allocator;
    config.default_locale = (QJSIntlBytes){"en-US",5};
    config.default_time_zone = (QJSIntlBytes){"UTC",3};
    assert(qjs_intl_provider_new(&config, &provider) == QJS_INTL_OK);
    assert(qjs_intl_native_provider_duration_available(provider) == QJS_INTL_OK);
    assert(qjs_intl_native_provider_duration_key_values(provider, locale,
            (QJSIntlBytes){"nu",2}, &systems) == QJS_INTL_OK);
    assert(systems.count && systems.items[0].length == 4 && !memcmp(systems.items[0].data,"latn",4));
    for (i = 0; i < systems.count; i++)
        assert(qjs_intl_native_provider_duration_clock(provider, locale, systems.items[i], &two) == QJS_INTL_OK);
    qjs_intl_tag_list_clear(provider, &systems);
    assert(qjs_intl_native_provider_duration_clock(provider, locale, numbering, &two) == QJS_INTL_OK && !two);
    options(&o);
    assert(qjs_intl_native_provider_duration_open(provider, locale, numbering, &o, &duration) == QJS_INTL_OK);
    assert(qjs_intl_native_provider_duration_open(provider, (QJSIntlBytes){"fi",2}, numbering, &o,
            &unsupported) == QJS_INTL_UNSUPPORTED && !unsupported);
    memset(&record, 0, sizeof(record));
    for (i = 0; i < 10; i++) record.magnitudes[i] = (QJSIntlBytes){"0",1};
    record.sign = 1;
    record.magnitudes[4] = (QJSIntlBytes){"1",1};
    record.magnitudes[5] = (QJSIntlBytes){"2",1};
    record.magnitudes[6] = (QJSIntlBytes){"3",1};
    record.magnitudes[9] = (QJSIntlBytes){"1",1};
    /* Provider and borrowed view are released before the retained handle runs. */
    qjs_intl_provider_free(provider); provider = NULL;
    assert(qjs_intl_native_duration_format(duration, &record, &output) == QJS_INTL_OK);
    assert(text_is(&output, "1:02:03.000000001"));
    baseline = state.live;
    /* Every warm format allocation position; restore after each failure. */
    state.calls = 0;
    {
        QJSIntlFormatted probe = {0};
        assert(qjs_intl_native_duration_format(duration, &record, &probe) == QJS_INTL_OK);
        calls = state.calls;
        qjs_intl_native_duration_result_clear(&allocator, &probe);
    }
    assert(state.live == baseline);
    for (position = 1; position <= calls; position++) {
        QJSIntlFormatted failed = {0}, retry = {0};
        state.calls = 0; state.fail = position;
        assert(qjs_intl_native_duration_format(duration, &record, &failed) == QJS_INTL_NO_MEMORY);
        state.fail = 0;
        assert(!failed.text && !failed.parts && !failed.length && !failed.part_count);
        qjs_intl_native_duration_result_clear(&allocator, &failed);
        assert(state.live == baseline);
        assert(qjs_intl_native_duration_format(duration, &record, &retry) == QJS_INTL_OK);
        assert(text_is(&retry, "1:02:03.000000001"));
        qjs_intl_native_duration_result_clear(&allocator, &retry);
        assert(state.live == baseline);
    }
    qjs_intl_native_duration_close(duration);
    /* Owned result text, parts and static unit names survive handle close. */
    assert(text_is(&output, "1:02:03.000000001"));
    assert(output.parts[0].unit.length == 4 && !memcmp(output.parts[0].unit.data,"hour",4));
    qjs_intl_native_duration_result_clear(&allocator, &output);
    assert(!state.live);
    printf("native-duration-provider: warm allocation positions=%zu\n", calls);
    return 0;
}

/* Native Locale information ownership against the compiled CLDR 48.2 bundle.
 * Copyright (c) 2026 Yan-Jie Wang. SPDX-License-Identifier: MIT */
#include "../src/intl/native-locale-info.h"
#include "../src/intl/data/locale-metadata.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct AllocationState {
    size_t attempts, fail_at, live;
} AllocationState;

static void *allocate(void *opaque, size_t size)
{
    AllocationState *state = opaque;
    void *result;
    assert(size);
    if (++state->attempts == state->fail_at)
        return NULL;
    result = malloc(size);
    if (result)
        state->live++;
    return result;
}

static void *resize(void *opaque, void *ptr, size_t size)
{
    AllocationState *state = opaque;
    int fresh = ptr == NULL;
    void *result;
    assert(size);
    if (++state->attempts == state->fail_at)
        return NULL;
    result = realloc(ptr, size);
    if (result && fresh)
        state->live++;
    return result;
}

static void release(void *opaque, void *ptr)
{
    AllocationState *state = opaque;
    if (ptr) {
        assert(state->live);
        state->live--;
        free(ptr);
    }
}

static QJSIntlBytes bytes(const char *text)
{
    QJSIntlBytes result = { text, strlen(text) };
    return result;
}

static void zero_result(const QJSIntlNativeLocaleInfoResult *result)
{
    assert(!result->list.items && !result->list.count && !result->value &&
           !result->defined && !result->first_day && !result->weekend_mask &&
           result->direction == QJS_INTL_LOCALE_DIRECTION_UNDEFINED);
}

int main(void)
{
    static const QJSIntlLocaleInfoField fields[] = {
        QJS_INTL_LOCALE_HOUR_CYCLES, QJS_INTL_LOCALE_WEEK_INFO,
        QJS_INTL_LOCALE_TEXT_INFO, QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK,
        QJS_INTL_LOCALE_CALENDARS, QJS_INTL_LOCALE_COLLATIONS,
        QJS_INTL_LOCALE_NUMBERING_SYSTEMS
    };
    AllocationState state = { 0 };
    QJSIntlAllocator allocator = { &state, allocate, resize, release };
    QJSIntlDataView view;
    /* No capability is inferred from metadata. Explicit constructor slots
       are independent of formatter availability. */
    QJSIntlLocaleInfoCapabilities capabilities = { 0 };
    QJSIntlLocaleInfoRequest request = { 0 };
    QJSIntlNativeLocaleInfoResult result;
    size_t i, count, failure;

    assert(qjs_intl_data_open(qjs_intl_locale_metadata_blob,
                             qjs_intl_locale_metadata_blob_size, &view) ==
           QJS_INTL_DATA_OK);
    request.locale = bytes("en-US-u-rg-gbzzzz");
    request.first_day = bytes("tue");
    request.calendar = bytes("unknown");
    request.collation = bytes("search");
    request.numbering_system = bytes("unknown");
    for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        state.attempts = 0;
        assert(qjs_intl_native_locale_info_get(&allocator, &view, &capabilities,
               &request, fields[i], &result) == QJS_INTL_OK);
        count = state.attempts;
        assert(count && result.defined);
        if (fields[i] == QJS_INTL_LOCALE_WEEK_INFO)
            assert(result.first_day == 2 && result.weekend_mask == 96);
        if (fields[i] == QJS_INTL_LOCALE_TEXT_INFO)
            assert(result.direction == QJS_INTL_LOCALE_DIRECTION_LTR);
        if (fields[i] == QJS_INTL_LOCALE_FIRST_DAY_OF_WEEK)
            assert(result.value && !strcmp(result.value, "tue"));
        qjs_intl_native_locale_info_result_clear(&allocator, &result);
        zero_result(&result);
        assert(!state.live);
        for (failure = 1; failure <= count; failure++) {
            state.attempts = 0;
            state.fail_at = failure;
            memset(&result, 0xff, sizeof(result));
            assert(qjs_intl_native_locale_info_get(&allocator, &view,
                   &capabilities, &request, fields[i], &result) ==
                   QJS_INTL_NO_MEMORY);
            zero_result(&result);
            assert(!state.live);
            qjs_intl_native_locale_info_result_clear(&allocator, &result);
        }
        state.fail_at = 0;
    }
    request.hour_cycle.data = NULL;
    request.hour_cycle.length = 3;
    memset(&result, 0xff, sizeof(result));
    assert(qjs_intl_native_locale_info_get(&allocator, &view, &capabilities,
           &request, QJS_INTL_LOCALE_HOUR_CYCLES, &result) ==
           QJS_INTL_INVALID_ARGUMENT);
    zero_result(&result);
    request.hour_cycle.length = 0;
    request.calendar.data = NULL;
    request.calendar.length = 0;
    assert(qjs_intl_native_locale_info_get(&allocator, &view, &capabilities,
           &request, QJS_INTL_LOCALE_CALENDARS, &result) == QJS_INTL_UNSUPPORTED);
    zero_result(&result);
    qjs_intl_native_locale_info_result_clear(&allocator, NULL);
    assert(!state.live);
    return 0;
}

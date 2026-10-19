/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "native-digital.h"
#include "../common/format-builder.h"
#include <string.h>

void qjs_intl_duration_native_digital_clear(const QJSIntlAllocator *allocator,
                                          QJSIntlNativeDurationDigital *result)
{
    if (!result || !allocator || !allocator->free)
        return;
    if (result->storage)
        allocator->free(allocator->opaque, result->storage);
    memset(result, 0, sizeof(*result));
}

/* The endpoints are unquoted field boundaries, so quotes inside the slice
   are paired. Copy literal UTF-8 bytes, removing delimiters and collapsing
   doubled apostrophes. The initial scalar validation covers the whole input. */
static size_t literal(char *out, QJSIntlBytes pattern, size_t start, size_t end)
{
    size_t count = 0;
    while (start < end) {
        char c = pattern.data[start++];
        if (c == '\'') {
            if (start == end || pattern.data[start] != '\'')
                continue;
            start++;
        }
        out[count++] = c;
    }
    return count;
}

typedef struct DigitalPattern {
    size_t starts[3], ends[3];
} DigitalPattern;

static QJSIntlStatus parse(QJSIntlBytes pattern, DigitalPattern *layout)
{
    QJSIntlFormatBuilder validation = {0};
    QJSIntlStatus status;
    size_t at = 0, starts[3], ends[3], fields = 0;
    int quoted = 0;
    if (!pattern.data && pattern.length)
        return QJS_INTL_INVALID_ARGUMENT;
    validation.limit = SIZE_MAX;
    status = qjs_intl_format_builder_text(&validation, pattern, NULL, NULL);
    if (status)
        return status;
    while (at < pattern.length) {
        char c = pattern.data[at];
        if (c == '\'') {
            if (at + 1 < pattern.length && pattern.data[at + 1] == '\'')
                at += 2;
            else {
                quoted = !quoted;
                at++;
            }
        } else if (!quoted && (c == 'h' || c == 'm' || c == 's')) {
            if (fields == 3 || c != "hms"[fields])
                return QJS_INTL_DATA_ERROR;
            starts[fields] = at;
            do {
                at++;
            } while (at < pattern.length && pattern.data[at] == c);
            if (at - starts[fields] > 2)
                return QJS_INTL_DATA_ERROR;
            ends[fields++] = at;
        } else {
            at++;
        }
    }
    if (quoted || fields != 3)
        return QJS_INTL_DATA_ERROR;
    memcpy(layout->starts, starts, sizeof(starts));
    memcpy(layout->ends, ends, sizeof(ends));
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_duration_native_digital_validate(QJSIntlBytes pattern)
{
    DigitalPattern layout;
    return parse(pattern, &layout);
}

QJSIntlStatus qjs_intl_duration_native_digital(const QJSIntlAllocator *allocator, QJSIntlBytes pattern,
                                             QJSIntlNativeDurationDigital *out)
{
    QJSIntlNativeDurationDigital result = {0};
    DigitalPattern layout;
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    status = parse(pattern, &layout);
    if (status)
        return status;
    /* Dequoting cannot expand the two nonoverlapping literal slices. */
    result.storage = allocator->malloc(allocator->opaque, pattern.length);
    if (!result.storage)
        return QJS_INTL_NO_MEMORY;
    result.hour_minute.data = result.storage;
    result.hour_minute.length = literal(result.storage, pattern, layout.ends[0], layout.starts[1]);
    result.minute_second.data = result.storage + result.hour_minute.length;
    result.minute_second.length = literal(result.storage + result.hour_minute.length,
                                          pattern, layout.ends[1], layout.starts[2]);
    result.hour_width = (unsigned int)(layout.ends[0] - layout.starts[0]);
    *out = result;
    return QJS_INTL_OK;
}

/* Convert native typed partitions to ordinary JavaScript parts objects.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../intl.h"
#include "../../internal/object.h"

static int property(JSContext *ctx, JSValueConst object, const char *name, JSValue value)
{
    if (JS_IsException(value))
        return -1;
    return JS_DefinePropertyValueStr(ctx, object, name, value, JS_PROP_C_W_E | JS_PROP_THROW);
}

JSValue js_intl_format_parts(JSContext *ctx, const QJSIntlFormatted *ordinary,
                             const QJSIntlFormattedRange *range,
                             const char *const *names, size_t name_count)
{
    static const char *const sources[] = {"shared", "startRange", "endRange"};
    const uint16_t *text = range ? range->text : ordinary->text;
    size_t length = range ? range->length : ordinary->length;
    size_t count = range ? range->part_count : ordinary->part_count;
    size_t end = 0, i;
    JSValue array;
    if (count > UINT32_MAX)
        return JS_ThrowRangeError(ctx, "too many Intl parts");
    array = JS_NewArray(ctx);
    if (JS_IsException(array))
        return array;
    for (i = 0; i < count; i++) {
        size_t start = range ? range->parts[i].start : ordinary->parts[i].start;
        size_t limit = range ? range->parts[i].end : ordinary->parts[i].end;
        QJSIntlPartType type = range ? range->parts[i].type : ordinary->parts[i].type;
        unsigned int source = range ? range->parts[i].source : 0;
        JSValue part;
        /* A service supplies its own whitelist; sharing conversion must not
           make a numeric result silently accept a date or list part. */
        if (start != end || limit < start || limit > length ||
            (unsigned int)type >= name_count || !names[type] || source >= countof(sources))
            goto invalid;
        end = limit;
        part = JS_NewObject(ctx);
        if (JS_IsException(part))
            goto fail;
        if (property(ctx, part, "type", JS_NewString(ctx, names[type])) < 0 ||
            property(ctx, part, "value", JS_NewStringUTF16(ctx, text ? text + start : NULL, limit - start)) < 0 ||
            (range && property(ctx, part, "source", JS_NewString(ctx, sources[source])) < 0)) {
            JS_FreeValue(ctx, part);
            goto fail;
        }
        if (JS_DefinePropertyValueUint32(ctx, array, (uint32_t)i, part, JS_PROP_C_W_E | JS_PROP_THROW) < 0)
            goto fail;
    }
    if (end != length)
        goto invalid;
    return array;
invalid:
    JS_ThrowInternalError(ctx, "invalid Intl parts");
fail:
    JS_FreeValue(ctx, array);
    return JS_EXCEPTION;
}

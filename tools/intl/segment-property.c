/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "segment-property.h"
#include "../../src/cutils/cutils.h"
#include "../../src/unicode/libunicode.h"
#include "../../src/unicode/break-properties.h"
#include <string.h>

static const uint32_t unicode_domain[] = {0, 0x110000};

static const char *const grapheme_names[] = {
#define UNICODE_GRAPHEME_BREAK
#define DEF(name) #name,
#include "../../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_GRAPHEME_BREAK
};
static const char *const word_names[] = {
#define UNICODE_WORD_BREAK
#define DEF(name) #name,
#include "../../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_WORD_BREAK
};
static const char *const sentence_names[] = {
#define UNICODE_SENTENCE_BREAK
#define DEF(name) #name,
#include "../../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_SENTENCE_BREAK
};
static const char *const conjunct_names[] = {
#define UNICODE_INDIC_CONJUNCT_BREAK
#define DEF(name) #name,
#include "../../src/unicode/break-properties-def.h"
#undef DEF
#undef UNICODE_INDIC_CONJUNCT_BREAK
};

static void *resize(void *opaque, void *pointer, size_t size)
{
    const QJSIntlAllocator *allocator = opaque;
    if (!size) {
        if (pointer)
            allocator->free(allocator->opaque, pointer);
        return NULL;
    }
    return pointer ? allocator->realloc(allocator->opaque, pointer, size) :
                     allocator->malloc(allocator->opaque, size);
}

static QJSIntlStatus name(QJSIntlBytes input, char *output, size_t capacity)
{
    size_t i;
    if (!input.data && input.length)
        return QJS_INTL_INVALID_ARGUMENT;
    for (i = 0; i < input.length; i++) {
        unsigned char c = input.data[i];
        if (!c || c >= 128)
            return QJS_INTL_INVALID_ARGUMENT;
    }
    if (input.length >= capacity)
        return QJS_INTL_UNSUPPORTED;
    if (input.length)
        memcpy(output, input.data, input.length);
    output[input.length] = 0;
    return QJS_INTL_OK;
}

static int field(CharRange *range, const char *value, const char *const *names,
                  size_t count, unsigned int mask, unsigned int shift)
{
    size_t selected;
    uint32_t code, start = 0;
    int inside = 0;
    for (selected = 0; selected < count; selected++) {
        if (!strcmp(value, names[selected]))
            break;
    }
    if (selected == count)
        return -2;
    for (code = 0; code <= 0x110000; code++) {
        int matches = code < 0x110000 &&
            ((unicode_break_properties(code) & mask) >> shift) == selected;
        if (matches && !inside)
            start = code;
        else if (!matches && inside && cr_add_interval(range, start, code))
            return -1;
        inside = matches;
    }
    return 0;
}

static int query(CharRange *range, const char *property, const char *value)
{
    int result;
    if (!*property) {
        result = unicode_general_category(range, value);
        if (result == -2)
            result = unicode_script(range, value, 0);
        if (result == -2)
            result = unicode_prop(range, value);
        return result;
    }
    if (!strcmp(property, "General_Category") || !strcmp(property, "gc"))
        return unicode_general_category(range, value);
    if (!strcmp(property, "Script") || !strcmp(property, "sc"))
        return unicode_script(range, value, 0);
    if (!strcmp(property, "Script_Extensions") || !strcmp(property, "scx"))
        return unicode_script(range, value, 1);
    if (!strcmp(property, "Grapheme_Cluster_Break") || !strcmp(property, "GCB"))
        return field(range, value, grapheme_names, UNICODE_GCB_COUNT,
                     UNICODE_BREAK_GCB_MASK, UNICODE_BREAK_GCB_SHIFT);
    if (!strcmp(property, "Word_Break") || !strcmp(property, "WB"))
        return field(range, value, word_names, UNICODE_WB_COUNT,
                     UNICODE_BREAK_WB_MASK, UNICODE_BREAK_WB_SHIFT);
    if (!strcmp(property, "Sentence_Break") || !strcmp(property, "SB"))
        return field(range, value, sentence_names, UNICODE_SB_COUNT,
                     UNICODE_BREAK_SB_MASK, UNICODE_BREAK_SB_SHIFT);
    if (!strcmp(property, "Indic_Conjunct_Break") || !strcmp(property, "InCB"))
        return field(range, value, conjunct_names, UNICODE_INCB_COUNT,
                     UNICODE_BREAK_INCB_MASK, UNICODE_BREAK_INCB_SHIFT);
    if (!strcmp(value, "True") || !strcmp(value, "Yes"))
        return unicode_prop(range, property);
    if (!strcmp(value, "False") || !strcmp(value, "No")) {
        result = unicode_prop(range, property);
        if (!result)
            result = cr_op1(range, unicode_domain, 2, CR_OP_XOR);
        return result;
    }
    return -2;
}

void qjs_intl_segment_ranges_clear(const QJSIntlAllocator *allocator, QJSIntlSegmentRanges *ranges)
{
    if (ranges) {
        if (ranges->points)
            allocator->free(allocator->opaque, ranges->points);
        memset(ranges, 0, sizeof(*ranges));
    }
}

QJSIntlStatus qjs_intl_segment_property(const QJSIntlAllocator *allocator, QJSIntlBytes property,
                                       QJSIntlBytes value, QJSIntlSegmentRanges *out)
{
    CharRange range;
    QJSIntlStatus status;
    char property_name[96], value_name[96];
    int result;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free ||
        !value.length)
        return QJS_INTL_INVALID_ARGUMENT;
    status = name(property, property_name, sizeof(property_name));
    if (!status)
        status = name(value, value_name, sizeof(value_name));
    if (status)
        return status;
    cr_init(&range, (void *)allocator, resize);
    result = query(&range, property_name, value_name);
    /* The general Unicode helpers use UINT32_MAX as their complement domain
       and may append adjacent category runs. Intersect once to clamp every
       route to Unicode code points and normalize its interval representation. */
    if (!result)
        result = cr_op1(&range, unicode_domain, 2, CR_OP_INTER);
    if (result) {
        cr_free(&range);
        return result == -2 ? QJS_INTL_UNSUPPORTED : QJS_INTL_NO_MEMORY;
    }
    out->points = range.points;
    out->count = (uint32_t)range.len;
    return QJS_INTL_OK;
}

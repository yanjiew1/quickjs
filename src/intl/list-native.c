/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Original implementation of ECMA-402 CreatePartsFromList /
 * DeconstructPattern, revision 7ae78cfdf8255468ffc8ebda33dafaea952808dd.
 * Context predicates adapted from ICU78.3 listformatter.cpp, commit
 * 21d1eb0f306e1141c10931e914dfc038c06121da.
 * Copyright (C) 2013-2016, International Business Machines Corporation
 * and others. All Rights Reserved. Copyright 2016 and later Unicode,
 * Inc. and others. See evidence/ICU-LICENSE for the Unicode license.
 */
#include "list-native.h"
#include <string.h>

typedef struct ListPattern {
    uint16_t *text;
    size_t length;
    QJSIntlUTF16 literal[3];
    unsigned char order[2];
} ListPattern;
struct QJSIntlNativeList {
    QJSIntlAllocator allocator;
    ListPattern patterns[8];
    QJSIntlListContext context;
    QJSIntlListScriptRange *hebrew_script;
    size_t hebrew_script_count;
};
typedef struct ListFrame {
    size_t item;
    unsigned char step;
} ListFrame;

static int add_size(size_t a, size_t b, size_t *out)
{
    if (a > SIZE_MAX - b) return 0;
    *out = a + b;
    return 1;
}
static int mul_size(size_t a, size_t b, size_t *out)
{
    if (b && a > SIZE_MAX / b) return 0;
    *out = a * b;
    return 1;
}
static int allocator_valid(const QJSIntlAllocator *a)
{
    return a && a->malloc && a->free;
}
void qjs_intl_native_list_result_clear(const QJSIntlAllocator *a,
                                      QJSIntlFormatted *out)
{
    if (!out) return;
    if (allocator_valid(a)) {
        if (out->text) a->free(a->opaque, out->text);
        if (out->parts) a->free(a->opaque, out->parts);
    }
    memset(out, 0, sizeof(*out));
}
void qjs_intl_native_list_close(QJSIntlNativeList *list)
{
    size_t i;
    QJSIntlAllocator a;
    if (!list) return;
    a = list->allocator;
    for (i = 0; i < 8; i++)
        if (list->patterns[i].text)
            a.free(a.opaque, list->patterns[i].text);
    if (list->hebrew_script) a.free(a.opaque, list->hebrew_script);
    a.free(a.opaque, list);
}
/* Decode one scalar strictly, independent of libc locale. */
static int next_scalar(QJSIntlBytes s, size_t *cursor, uint32_t *out)
{
    const unsigned char *p = (const unsigned char *)s.data;
    size_t i = *cursor, extra;
    uint32_t c, minimum;
    if (i == s.length) return 0;
    c = p[i++];
    if (c < 0x80) { minimum = 0; extra = 0; }
    else if (c >= 0xc2 && c <= 0xdf) { c &= 0x1f; minimum = 0x80; extra = 1; }
    else if (c >= 0xe0 && c <= 0xef) { c &= 0x0f; minimum = 0x800; extra = 2; }
    else if (c >= 0xf0 && c <= 0xf4) { c &= 7; minimum = 0x10000; extra = 3; }
    else return -1;
    if (extra > s.length - i) return -1;
    while (extra--) {
        if ((p[i] & 0xc0) != 0x80) return -1;
        c = (c << 6) | (p[i++] & 0x3f);
    }
    if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff) || c == 0)
        return -1;
    *cursor = i;
    *out = c;
    return 1;
}
static QJSIntlStatus load_pattern(const QJSIntlAllocator *a,
                                  QJSIntlBytes input, ListPattern *out)
{
    size_t cursor = 0, length = 0, bytes, i, start, tokens = 0;
    uint32_t c;
    int r;
    unsigned int seen = 0;
    if (!input.data && input.length) return QJS_INTL_INVALID_ARGUMENT;
    while ((r = next_scalar(input, &cursor, &c)) > 0)
        if (!add_size(length, c > 0xffff ? 2 : 1, &length))
            return QJS_INTL_OVERFLOW;
    if (r < 0 || !length) return QJS_INTL_DATA_ERROR;
    if (!mul_size(length, sizeof(uint16_t), &bytes)) return QJS_INTL_OVERFLOW;
    out->text = a->malloc(a->opaque, bytes);
    if (!out->text) return QJS_INTL_NO_MEMORY;
    out->length = length;
    cursor = 0;
    i = 0;
    while (next_scalar(input, &cursor, &c) > 0) {
        if (c > 0xffff) {
            c -= 0x10000;
            out->text[i++] = (uint16_t)(0xd800 + (c >> 10));
            out->text[i++] = (uint16_t)(0xdc00 + (c & 0x3ff));
        } else out->text[i++] = (uint16_t)c;
    }
    start = 0;
    for (i = 0; i < length; i++) {
        if (out->text[i] == '}') return QJS_INTL_DATA_ERROR;
        if (out->text[i] != '{') continue;
        if (tokens == 2 || length - i < 3 || out->text[i + 2] != '}' ||
            (out->text[i + 1] != '0' && out->text[i + 1] != '1'))
            return QJS_INTL_DATA_ERROR;
        c = out->text[i + 1] - '0';
        if (seen & (1u << c)) return QJS_INTL_DATA_ERROR;
        seen |= 1u << c;
        out->literal[tokens].data = out->text + start;
        out->literal[tokens].length = i - start;
        out->order[tokens++] = (unsigned char)c;
        i += 2;
        start = i + 1;
    }
    if (seen != 3) return QJS_INTL_DATA_ERROR;
    out->literal[2].data = out->text + start;
    out->literal[2].length = length - start;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_list_open(const QJSIntlAllocator *a,
                                      const QJSIntlListTemplates *templates,
                                      QJSIntlNativeList **out)
{
    QJSIntlNativeList *list;
    QJSIntlStatus status;
    QJSIntlListContext context;
    size_t i, bytes = 0;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!allocator_valid(a) || !templates || (unsigned int)templates->context > 3)
        return QJS_INTL_INVALID_ARGUMENT;
    context = templates->context;
    if (context == QJS_INTL_LIST_CONTEXT_HEBREW_AND) {
        if (!templates->hebrew_script || !templates->hebrew_script_count)
            return QJS_INTL_DATA_ERROR;
        if (!mul_size(templates->hebrew_script_count,
                      sizeof(QJSIntlListScriptRange), &bytes))
            return QJS_INTL_OVERFLOW;
        for (i = 0; i < templates->hebrew_script_count; i++) {
            uint32_t first = templates->hebrew_script[i].first;
            uint32_t last = templates->hebrew_script[i].last;
            if (first > last || last > 0x10ffff ||
                (first <= 0xdfff && last >= 0xd800) ||
                (i && first <= templates->hebrew_script[i - 1].last))
                return QJS_INTL_DATA_ERROR;
        }
    }
    list = a->malloc(a->opaque, sizeof(*list));
    if (!list) return QJS_INTL_NO_MEMORY;
    memset(list, 0, sizeof(*list));
    list->allocator = *a;
    list->context = context;
    for (i = 0; i < (context ? 8u : 4u); i++) {
        status = load_pattern(a, i < 4 ? templates->base[i] :
                              templates->alternate[i - 4], &list->patterns[i]);
        if (status != QJS_INTL_OK) {
            qjs_intl_native_list_close(list);
            return status;
        }
    }
    if (context == QJS_INTL_LIST_CONTEXT_HEBREW_AND) {
        list->hebrew_script = a->malloc(a->opaque, bytes);
        if (!list->hebrew_script) {
            qjs_intl_native_list_close(list);
            return QJS_INTL_NO_MEMORY;
        }
        memcpy(list->hebrew_script, templates->hebrew_script, bytes);
        list->hebrew_script_count = templates->hebrew_script_count;
    }
    *out = list;
    return QJS_INTL_OK;
}
static QJSIntlStatus emit(QJSIntlUTF16 piece, QJSIntlPartType type,
                          QJSIntlFormatted *out, size_t *length,
                          size_t *parts, int write)
{
    size_t end, next;
    if (!add_size(*length, piece.length, &end) || !add_size(*parts, 1, &next))
        return QJS_INTL_OVERFLOW;
    if (write) {
        QJSIntlPart *p = out->parts + *parts;
        if (piece.length)
            memcpy(out->text + *length, piece.data, piece.length * sizeof(uint16_t));
        memset(p, 0, sizeof(*p));
        p->start = *length;
        p->end = end;
        p->type = type;
        p->source = QJS_INTL_SOURCE_SINGLE;
    }
    *length = end;
    *parts = next;
    return QJS_INTL_OK;
}
static uint16_t ascii_lower(uint16_t c)
{
    return c >= 'A' && c <= 'Z' ? (uint16_t)(c + ('a' - 'A')) : c;
}
static int use_alternate(const QJSIntlNativeList *list, QJSIntlUTF16 next)
{
    uint16_t first, second;
    uint32_t scalar;
    size_t lo, hi;
    if (!next.length || !list->context) return 0;
    first = ascii_lower(next.data[0]);
    second = next.length > 1 ? ascii_lower(next.data[1]) : 0;
    if (list->context == QJS_INTL_LIST_CONTEXT_SPANISH_AND)
        return first == 'i' || (first == 'h' && second == 'i' &&
               (next.length == 2 || (ascii_lower(next.data[2]) != 'a' &&
                                    ascii_lower(next.data[2]) != 'e')));
    if (list->context == QJS_INTL_LIST_CONTEXT_SPANISH_OR)
        return first == 'o' || first == '8' || (first == 'h' && second == 'o') ||
               (first == '1' && second == '1' &&
                (next.length == 2 || next.data[2] == ' '));
    scalar = next.data[0];
    if (scalar >= 0xd800 && scalar <= 0xdbff && next.length > 1 &&
        next.data[1] >= 0xdc00 && next.data[1] <= 0xdfff)
        scalar = 0x10000 + ((scalar - 0xd800) << 10) + (next.data[1] - 0xdc00);
    lo = 0;
    hi = list->hebrew_script_count;
    while (lo < hi) {
        size_t middle = lo + (hi - lo) / 2;
        if (scalar < list->hebrew_script[middle].first) hi = middle;
        else if (scalar > list->hebrew_script[middle].last) lo = middle + 1;
        else return 0;
    }
    return 1;
}
/* Expand the right-tail expression iteratively. An O(n) explicit stack avoids
 * recursion and repeated copies of already formatted tails. The measure and
 * write traversals are identical; allocation is checked between them.
 */
static QJSIntlStatus traverse(const QJSIntlNativeList *list,
                              const QJSIntlUTF16 *items, size_t count,
                              ListFrame *stack, QJSIntlFormatted *out,
                              size_t *length, size_t *parts, int write)
{
    size_t depth = 1;
    int alternate = use_alternate(list, items[count - 1]);
    QJSIntlStatus status;
    stack[0].item = 0;
    stack[0].step = 0;
    while (depth) {
        ListFrame *f = &stack[depth - 1];
        const ListPattern *p;
        QJSIntlUTF16 piece;
        unsigned char step, which;
        if (f->item == count - 1) {
            status = emit(items[f->item], QJS_INTL_PART_ELEMENT,
                          out, length, parts, write);
            depth--;
            if (status != QJS_INTL_OK) return status;
            continue;
        }
        {
            size_t pattern_index = count == 2 ? 0 : f->item == 0 ? 1 :
                                   f->item == count - 2 ? 3 : 2;
            if (alternate && (pattern_index == 0 || pattern_index == 3))
                pattern_index += 4;
            p = &list->patterns[pattern_index];
        }
        step = f->step++;
        if (step == 5) { depth--; continue; }
        if (!(step & 1)) {
            piece = p->literal[step / 2];
            if (!piece.length) continue;
            status = emit(piece, QJS_INTL_PART_LITERAL, out, length, parts, write);
        } else {
            which = p->order[step / 2];
            if (which == 1) {
                stack[depth].item = f->item + 1;
                stack[depth++].step = 0;
                continue;
            }
            status = emit(items[f->item], QJS_INTL_PART_ELEMENT,
                          out, length, parts, write);
        }
        if (status != QJS_INTL_OK) return status;
    }
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_list_format(const QJSIntlNativeList *list,
                                        const QJSIntlUTF16 *items,
                                        size_t count, QJSIntlFormatted *out)
{
    ListFrame *stack = NULL;
    QJSIntlStatus status = QJS_INTL_OK;
    size_t i, length = 0, parts = 0, bytes, text_bytes, parts_bytes;
    const QJSIntlAllocator *a;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!list || (count && !items)) return QJS_INTL_INVALID_ARGUMENT;
    a = &list->allocator;
    if (!mul_size(count, sizeof(*items), &bytes)) return QJS_INTL_OVERFLOW;
    for (i = 0; i < count; i++) {
        if (items[i].length && !items[i].data) return QJS_INTL_INVALID_ARGUMENT;
        if (!mul_size(items[i].length, sizeof(uint16_t), &bytes))
            return QJS_INTL_OVERFLOW;
    }
    if (!count) return QJS_INTL_OK;
    if (!mul_size(count, sizeof(*stack), &bytes)) return QJS_INTL_OVERFLOW;
    stack = a->malloc(a->opaque, bytes);
    if (!stack) return QJS_INTL_NO_MEMORY;
    status = traverse(list, items, count, stack, out, &length, &parts, 0);
    if (status != QJS_INTL_OK) goto fail;
    if (!mul_size(length, sizeof(uint16_t), &text_bytes) ||
        !mul_size(parts, sizeof(QJSIntlPart), &parts_bytes)) {
        status = QJS_INTL_OVERFLOW;
        goto fail;
    }
    if (text_bytes) {
        out->text = a->malloc(a->opaque, text_bytes);
        if (!out->text) { status = QJS_INTL_NO_MEMORY; goto fail; }
    }
    out->parts = a->malloc(a->opaque, parts_bytes);
    if (!out->parts) { status = QJS_INTL_NO_MEMORY; goto fail; }
    out->length = length;
    out->part_count = parts;
    length = parts = 0;
    status = traverse(list, items, count, stack, out, &length, &parts, 1);
    if (status != QJS_INTL_OK) goto fail;
    a->free(a->opaque, stack);
    return QJS_INTL_OK;
 fail:
    a->free(a->opaque, stack);
    qjs_intl_native_list_result_clear(a, out);
    return status;
}

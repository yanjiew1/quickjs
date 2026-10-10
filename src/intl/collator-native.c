/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
 * Original implementation of UTS10 revision55 steps S1/S2/S3, authored for
 * this packet. Not a port of ICU or another collation implementation.
 * ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, CompareStrings.
 */
#include "collator-native.h"
#include "cutils/cutils.h"
#include "unicode/libunicode.h"
#include <limits.h>
#include <string.h>

struct QJSIntlNativeCollator {
    QJSIntlAllocator allocator;
    QJSIntlCollationData data;
    QJSIntlCollatorOptions options;
    QJSIntlCollatorLimits limits;
};
typedef struct Work {
    const QJSIntlNativeCollator *collator;
    size_t used, limit;
    QJSIntlStatus error;
    void *normalization_buffer;
} Work;
typedef union WorkHeader {
    size_t bytes;
    uint64_t alignment;
} WorkHeader;
typedef struct CE {
    uint32_t primary, secondary, tertiary, flags;
} CE;
typedef struct CEArray {
    CE *items;
    size_t length, capacity;
} CEArray;

static void work_free(Work *w, void *ptr)
{
    WorkHeader *header;
    if (!ptr) return;
    header = (WorkHeader *)ptr - 1;
    w->used -= header->bytes;
    w->collator->allocator.free(w->collator->allocator.opaque, header);
}
static void *work_resize(Work *w, void *ptr, size_t bytes)
{
    WorkHeader *header = ptr ? (WorkHeader *)ptr - 1 : NULL;
    size_t old = header ? header->bytes : 0;
    const QJSIntlAllocator *a = &w->collator->allocator;
    if (!bytes) { work_free(w, ptr); return NULL; }
    if (bytes > SIZE_MAX - sizeof(*header) ||
        bytes > w->limit - (w->used - old)) {
        w->error = QJS_INTL_OVERFLOW;
        return NULL;
    }
    if (header) header = a->realloc(a->opaque, header, sizeof(*header) + bytes);
    else header = a->malloc(a->opaque, sizeof(*header) + bytes);
    if (!header) { w->error = QJS_INTL_NO_MEMORY; return NULL; }
    header->bytes = bytes;
    w->used = w->used - old + bytes;
    return header + 1;
}
/* libunicode's DynBuf callback allows size0. Adapt it to allocator.free,
 * retain its one owning buffer, and recover that buffer if normalization
 * fails before publishing *pdst. No second Unicode table is introduced.
 */
static void *normalization_resize(void *opaque, void *ptr, size_t bytes)
{
    Work *w = opaque;
    void *result;
    if (bytes > INT_MAX) { w->error = QJS_INTL_OVERFLOW; return NULL; }
    result = work_resize(w, ptr, bytes);
    if (result || !bytes) w->normalization_buffer = result;
    return result;
}
static QJSIntlStatus normalize(Work *w, QJSIntlUTF16 in,
                              uint32_t **out, size_t *length)
{
    uint32_t *decoded = NULL, *normalized = NULL;
    size_t i, n = 0;
    int normalized_length;
    *out = NULL; *length = 0;
    if ((!in.data && in.length) || in.length > (size_t)INT_MAX / sizeof(uint32_t) ||
        (w->collator->limits.max_input_units &&
         in.length > w->collator->limits.max_input_units))
        return !in.data && in.length ? QJS_INTL_INVALID_ARGUMENT : QJS_INTL_OVERFLOW;
    if (!in.length) return QJS_INTL_OK;
    decoded = work_resize(w, NULL, in.length * sizeof(*decoded));
    if (!decoded) return w->error;
    for (i = 0; i < in.length; i++) {
        uint32_t cp = in.data[i];
        if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < in.length &&
            in.data[i + 1] >= 0xdc00 && in.data[i + 1] <= 0xdfff) {
            cp = 0x10000 + ((cp - 0xd800) << 10) + in.data[++i] - 0xdc00;
        }
        decoded[n++] = cp; /* individual unpaired units intentionally survive */
    }
    w->normalization_buffer = NULL;
    normalized_length = unicode_normalize(&normalized, decoded, (int)n,
                                  UNICODE_NFD, w, normalization_resize);
    work_free(w, decoded);
    if (normalized_length < 0) {
        work_free(w, w->normalization_buffer);
        w->normalization_buffer = NULL;
        return w->error == QJS_INTL_OK ? QJS_INTL_NO_MEMORY : w->error;
    }
    w->normalization_buffer = NULL;
    *out = normalized; *length = (size_t)normalized_length;
    return QJS_INTL_OK;
}
static int field(const QJSIntlDataSection *s, uint32_t i, uint32_t offset,
                 uint32_t *out)
{
    return qjs_intl_data_record_u32(s, i, offset, out) == QJS_INTL_DATA_OK;
}
static int child(const QJSIntlCollationData *d, uint32_t node, uint32_t cp,
                 uint32_t *out)
{
    uint32_t first, count, lo = 0, hi;
    if (!field(&d->nodes, node, 4, &first) ||
        !field(&d->nodes, node, 8, &count)) return -1;
    hi = count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, value;
        if (!field(&d->nodes, first + mid, 0, &value)) return -1;
        if (value < cp) lo = mid + 1;
        else hi = mid;
    }
    if (lo < count) {
        uint32_t value;
        if (!field(&d->nodes, first + lo, 0, &value)) return -1;
        if (value == cp) { *out = first + lo; return 1; }
    }
    return 0;
}
static QJSIntlStatus append(Work *w, CEArray *a, CE ce, int *shifted)
{
    if (w->collator->options.ignore_punctuation) {
        if ((ce.flags & 1) && ce.primary) {
            *shifted = 1;
            return QJS_INTL_OK;
        }
        if (*shifted && !ce.primary) return QJS_INTL_OK;
        if (ce.primary) *shifted = 0;
    }
    if (a->length == a->capacity) {
        size_t capacity = a->capacity ? a->capacity + a->capacity / 2 + 1 : 32;
        CE *items;
        if (capacity < a->capacity || capacity > SIZE_MAX / sizeof(*items))
            return QJS_INTL_OVERFLOW;
        items = work_resize(w, a->items, capacity * sizeof(*items));
        if (!items) return w->error;
        a->items = items; a->capacity = capacity;
    }
    a->items[a->length++] = ce;
    return QJS_INTL_OK;
}
static QJSIntlStatus emit_node(Work *w, CEArray *a, uint32_t node, int *shifted)
{
    const QJSIntlCollationData *d = &w->collator->data;
    uint32_t first, count, i;
    if (!field(&d->nodes, node, 12, &first) ||
        !field(&d->nodes, node, 16, &count)) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < count; i++) {
        CE ce;
        QJSIntlStatus s;
        if (!field(&d->ces, first + i, 0, &ce.primary) ||
            !field(&d->ces, first + i, 4, &ce.secondary) ||
            !field(&d->ces, first + i, 8, &ce.tertiary) ||
            !field(&d->ces, first + i, 12, &ce.flags)) return QJS_INTL_DATA_ERROR;
        ce.primary <<= 16;
        s = append(w, a, ce, shifted);
        if (s != QJS_INTL_OK) return s;
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus emit_implicit(Work *w, CEArray *a, uint32_t cp, int *shifted)
{
    const QJSIntlDataSection *ranges = &w->collator->data.implicit;
    uint32_t lo = 0, hi = ranges->record_count;
    uint32_t lead = 0xfbc0 + (cp >> 15), trail = (cp & 0x7fff) | 0x8000;
    CE ce;
    QJSIntlStatus s;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, last;
        if (!field(ranges, mid, 4, &last)) return QJS_INTL_DATA_ERROR;
        if (last < cp) lo = mid + 1;
        else hi = mid;
    }
    if (lo < ranges->record_count) {
        uint32_t first, base, origin;
        if (!field(ranges, lo, 0, &first) || !field(ranges, lo, 8, &base) ||
            !field(ranges, lo, 12, &origin)) return QJS_INTL_DATA_ERROR;
        if (cp >= first) {
            lead = base & UINT32_C(0x7fffffff);
            if (base & UINT32_C(0x80000000)) lead += cp >> 15;
            else trail = (cp - origin) | 0x8000;
        }
    }
    ce.primary = lead << 16; ce.secondary = 0x20; ce.tertiary = 2; ce.flags = 0;
    s = append(w, a, ce, shifted);
    if (s != QJS_INTL_OK) return s;
    ce.primary = trail << 16; ce.secondary = 0; ce.tertiary = 0;
    return append(w, a, ce, shifted);
}
static int digit(const QJSIntlCollationData *d, uint32_t cp)
{
    uint32_t lo = 0, hi = d->digits.record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, zero;
        if (!field(&d->digits, mid, 0, &zero)) return -2;
        if (zero + 9 < cp) lo = mid + 1;
        else hi = mid;
    }
    if (lo < d->digits.record_count) {
        uint32_t zero;
        if (!field(&d->digits, lo, 0, &zero)) return -2;
        if (cp >= zero) return (int)(cp - zero);
    }
    return -1;
}
static QJSIntlStatus emit_number(Work *w, CEArray *a, const uint32_t *cp,
                                 size_t first, size_t last, int *shifted)
{
    const QJSIntlCollationData *d = &w->collator->data;
    size_t i, significant;
    CE ce;
    QJSIntlStatus s;
    while (first + 1 < last && digit(d, cp[first]) == 0) first++;
    significant = last - first;
    ce.primary = d->numeric_primary;
    ce.secondary = 0x20; ce.tertiary = 2; ce.flags = 0;
    s = append(w, a, ce, shifted);
    if (s != QJS_INTL_OK) return s;
    ce.secondary = 0; ce.tertiary = 0;
    /* Fixed-width big-endian length precedes digits. No integer conversion
     * of the value, chunk limit, host locale or overflow-prone power of10.
     * The length prefix is equal for equal numbers across 32/64-bit targets;
     * serialized keys are never exposed or compared between processes.
     */
    for (i = sizeof(size_t); i; i--) {
        ce.primary = 1 + (uint32_t)((significant >> ((i - 1) * CHAR_BIT)) & 0xff);
        s = append(w, a, ce, shifted);
        if (s != QJS_INTL_OK) return s;
    }
    for (i = first; i < last; i++) {
        int value = digit(d, cp[i]);
        if (value < 0) return QJS_INTL_DATA_ERROR;
        ce.primary = 0x100 + (uint32_t)value;
        s = append(w, a, ce, shifted);
        if (s != QJS_INTL_OK) return s;
    }
    return QJS_INTL_OK;
}
static QJSIntlStatus elements(Work *w, uint32_t *cp, size_t length, CEArray *out)
{
    const QJSIntlCollationData *d = &w->collator->data;
    size_t i = 0;
    int shifted = 0;
    while (i < length) {
        uint32_t node = 0, best = UINT32_MAX, count;
        size_t j = i, end = i + 1, depth = 0;
        QJSIntlStatus s;
        int found;
        if (cp[i] == UINT32_MAX) { i++; continue; }
        if (w->collator->options.numeric) {
            int value = digit(d, cp[i]);
            if (value == -2) return QJS_INTL_DATA_ERROR;
            if (value >= 0) {
                size_t last = i + 1;
                while (last < length) {
                    value = digit(d, cp[last]);
                    if (value == -2) return QJS_INTL_DATA_ERROR;
                    if (value < 0) break;
                    last++;
                }
                s = emit_number(w, out, cp, i, last, &shifted);
                if (s != QJS_INTL_OK) return s;
                i = last;
                continue;
            }
        }
        /* S2.1: longest contiguous match, including partial nonterminal
         * prefixes. Removed lookahead marks are skipped on subsequent scans.
         */
        while (j < length) {
            uint32_t next;
            if (cp[j] == UINT32_MAX) { j++; continue; }
            found = child(d, node, cp[j], &next);
            if (found < 0) return QJS_INTL_DATA_ERROR;
            if (!found) break;
            if (++depth > d->max_depth) return QJS_INTL_DATA_ERROR;
            node = next; j++;
            if (!field(&d->nodes, node, 16, &count)) return QJS_INTL_DATA_ERROR;
            if (count) { best = node; end = j; }
        }
        if (best != UINT32_MAX) {
            unsigned int blocked = 0;
            uint32_t children;
            if (!field(&d->nodes, best, 8, &children)) return QJS_INTL_DATA_ERROR;
            /* S2.1.1..3: extend an existing terminal match by one unblocked
             * non-starter at a time. A missing intermediate terminal cannot
             * be invented. All skipped marks remain in their original order.
             */
            for (j = end; children && j < length; j++) {
                uint32_t next;
                unsigned int cc;
                if (cp[j] == UINT32_MAX) continue;
                cc = (unsigned int)unicode_get_combining_class(cp[j]);
                if (!cc) break;
                found = child(d, best, cp[j], &next);
                if (found < 0) return QJS_INTL_DATA_ERROR;
                if (found && cc > blocked) {
                    if (!field(&d->nodes, next, 16, &count))
                        return QJS_INTL_DATA_ERROR;
                    if (count) {
                        best = next; cp[j] = UINT32_MAX;
                        if (!field(&d->nodes, best, 8, &children))
                            return QJS_INTL_DATA_ERROR;
                        continue;
                    }
                }
                if (cc > blocked) blocked = cc;
            }
            s = emit_node(w, out, best, &shifted);
        } else {
            s = emit_implicit(w, out, cp[i], &shifted);
        }
        if (s != QJS_INTL_OK) return s;
        i = end;
    }
    return QJS_INTL_OK;
}
static uint32_t weight(const QJSIntlNativeCollator *c, CE ce, unsigned int level)
{
    uint32_t case_weight;
    if (level == 0) return ce.primary;
    if (level == 1) return ce.secondary;
    case_weight = ce.flags & 2 ? 3 : 1;
    if (c->options.case_first == QJS_INTL_COLLATOR_CASE_UPPER)
        case_weight = 4 - case_weight;
    if (level == 2) return ce.primary ? case_weight : 0;
    if (!ce.tertiary) return 0;
    if (c->options.case_first == QJS_INTL_COLLATOR_CASE_FALSE) return ce.tertiary;
    /* CLDR49 Case_Weights: tertiary-only elements use c=3 even with upper
     * first, preserving WF2. Otherwise c is stronger than every variant.
     */
    if (!ce.primary && !ce.secondary) case_weight = 3;
    return (case_weight << 16) | ce.tertiary;
}
static int compare_level(const QJSIntlNativeCollator *c,
                          const CEArray *a, const CEArray *b, unsigned int level)
{
    size_t i = 0, j = 0;
    for (;;) {
        uint32_t x = 0, y = 0;
        while (i < a->length && !x) x = weight(c, a->items[i++], level);
        while (j < b->length && !y) y = weight(c, b->items[j++], level);
        if (x != y) return x < y ? -1 : 1;
        if (!x) return 0;
    }
}
static QJSIntlStatus capability(const QJSIntlCollationData *data,
                                uint32_t locale, QJSIntlCollatorUsage usage,
                                uint32_t *row)
{
    uint32_t lo = 0, hi = data->capabilities.record_count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2, value, use;
        if (!field(&data->capabilities, mid, 0, &value) ||
            !field(&data->capabilities, mid, 4, &use)) return QJS_INTL_DATA_ERROR;
        if (value < locale || (value == locale && use < (uint32_t)usage))
            lo = mid + 1;
        else hi = mid;
    }
    if (lo < data->capabilities.record_count) {
        uint32_t value, use;
        if (!field(&data->capabilities, lo, 0, &value) ||
            !field(&data->capabilities, lo, 4, &use)) return QJS_INTL_DATA_ERROR;
        if (value == locale && use == (uint32_t)usage) {
            *row = lo;
            return QJS_INTL_OK;
        }
    }
    return QJS_INTL_UNSUPPORTED;
}
QJSIntlStatus qjs_intl_native_collator_defaults(const QJSIntlCollationData *data,
                                               uint32_t locale,
                                               QJSIntlCollatorUsage usage,
                                               QJSIntlCollatorOptions *out)
{
    uint32_t row, sensitivity, punctuation;
    QJSIntlStatus s;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data || (unsigned int)usage > 1) return QJS_INTL_INVALID_ARGUMENT;
    s = capability(data, locale, usage, &row);
    if (s != QJS_INTL_OK) return s;
    if (!field(&data->capabilities, row, 12, &sensitivity) ||
        !field(&data->capabilities, row, 16, &punctuation) ||
        sensitivity > 3 || punctuation > 1) return QJS_INTL_DATA_ERROR;
    out->usage = usage;
    out->sensitivity = (QJSIntlCollatorSensitivity)sensitivity;
    out->ignore_punctuation = (uint8_t)punctuation;
    out->case_first = QJS_INTL_COLLATOR_CASE_FALSE;
    return QJS_INTL_OK;
}
QJSIntlStatus qjs_intl_native_collator_open(const QJSIntlAllocator *a,
                                           const QJSIntlCollationData *data,
                                           uint32_t locale,
                                           QJSIntlBytes collation,
                                           const QJSIntlCollatorOptions *options,
                                           const QJSIntlCollatorLimits *limits,
                                           QJSIntlNativeCollator **out)
{
    QJSIntlNativeCollator *c;
    uint32_t row;
    QJSIntlStatus s;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = NULL;
    if (!a || !a->malloc || !a->realloc || !a->free || !data || !options ||
        (unsigned int)options->usage > 1 ||
        (unsigned int)options->sensitivity > 3 ||
        (unsigned int)options->case_first > 2 || options->numeric > 1 ||
        options->ignore_punctuation > 1 || (!collation.data && collation.length))
        return QJS_INTL_INVALID_ARGUMENT;
    if (collation.length && (collation.length != 7 ||
                              memcmp(collation.data, "default", 7)))
        return QJS_INTL_UNSUPPORTED;
    if (!data->nodes.record_count || !data->max_depth)
        return QJS_INTL_INVALID_ARGUMENT;
    s = capability(data, locale, options->usage, &row);
    if (s != QJS_INTL_OK) return s;
    /* Usage is resolved explicitly even though both reviewed policies share
     * this immutable root trie and weight owner. Distinct CLDR search rules
     * are an optional later data profile, not silently claimed here.
     */
    c = a->malloc(a->opaque, sizeof(*c));
    if (!c) return QJS_INTL_NO_MEMORY;
    c->allocator = *a; c->data = *data; c->options = *options;
    if (limits) c->limits = *limits;
    else memset(&c->limits, 0, sizeof(c->limits));
    *out = c;
    return QJS_INTL_OK;
}
void qjs_intl_native_collator_close(QJSIntlNativeCollator *c)
{
    if (c) c->allocator.free(c->allocator.opaque, c);
}
QJSIntlStatus qjs_intl_native_collator_compare(const QJSIntlNativeCollator *c,
                                              QJSIntlUTF16 left,
                                              QJSIntlUTF16 right, int *out)
{
    Work w;
    uint32_t *a = NULL, *b = NULL;
    size_t a_length = 0, b_length = 0;
    CEArray ac, bc;
    QJSIntlStatus status;
    int result;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    *out = 0;
    if (!c) return QJS_INTL_INVALID_ARGUMENT;
    memset(&w, 0, sizeof(w)); memset(&ac, 0, sizeof(ac)); memset(&bc, 0, sizeof(bc));
    w.collator = c;
    w.limit = c->limits.max_work_bytes ? c->limits.max_work_bytes : SIZE_MAX;
    status = normalize(&w, left, &a, &a_length);
    if (status != QJS_INTL_OK) goto done;
    status = normalize(&w, right, &b, &b_length);
    if (status != QJS_INTL_OK) goto done;
    status = elements(&w, a, a_length, &ac);
    if (status != QJS_INTL_OK) goto done;
    status = elements(&w, b, b_length, &bc);
    if (status != QJS_INTL_OK) goto done;
    result = compare_level(c, &ac, &bc, 0);
    if (!result && (c->options.sensitivity == QJS_INTL_COLLATOR_ACCENT ||
                    c->options.sensitivity == QJS_INTL_COLLATOR_VARIANT))
        result = compare_level(c, &ac, &bc, 1);
    if (!result && c->options.sensitivity == QJS_INTL_COLLATOR_CASE)
        result = compare_level(c, &ac, &bc, 2);
    if (!result && c->options.sensitivity == QJS_INTL_COLLATOR_VARIANT)
        result = compare_level(c, &ac, &bc, 3);
    *out = result;
done:
    work_free(&w, ac.items); work_free(&w, bc.items);
    work_free(&w, a); work_free(&w, b);
    return status;
}

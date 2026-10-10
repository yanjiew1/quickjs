/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "collator-native-data.h"
#include <string.h>

static int field(const QJSIntlDataSection *s, uint32_t i, uint32_t offset,
                 uint32_t *out)
{
    return qjs_intl_data_record_u32(s, i, offset, out) == QJS_INTL_DATA_OK;
}
static int scalar(uint32_t cp)
{
    return cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}
static int upper_tertiary(uint32_t t)
{
    return (t >= 8 && t <= 12) || t == 14 || t == 17 || t == 18 || t == 29;
}
static int span(uint32_t first, uint32_t count, uint32_t total)
{
    return count ? first <= total && count <= total - first : first == 0;
}
QJSIntlStatus qjs_intl_collation_data_init(const QJSIntlDataView *view,
                                         QJSIntlCollationData *out)
{
    QJSIntlCollationData d;
    QJSIntlDataSection *sections[6], locales;
    uint32_t i, cursor, level_end, depth, previous, present = 0;
    uint32_t cp, first, count, ce_first, ce_count, v[4];
    QJSIntlDataStatus status;
    if (!out) return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!view) return QJS_INTL_INVALID_ARGUMENT;
    memset(&d, 0, sizeof(d));
    sections[0] = &d.nodes; sections[1] = &d.ces;
    sections[2] = &d.implicit; sections[3] = &d.digits;
    sections[4] = &d.capabilities; sections[5] = &d.config;
    for (i = 0; i < 6; i++) {
        uint32_t width = i == 0 || i == 4 ? 20 : i == 3 ? 4 : 16;
        status = qjs_intl_data_section(view, QJS_INTL_DATA_COLLATION_NODE + i, sections[i]);
        if (status == QJS_INTL_DATA_NOT_FOUND) continue;
        if (status != QJS_INTL_DATA_OK || sections[i]->record_width != width)
            return QJS_INTL_DATA_ERROR;
        present++;
    }
    if (!present) return QJS_INTL_UNSUPPORTED;
    if (present != 6 || !d.nodes.record_count || !d.ces.record_count ||
        !d.implicit.record_count || !d.digits.record_count ||
        !d.capabilities.record_count || d.config.record_count != 1 ||
        qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) !=
                                                       QJS_INTL_DATA_OK)
        return QJS_INTL_DATA_ERROR;
    for (i = 0; i < 4; i++)
        if (!field(&d.config, 0, 4 * i, &v[i])) return QJS_INTL_DATA_ERROR;
    if (v[0] != 2 || !v[1] || v[1] >= UINT32_C(0xfb000000) ||
        !v[2] || v[2] > 64 || v[3] != (18u << 16))
        return QJS_INTL_DATA_ERROR;
    d.numeric_primary = v[1]; d.max_depth = v[2];
    /* BFS layout proves a single parent per node, no cycle, no unused nodes.
     * Track layer boundaries without allocating a validation stack/table. */
    cursor = 1; level_end = 1; depth = 0;
    for (i = 0; i < d.nodes.record_count; i++) {
        uint32_t j, previous_cp = 0;
        if (i == level_end) { depth++; level_end = cursor; }
        if (depth > d.max_depth ||
            !field(&d.nodes, i, 0, &cp) ||
            !field(&d.nodes, i, 4, &first) ||
            !field(&d.nodes, i, 8, &count) ||
            !field(&d.nodes, i, 12, &ce_first) ||
            !field(&d.nodes, i, 16, &ce_count) ||
            (i ? !scalar(cp) : cp != UINT32_MAX || ce_count || ce_first) ||
            !span(first, count, d.nodes.record_count) ||
            !span(ce_first, ce_count, d.ces.record_count) ||
            (!count && !ce_count) || (count && first != cursor))
            return QJS_INTL_DATA_ERROR;
        for (j = 0; j < count; j++) {
            uint32_t child_cp;
            if (!field(&d.nodes, first + j, 0, &child_cp) ||
                !scalar(child_cp) || (j && child_cp <= previous_cp))
                return QJS_INTL_DATA_ERROR;
            previous_cp = child_cp;
        }
        cursor += count; /* checked span + first==cursor above */
    }
    if (cursor != d.nodes.record_count) return QJS_INTL_DATA_ERROR;
    for (i = 0; i < d.ces.record_count; i++) {
        uint32_t j;
        for (j = 0; j < 4; j++)
            if (!field(&d.ces, i, j * 4, &v[j])) return QJS_INTL_DATA_ERROR;
        if (v[0] > 0xffff || v[1] > 0xffff || v[2] > 0xffff || v[3] > 3 ||
            ((v[3] & 1) && !v[0]) ||
            !!(v[3] & 2) != upper_tertiary(v[2]))
            return QJS_INTL_DATA_ERROR;
    }
    previous = 0;
    for (i = 0; i < d.implicit.record_count; i++) {
        uint32_t j, base;
        for (j = 0; j < 4; j++)
            if (!field(&d.implicit, i, j * 4, &v[j])) return QJS_INTL_DATA_ERROR;
        base = v[2] & UINT32_C(0x7fffffff);
        if (!scalar(v[0]) || !scalar(v[1]) || v[0] > v[1] ||
            (v[0] <= 0xdfff && v[1] >= 0xd800) || (i && v[0] <= previous))
            return QJS_INTL_DATA_ERROR;
        if (v[2] & UINT32_C(0x80000000)) {
            if ((base != 0xfb40 && base != 0xfb80) || v[3])
                return QJS_INTL_DATA_ERROR;
        } else if (base < 0xfb00 || base > 0xfb05 || v[3] > v[0] ||
                   v[1] - v[3] > 0x7fff) {
            return QJS_INTL_DATA_ERROR;
        }
        previous = v[1];
    }
    previous = 0;
    for (i = 0; i < d.digits.record_count; i++) {
        if (!field(&d.digits, i, 0, &cp) || !scalar(cp) || cp > 0x10fff6 ||
            (cp <= 0xdfff && cp + 9 >= 0xd800) || (i && cp <= previous))
            return QJS_INTL_DATA_ERROR;
        previous = cp + 9;
    }
    previous = 0;
    for (i = 0; i < d.capabilities.record_count; i++) {
        uint32_t j, locale;
        if (!field(&d.capabilities, i, 0, &locale)) return QJS_INTL_DATA_ERROR;
        for (j = 0; j < 4; j++)
            if (!field(&d.capabilities, i, 4 + 4 * j, &v[j]))
                return QJS_INTL_DATA_ERROR;
        /* Adjacent sort/search pairs prove one common internal locale set.
         * No row can advertise a locale with only one required usage.
         */
        if (locale >= locales.record_count || v[0] != (i & 1u) ||
            (i && ((i & 1u) ? locale != previous : locale <= previous)) ||
            v[1] != 31 || v[2] != 3 || v[3] != 0)
            return QJS_INTL_DATA_ERROR;
        previous = locale;
    }
    if (d.capabilities.record_count & 1u) return QJS_INTL_DATA_ERROR;
    *out = d;
    return QJS_INTL_OK;
}
QJSIntlDataStatus qjs_intl_data_validate_collation_extension(
                                                    const QJSIntlDataView *v)
{
    QJSIntlCollationData d;
    QJSIntlStatus s = qjs_intl_collation_data_init(v, &d);
    return s == QJS_INTL_OK || s == QJS_INTL_UNSUPPORTED ?
                                      QJS_INTL_DATA_OK : QJS_INTL_DATA_INVALID;
}

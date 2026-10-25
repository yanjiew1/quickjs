/* Sparse native unit forms; no formatter fallback or API availability policy.
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
#ifndef QJS_INTL_UNIT_DATA_H
#define QJS_INTL_UNIT_DATA_H
#include "../native/data.h"

typedef enum QJSIntlUnitWidth {
    QJS_INTL_UNIT_LONG,
    QJS_INTL_UNIT_SHORT,
    QJS_INTL_UNIT_NARROW,
    QJS_INTL_UNIT_WIDTH_COUNT
} QJSIntlUnitWidth;

typedef enum QJSIntlUnitGroupKind {
    QJS_INTL_UNIT_GROUP_COMPOUND,
    QJS_INTL_UNIT_GROUP_SINGLE
} QJSIntlUnitGroupKind;

typedef enum QJSIntlUnitField {
    QJS_INTL_UNIT_DISPLAY_NAME,
    QJS_INTL_UNIT_PATTERN,
    QJS_INTL_UNIT_PER_PATTERN,
    QJS_INTL_UNIT_GENDER,
    QJS_INTL_UNIT_COMPOUND_PATTERN,
    QJS_INTL_UNIT_FIELD_COUNT
} QJSIntlUnitField;

typedef enum QJSIntlUnitCount {
    QJS_INTL_UNIT_COUNT_NONE,
    QJS_INTL_UNIT_COUNT_EXACT_ZERO,
    QJS_INTL_UNIT_COUNT_EXACT_ONE,
    QJS_INTL_UNIT_COUNT_ZERO,
    QJS_INTL_UNIT_COUNT_ONE,
    QJS_INTL_UNIT_COUNT_TWO,
    QJS_INTL_UNIT_COUNT_FEW,
    QJS_INTL_UNIT_COUNT_MANY,
    QJS_INTL_UNIT_COUNT_OTHER,
    QJS_INTL_UNIT_COUNT_COUNT
} QJSIntlUnitCount;

enum { QJS_INTL_UNIT_FORM_VALUE, QJS_INTL_UNIT_FORM_BLOCKED };

typedef struct QJSIntlUnitGroup {
    QJSIntlUnitGroupKind kind;
    QJSIntlBytes name;
} QJSIntlUnitGroup;

typedef struct QJSIntlUnitMap {
    const uint8_t *entries;
    uint32_t count;
} QJSIntlUnitMap;

typedef struct QJSIntlUnitForm {
    QJSIntlUnitField field;
    QJSIntlUnitCount count;
    QJSIntlBytes case_name, text;
    unsigned int state;
} QJSIntlUnitForm;

typedef struct QJSIntlUnitOverlay {
    QJSIntlBytes locale;
    QJSIntlUnitWidth width;
    uint32_t group, next; /* Group is the source group, which aliases may change. */
    QJSIntlUnitMap map;
} QJSIntlUnitOverlay;

typedef struct QJSIntlUnitData {
    QJSIntlData data;
    QJSIntlSection section;
    const uint8_t *groups, *cases, *locale_rows, *rows, *maps, *entries;
    const uint8_t *overlay_locales, *overlay_rows, *nodes;
    uint32_t group_count, case_count, row_count, map_count, entry_count;
    uint32_t overlay_row_count, node_count;
} QJSIntlUnitData;

/* Require an unchanged validated data view. Borrow its immutable storage;
   temporary validation memory is released on every path. Follow pack.h's
   disjoint output contract; non-NULL outputs are reset on failure. Template
   syntax, grammatical fallback and sanctioned API identifiers have other owners. */
QJSIntlStatus qjs_intl_unit_data_open(const QJSIntlData *, const QJSIntlAllocator *,
                                      QJSIntlUnitData *);

/* Source group inventory includes unsanctioned identifiers. Indexed/exact
   queries allocate nothing and borrow the unchanged owner. A well-formed
   missing name returns UNSUPPORTED. Only "per" is a compound group in v1. */
QJSIntlStatus qjs_intl_unit_group_at(const QJSIntlUnitData *, uint32_t index, QJSIntlUnitGroup *);
QJSIntlStatus qjs_intl_unit_group_find(const QJSIntlUnitData *, QJSIntlUnitGroupKind,
                                       QJSIntlBytes name, uint32_t *index);
QJSIntlStatus qjs_intl_unit_map_at(const QJSIntlUnitData *, uint32_t locale, QJSIntlUnitWidth,
                                   uint32_t group, QJSIntlUnitMap *);

/* Ordered local source layers retain alias origins, before exact-leaf merging.
   Head/next IDs are one-based; zero ends an empty/exhausted chain. A nonzero
   next ID is smaller than its child, so traversal is bounded. Nodes contain
   nonempty maps, including blocked and empty leaves. Queries borrow storage
   and perform no selector fallback. Origin locales need not be in the selected
   profile's locale inventory. Require IDs returned by this unchanged owner. */
QJSIntlStatus qjs_intl_unit_overlay_head(const QJSIntlUnitData *, uint32_t locale,
                                        QJSIntlUnitWidth, uint32_t group, uint32_t *);
QJSIntlStatus qjs_intl_unit_overlay_at(const QJSIntlUnitData *, uint32_t id, QJSIntlUnitOverlay *);

/* Require a map returned by this unchanged owner. No plural/case/width
   fallback is performed. Find returns UNSUPPORTED for an absent selector;
   explicit empty text is VALUE with length zero, distinct from BLOCKED.
   Indexed forms use field/count/case order.
   An empty case_name slice denotes the default case and may have NULL data. */
QJSIntlStatus qjs_intl_unit_form_at(const QJSIntlUnitData *, const QJSIntlUnitMap *, uint32_t index,
                                    QJSIntlUnitForm *);
QJSIntlStatus qjs_intl_unit_form_find(const QJSIntlUnitData *, const QJSIntlUnitMap *,
                                      QJSIntlUnitField, QJSIntlUnitCount, QJSIntlBytes case_name,
                                      QJSIntlUnitForm *);
#endif

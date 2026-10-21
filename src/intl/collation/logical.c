/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "logical.h"
#include <string.h>

#define FIRST_HEAD UINT32_C(0xfb00)
/* UCA 18's generic family reaches FBC0 + (10FFFF >> 15) = FBE1. */
#define LAST_HEAD UINT32_C(0xfbe1)
#define TAIL_COUNT UINT32_C(0x8000)

QJSIntlStatus qjs_intl_collation_primary_ordinal(uint32_t key, uint32_t *ordinal)
{
    uint32_t first = key >> 16, tail = key & 0xffff, intervals;
    if (!ordinal)
        return QJS_INTL_INVALID_ARGUMENT;
    *ordinal = 0;
    if (tail && (first < FIRST_HEAD || first > LAST_HEAD || tail < 0x8000))
        return QJS_INTL_INVALID_ARGUMENT;
    intervals = first <= FIRST_HEAD ? 0 :
        first > LAST_HEAD ? LAST_HEAD - FIRST_HEAD + 1 : first - FIRST_HEAD;
    /* Each prior head contributes its complete virtual tail interval. The
       ordinary head anchor itself precedes those tails; the next ordinary
       primary follows them. No per-codepoint or per-program root array. */
    *ordinal = first + intervals * TAIL_COUNT + (tail ? tail - 0x8000 + 1 : 0);
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_collation_logical_root(const QJSIntlCollationElement *first,
                                             const QJSIntlCollationElement *second,
                                             QJSIntlCollationLogical *out, unsigned int *consumed)
{
    uint32_t key;
    unsigned int count = 1;
    if (out)
        memset(out, 0, sizeof(*out));
    if (consumed)
        *consumed = 0;
    if (!first || !out || !consumed)
        return QJS_INTL_INVALID_ARGUMENT;
    if (first->variable > 1 || (first->variable && !first->primary) ||
        (first->primary && (!first->secondary || !first->tertiary)) ||
        (first->secondary && !first->tertiary))
        return QJS_INTL_DATA_ERROR;
    key = (uint32_t)first->primary << 16;
    if (first->primary >= FIRST_HEAD && first->primary <= LAST_HEAD) {
        if (!second || first->variable || second->variable || second->primary < 0x8000 ||
            second->secondary || second->tertiary)
            return QJS_INTL_DATA_ERROR;
        key |= second->primary;
        count = 2;
    }
    if (qjs_intl_collation_primary_ordinal(key, &out->primary))
        return QJS_INTL_DATA_ERROR;
    out->secondary = first->secondary;
    out->tertiary = first->tertiary;
    out->variable = first->variable;
    out->implicit = count == 2;
    *consumed = count;
    return QJS_INTL_OK;
}

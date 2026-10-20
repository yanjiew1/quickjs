/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "dayperiod-code.h"
#include <string.h>

#define NAME(s) {s, sizeof(s) - 1}
static const QJSIntlBytes codes[QJS_INTL_DAYPERIOD_CODE_COUNT] = {
    NAME("am"), NAME("pm"), NAME("midnight"), NAME("noon"),
    NAME("morning1"), NAME("morning2"), NAME("afternoon1"), NAME("afternoon2"),
    NAME("evening1"), NAME("evening2"), NAME("night1"), NAME("night2")
};
#undef NAME

QJSIntlBytes qjs_intl_dayperiod_code_name(uint32_t code)
{
    return code < QJS_INTL_DAYPERIOD_CODE_COUNT ? codes[code] : (QJSIntlBytes){0};
}

int qjs_intl_dayperiod_code_find(QJSIntlBytes name)
{
    size_t i;
    if (!name.data)
        return -1;
    for (i = 0; i < QJS_INTL_DAYPERIOD_CODE_COUNT; i++) {
        if (name.length == codes[i].length && !memcmp(name.data, codes[i].data, name.length))
            return (int)i;
    }
    return -1;
}

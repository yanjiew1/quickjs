/* Exact LDML letter/mark boundaries; symbols are not ASCII-only identifiers.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/currency/boundary.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

int main(void)
{
    static const struct {
        const char *text;
        unsigned int before, after;
    } cases[] = {
        {"", 0, 0},
        {"USD", 1, 1},
        {"$CA", 1, 0},
        {"CA$", 0, 1},
        {"Á", 1, 1},
        {"Á⃝", 1, 1},
        {"$́", 0, 0},
        {"́A", 1, 0},
        {"字", 1, 1},
        {"𐐀́", 1, 1},
        {"ʰ", 1, 1},
        {"Ⅰ", 0, 0},
        {"ͅ", 0, 0},
        {"A؜", 0, 1},
        {"؜A", 1, 0},
        {"A ", 0, 1},
        {" A", 1, 0},
    };
    static const char *invalid[] = {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xc2"};
    unsigned int before, after;
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        QJSIntlBytes text = {cases[i].text, strlen(cases[i].text)};
        assert(!qjs_intl_currency_boundary(text, &before, &after));
        assert(before == cases[i].before && after == cases[i].after);
    }
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        before = after = 99;
        assert(qjs_intl_currency_boundary((QJSIntlBytes){invalid[i], strlen(invalid[i])},
                                           &before, &after) == QJS_INTL_INVALID_ARGUMENT);
        assert(!before && !after);
    }
    assert(!qjs_intl_currency_boundary((QJSIntlBytes){0}, &before, &after));
    assert(!before && !after);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){NULL, 1}, &before, &after) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){"A", (size_t)INT_MAX + 1}, &before, &after) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){"A", 1}, NULL, &after) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){"A", 1}, &before, NULL) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(qjs_intl_currency_boundary((QJSIntlBytes){"A\0B", 3}, &before, &after) ==
           QJS_INTL_INVALID_ARGUMENT);
    assert(!before && !after);
    return 0;
}

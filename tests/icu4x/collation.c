/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "../icu4c/helpers.h"
#include "../../src/intl/collation/collation.h"

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlCollationService *service;
    QJSIntlCollator *collator;
    QJSIntlCollationOptions options;
    const QJSIntlBytes *locales;
    QJSIntlBytes type;
    size_t count, locale, constructions = 0, en, de;
    uint32_t cursor;
    int supported, order;
    char name[] = "phonebk";
    static const uint16_t ae[] = {'a', 'e'}, umlaut[] = {0xe4};

    assert(!qjs_intl_collation_open(&allocator, NULL, &service));
    assert(!qjs_intl_collation_available(service, &locales, &count));
    en = find(locales, count, "en");
    de = find(locales, count, "de");
    /* Direct metadata admission and guarded constructor admission must agree
       for every named type at every declared source locale, not just en/de. */
    for (locale = 0; locale < count; locale++) {
        cursor = 0;
        assert(!qjs_intl_collation_defaults(service, locale, QJS_INTL_COLLATION_SORT, &options));
        while (!qjs_intl_collation_type_next(service, locale, QJS_INTL_COLLATION_SORT, &cursor,
                                             &type)) {
            assert(!qjs_intl_collation_type_supported(service, locale, QJS_INTL_COLLATION_SORT,
                                                      type, &supported) &&
                   supported);
            options.collation = type;
            assert(!qjs_intl_collator_open(service, locale, &options, &collator));
            qjs_intl_collator_close(collator);
            constructions++;
        }
    }
    assert(constructions > count);
    assert(!qjs_intl_collation_type_supported(service, en, QJS_INTL_COLLATION_SORT,
                                              bytes("phonebk"), &supported) &&
           !supported);
    assert(!qjs_intl_collation_type_supported(service, en, QJS_INTL_COLLATION_SORT,
                                              bytes("unknown"), &supported) &&
           !supported);
    cursor = 0;
    assert(qjs_intl_collation_type_next(service, en, QJS_INTL_COLLATION_SEARCH, &cursor, &type) ==
               QJS_INTL_UNSUPPORTED &&
           !cursor && !type.length);
    cursor = UINT32_MAX;
    assert(qjs_intl_collation_type_next(service, en, QJS_INTL_COLLATION_SORT, &cursor, &type) ==
               QJS_INTL_INVALID_ARGUMENT &&
           cursor == UINT32_MAX && !type.length);
    assert(qjs_intl_collation_available_type_next(service, &cursor, &type) ==
               QJS_INTL_INVALID_ARGUMENT &&
           cursor == UINT32_MAX);
    assert(!qjs_intl_collation_defaults(service, en, QJS_INTL_COLLATION_SORT, &options));
    options.collation = bytes("phonebk");
    collator = (QJSIntlCollator *)(uintptr_t)1;
    assert(qjs_intl_collator_open(service, en, &options, &collator) == QJS_INTL_UNSUPPORTED &&
           !collator);
    options.collation = (QJSIntlBytes){0};
    state.calls = 0;
    state.fail_at = 1;
    assert(qjs_intl_collator_open(service, en, &options, &collator) == QJS_INTL_NO_MEMORY &&
           !collator);
    state.fail_at = 0;

    assert(!qjs_intl_collation_defaults(service, de, QJS_INTL_COLLATION_SORT, &options));
    options.sensitivity = QJS_INTL_COLLATION_BASE;
    options.collation = bytes(name);
    assert(!qjs_intl_collator_open(service, de, &options, &collator));
    memset(name, 'x', sizeof(name));
    qjs_intl_collation_close(service);
    assert(!qjs_intl_collator_compare(collator, ae, 2, umlaut, 1, &order) && !order);
    assert(qjs_intl_collator_compare(collator, ae, SIZE_MAX, umlaut, 1, &order) ==
               QJS_INTL_OVERFLOW &&
           !order);
    qjs_intl_collator_close(collator);
    assert(!state.live);
    return 0;
}

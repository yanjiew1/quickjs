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
#include "helpers.h"
#include "../../src/intl/collation/collation.h"

static void compare(QJSIntlCollator *collator, const uint16_t *left, size_t left_length,
                    const uint16_t *right, size_t right_length, int expected)
{
    int order = 123;

    assert(!qjs_intl_collator_compare(collator, left, left_length, right, right_length, &order));
    assert(order == expected);
}

int main(void)
{
    Allocation state = {0};
    QJSIntlAllocator allocator = icu4c_test_allocator(&state);
    QJSIntlCollationService *service;
    QJSIntlCollator *collator;
    QJSIntlCollationOptions options;
    const QJSIntlBytes *available;
    QJSIntlBytes type;
    size_t count, en, de, calls, i, fail_ids[10], fail_count;
    uint32_t cursor = 0;
    int supported, order;
    static const uint16_t composed[] = {0xe9}, decomposed[] = {'e', 0x301};
    static const uint16_t non_fcd[] = {'a', 0x301, 0x327}, ordered[] = {'a', 0x327, 0x301};
    static const uint16_t two[] = {'2'}, ten[] = {'1', '0'};
    static const uint16_t nul_a[] = {'a', 0, 'b'}, nul_b[] = {'a', 0, 'c'};
    static const uint16_t lone[] = {0xd800};
    static const uint16_t ae[] = {'a', 'e'}, umlaut[] = {0xe4};

    state.calls = 0;
    assert(!qjs_intl_collation_open(&allocator, NULL, &service));
    calls = state.calls;
    assert(!qjs_intl_collation_available(service, &available, &count) && count > 2);
    en = find(available, count, "en");
    de = find(available, count, "de");
    assert(!qjs_intl_collation_type_supported(service, de, QJS_INTL_COLLATION_SORT,
                                              bytes("phonebk"), &supported));
    assert(supported);
    assert(!qjs_intl_collation_type_supported(service, de, QJS_INTL_COLLATION_SEARCH,
                                              bytes("phonebk"), &supported));
    assert(!supported);
    while (!qjs_intl_collation_available_type_next(service, &cursor, &type)) {
        assert(type.length && strcmp(type.data, "standard") && strcmp(type.data, "search"));
    }
    assert(cursor);
    assert(!qjs_intl_collation_defaults(service, de, QJS_INTL_COLLATION_SORT, &options));
    options.sensitivity = QJS_INTL_COLLATION_BASE;
    options.collation = bytes("phonebk");
    assert(!qjs_intl_collator_open(service, de, &options, &collator));
    compare(collator, umlaut, 1, ae, 2, 0);
    qjs_intl_collator_close(collator);
    assert(!qjs_intl_collation_defaults(service, en, QJS_INTL_COLLATION_SORT, &options));
    options.numeric = 1;
    assert(!qjs_intl_collator_open(service, en, &options, &collator));
    qjs_intl_collation_close(service);

    /* The collator owns its ICU handle and allocator, not the service rows. */
    compare(collator, composed, 1, decomposed, 2, 0);
    compare(collator, non_fcd, 3, ordered, 3, 0);
    compare(collator, two, 1, ten, 2, -1);
    compare(collator, nul_a, 3, nul_b, 3, -1);
    compare(collator, lone, 1, lone, 1, 0);
    compare(collator, NULL, 0, NULL, 0, 0);
    assert(qjs_intl_collator_compare(collator, NULL, 1, NULL, 0, &order) ==
               QJS_INTL_INVALID_ARGUMENT &&
           !order);
    qjs_intl_collator_close(collator);
    assert(!state.live);

    /* Sample early, middle and final owner allocations. Full catalogue
       construction at every allocation point would make this a slow test. */
    fail_count = calls < 8 ? calls : 8;
    for (i = 0; i < fail_count; i++)
        fail_ids[i] = i + 1;
    fail_ids[fail_count++] = (calls + 1) / 2;
    fail_ids[fail_count++] = calls;
    for (i = 0; i < fail_count; i++) {
        state.calls = 0;
        state.fail_at = fail_ids[i];
        assert(qjs_intl_collation_open(&allocator, NULL, &service) == QJS_INTL_NO_MEMORY &&
               !service);
        assert(!state.live);
    }
    qjs_intl_collation_close(NULL);
    qjs_intl_collator_close(NULL);
    return 0;
}

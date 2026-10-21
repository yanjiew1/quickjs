/* UCA 18 Table 16 fixtures, independent of root-data lookup/comparison.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "../src/intl/collation/weights.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    static const struct {
        uint32_t code;
        uint16_t first, second;
    } cases[] = {
        { 0x17000, 0xfb00, 0x8000 }, { 0x187ff, 0xfb00, 0x97ff },
        { 0x18800, 0xfb01, 0x8000 }, { 0x18aff, 0xfb01, 0x82ff },
        { 0x18d20, 0xfb00, 0x9d20 }, { 0x18df2, 0xfb01, 0x85f2 },
        { 0x1b170, 0xfb02, 0x8000 }, { 0x1b2fb, 0xfb02, 0x818b },
        { 0x18b00, 0xfb03, 0x8000 }, { 0x18cff, 0xfb03, 0x81ff },
        { 0x18e00, 0xfb04, 0x8000 }, { 0x191d2, 0xfb04, 0x83d2 },
        { 0x3d000, 0xfb05, 0x8000 }, { 0x3fc3f, 0xfb05, 0xac3f },
        { 0x4e00, 0xfb40, 0xce00 }, { 0xfa0e, 0xfb41, 0xfa0e },
        { 0x3400, 0xfb80, 0xb400 }, { 0x20000, 0xfb84, 0x8000 },
        /* Unassigned holes in otherwise specialized blocks remain generic. */
        { 0x18cdb, 0xfbc3, 0x8cdb }, { 0x18d21, 0xfbc3, 0x8d21 },
        { 0x18df3, 0xfbc3, 0x8df3 }, { 0x19192, 0xfbc3, 0x9192 },
        { 0x191d3, 0xfbc3, 0x91d3 }, { 0x1b2fc, 0xfbc3, 0xb2fc },
        { 0x378, 0xfbc0, 0x8378 },
        /* Ill-formed UTF-16 and private use need deterministic weights too. */
        { 0xd800, 0xfbc1, 0xd800 }, { 0xe000, 0xfbc1, 0xe000 },
        { 0x10ffff, 0xfbe1, 0xffff }
    };
    QJSIntlCollationElement elements[2], zero[2];
    size_t i;
    memset(zero, 0, sizeof(zero));
    for (i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        memset(elements, 0xff, sizeof(elements));
        assert(!qjs_intl_collation_implicit(cases[i].code, elements));
        assert(elements[0].primary == cases[i].first);
        assert(elements[1].primary == cases[i].second);
        assert(elements[0].secondary == 0x20 && elements[0].tertiary == 2);
        assert(!elements[1].secondary && !elements[1].tertiary);
        assert(!elements[0].variable && !elements[1].variable);
    }
    assert(qjs_intl_collation_implicit(0x110000, elements) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(elements, zero, sizeof(elements)));
    memset(elements, 0xff, sizeof(elements));
    assert(qjs_intl_collation_implicit(UINT32_MAX, elements) == QJS_INTL_INVALID_ARGUMENT);
    assert(!memcmp(elements, zero, sizeof(elements)));
    assert(qjs_intl_collation_implicit(0x17000, NULL) == QJS_INTL_INVALID_ARGUMENT);
    return 0;
}

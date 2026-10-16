/* Native radix conversion regression tests.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "dtoa.h"

int main(void)
{
    static const struct {
        const char *text;
        int radix;
        double expected;
    } cases[] = {
        { "1@2147483636", 16, INFINITY },
        { "1@-2147483636", 16, 0 },
        { "-1@-2147483636", 16, -0.0 },
        { "1p2147483636", 16, INFINITY },
        { "0.00001p-2147483636", 16, 0 },
        { "111111111111e2147483636", 10, INFINITY },
        { "0.000000000001e-2147483636", 10, 0 },
        { "0@2147483636", 16, 0 },
        { "1@2147483636", 3, INFINITY },
        { "-1@-2147483636", 36, -0.0 },
        { "1@2", 3, 9 },
        { "1@2", 16, 256 },
        { "1p2", 16, 4 },
    };
    JSATODTempMem mem;
    const char *end;
    unsigned i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        double value = js_atod(cases[i].text, &end, cases[i].radix, 0, &mem);
        assert(end == cases[i].text + strlen(cases[i].text));
        assert(value == cases[i].expected);
        assert(!!signbit(value) == !!signbit(cases[i].expected));
    }
    return 0;
}

/* Native radix conversion regression tests.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT
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

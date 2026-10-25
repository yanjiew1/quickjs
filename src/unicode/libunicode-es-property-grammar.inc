/*
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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

#include "libunicode-es-binary-properties.h"

int lre_unicode_property_allowed(const char *name)
{
#define UNICODE_ES_NAME(names, value) names,
    static const char *const aliases[] = {
        "ASCII", "Any", "Assigned",
        UNICODE_ES_BINARY_PROPERTIES(UNICODE_ES_NAME)
    };
#undef UNICODE_ES_NAME
    size_t i, length;
    if (!name || !*name)
        return 0;
    length = strlen(name);
    for (i = 0; i < countof(aliases); i++) {
        const char *start = aliases[i], *end;
        do {
            end = strchr(start, ',');
            if (length == (end ? (size_t)(end - start) : strlen(start)) &&
                !memcmp(name, start, length))
                return 1;
            if (!end)
                break;
            start = end + 1;
        } while (*start);
    }
    return 0;
}

/* Windows-to-IANA mapping through ICU4X CLDR data.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
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
#include "windows-zone.h"
#include "../icu4x/timezone.h"
#include <string.h>

int qjs_tz_windows_lookup_identifier(const char *key, const char *territory, char *output,
                                     size_t capacity)
{
    QJSICU4XZoneName name;
    size_t length;
    int error;

    if (!key || !output || !capacity)
        return -1;
    error =
        qjs_icu4x_windows_timezone((const uint8_t *)key, strlen(key), (const uint8_t *)territory,
                                   territory ? strlen(territory) : 0, &name);
    if (error)
        return -1;
    length = strlen(name.name);
    if (length >= capacity)
        return -1;
    memcpy(output, name.name, length + 1);
    return 0;
}

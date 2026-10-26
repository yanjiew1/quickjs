/* ICU duration locale admission over the shared Number/List composition.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#include "provider.h"
#include "digital.h"
#include "../icu4c/resource.h"
#include <string.h>
#include <unicode/udata.h>

QJSIntlStatus qjs_intl_duration_digital_pattern(const QJSIntlAllocator *allocator, QJSIntlBytes tag,
                                     char **out)
{
    const char *const path[] = {"durationUnits", "hms"};
    UResourceBundle *resource = NULL;
    QJSIntlStatus error;
    char *locale = NULL;
    int blocked = 0;

    *out = NULL;
    error = qjs_icu4c_locale_id(allocator, tag, &locale);
    if (!error)
        error = qjs_icu4c_resource_package_path(
            allocator, U_ICUDATA_NAME U_TREE_SEPARATOR_STRING "unit", locale, path, 2, &resource);
    if (!error)
        error = qjs_icu4c_resource_text(allocator, resource, out, &blocked);
    ures_close(resource);
    if (locale)
        allocator->free(allocator->opaque, locale);
    if (!error)
        error = blocked ? QJS_INTL_DATA_ERROR
                        : qjs_intl_duration_digital_validate((QJSIntlBytes){*out, strlen(*out)});
    if (error && *out) {
        allocator->free(allocator->opaque, *out);
        *out = NULL;
    }
    return error;
}

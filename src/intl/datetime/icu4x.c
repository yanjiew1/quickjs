/* ICU4X source binding for the shared date-program service.
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
#include "../icu4x/datetime-data.h"
#include "../icu4x/common.h"
#include "../../icu4x/datetime-names.h"
#include <string.h>

QJSIntlStatus qjs_intl_datetime_provider_pack_open(const QJSIntlAllocator *allocator,
                                                   const QJSIntlData *checked_data,
                                                   QJSIntlData *out)
{
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || checked_data)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_icu4x_status(qjs_icu4x_datetime_check_version(
        (const uint8_t *)qjs_icu4x_datetime_version, strlen(qjs_icu4x_datetime_version)));
    if (!status)
        status =
            qjs_intl_data_open(qjs_icu4x_datetime_bytes, qjs_icu4x_datetime_length, allocator, out);
    return status;
}

const QJSIntlData *qjs_intl_datetime_provider_dependency_data(const QJSIntlData *pack)
{
    (void)pack;
    return NULL;
}

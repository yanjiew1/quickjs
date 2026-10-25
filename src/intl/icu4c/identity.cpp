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
#include "identity.h"
extern "C" {
#include "common.h"
#include "../common/locale-grammar.h"
}
#include <unicode/bytestream.h>
#include <unicode/locid.h>
#include <unicode/stringpiece.h>
#include <unicode/timezone.h>
#include <unicode/unistr.h>
#include <climits>
#include <cstring>
#include <new>

namespace {
/* ByteSink makes ICU's result use the engine allocator without changing ICU's
   process-global allocation hooks. A failed append remains sticky even if ICU
   continues emitting, and no partial result crosses the C boundary. */
class IdentitySink final : public icu::ByteSink {
  public:
    explicit IdentitySink(const QJSIntlAllocator &allocator) : allocator(allocator) {}
    ~IdentitySink() override
    {
        if (buffer)
            allocator.free(allocator.opaque, buffer);
    }

    void Append(const char *bytes, int32_t count) override
    {
        if (error || count <= 0)
            return;
        if (static_cast<size_t>(count) > SIZE_MAX - length - 1) {
            error = QJS_INTL_OVERFLOW;
            return;
        }
        size_t required = length + static_cast<size_t>(count) + 1;
        if (required > capacity) {
            size_t grown = capacity > SIZE_MAX / 2 ? SIZE_MAX : capacity * 2;
            size_t next = required > grown ? required : grown;
            char *replacement =
                static_cast<char *>(allocator.realloc(allocator.opaque, buffer, next));

            if (!replacement) {
                error = QJS_INTL_NO_MEMORY;
                return;
            }
            buffer = replacement;
            capacity = next;
        }
        std::memcpy(buffer + length, bytes, static_cast<size_t>(count));
        length += static_cast<size_t>(count);
        buffer[length] = 0;
    }

    QJSIntlStatus release(char **out)
    {
        if (error)
            return error;
        if (!buffer)
            return QJS_INTL_DATA_ERROR; // A valid locale tag is never empty.
        *out = buffer;
        buffer = nullptr;
        return QJS_INTL_OK;
    }

  private:
    QJSIntlAllocator allocator;
    char *buffer = nullptr;
    size_t length = 0, capacity = 0;
    QJSIntlStatus error = QJS_INTL_OK;
};
} // namespace

extern "C" QJSIntlStatus qjs_icu4c_locale_component(const QJSIntlAllocator *allocator,
                                                    QJSIntlBytes input,
                                                    QJSICU4CLocaleOperation operation, char **out)
{
    if (out)
        *out = nullptr;
    if (!out || !allocator || !allocator->malloc || !allocator->realloc || !allocator->free ||
        static_cast<unsigned int>(operation) > QJS_ICU4C_MINIMIZE)
        return QJS_INTL_INVALID_ARGUMENT;
    if (input.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    if (!qjs_intl_unicode_locale_well_formed(input.data, input.length))
        return QJS_INTL_INVALID_ARGUMENT;
    try {
        UErrorCode status = U_ZERO_ERROR;
        icu::Locale locale = icu::Locale::forLanguageTag(
            icu::StringPiece(input.data, static_cast<int32_t>(input.length)), status);

        locale.canonicalize(status);
        if (operation == QJS_ICU4C_MAXIMIZE)
            locale.addLikelySubtags(status);
        else if (operation == QJS_ICU4C_MINIMIZE)
            locale.minimizeSubtags(status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        IdentitySink sink(*allocator);
        locale.toLanguageTag(sink, status);
        if (U_FAILURE(status))
            return qjs_icu4c_status(status);
        return sink.release(out);
    } catch (const std::bad_alloc &) {
        return QJS_INTL_NO_MEMORY;
    } catch (...) {
        // No C++ exception may unwind through the C engine's frames.
        return QJS_INTL_DATA_ERROR;
    }
}

extern "C" QJSIntlStatus qjs_icu4c_zone_region(QJSIntlBytes source_zone, char region[4],
                                               size_t *length)
{
    if (region)
        region[0] = 0;
    if (length)
        *length = 0;
    if (!region || !length || !source_zone.data || !source_zone.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (source_zone.length > INT32_MAX)
        return QJS_INTL_OVERFLOW;
    for (size_t i = 0; i < source_zone.length; i++) {
        if (!source_zone.data[i] || static_cast<unsigned char>(source_zone.data[i]) > 127)
            return QJS_INTL_INVALID_ARGUMENT;
    }
    try {
        UErrorCode status = U_ZERO_ERROR;
        icu::UnicodeString zone = icu::UnicodeString::fromUTF8(
            icu::StringPiece(source_zone.data, static_cast<int32_t>(source_zone.length)));
        if (zone.isBogus())
            return QJS_INTL_NO_MEMORY;
        int32_t count = icu::TimeZone::getRegion(zone, region, 4, status);
        if (U_FAILURE(status)) {
            region[0] = 0;
            return qjs_icu4c_status(status);
        }
        if (count < 0 || count > 3) {
            region[0] = 0;
            return QJS_INTL_DATA_ERROR;
        }
        *length = static_cast<size_t>(count);
        return QJS_INTL_OK;
    } catch (const std::bad_alloc &) {
        return QJS_INTL_NO_MEMORY;
    } catch (...) {
        return QJS_INTL_DATA_ERROR;
    }
}

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
#include "resource.h"
#include <limits.h>
#include <string.h>
#include <unicode/uloc.h>
#include <unicode/ustring.h>

QJSIntlStatus qjs_icu4c_resource_text(const QJSIntlAllocator *allocator,
                                      const UResourceBundle *resource, char **out, int *blocked)
{
    UErrorCode status = U_ZERO_ERROR;
    const UChar *text;
    int32_t length, bytes, i;
    char *result;

    *out = NULL;
    *blocked = 0;
    text = ures_getString(resource, &length, &status);
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (length == 3 && text[0] == 0x2205 && text[1] == 0x2205 && text[2] == 0x2205) {
        *blocked = 1;
        return QJS_INTL_OK;
    }
    for (i = 0; i < length; i++) {
        if (!text[i])
            return QJS_INTL_DATA_ERROR; /* CLDR text cannot contain embedded NUL. */
    }
    u_strToUTF8(NULL, 0, &bytes, text, length, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR)
        status = U_ZERO_ERROR;
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (bytes < 0 || bytes == INT32_MAX)
        return QJS_INTL_OVERFLOW;
    result = allocator->malloc(allocator->opaque, (size_t)bytes + 1);
    if (!result)
        return QJS_INTL_NO_MEMORY;
    u_strToUTF8(result, bytes + 1, NULL, text, length, &status);
    if (U_FAILURE(status)) {
        allocator->free(allocator->opaque, result);
        return qjs_icu4c_status(status);
    }
    *out = result;
    return QJS_INTL_OK;
}

static QJSIntlStatus base_name(const QJSIntlAllocator *allocator, const char *locale, char **out)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = uloc_getBaseName(locale, NULL, 0, &status);
    char *result;

    *out = NULL;
    if (status == U_BUFFER_OVERFLOW_ERROR)
        status = U_ZERO_ERROR;
    if (U_FAILURE(status))
        return qjs_icu4c_status(status);
    if (length < 0 || length == INT32_MAX)
        return QJS_INTL_OVERFLOW;
    if (!length)
        return qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){"root", 4}, out);
    result = allocator->malloc(allocator->opaque, (size_t)length + 1);
    if (!result)
        return QJS_INTL_NO_MEMORY;
    uloc_getBaseName(locale, result, length + 1, &status);
    if (U_FAILURE(status)) {
        allocator->free(allocator->opaque, result);
        return qjs_icu4c_status(status);
    }
    *out = result;
    return QJS_INTL_OK;
}

static QJSIntlStatus resource_paths(const QJSIntlAllocator *allocator, const char *package,
                                    const char *locale, const QJSICU4CResourcePath *paths,
                                    size_t path_count, int keep_marker, UResourceBundle **out)
{
    char *current = NULL;
    QJSIntlStatus error;
    unsigned int depth;
    uint32_t blocked_paths = 0;

    if (out)
        *out = NULL;
    if (!out || !paths || !path_count || path_count > 32)
        return QJS_INTL_INVALID_ARGUMENT;
    error = base_name(allocator, locale, &current);
    if (error)
        return error;
    error = QJS_INTL_UNSUPPORTED;
    for (depth = 0; depth < 32; depth++) {
        UErrorCode status = U_ZERO_ERROR;
        UResourceBundle *bundle = ures_openDirect(package, current, &status);
        UResourceBundle *child = NULL, *parent = NULL;
        size_t i;
        char *next = NULL;
        int blocked = 0;

        if (U_SUCCESS(status)) {
            size_t variant;

            for (variant = 0; variant < path_count; variant++) {
                const char *const *path = paths[variant].keys;
                size_t count = paths[variant].count;

                if (blocked_paths & (UINT32_C(1) << variant))
                    continue;
                status = U_ZERO_ERROR;
                if (!count) {
                    *out = bundle;
                    error = QJS_INTL_OK;
                    goto done;
                }
                for (i = 0; i < count; i++) {
                    UResourceBundle *next_child =
                        ures_getByKey(child ? child : bundle, path[i], NULL, &status);

                    ures_close(child);
                    child = next_child;
                    if (U_FAILURE(status))
                        break;
                    if (ures_getType(child) == URES_STRING) {
                        int32_t length;
                        const UChar *text = ures_getString(child, &length, &status);

                        if (U_SUCCESS(status) && length == 3 && text[0] == 0x2205 &&
                            text[1] == 0x2205 && text[2] == 0x2205) {
                            if (keep_marker)
                                break;
                            blocked_paths |= UINT32_C(1) << variant;
                            status = U_MISSING_RESOURCE_ERROR;
                            break;
                        }
                    }
                }
                if (U_SUCCESS(status)) {
                    *out = child;
                    ures_close(bundle);
                    error = QJS_INTL_OK;
                    goto done;
                }
                ures_close(child);
                child = NULL;
                if (status != U_MISSING_RESOURCE_ERROR)
                    break;
            }
            /* Every variant can be explicitly blocked at this node. The
               parent resolver below still needs the ordinary missing status. */
            if (variant == path_count)
                status = U_MISSING_RESOURCE_ERROR;
        }
        if (status != U_MISSING_RESOURCE_ERROR) {
            error = qjs_icu4c_status(status);
            ures_close(bundle);
            break;
        }
        if (!strcmp(current, "root")) {
            ures_close(bundle);
            break;
        }
        if (bundle) {
            status = U_ZERO_ERROR;
            parent = ures_getByKey(bundle, "%%Parent", NULL, &status);
            if (U_SUCCESS(status)) {
                error = qjs_icu4c_resource_text(allocator, parent, &next, &blocked);
                if (!error && (blocked || !next[0]))
                    error = QJS_INTL_DATA_ERROR;
            } else if (status == U_MISSING_RESOURCE_ERROR) {
                error = QJS_INTL_OK;
            } else {
                error = qjs_icu4c_status(status);
            }
            ures_close(parent);
        } else {
            error = QJS_INTL_OK;
        }
        if (!error && !next) {
            /* Ordinary parents only remove subtags, so current's capacity is
               sufficient. Explicit CLDR parents above may be longer instead. */
            status = U_ZERO_ERROR;
            size_t capacity = strlen(current) + 1;

            error = qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){current, capacity - 1}, &next);
            if (!error) {
                int32_t length = uloc_getParent(current, next, (int32_t)capacity, &status);

                if (U_FAILURE(status))
                    error = qjs_icu4c_status(status);
                else if (!length) {
                    allocator->free(allocator->opaque, next);
                    next = NULL;
                    error = qjs_icu4c_copy_bytes(allocator, (QJSIntlBytes){"root", 4}, &next);
                }
            }
        }
        ures_close(bundle);
        if (error) {
            if (next)
                allocator->free(allocator->opaque, next);
            break;
        }
        if (next) {
            allocator->free(allocator->opaque, current);
            current = next;
        }
        error = QJS_INTL_UNSUPPORTED;
    }
    if (depth == 32)
        error = QJS_INTL_DATA_ERROR; /* Malformed cyclic resource parents. */
done:
    allocator->free(allocator->opaque, current);
    return error;
}

QJSIntlStatus qjs_icu4c_resource_variants(const QJSIntlAllocator *allocator, const char *package,
                                          const char *locale, const QJSICU4CResourcePath *paths,
                                          size_t count, UResourceBundle **out)
{
    return resource_paths(allocator, package, locale, paths, count, 0, out);
}

QJSIntlStatus qjs_icu4c_resource_package_path(const QJSIntlAllocator *allocator,
                                              const char *package, const char *locale,
                                              const char *const *path, size_t count,
                                              UResourceBundle **out)
{
    QJSICU4CResourcePath single = {path, count};

    return resource_paths(allocator, package, locale, &single, 1, 1, out);
}

QJSIntlStatus qjs_icu4c_resource_path(const QJSIntlAllocator *allocator, const char *locale,
                                      const char *const *path, size_t count, UResourceBundle **out)
{
    return qjs_icu4c_resource_package_path(allocator, NULL, locale, path, count, out);
}

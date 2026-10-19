/* Shared fixed locale algorithms. Alias-dependent work belongs to locale.h.
 * ECMA402 730ca2b88c305862db7be2f83c510dcca33ab48c, reviewed 2026-10-19.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "identifier.h"
#include "../common/locale-grammar.h"
#include <string.h>

typedef struct Cursor {
    QJSIntlBytes tag;
    size_t position;
} Cursor;

static QJSIntlBytes next(Cursor *cursor)
{
    size_t start = cursor->position, end = start;
    QJSIntlBytes value = {NULL, 0};

    if (start >= cursor->tag.length)
        return value;
    while (end < cursor->tag.length && cursor->tag.data[end] != '-')
        end++;
    value.data = cursor->tag.data + start;
    value.length = end - start;
    cursor->position = end < cursor->tag.length ? end + 1 : end;
    return value;
}

static int kind(QJSIntlBytes value, QJSIntlLocaleSubtagKind expected)
{
    return qjs_intl_unicode_subtag_well_formed(expected, value.data,
                                               value.length);
}

QJSIntlStatus qjs_intl_locale_components(QJSIntlBytes tag,
                                         QJSIntlLocaleComponents *out)
{
    QJSIntlLocaleComponents result = {0};
    Cursor cursor = {tag, 0};
    QJSIntlBytes token;
    const char *base_end;

    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !tag.data || !tag.length)
        return QJS_INTL_INVALID_ARGUMENT;
    result.language = next(&cursor);
    if (!kind(result.language, QJS_INTL_LANGUAGE_SUBTAG))
        return QJS_INTL_INVALID_ARGUMENT;
    base_end = result.language.data + result.language.length;
    token = next(&cursor);
    if (kind(token, QJS_INTL_SCRIPT_SUBTAG)) {
        result.script = token;
        base_end = token.data + token.length;
        token = next(&cursor);
    }
    if (kind(token, QJS_INTL_REGION_SUBTAG)) {
        result.region = token;
        base_end = token.data + token.length;
        token = next(&cursor);
    }
    while (kind(token, QJS_INTL_VARIANT_SUBTAG)) {
        if (!result.variants.data)
            result.variants.data = token.data;
        base_end = token.data + token.length;
        result.variants.length = base_end - result.variants.data;
        token = next(&cursor);
    }
    result.base_name = (QJSIntlBytes){tag.data, base_end - tag.data};
    while (token.data) {
        QJSIntlBytes singleton = token;
        const char *end;

        if (singleton.length != 1)
            return QJS_INTL_INVALID_ARGUMENT;
        if (*singleton.data == 'x')
            break;
        token = next(&cursor);
        if (!token.data || token.length < 2)
            return QJS_INTL_INVALID_ARGUMENT;
        do {
            end = token.data + token.length;
            token = next(&cursor);
        } while (token.data && token.length > 1);
        if (*singleton.data == 'u') {
            result.unicode_extension.data = singleton.data - 1;
            result.unicode_extension.length = end - (singleton.data - 1);
        }
    }
    *out = result;
    return QJS_INTL_OK;
}

QJSIntlBytes qjs_intl_locale_keyword(const QJSIntlLocaleComponents *components,
                                     const char key[2])
{
    Cursor cursor = {components->unicode_extension, 3};
    QJSIntlBytes token = next(&cursor);

    while (token.data && token.length > 2)
        token = next(&cursor);
    while (token.data) {
        QJSIntlBytes current = token, value;
        const char *end = current.data + current.length;

        token = next(&cursor);
        value.data = token.data ? token.data : end;
        while (token.data && token.length > 2) {
            end = token.data + token.length;
            token = next(&cursor);
        }
        value.length = end >= value.data ? (size_t)(end - value.data) : 0;
        if (current.length == 2 && current.data[0] == key[0] &&
            current.data[1] == key[1])
            return value;
    }
    return (QJSIntlBytes){NULL, 0};
}

typedef struct Buffer {
    const QJSIntlAllocator *allocator;
    char *data;
    size_t size, capacity;
    QJSIntlStatus status;
} Buffer;

static int append(Buffer *buffer, QJSIntlBytes text)
{
    size_t capacity, need;
    char *data;

    if (buffer->status)
        return -1;
    if (text.length > SIZE_MAX - buffer->size - 1) {
        buffer->status = QJS_INTL_OVERFLOW;
        return -1;
    }
    need = buffer->size + text.length + 1;
    if (need > buffer->capacity) {
        capacity =
            buffer->capacity > SIZE_MAX / 2 ? need : buffer->capacity * 2;
        if (capacity < need)
            capacity = need;
        data = buffer->allocator->realloc(buffer->allocator->opaque,
                                          buffer->data, capacity);
        if (!data) {
            buffer->status = QJS_INTL_NO_MEMORY;
            return -1;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    if (text.length)
        memcpy(buffer->data + buffer->size, text.data, text.length);
    buffer->size += text.length;
    buffer->data[buffer->size] = 0;
    return 0;
}

static void append_subtag(Buffer *buffer, QJSIntlBytes text)
{
    if (text.data) {
        append(buffer, (QJSIntlBytes){"-", 1});
        append(buffer, text);
    }
}

static int allocator_valid(const QJSIntlAllocator *allocator)
{
    return allocator && allocator->malloc && allocator->realloc &&
           allocator->free;
}

static QJSIntlStatus finish(Buffer *buffer, char **out)
{
    if (buffer->status) {
        buffer->allocator->free(buffer->allocator->opaque, buffer->data);
        return buffer->status;
    }
    *out = buffer->data;
    return QJS_INTL_OK;
}

QJSIntlStatus qjs_intl_locale_replace_base(
    const QJSIntlAllocator *allocator, QJSIntlBytes tag,
    const QJSIntlLocaleBaseOptions *options, char **out)
{
    QJSIntlLocaleComponents parts;
    Buffer buffer = {allocator, NULL, 0, 0, QJS_INTL_OK};
    QJSIntlStatus status;

    if (out)
        *out = NULL;
    if (!out || !options || !allocator_valid(allocator))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_components(tag, &parts);
    if (status)
        return status;
    append(&buffer,
           options->language.data ? options->language : parts.language);
    append_subtag(&buffer,
                  options->script.data ? options->script : parts.script);
    append_subtag(&buffer,
                  options->region.data ? options->region : parts.region);
    append_subtag(&buffer,
                  options->variants.data ? options->variants : parts.variants);
    append(&buffer, (QJSIntlBytes){tag.data + parts.base_name.length,
                                   tag.length - parts.base_name.length});
    return finish(&buffer, out);
}

static int key_index(const char key[2])
{
    int first = key[0] >= '0' && key[0] <= '9'   ? key[0] - '0'
                : key[0] >= 'a' && key[0] <= 'z' ? key[0] - 'a' + 10
                                                 : -1;

    if (first < 0 || key[1] < 'a' || key[1] > 'z')
        return -1;
    return first * 26 + key[1] - 'a';
}

QJSIntlStatus qjs_intl_locale_replace_keywords(
    const QJSIntlAllocator *allocator, QJSIntlBytes tag,
    const QJSIntlLocaleKeyword *keywords, size_t count, char **out)
{
    QJSIntlLocaleComponents parts;
    unsigned char seen[36 * 26] = {0};
    Buffer buffer = {allocator, NULL, 0, 0, QJS_INTL_OK};
    QJSIntlStatus status;
    QJSIntlBytes token, original;
    Cursor cursor;
    size_t i, prefix, suffix;

    if (out)
        *out = NULL;
    if (!out || !allocator_valid(allocator) || (!keywords && count) ||
        count > sizeof(seen))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_locale_components(tag, &parts);
    if (status)
        return status;
    for (i = 0; i < count; i++) {
        int index = key_index(keywords[i].key);
        QJSIntlBytes value = keywords[i].value;

        if (index < 0 || seen[index] || (!value.data && value.length) ||
            (value.length &&
             !qjs_intl_unicode_type_well_formed(value.data, value.length)))
            return QJS_INTL_INVALID_ARGUMENT;
        seen[index] = 1;
    }
    prefix = parts.unicode_extension.data
                 ? (size_t)(parts.unicode_extension.data - tag.data)
                 : parts.base_name.length;
    suffix = prefix + parts.unicode_extension.length;
    append(&buffer, (QJSIntlBytes){tag.data, prefix});
    if (count || parts.unicode_extension.data) {
        append(&buffer, (QJSIntlBytes){"-u", 2});
        cursor = (Cursor){parts.unicode_extension, 3};
        token = next(&cursor);
        while (token.data && token.length > 2) {
            append_subtag(&buffer, token);
            token = next(&cursor);
        }
        while (token.data) {
            int index = token.length == 2 ? key_index(token.data) : -1;
            const char *end = token.data + token.length;
            original.data = token.data;
            token = next(&cursor);
            while (token.data && token.length > 2) {
                end = token.data + token.length;
                token = next(&cursor);
            }
            original.length = end - original.data;
            if (index < 0 || !seen[index])
                append_subtag(&buffer, original);
        }
        for (i = 0; i < count; i++) {
            append_subtag(&buffer, (QJSIntlBytes){keywords[i].key, 2});
            if (keywords[i].value.length)
                append_subtag(&buffer, keywords[i].value);
        }
    }
    append(&buffer, (QJSIntlBytes){tag.data + suffix, tag.length - suffix});
    return finish(&buffer, out);
}

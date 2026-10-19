/* Iterative CLDR language-name composition, without nested temporary strings.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "language-format.h"
#include "../common/format-builder.h"
#include "../common/pair-pattern.h"
#include "../locale/identifier.h"
#include "../../cutils/cutils.h"
#include <string.h>

typedef struct Cursor {
    QJSIntlBytes text;
    size_t at;
} Cursor;

static QJSIntlBytes next(Cursor *cursor)
{
    size_t start = cursor->at;
    if (start == cursor->text.length)
        return (QJSIntlBytes){0};
    while (cursor->at < cursor->text.length && cursor->text.data[cursor->at] != '-')
        cursor->at++;
    {
        QJSIntlBytes token = {cursor->text.data + start, cursor->at - start};
        if (cursor->at < cursor->text.length)
            cursor->at++;
        return token;
    }
}

static int equal_lower(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t i;
    if (a.length != b.length)
        return 0;
    for (i = 0; i < a.length; i++) {
        unsigned int c = (uint8_t)b.data[i];
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
        if ((uint8_t)a.data[i] != c)
            return 0;
    }
    return 1;
}

/* Count first, then fill one borrowed-slice array. The matched source key
   consumes an ordered subset, including the primary language. */
static QJSIntlStatus qualifiers(const QJSIntlNameData *data, uint32_t locale,
                               QJSIntlNameStyle style, QJSIntlBytes input,
                               QJSIntlBytes matched, QJSIntlBytes *labels,
                               size_t *count)
{
    QJSIntlLocaleComponents parts;
    Cursor source = {matched, 0}, value = {input, 0};
    QJSIntlBytes wanted = next(&source), token;
    QJSIntlStatus status = qjs_intl_locale_components(input, &parts);
    *count = 0;
    if (status)
        return status;
    while ((token = next(&value)).data) {
        if (wanted.data && equal_lower(wanted, token)) {
            wanted = next(&source);
            continue;
        }
        if (labels) {
            char normalized[8];
            QJSIntlNameLabel label;
            QJSIntlNameGroup group = token.data == parts.script.data ? QJS_INTL_NAME_SCRIPT :
                                    token.data == parts.region.data ? QJS_INTL_NAME_REGION :
                                                                     QJS_INTL_NAME_VARIANT;
            size_t i;
            if (token.length > sizeof(normalized))
                return QJS_INTL_DATA_ERROR;
            for (i = 0; i < token.length; i++) {
                unsigned int c = (uint8_t)token.data[i];
                normalized[i] = c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
            }
            status = qjs_intl_name_select(data, locale, group, style, 0,
                                          (QJSIntlBytes){normalized, token.length}, &label);
            if (status)
                return status;
            labels[*count] = label.found ? label.entry.text : token;
        }
        (*count)++;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus pattern(const QJSIntlNameData *data, uint32_t locale,
                            const char *key, QJSIntlPairPattern *out)
{
    QJSIntlNameLabel label;
    QJSIntlStatus status = qjs_intl_name_select(data, locale, QJS_INTL_NAME_PATTERN,
                                               QJS_INTL_NAME_LONG, 0,
                                               (QJSIntlBytes){key, strlen(key)}, &label);
    if (status)
        return status;
    if (!label.found)
        return QJS_INTL_UNSUPPORTED;
    status = qjs_intl_pair_pattern_parse(label.entry.text, out);
    return status == QJS_INTL_UNSUPPORTED ? QJS_INTL_DATA_ERROR : status;
}

static int contains(QJSIntlBytes text, QJSIntlBytes scalar)
{
    size_t i;
    if (scalar.length > text.length)
        return 0;
    for (i = 0; i <= text.length - scalar.length; i++) {
        if (!memcmp(text.data + i, scalar.data, scalar.length))
            return 1;
    }
    return 0;
}

static QJSIntlStatus scalar_next(QJSIntlBytes text, size_t *at)
{
    const uint8_t *end;
    size_t remaining = text.length - *at;
    int c = unicode_from_utf8((const uint8_t *)text.data + *at,
                             remaining < 4 ? (int)remaining : 4, &end);
    if (c <= 0 || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        return QJS_INTL_DATA_ERROR;
    *at = (const char *)end - text.data;
    return QJS_INTL_OK;
}

typedef struct Render {
    const QJSIntlNameData *data;
    uint32_t locale;
    QJSIntlPairPattern outer, separator;
    QJSIntlBytes base, *labels;
    size_t count;
    QJSIntlFormatBuilder builder;
} Render;

static QJSIntlStatus literal(Render *render, QJSIntlBytes text)
{
    return qjs_intl_format_builder_text(&render->builder, text, NULL, NULL);
}

/* Native locale-name nesting applies to qualifier labels, not the base name.
   Replace once, using only scalar brackets also present in outer literals.
   This implements the source locale-pattern convention, not a general-purpose
   Unicode bracket-nesting algorithm. Pattern punctuation stays literal. */
static QJSIntlStatus label(Render *render, QJSIntlBytes text)
{
    size_t at = 0;
    if (!render->count)
        return literal(render, text);
    while (at < text.length) {
        size_t start = at;
        QJSIntlBytes scalar;
        QJSIntlNameLabel replacement;
        QJSIntlStatus status = scalar_next(text, &at);
        if (status)
            return status;
        scalar = (QJSIntlBytes){text.data + start, at - start};
        status = qjs_intl_name_select(render->data, render->locale, QJS_INTL_NAME_BRACKET,
                                      QJS_INTL_NAME_LONG, 0, scalar, &replacement);
        if (status)
            return status;
        if (replacement.found && (contains(render->outer.prefix, scalar) ||
                                  contains(render->outer.separator, scalar) ||
                                  contains(render->outer.suffix, scalar))) {
            size_t end = 0;
            scalar = replacement.entry.text;
            if (!scalar.length || scalar_next(scalar, &end) || end != scalar.length)
                return QJS_INTL_DATA_ERROR;
        }
        status = literal(render, scalar);
        if (status)
            return status;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus list(Render *render)
{
    const QJSIntlPairPattern *p = &render->separator;
    QJSIntlStatus status;
    size_t i;
    /* Flatten left-folded separator applications in their actual operand
       order. No recursive calls or repeatedly copied intermediate strings. */
    for (i = render->count - 1; i; i--) {
        if ((status = literal(render, p->prefix)))
            return status;
        if (p->first && ((status = label(render, render->labels[i])) ||
                         (status = literal(render, p->separator))))
            return status;
    }
    if ((status = label(render, render->labels[0])))
        return status;
    for (i = 1; i < render->count; i++) {
        if (!p->first && ((status = literal(render, p->separator)) ||
                          (status = label(render, render->labels[i]))))
            return status;
        if ((status = literal(render, p->suffix)))
            return status;
    }
    return QJS_INTL_OK;
}

static QJSIntlStatus render_name(Render *render)
{
    QJSIntlStatus status;
    if (!render->count)
        return literal(render, render->base);
    if ((status = literal(render, render->outer.prefix)))
        return status;
    status = render->outer.first ? list(render) : literal(render, render->base);
    if (status || (status = literal(render, render->outer.separator)))
        return status;
    status = render->outer.first ? literal(render, render->base) : list(render);
    if (status)
        return status;
    return literal(render, render->outer.suffix);
}

void qjs_intl_name_text_clear(const QJSIntlAllocator *allocator, QJSIntlNameText *out)
{
    if (!out)
        return;
    if (allocator && allocator->free && out->text)
        allocator->free(allocator->opaque, out->text);
    memset(out, 0, sizeof(*out));
}

QJSIntlStatus qjs_intl_language_name_format(const QJSIntlNameData *data, uint32_t locale,
                                          QJSIntlNameStyle style, int dialect,
                                          QJSIntlBytes input,
                                          const QJSIntlAllocator *allocator, size_t limit,
                                          QJSIntlNameText *out)
{
    QJSIntlLanguageNameMatch match;
    Render render = {0};
    QJSIntlStatus status;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || !allocator || !allocator->malloc || !allocator->free || !limit ||
        limit > SIZE_MAX / sizeof(uint16_t))
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_language_name_match(data, locale, style, dialect, input, &match);
    if (status || !match.label.found)
        return status;
    render.data = data;
    render.locale = locale;
    render.base = match.label.entry.text;
    render.builder.limit = limit;
    status = qualifiers(data, locale, style, input, match.code, NULL, &render.count);
    if (status)
        return status;
    if (render.count) {
        if (render.count > SIZE_MAX / sizeof(*render.labels))
            return QJS_INTL_OVERFLOW;
        render.labels = allocator->malloc(allocator->opaque, render.count * sizeof(*render.labels));
        if (!render.labels)
            return QJS_INTL_NO_MEMORY;
        status = qualifiers(data, locale, style, input, match.code, render.labels, &render.count);
        if (!status)
            status = pattern(data, locale, "localePattern", &render.outer);
        if (!status && render.count > 1)
            status = pattern(data, locale, "localeSeparator", &render.separator);
    }
    if (!status)
        status = render_name(&render);
    if (!status && render.builder.length) {
        render.builder.result.text = allocator->malloc(allocator->opaque,
                                                      render.builder.length * sizeof(uint16_t));
        if (!render.builder.result.text)
            status = QJS_INTL_NO_MEMORY;
        else {
            render.builder.length = 0;
            status = render_name(&render);
        }
    }
    if (render.labels)
        allocator->free(allocator->opaque, render.labels);
    if (status) {
        if (render.builder.result.text)
            allocator->free(allocator->opaque, render.builder.result.text);
        return status;
    }
    *out = (QJSIntlNameText){render.builder.result.text, render.builder.length, 1};
    return QJS_INTL_OK;
}

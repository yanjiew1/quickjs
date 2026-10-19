/* CLDR longest matching name, including partial variant compounds.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "language-match.h"
#include "../locale/identifier.h"
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
        QJSIntlBytes result = {cursor->text.data + start, cursor->at - start};
        if (cursor->at < cursor->text.length)
            cursor->at++;
        return result;
    }
}

static int equal_lower(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t i;
    if (a.length != b.length)
        return 0;
    for (i = 0; i < a.length; i++) {
        unsigned int left = (uint8_t)a.data[i], right = (uint8_t)b.data[i];
        if (left >= 'A' && left <= 'Z')
            left += 'a' - 'A';
        if (right >= 'A' && right <= 'Z')
            right += 'a' - 'A';
        if (left != right)
            return 0;
    }
    return 1;
}

static int compare_bytes(QJSIntlBytes a, QJSIntlBytes b)
{
    size_t length = a.length < b.length ? a.length : b.length;
    int order = length ? memcmp(a.data, b.data, length) : 0;
    return order ? order : (a.length > b.length) - (a.length < b.length);
}

static size_t included_subtags(QJSIntlBytes candidate, QJSIntlBytes input)
{
    Cursor source = {candidate, 0}, value = {input, 0};
    QJSIntlBytes wanted = next(&source), actual = next(&value);
    size_t count = 1;
    if (!equal_lower(wanted, actual))
        return 0;
    while ((wanted = next(&source)).data) {
        do {
            actual = next(&value);
        } while (actual.data && !equal_lower(wanted, actual));
        if (!actual.data)
            return 0;
        count++;
    }
    return count;
}

/* Both candidates matched the same number of input positions. The first input
   position included by only one candidate decides the earlier-position tie.
   This needs no input-sized bitset or per-candidate allocation. */
static int earlier_candidate(QJSIntlBytes a, QJSIntlBytes b, QJSIntlBytes input)
{
    Cursor left = {a, 0}, right = {b, 0}, value = {input, 0};
    QJSIntlBytes left_token = next(&left), right_token = next(&right), actual;
    while ((actual = next(&value)).data) {
        int left_match = left_token.data && equal_lower(left_token, actual);
        int right_match = right_token.data && equal_lower(right_token, actual);
        if (left_match != right_match)
            return left_match;
        if (left_match) {
            left_token = next(&left);
            right_token = next(&right);
        }
    }
    return compare_bytes(a, b) < 0;
}

static uint32_t lower_bound(const QJSIntlNameData *data, QJSIntlBytes language)
{
    uint32_t low = 0, high = data->key_count;
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        QJSIntlNameGroup group;
        QJSIntlBytes key;
        qjs_intl_name_key_at(data, mid, &group, &key);
        if (group == QJS_INTL_NAME_LANGUAGE && compare_bytes(key, language) < 0)
            low = mid + 1;
        else
            high = mid;
    }
    return low;
}

QJSIntlStatus qjs_intl_language_name_match(const QJSIntlNameData *data, uint32_t locale,
                                         QJSIntlNameStyle style, int dialect,
                                         QJSIntlBytes input, QJSIntlLanguageNameMatch *out)
{
    QJSIntlLanguageNameMatch best = {0};
    QJSIntlLocaleComponents components;
    QJSIntlStatus status;
    uint32_t i, head;
    if (out)
        memset(out, 0, sizeof(*out));
    if (!out || (unsigned int)style > QJS_INTL_NAME_NARROW || (dialect != 0 && dialect != 1) ||
        input.length > PTRDIFF_MAX)
        return QJS_INTL_INVALID_ARGUMENT;
    status = qjs_intl_name_head(data, locale, QJS_INTL_NAME_LANGUAGE, &head);
    if (status)
        return status;
    status = qjs_intl_locale_components(input, &components);
    if (status || components.base_name.length != input.length)
        return QJS_INTL_INVALID_ARGUMENT;
    if (!head)
        return QJS_INTL_OK;
    if (!dialect) {
        status = qjs_intl_name_select(data, locale, QJS_INTL_NAME_LANGUAGE, style, 0,
                                      components.language, &best.label);
        if (status)
            return status;
        if (best.label.found) {
            best.code = best.label.entry.normalized;
            best.subtags = 1;
        }
    } else {
        for (i = lower_bound(data, components.language); i < data->key_count; i++) {
            QJSIntlNameGroup group;
            QJSIntlBytes key;
            QJSIntlNameLabel label;
            size_t subtags;
            qjs_intl_name_key_at(data, i, &group, &key);
            if (group != QJS_INTL_NAME_LANGUAGE || key.length < components.language.length ||
                memcmp(key.data, components.language.data, components.language.length) ||
                (key.length > components.language.length && key.data[components.language.length] != '-'))
                break;
            subtags = included_subtags(key, input);
            if (!subtags || subtags < best.subtags)
                continue;
            status = qjs_intl_name_select(data, locale, QJS_INTL_NAME_LANGUAGE, style, 0, key, &label);
            if (status)
                return status;
            if (!label.found)
                continue;
            if (subtags > best.subtags || earlier_candidate(key, best.code, input)) {
                best.label = label;
                best.code = key;
                best.subtags = subtags;
            }
        }
    }
    *out = best;
    return QJS_INTL_OK;
}

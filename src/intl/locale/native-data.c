/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#include "native-data.h"
#include "../native/wire.h"
#include "../common/locale-grammar.h"
#include <stdlib.h>
#include <string.h>

typedef struct Validation {
    const QJSIntlAllocator *allocator;
    QJSIntlStatus error;
} Validation;

static uint32_t field(const QJSIntlSection *table, unsigned int width,
                      uint32_t row, unsigned int column)
{
    return qjs_intl_read_u32(table->bytes + ((size_t)row * width + column) * 4);
}

static int extent(const QJSIntlSection *table, unsigned int width)
{
    return table->length % (width * 4) == 0 && table->length / (width * 4) == table->count;
}

static int text_at(const QJSIntlLocaleMetadata *data, uint32_t index, QJSIntlBytes *out)
{
    return qjs_intl_data_string(&data->data, index, out) == QJS_INTL_OK;
}

static int equal(QJSIntlBytes text, const char *literal)
{
    return text.length == strlen(literal) && !memcmp(text.data, literal, text.length);
}

static int lower(int c)
{
    return c >= 'a' && c <= 'z';
}

static int upper(int c)
{
    return c >= 'A' && c <= 'Z';
}

static int digit(int c)
{
    return c >= '0' && c <= '9';
}

static int token_order(const void *left, const void *right)
{
    const QJSIntlBytes *a = left, *b = right;
    size_t size = a->length < b->length ? a->length : b->length;
    int order = memcmp(a->data, b->data, size);
    return order ? order : (a->length > b->length) - (a->length < b->length);
}

/* Sorting borrowed token slices avoids quadratic duplicate detection for
   long metadata fields. Only fields with multiple tokens allocate scratch. */
static int unique_tokens(Validation *validation, QJSIntlBytes text, int separator)
{
    const QJSIntlAllocator *allocator = validation->allocator;
    QJSIntlBytes *tokens;
    size_t i, count = 1, start = 0, index = 0;
    int valid = 1;

    for (i = 0; i < text.length; i++)
        count += text.data[i] == separator;
    if (count == 1)
        return 1;
    if (count > SIZE_MAX / sizeof(*tokens)) {
        validation->error = QJS_INTL_OVERFLOW;
        return 0;
    }
    tokens = allocator->malloc(allocator->opaque, count * sizeof(*tokens));
    if (!tokens) {
        validation->error = QJS_INTL_NO_MEMORY;
        return 0;
    }
    for (i = 0; i <= text.length; i++) {
        if (i == text.length || text.data[i] == separator) {
            tokens[index].data = text.data + start;
            tokens[index++].length = i - start;
            start = i + 1;
        }
    }
    qsort(tokens, count, sizeof(*tokens), token_order);
    for (i = 1; i < count; i++) {
        if (!token_order(&tokens[i - 1], &tokens[i])) {
            valid = 0;
            break;
        }
    }
    allocator->free(allocator->opaque, tokens);
    return valid;
}

static int next_subtag(QJSIntlBytes text, size_t *at, QJSIntlBytes *token)
{
    size_t start = *at, end = start;
    if (start == text.length)
        return 0;
    while (end < text.length && text.data[end] != '-')
        end++;
    if (end == start || end - start > 8 || (end + 1 == text.length))
        return -1;
    token->data = text.data + start;
    token->length = end - start;
    *at = end < text.length ? end + 1 : end;
    return 1;
}

static int source_tag(Validation *validation, QJSIntlBytes text)
{
    QJSIntlBytes token, variants;
    size_t at = 0, i;
    int more;

    if (equal(text, "root"))
        return 1;
    if (next_subtag(text, &at, &token) != 1 ||
        !(token.length == 2 || token.length == 3 || token.length >= 5))
        return 0;
    for (i = 0; i < token.length; i++) {
        if (!lower((unsigned char)token.data[i]))
            return 0;
    }
    more = next_subtag(text, &at, &token);
    if (more <= 0)
        return more == 0;
    if (token.length == 4 && upper((unsigned char)token.data[0]) &&
        lower((unsigned char)token.data[1]) && lower((unsigned char)token.data[2]) &&
        lower((unsigned char)token.data[3])) {
        more = next_subtag(text, &at, &token);
        if (more <= 0)
            return more == 0;
    }
    if ((token.length == 2 && upper((unsigned char)token.data[0]) && upper((unsigned char)token.data[1])) ||
        (token.length == 3 && digit((unsigned char)token.data[0]) &&
         digit((unsigned char)token.data[1]) && digit((unsigned char)token.data[2]))) {
        more = next_subtag(text, &at, &token);
        if (more <= 0)
            return more == 0;
    }
    variants.data = token.data;
    variants.length = text.length - (size_t)(token.data - text.data);
    do {
        if (!(token.length >= 5 || (token.length == 4 && digit((unsigned char)token.data[0]))))
            return 0;
        for (i = 0; i < token.length; i++) {
            if (!lower((unsigned char)token.data[i]) && !digit((unsigned char)token.data[i]))
                return 0;
        }
        more = next_subtag(text, &at, &token);
    } while (more == 1);
    return more == 0 && unique_tokens(validation, variants, '-');
}

/* Locale alias components use lowercase alphanumeric words and hyphens.
   Replacement lists separate complete identifiers with single ASCII spaces. */
static int alias_words(QJSIntlBytes text, int list, uint32_t *count)
{
    size_t i;
    int separator = 1;
    uint32_t words = 1;

    if (!text.length)
        return 0;
    for (i = 0; i < text.length; i++) {
        int c = (unsigned char)text.data[i];
        if (lower(c) || digit(c)) {
            separator = 0;
        } else if ((c == '-' || (list && c == ' ')) && !separator) {
            separator = 1;
            words += c == ' ';
        } else {
            return 0;
        }
    }
    if (separator)
        return 0;
    if (count)
        *count = words;
    return 1;
}

static int legacy_tokens(QJSIntlBytes text)
{
    size_t i;
    int separator = 1;

    if (!text.length)
        return 1;
    for (i = 0; i < text.length; i++) {
        unsigned int c = (unsigned char)text.data[i];
        if (c >= 0x21 && c <= 0x7e)
            separator = 0;
        else if (c == ' ' && !separator)
            separator = 1;
        else
            return 0;
    }
    return !separator;
}

static int target_tag(QJSIntlBytes text)
{
    size_t i = 0, length;
    while (i < text.length && lower((unsigned char)text.data[i]))
        i++;
    if (!(i == 2 || i == 3 || (i >= 5 && i <= 8)) ||
        i >= text.length || text.data[i++] != '-')
        return 0;
    if (text.length - i < 6 || !upper((unsigned char)text.data[i++]))
        return 0;
    for (length = 0; length < 3; length++) {
        if (!lower((unsigned char)text.data[i++]))
            return 0;
    }
    if (text.data[i++] != '-')
        return 0;
    length = text.length - i;
    if (length == 2)
        return upper((unsigned char)text.data[i]) && upper((unsigned char)text.data[i + 1]);
    return length == 3 && digit((unsigned char)text.data[i]) &&
           digit((unsigned char)text.data[i + 1]) && digit((unsigned char)text.data[i + 2]);
}

static int valid_aliases(Validation *validation, const QJSIntlLocaleMetadata *data)
{
    uint32_t i, previous_kind = 0, previous_source = 0;
    for (i = 0; i < data->aliases.count; i++) {
        uint32_t kind = field(&data->aliases, 4, i, 0), source = field(&data->aliases, 4, i, 1);
        uint32_t count = field(&data->aliases, 4, i, 3), actual;
        QJSIntlBytes from, targets;
        if (kind > 4 || !text_at(data, source, &from) ||
            !text_at(data, field(&data->aliases, 4, i, 2), &targets) ||
            !alias_words(from, 0, NULL) || !alias_words(targets, 1, &actual) ||
            actual != count || !unique_tokens(validation, targets, ' ') ||
            ((kind != 2 && kind != 4) && count != 1) ||
            (i && (kind < previous_kind || (kind == previous_kind && source <= previous_source))))
            return 0;
        previous_kind = kind;
        previous_source = source;
    }
    return 1;
}

static int valid_likely(Validation *validation, const QJSIntlLocaleMetadata *data)
{
    uint32_t i, previous = 0;
    int und = 0;
    for (i = 0; i < data->likely.count; i++) {
        uint32_t name = field(&data->likely, 2, i, 0);
        QJSIntlBytes source, target;
        if (!text_at(data, name, &source) || !text_at(data, field(&data->likely, 2, i, 1), &target) ||
            !source_tag(validation, source) ||
            !target_tag(target) || (i && name <= previous))
            return 0;
        und |= equal(source, "und");
        previous = name;
    }
    return und;
}

static int row_order(const QJSIntlSection *table, unsigned int width,
                     uint32_t a, uint32_t b, unsigned int fields)
{
    unsigned int i;
    for (i = 0; i < fields; i++) {
        uint32_t left = field(table, width, a, i), right = field(table, width, b, i);
        if (left != right)
            return left < right ? -1 : 1;
    }
    return 0;
}

static int variable_name(QJSIntlBytes text)
{
    static const char *const names[] = {
        "CODEPOINTS", "REORDER_CODE", "RG_KEY_VALUE", "SCRIPT_CODE", "SUBDIVISION_CODE", "PRIVATE_USE",
    };
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(*names); i++) {
        if (equal(text, names[i]))
            return 1;
    }
    return 0;
}

static int timezone_labels(QJSIntlBytes iana, QJSIntlBytes region)
{
    size_t i;
    int slash = 0, segment = 0;
    if (region.length && (region.length != 2 || !upper((unsigned char)region.data[0]) ||
        !upper((unsigned char)region.data[1])))
        return 0;
    for (i = 0; i < iana.length; i++) {
        int c = (unsigned char)iana.data[i];
        if (lower(c) || upper(c) || (slash && (c == '_' || c == '-')))
            segment = 1;
        else if (c == '/' && segment) {
            segment = 0;
            slash = 1;
        } else
            return 0;
    }
    return !iana.length || (slash && segment);
}

/* Locate a row by its leading string-ID tuple. */
static uint32_t find_row(const QJSIntlSection *table, unsigned int width,
                         const uint32_t *wanted, unsigned int identities)
{
    uint32_t low = 0, high = table->count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2;
        unsigned int i;
        int order = 0;
        for (i = 0; i < identities; i++) {
            uint32_t found = field(table, width, middle, i);
            if (wanted[i] != found) {
                order = wanted[i] < found ? -1 : 1;
                break;
            }
        }
        if (!order)
            return middle;
        if (order < 0)
            high = middle;
        else
            low = middle + 1;
    }
    return table->count;
}

static uint32_t preferred_target(const QJSIntlSection *table, unsigned int width,
                                 uint32_t row, unsigned int identities)
{
    uint32_t wanted[3], preferred = field(table, width, row, 4);
    unsigned int i;
    if (!preferred)
        return UINT32_MAX; /* Empty preferred name: a terminal identity. */
    for (i = 0; i < identities; i++)
        wanted[i] = i + 1 == identities ? preferred : field(table, width, row, i);
    return find_row(table, width, wanted, identities);
}

static int preferred_graph(const QJSIntlSection *table, unsigned int width,
                           unsigned int identities, uint8_t *states)
{
    uint32_t i;
    memset(states, 0, table->count);
    for (i = 0; i < table->count; i++) {
        uint32_t next = i;
        while (next != UINT32_MAX && !states[next]) {
            states[next] = 1;
            next = preferred_target(table, width, next, identities);
            if (next == table->count)
                return 0;
        }
        if (next != UINT32_MAX && states[next] == 1)
            return 0;
        next = i;
        while (next != UINT32_MAX && states[next] == 1) {
            states[next] = 2;
            next = preferred_target(table, width, next, identities);
        }
    }
    return 1;
}

static int valid_keywords(Validation *validation, const QJSIntlLocaleMetadata *data)
{
    uint32_t i;
    for (i = 0; i < data->keys.count; i++) {
        QJSIntlBytes ext, name, aliases, preferred;
        if (!text_at(data, field(&data->keys, 6, i, 0), &ext) ||
            !text_at(data, field(&data->keys, 6, i, 1), &name) ||
            !text_at(data, field(&data->keys, 6, i, 3), &aliases) ||
            !text_at(data, field(&data->keys, 6, i, 4), &preferred) ||
            name.length != 2 || !legacy_tokens(aliases) || !unique_tokens(validation, aliases, ' ') ||
            field(&data->keys, 6, i, 2) > 3 ||
            field(&data->keys, 6, i, 5) > 1 ||
            (i && row_order(&data->keys, 6, i - 1, i, 2) >= 0))
            return 0;
        if (equal(ext, "u")) {
            if (!(lower(name.data[0]) || digit(name.data[0])) || !lower(name.data[1]))
                return 0;
        } else if (equal(ext, "t")) {
            if (!lower(name.data[0]) || !digit(name.data[1]))
                return 0;
        } else
            return 0;
    }
    for (i = 0; i < data->types.count; i++) {
        QJSIntlBytes values[8];
        uint32_t flags = field(&data->types, 8, i, 5), wanted[2];
        unsigned int j;
        for (j = 0; j < 8; j++) {
            if (j != 5 && !text_at(data, field(&data->types, 8, i, j), &values[j]))
                return 0;
        }
        if (flags > 3 || !!(flags & 2) != variable_name(values[2]) ||
            (!(flags & 2) && (!alias_words(values[2], 0, NULL) ||
              !qjs_intl_unicode_type_well_formed(values[2].data, values[2].length))) ||
            !legacy_tokens(values[3]) || !unique_tokens(validation, values[3], ' ') ||
            !timezone_labels(values[6], values[7]) ||
            (i && row_order(&data->types, 8, i - 1, i, 3) >= 0))
            return 0;
        wanted[0] = field(&data->types, 8, i, 0);
        wanted[1] = field(&data->types, 8, i, 1);
        if (find_row(&data->keys, 6, wanted, 2) == data->keys.count)
            return 0;
    }
    return 1;
}

QJSIntlStatus qjs_intl_locale_metadata_open(const QJSIntlData *data,
                                          const QJSIntlAllocator *allocator,
                                          QJSIntlLocaleMetadata *out)
{
    QJSIntlLocaleMetadata view;
    uint8_t *states;
    size_t scratch;
    int valid;
    Validation validation = {allocator, QJS_INTL_DATA_ERROR};

    if (!out)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data || !data->pack.bytes || !allocator || !allocator->malloc || !allocator->free)
        return QJS_INTL_INVALID_ARGUMENT;
    memset(&view, 0, sizeof(view));
    view.data = *data;
    if (qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_ALIASES, &view.aliases) ||
        qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_LIKELY, &view.likely) ||
        qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_KEYS, &view.keys) ||
        qjs_intl_pack_section(&data->pack, QJS_INTL_SECTION_TYPES, &view.types) ||
        !extent(&view.aliases, 4) || !extent(&view.likely, 2) ||
        !extent(&view.keys, 6) || !extent(&view.types, 8) ||
        !valid_aliases(&validation, &view) || !valid_likely(&validation, &view) ||
        !valid_keywords(&validation, &view))
        return validation.error;
    scratch = view.keys.count > view.types.count ? view.keys.count : view.types.count;
    if (scratch) {
        states = allocator->malloc(allocator->opaque, scratch);
        if (!states)
            return QJS_INTL_NO_MEMORY;
        valid = preferred_graph(&view.keys, 6, 2, states) && preferred_graph(&view.types, 8, 3, states);
        allocator->free(allocator->opaque, states);
        if (!valid)
            return QJS_INTL_DATA_ERROR;
    }
    *out = view;
    return QJS_INTL_OK;
}

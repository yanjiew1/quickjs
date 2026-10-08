/* Engine-independent locale grammar and ICU-backed immutable data.
 * ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd, 2026-10-08.
 * Anchors: sec-iswellformedlanguagetag, sec-availablecanonicalunits,
 * sup-availablenamedtimezoneidentifiers. */
#include "locale-data.h"
#include <string.h>
#include <unicode/ucal.h>
#include <unicode/ures.h>
#include "intl-time-zone-names.h"
#include "intl-unit-names.h"
#include "intl-calendar-types.h"

typedef struct IntlSlice { const char *text; size_t length; } IntlSlice;
typedef struct IntlCursor { const char *text; size_t length, offset; } IntlCursor;
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int alpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int digit(int c) { return c >= '0' && c <= '9'; }
static int alnum(int c) { return alpha(c) || digit(c); }
static int slice_all(IntlSlice s, int (*test)(int))
{
    size_t i;
    for (i = 0; i < s.length; i++) {
        if (!test((unsigned char)s.text[i]))
            return 0;
    }
    return 1;
}
static int slice_equal(IntlSlice a, IntlSlice b)
{
    size_t i;
    if (a.length != b.length)
        return 0;
    for (i = 0; i < a.length; i++) {
        if (lower((unsigned char)a.text[i]) != lower((unsigned char)b.text[i]))
            return 0;
    }
    return 1;
}
static int cursor_next(IntlCursor *cursor, IntlSlice *token)
{
    size_t start = cursor->offset, end = start;
    if (start == cursor->length) return 0;
    while (end < cursor->length && cursor->text[end] != '-') end++;
    token->text = cursor->text + start; token->length = end - start;
    if (!token->length || token->length > 8) return -1;
    cursor->offset = end < cursor->length ? end + 1 : end;
    /* A final separator represents an empty final subtag. */
    if (end < cursor->length && cursor->offset == cursor->length) return -1;
    return 1;
}
static int cursor_peek(IntlCursor cursor, IntlSlice *token) { return cursor_next(&cursor, token); }
static int is_language(IntlSlice s)
{
    return (s.length == 2 || s.length == 3 || (s.length >= 5 && s.length <= 8)) && slice_all(s, alpha);
}
static int is_script(IntlSlice s) { return s.length == 4 && slice_all(s, alpha); }
static int is_region(IntlSlice s)
{
    return (s.length == 2 && slice_all(s, alpha)) || (s.length == 3 && slice_all(s, digit));
}
static int is_variant(IntlSlice s)
{
    return ((s.length >= 5 && s.length <= 8) || (s.length == 4 && digit(s.text[0]))) && slice_all(s, alnum);
}
static int duplicate_variant(const char *start, const char *end, IntlSlice candidate)
{
    while (start < end) {
        const char *p = start;
        while (p < end && *p != '-') p++;
        if (slice_equal((IntlSlice){ start, p - start }, candidate)) return 1;
        start = p + 1;
    }
    return 0;
}
static int parse_language(IntlCursor *cursor)
{
    IntlSlice token; const char *variants = NULL; int r;
    r = cursor_next(cursor, &token); if (r != 1 || !is_language(token)) return 0;
    r = cursor_peek(*cursor, &token);
    if (r < 0) return 0;
    if (r && is_script(token)) { cursor_next(cursor, &token); r = cursor_peek(*cursor, &token); }
    if (r < 0) return 0;
    if (r && is_region(token)) { cursor_next(cursor, &token); r = cursor_peek(*cursor, &token); }
    if (r < 0) return 0;
    while (r && is_variant(token)) {
        if (!variants) variants = token.text;
        else if (duplicate_variant(variants, token.text, token)) return 0;
        cursor_next(cursor, &token); r = cursor_peek(*cursor, &token);
        if (r < 0) return 0;
    }
    return 1;
}
int intl_unicode_type_well_formed(const char *type, size_t length)
{
    IntlCursor cursor = { type, length, 0 }; IntlSlice token; size_t i; int r;
    if (!length) return 0;
    for (i = 0; i < length; i++) if (type[i] != '-' && !alnum((unsigned char)type[i])) return 0;
    while ((r = cursor_next(&cursor, &token)) == 1) if (token.length < 3) return 0;
    return r == 0;
}
int intl_unicode_locale_well_formed(const char *tag, size_t length)
{
    IntlCursor cursor = { tag, length, 0 }; IntlSlice token; unsigned char seen[128] = { 0 }; size_t i; int r;
    if (!length) return 0;
    for (i = 0; i < length; i++) if (tag[i] != '-' && !alnum((unsigned char)tag[i])) return 0;
    if (!parse_language(&cursor)) return 0;
    while ((r = cursor_next(&cursor, &token)) == 1) {
        int singleton, payload = 0;
        if (token.length != 1) return 0;
        singleton = lower((unsigned char)token.text[0]);
        if (singleton == 'x') {
            while ((r = cursor_next(&cursor, &token)) == 1) payload = 1;
            return r == 0 && payload != 0;
        }
        if (seen[singleton]) return 0;
        seen[singleton] = 1;
        if (singleton == 't') {
            r = cursor_peek(cursor, &token); if (r != 1) return 0;
            if (is_language(token)) { if (!parse_language(&cursor)) return 0; payload = 1; }
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length != 1) {
                if (token.length != 2 || !alpha(token.text[0]) || !digit(token.text[1])) return 0;
                cursor_next(&cursor, &token);
                r = cursor_peek(cursor, &token); if (r != 1 || token.length < 3) return 0;
                do { cursor_next(&cursor, &token); r = cursor_peek(cursor, &token); } while (r == 1 && token.length >= 3);
                if (r < 0) return 0;
                payload = 1;
            }
        } else if (singleton == 'u') {
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length >= 3) { cursor_next(&cursor, &token); payload = 1; }
            if (r < 0) return 0;
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length != 1) {
                if (token.length != 2 || !alpha(token.text[1])) return 0;
                cursor_next(&cursor, &token); payload = 1;
                while ((r = cursor_peek(cursor, &token)) == 1 && token.length >= 3) cursor_next(&cursor, &token);
                if (r < 0) return 0;
            }
        } else {
            while ((r = cursor_peek(cursor, &token)) == 1 && token.length != 1) { if (token.length < 2) return 0; cursor_next(&cursor, &token); payload = 1; }
        }
        if (r < 0 || !payload) return 0;
    }
    return r == 0;
}
static const size_t iana_count = sizeof(intl_iana_zone_names) / sizeof(*intl_iana_zone_names);
static const size_t unit_count = sizeof(intl_unit_names) / sizeof(*intl_unit_names);
const char *intl_iana_zone_name(const char *identifier, size_t length)
{
    size_t i;
    for (i = 0; i < iana_count; i++) if (slice_equal((IntlSlice){ identifier, length },
        (IntlSlice){ intl_iana_zone_names[i], strlen(intl_iana_zone_names[i]) })) return intl_iana_zone_names[i];
    return NULL;
}
size_t intl_iana_zone_count(void) { return iana_count; }
const char *intl_iana_zone_at(size_t index) { return index < iana_count ? intl_iana_zone_names[index] : NULL; }
size_t intl_sanctioned_unit_count(void) { return unit_count; }
const char *intl_sanctioned_unit_at(size_t index) { return index < unit_count ? intl_unit_names[index] : NULL; }
size_t intl_calendar_type_count(void)
{
    return sizeof(intl_calendar_types) / sizeof(*intl_calendar_types);
}
const char *intl_calendar_type_at(size_t index)
{
    return index < intl_calendar_type_count() ? intl_calendar_types[index] : NULL;
}
const char *intl_calendar_type_name(const char *type, size_t length)
{
    size_t i;
    IntlSlice input = { type, length };
    if (!type)
        return NULL;
    for (i = 0; i < intl_calendar_type_count(); i++) {
        const char *name = intl_calendar_types[i];
        if (slice_equal(input, (IntlSlice){ name, strlen(name) }))
            return name;
    }
    for (i = 0; i < sizeof(intl_calendar_aliases) / sizeof(*intl_calendar_aliases); i++) {
        const char *alias = intl_calendar_aliases[i].alias;
        if (slice_equal(input, (IntlSlice){ alias, strlen(alias) }))
            return intl_calendar_aliases[i].canonical;
    }
    return NULL;
}
int32_t intl_iana_zone_primary(const char *identifier, UChar *output, int32_t capacity, UErrorCode *status)
{
    UChar input[128], primary[128]; size_t i; int32_t required, length;
    if (U_FAILURE(*status)) return 0;
    if (!identifier || strlen(identifier) >= sizeof(input) / sizeof(*input)) { *status = U_ILLEGAL_ARGUMENT_ERROR; return 0; }
    length = strlen(identifier); for (i = 0; i < (size_t)length; i++) input[i] = (unsigned char)identifier[i];
    required = ucal_getIanaTimeZoneID(input, length, primary, sizeof(primary) / sizeof(*primary), status);
    if (U_FAILURE(*status)) return required;
    if ((required == 7 && !memcmp(primary, (UChar[]){'E','t','c','/','U','T','C'}, 7 * sizeof(UChar))) ||
        (required == 7 && !memcmp(primary, (UChar[]){'E','t','c','/','G','M','T'}, 7 * sizeof(UChar))) ||
        (required == 3 && !memcmp(primary, (UChar[]){'G','M','T'}, 3 * sizeof(UChar)))) {
        primary[0] = 'U'; primary[1] = 'T'; primary[2] = 'C'; required = 3;
    }
    if (capacity < 0 || (!output && capacity)) { *status = U_ILLEGAL_ARGUMENT_ERROR; return 0; }
    if (capacity) { int32_t copy = required < capacity ? required : capacity; memcpy(output, primary, (size_t)copy * sizeof(*output)); if (required < capacity) output[required] = 0; }
    if (required > capacity) *status = U_BUFFER_OVERFLOW_ERROR;
    else if (required == capacity) *status = U_STRING_NOT_TERMINATED_WARNING;
    return required;
}

int intl_region_has_week_data(const char *region, UErrorCode *status)
{
    UResourceBundle *bundle = NULL, *table = NULL, *entry = NULL;
    UErrorCode lookup_status = U_ZERO_ERROR;
    char country[4];
    size_t length, i;
    int result = 0;

    if (U_FAILURE(*status))
        return 0;
    if (!region)
        return 0;
    length = strlen(region);
    if (length != 2 && length != 3) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    for (i = 0; i < length; i++) {
        int c = (unsigned char)region[i];
        country[i] = c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
    }
    country[length] = 0;
    bundle = ures_openDirect(NULL, "supplementalData", status);
    if (U_FAILURE(*status))
        goto done;
    table = ures_getByKey(bundle, "weekData", NULL, status);
    if (U_FAILURE(*status))
        goto done;
    entry = ures_getByKey(table, country, NULL, &lookup_status);
    if (lookup_status == U_MISSING_RESOURCE_ERROR)
        goto done;
    if (U_FAILURE(lookup_status)) {
        *status = lookup_status;
        goto done;
    }
    result = entry != NULL;
done:
    ures_close(entry);
    ures_close(table);
    ures_close(bundle);
    return result;
}

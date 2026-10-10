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
static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
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

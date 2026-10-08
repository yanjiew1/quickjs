/* Locale syntax and data backend. No JavaScript values or allocator types. */
#ifndef QUICKJS_INTL_LOCALE_DATA_H
#define QUICKJS_INTL_LOCALE_DATA_H
#include <stddef.h>
#include "icu-config.h"

/* ECMA402 Unicode BCP47 grammar, including duplicate variants/singletons.
 * These validators allocate no memory and accept explicit byte lengths. */
int intl_unicode_locale_well_formed(const char *tag, size_t length);
int intl_unicode_type_well_formed(const char *type, size_t length);

/* Read-only generated IANA2026a and pinned ECMA402 Table 2 data. */
const char *intl_iana_zone_name(const char *identifier, size_t length);
size_t intl_iana_zone_count(void);
const char *intl_iana_zone_at(size_t index);
size_t intl_sanctioned_unit_count(void);
const char *intl_sanctioned_unit_at(size_t index);

/* Intl Era/MonthCode's exact calendar table. Enumeration exposes the
 * sixteen canonical names; name lookup additionally accepts its two aliases.
 * Names are borrowed immutable data. Lookup is ASCII case-insensitive,
 * accepts an explicit byte length, and allocates no memory. Unknown types
 * return NULL; this is a support query, not a Unicode-type syntax validator. */
size_t intl_calendar_type_count(void);
const char *intl_calendar_type_at(size_t index);
const char *intl_calendar_type_name(const char *type, size_t length);

/* Test exact supplemental week-data availability, without world fallback.
 * Missing region data returns zero; backend failures propagate through status.
 * The region is a canonical two-letter or three-digit identifier. */
int intl_region_has_week_data(const char *region, UErrorCode *status);

/* ICU C IANA mapping preserves country-primary Links. The caller supplies
 * a known whitelist name and owns its output buffer. Required length excludes
 * the terminator; overflow follows ordinary ICU buffer conventions. */
int32_t intl_iana_zone_primary(const char *known_identifier, UChar *output,
                             int32_t capacity, UErrorCode *status);
#endif

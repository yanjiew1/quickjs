/* Pure C Unicode locale grammar; no ICU or engine dependency. */
#ifndef QUICKJS_INTL_LOCALE_GRAMMAR_H
#define QUICKJS_INTL_LOCALE_GRAMMAR_H
#include <stddef.h>

/* ECMA402 Unicode BCP47 grammar, including duplicate variants/singletons.
 * These validators allocate no memory and accept explicit byte lengths. */
int intl_unicode_locale_well_formed(const char *tag, size_t length);
int intl_unicode_type_well_formed(const char *type, size_t length);

#endif

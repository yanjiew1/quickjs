/* Pure C Unicode locale grammar; no ICU or engine dependency. */
#ifndef QUICKJS_INTL_LOCALE_GRAMMAR_H
#define QUICKJS_INTL_LOCALE_GRAMMAR_H
#include <stddef.h>

/* ECMA402 Unicode BCP47 grammar, rejecting duplicate variants/singletons.
 * Unlike the UTS35 validity rules, ECMA402 permits duplicate ukey/tkey
 * fields; canonicalization later keeps the first. This validates syntax,
 * not aliases or installed locale data. The validators allocate no memory
 * and accept explicit byte lengths; NULL is invalid even for an empty slice.
 */
int qjs_intl_unicode_locale_well_formed(const char *tag, size_t length);
int qjs_intl_unicode_type_well_formed(const char *type, size_t length);

typedef enum QJSIntlLocaleSubtagKind {
    QJS_INTL_LANGUAGE_SUBTAG,
    QJS_INTL_SCRIPT_SUBTAG,
    QJS_INTL_REGION_SUBTAG,
    QJS_INTL_VARIANT_SUBTAG
} QJSIntlLocaleSubtagKind;

/* One subtag, not a hyphenated sequence. No alias or casing requirement. */
int qjs_intl_unicode_subtag_well_formed(QJSIntlLocaleSubtagKind kind,
                                      const char *text, size_t length);

#endif

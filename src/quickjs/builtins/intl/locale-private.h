/* Native locale syntax and canonicalization helpers, private to Intl. */
#ifndef QUICKJS_INTL_LOCALE_PRIVATE_H
#define QUICKJS_INTL_LOCALE_PRIVATE_H
#include "intl-internal.h"
#ifdef CONFIG_INTL
#include "../../../intl/locale-grammar.h"
#ifndef CONFIG_INTL_NATIVE
#include "../../../intl/locale-data.h"
#include <unicode/uloc.h>
#include <unicode/ures.h>
#endif
typedef struct IntlLanguageId {
    char *language, *script, *region;
    JSIntlLocaleList variants;
} IntlLanguageId;
typedef struct IntlKeyword {
    char *key, *value;
} IntlKeyword;
typedef struct IntlExtension {
    char singleton;
    IntlLanguageId tlang;
    JSIntlLocaleList attributes;
    IntlKeyword *keywords;
    size_t count;
    char *other;
} IntlExtension;
typedef struct IntlTag {
    IntlLanguageId base;
    IntlExtension *extensions;
    size_t count;
    char *private_use;
} IntlTag;
int intl_parse_tag(JSContext *ctx, const char *tag, size_t length,
                   IntlTag *result);
void intl_tag_free(JSContext *ctx, IntlTag *tag);
char *intl_tag_string(JSContext *ctx, const IntlTag *tag);
char *intl_language_string(JSContext *ctx, const IntlLanguageId *id);
char *intl_canonicalize_uvalue(JSContext *ctx, const char *key,
                              const char *value);
const char *intl_tag_keyword(const IntlTag *tag, const char *key);
int intl_tag_set_keyword(JSContext *ctx, IntlTag *tag, const char *key,
                         const char *value);
JSValue intl_array_from_list(JSContext *ctx, const JSIntlLocaleList *list);
int intl_list_sort_unique(JSContext *ctx, JSIntlLocaleList *list);
int intl_values_list(JSContext *ctx, const char *key, JSIntlLocaleList *list);
char *intl_lookup_locale(JSContext *ctx, JSIntlService service,
                         const char *requested);
#endif
#endif

/* QuickJS Intl binding ownership.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QUICKJS_BUILTINS_INTL_H
#define QUICKJS_BUILTINS_INTL_H
#include "../internal/base.h"
#ifdef CONFIG_INTL
#include "../../intl/locale/locale.h"
#include "../../intl/locale/matching.h"
#include "../../intl/locale/resolve.h"
#include "../../intl/collation/collation.h"
#include "../../intl/list/list.h"
#include "../../intl/plural/plural.h"
#include "../../intl/number/number.h"
#include "../../intl/relative/relative.h"
#include "../../intl/duration/duration.h"
#include "../../intl/display/display.h"
#include "../../intl/datetime/datetime.h"
#ifdef CONFIG_INTL_SEGMENTER
#include "../../intl/segment/segment.h"
int js_intl_init_segmenter(JSContext *ctx, JSValueConst namespace_object);
#endif

typedef struct JSIntlLocaleList {
    char **items;
    size_t count, capacity;
} JSIntlLocaleList;

QJSIntlLocaleService *js_intl_locale_service(JSContext *ctx);
/* Borrow runtime-owned services for binding-level capability queries. */
QJSIntlNumberService *js_intl_number_service(JSContext *ctx);
QJSIntlCollationService *js_intl_collation_service(JSContext *ctx);
QJSIntlDateTimeService *js_intl_datetime_service(JSContext *ctx);
QJSIntlDisplayService *js_intl_display_service(JSContext *ctx);
QJSIntlRelativeService *js_intl_relative_service(JSContext *ctx);
JSValue js_intl_supported_values(JSContext *ctx, JSValueConst this_val,
                                 int argc, JSValueConst *argv);
const char *js_intl_default_locale(JSContext *ctx);
QJSIntlAllocator js_intl_allocator(JSRuntime *rt);
/* Lazily admit compiled shared tables; failure leaves the view retryable. */
const struct QJSIntlData *js_intl_data(JSContext *ctx);
int js_intl_ensure_intrinsics(JSContext *ctx);
int js_intl_init_locale(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_list_format(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_plural_rules(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_number_format(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_collator(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_relative_time_format(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_duration_format(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_display_names(JSContext *ctx, JSValueConst namespace_object);
int js_intl_init_date_time_format(JSContext *ctx, JSValueConst namespace_object);
/* Locale-method callers first brand-check their receiver (and handle Date
   NaN). Supply that Temporal receiver or the stored primitive Date value. */
JSValue js_intl_format_date_time(JSContext *ctx, JSValueConst receiver, int argc, JSValueConst *argv,
                                 QJSIntlDateRequired required, QJSIntlDateDefaults defaults);
/* Convert one native partition without coercion. Exactly one result is present;
   names is the service's part-type whitelist, with NULL for disallowed types.
   Native results remain owned by the caller on success and failure. */
JSValue js_intl_format_parts(JSContext *ctx, const QJSIntlFormatted *ordinary,
                             const QJSIntlFormattedRange *range,
                             const char *const *names, size_t name_count);
/* value is a validated Temporal.Duration receiver; use the intrinsic ctor. */
JSValue js_intl_format_duration(JSContext *ctx, JSValueConst value, int argc, JSValueConst *argv);
/* Intrinsic Number/BigInt locale formatting. value is a validated primitive;
   argc/argv supply locales/options. Never consult mutable global properties. */
JSValue js_intl_format_number(JSContext *ctx, JSValueConst value, int argc, JSValueConst *argv);
/* Already-coerced string inputs; argc/argv contain locales/options. Use the
   intrinsic Collator, never mutable global properties or its compare getter. */
JSValue js_intl_compare_strings(JSContext *ctx, JSValueConst a, JSValueConst b,
                                int argc, JSValueConst *argv);
JSValue js_intl_create_from_constructor(JSContext *ctx, JSValueConst new_target,
                                        JSClassID class_id);
JSValue js_intl_options_object(JSContext *ctx, JSValueConst value, BOOL coerce);
int js_intl_string_option(JSContext *ctx, JSValueConst object, const char *name,
                          const char *const *choices, size_t count, int fallback);
int js_intl_default_number_option(JSContext *ctx, JSValueConst value, int minimum,
                                  int maximum, int fallback, int *out);
int js_intl_get_number_option(JSContext *ctx, JSValueConst options, const char *name,
                              int minimum, int maximum, int fallback, int *out);
int js_intl_locale_matcher(JSContext *ctx, JSValueConst options);
/* Preserve the matched request's borrowed Unicode extension. The requested
   list must outlive match use. Default fallback has no extension. */
int js_intl_resolve_locale_match(JSContext *ctx, const QJSIntlBytes *available,
                                 size_t count, const JSIntlLocaleList *requested,
                                 QJSIntlLocaleMatch *out);
int js_intl_resolve_locale_index(JSContext *ctx, const QJSIntlBytes *available,
                                 size_t count, const JSIntlLocaleList *requested,
                                 size_t *out);
/* Read/coerce/validate a Unicode type option. Absent has NULL data; release
   owned with JS_FreeCString, including after failure (which resets outputs). */
int js_intl_unicode_type_option(JSContext *ctx, JSValueConst options, const char *name,
                                 QJSIntlBytes *out, const char **owned);
typedef QJSIntlLocaleValueSupported JSIntlLocaleValueSupported;
typedef struct JSIntlResolvedKeyword {
    char *locale, *value;
} JSIntlResolvedKeyword;
/* Fixed ResolveLocale policy for one relevant key. base is a canonical,
   extension-free service locale; extension comes from resolve_locale_match.
   default_value is supported, option is absent or a validated Unicode type.
   supported queries the selected service/locale without executing user code;
   OK with supported=0 means unsupported, non-OK means an operational error.
   Returned strings use js_free; inputs and callbacks are borrowed. Reset out
   on failure. Canonicalize option aliases before comparing/retaining a key.
   Multi-key/null resolution uses the shared locale resolver directly. */
int js_intl_resolve_locale_key(JSContext *ctx, QJSIntlBytes base, QJSIntlBytes extension,
                               const char key[2], QJSIntlBytes default_value, QJSIntlBytes option,
                               JSIntlLocaleValueSupported *supported, void *opaque,
                               JSIntlResolvedKeyword *out);
JSValue js_intl_filter_locales(JSContext *ctx, const QJSIntlBytes *available,
                                size_t count, JSValueConst locales,
                                JSValueConst options_value);
const char *js_intl_locale_tag(JSValueConst value);
int js_intl_error(JSContext *ctx, QJSIntlStatus status, const char *operation);
int js_intl_canonicalize_locale_list(JSContext *ctx, JSValueConst locales,
                                     JSIntlLocaleList *out);
void js_intl_locale_list_clear(JSRuntime *rt, JSIntlLocaleList *list);
JSValue js_intl_get_canonical_locales(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv);
#endif
#endif

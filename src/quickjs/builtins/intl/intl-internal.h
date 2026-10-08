/*
 * QuickJS native internationalization support
 *
 * Copyright (c) 2017-2025 Fabrice Bellard
 * Copyright (c) 2017-2025 Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/* Private native C Intl contract. Not part of the embedding ABI. */
#ifndef QUICKJS_INTL_INTERNAL_H
#define QUICKJS_INTL_INTERNAL_H
#include "../../internal/base.h"
#include "../../internal/runtime.h"
#include "../../internal/class.h"
#include "../../internal/allocator.h"
#include "../../internal/atom.h"
#include "../../internal/object.h"
#include "../../internal/function.h"
#include "../../internal/function-list.h"
#include "../../internal/c-function.h"
#include "../../internal/string.h"
#include "../../value/conversion.h"

#ifdef CONFIG_INTL
#include "../../../intl/icu-config.h"

/* Lists own their engine-allocated NUL-terminated strings and array. */
typedef struct JSIntlLocaleList {
    char **items;
    size_t count;
    size_t capacity;
} JSIntlLocaleList;

typedef enum JSIntlService {
    JS_INTL_COLLATOR, JS_INTL_COLLATOR_SEARCH, JS_INTL_NUMBER_FORMAT, JS_INTL_DATE_TIME_FORMAT,
    JS_INTL_PLURAL_RULES, JS_INTL_LIST_FORMAT, JS_INTL_RELATIVE_TIME_FORMAT,
    JS_INTL_DISPLAY_NAMES, JS_INTL_SEGMENTER, JS_INTL_DURATION_FORMAT,
    JS_INTL_SERVICE_COUNT
} JSIntlService;

#define JS_INTL_MAX_RESOLUTION_KEYS 4
typedef struct JSIntlResolutionKey {
    const char *key;                /* Unicode extension key, borrowed. */
    const char *option;             /* Borrowed string; NULL if absent. */
    BOOL suppress_extension;       /* Explicit null option, e.g. hour12. */
} JSIntlResolutionKey;

typedef struct JSIntlResolvedLocale {
    char *locale;                   /* Public resolved BCP47 tag. */
    char *data_locale;              /* Matched base BCP47 tag. */
    char *icu_locale;               /* ICU ID including resolved keys. */
    char *values[JS_INTL_MAX_RESOLUTION_KEYS]; /* In key argument order. */
    int key_count;
} JSIntlResolvedLocale;

/* Foundation-owned options: every helper performs exactly one Get. */
JSValue js_intl_coerce_options(JSContext *ctx, JSValueConst options);
JSValue js_intl_get_options(JSContext *ctx, JSValueConst options);
int js_intl_get_string_option(JSContext *ctx, JSValueConst options,
                             const char *property, const char *const *values,
                             int value_count, int fallback, int *result);
int js_intl_get_string_option_alloc(JSContext *ctx, JSValueConst options,
                                   const char *property, char **result);
int js_intl_get_bool_option(JSContext *ctx, JSValueConst options,
                           const char *property, int fallback, int *result);
int js_intl_get_number_option(JSContext *ctx, JSValueConst options,
                             const char *property, double minimum,
                             double maximum, int fallback, int *result);
int js_intl_default_number_option(JSContext *ctx, JSValueConst value,
                                 double minimum, double maximum,
                                 int fallback, int *result);
/* ToString preserving UTF16, embedded NUL, and unpaired surrogates. */
int js_intl_to_uchar(JSContext *ctx, JSValueConst value,
                   UChar **result, int32_t *length);
JSValue js_intl_from_uchar(JSContext *ctx, const UChar *value, int32_t length);
char *js_intl_strdup(JSContext *ctx, const char *value);
int js_intl_icu_error(JSContext *ctx, UErrorCode status, const char *operation);
/* Part helper borrows value/extra; defines own enumerable data properties. */
int js_intl_add_part(JSContext *ctx, JSValueConst parts, uint32_t index,
                     const char *type, JSValueConst value,
                     const char *extra_name, JSValueConst extra);
int js_intl_add_part_uchar(JSContext *ctx, JSValueConst parts, uint32_t index,
                          const char *type, const UChar *value, int32_t length,
                          const char *extra_name, JSValueConst extra);
int js_intl_define_string(JSContext *ctx, JSValueConst object,
                          const char *property, const char *value);
int js_intl_define_int(JSContext *ctx, JSValueConst object,
                       const char *property, int value);
int js_intl_define_bool(JSContext *ctx, JSValueConst object,
                        const char *property, BOOL value);
void js_intl_locale_list_free(JSContext *ctx, JSIntlLocaleList *list);
int js_intl_locale_list_append(JSContext *ctx, JSIntlLocaleList *list,
                               const char *value);
void js_intl_resolved_locale_free(JSContext *ctx, JSIntlResolvedLocale *locale);
/* Allocate ICU result buffers; required excludes terminator. */
UChar *js_intl_alloc_uchar(JSContext *ctx, int32_t required);
char *js_intl_alloc_char(JSContext *ctx, int32_t required);

/* Class helper: IDs are appended after existing private IDs, CONFIG_INTL only.
   Constructor registers the class in ctx->rt, creates an ordinary prototype,
   caches it in ctx->class_proto, and installs ctor on the borrowed Intl object.
   The class definition and function tables must have static storage duration. */
int js_intl_init_constructor(JSContext *ctx, JSValueConst intl,
                            JSClassID class_id, const JSClassDef *class_def,
                            const char *name, JSCFunction *constructor,
                            int length, JSCFunctionEnum cproto,
                            const JSCFunctionListEntry *constructor_functions,
                            int constructor_function_count,
                            const JSCFunctionListEntry *prototype_functions,
                            int prototype_function_count);
int js_intl_ensure_context(JSContext *ctx);
int js_intl_ensure_service(JSContext *ctx, JSIntlService service);
JSValue js_intl_new_c_function_data(JSContext *ctx, JSCFunctionData *func,
                                    int length, int magic, int data_len,
                                    JSValueConst *data);
int js_intl_register_class(JSContext *ctx, JSClassID class_id,
                           const JSClassDef *class_def);
JSValue js_intl_new_object(JSContext *ctx, JSValueConst new_target,
                           JSClassID class_id);
/* Borrowed context roots, retained independently of global property edits. */
JSValueConst js_intl_constructor(JSContext *ctx, JSClassID class_id);
JSValueConst js_intl_fallback_symbol(JSContext *ctx);
/* Context owns these lists. Only locale-resolution may publish a detached,
   fully built list into an empty slot. All lookup callers borrow it read-only.
   A non-NULL items array denotes a completed list containing DefaultLocale. */
JSIntlLocaleList *js_intl_available_locale_cache(JSContext *ctx,
                                                 JSIntlService service);
const char *js_intl_default_locale(JSContext *ctx);
const char *js_intl_default_time_zone(JSContext *ctx);
void js_intl_context_mark(JSRuntime *rt, JSContext *ctx,
                          JS_MarkFunc *mark_func);
void js_intl_context_free(JSContext *ctx);
void js_intl_context_memory_usage(JSContext *ctx, JSMemoryUsage *usage);
JSValue js_intl_string_locale_compare(JSContext *ctx, JSValueConst this_val,
                                      int argc, JSValueConst *argv);

/* Locale-owner API. Canonicalization validates ECMA402 grammar before ICU;
   ResolveLocale handles supported data, key defaults/options/extensions.
   It must not access options itself: callers preserve current ResolveOptions
   getter order and supply already-coerced resolution key values. */
char *js_intl_canonicalize_tag(JSContext *ctx, const char *tag, size_t length);
const char *js_intl_locale_tag(JSValueConst value);
int js_intl_is_unicode_type(const char *value);
char *js_intl_canonicalize_time_zone(JSContext *ctx, const char *identifier, size_t length);
int js_intl_primary_time_zones(JSContext *ctx, const char *region, JSIntlLocaleList *result);
int js_intl_canonicalize_locale_list(JSContext *ctx, JSValueConst locales,
                                    JSIntlLocaleList *result);
char *js_intl_locale_to_icu(JSContext *ctx, const char *canonical_tag);
char *js_intl_locale_from_icu(JSContext *ctx, const char *icu_locale);
int js_intl_resolve_locale(JSContext *ctx, JSIntlService service,
                           const JSIntlLocaleList *requested,
                           const char *matcher,
                           const JSIntlResolutionKey *keys, int key_count,
                           JSIntlResolvedLocale *result);
JSValue js_intl_supported_locales(JSContext *ctx, JSIntlService service,
                                 JSValueConst locales, JSValueConst options);
JSValue js_intl_get_canonical_locales(JSContext *ctx, JSValueConst this_val,
                                     int argc, JSValueConst *argv);
JSValue js_intl_supported_values_of(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv);

/* Each module initializer adds one actual service to borrowed Intl. */
int js_intl_init_locale(JSContext *ctx, JSValueConst intl);
int js_intl_init_collator(JSContext *ctx, JSValueConst intl);
int js_intl_init_number_format(JSContext *ctx, JSValueConst intl);
int js_intl_init_date_time_format(JSContext *ctx, JSValueConst intl);
int js_intl_init_plural_rules(JSContext *ctx, JSValueConst intl);
int js_intl_init_list_format(JSContext *ctx, JSValueConst intl);
int js_intl_init_relative_time_format(JSContext *ctx, JSValueConst intl);
int js_intl_init_display_names(JSContext *ctx, JSValueConst intl);
int js_intl_init_segmenter(JSContext *ctx, JSValueConst intl);
int js_intl_init_duration_format(JSContext *ctx, JSValueConst intl);
#endif /* CONFIG_INTL */
#endif /* QUICKJS_INTL_INTERNAL_H */

/* Native development locale resolution; ICU selection retains its owner.
 * Exact CanonicalizeLocaleList and option getter bodies are reused.
 * AvailableLocales is an admitted ListFormat profile, never metadata bits. */
#include "locale-private.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
static int native_remove_unicode_extension(char *tag)
{
    char *part = strchr(tag, '-');
    while (part) {
        char *next = strchr(part + 1, '-');
        size_t length = next ? (size_t)(next - part - 1) : strlen(part + 1);
        if (length == 1) {
            /* A private-use 'u' subtag is not a Unicode extension singleton. */
            if (part[1] == 'x')
                return 0;
            if (part[1] == 'u') {
                char *end = next;
                while (end) {
                    char *following = strchr(end + 1, '-');
                    size_t subtag_length = following ?
                        (size_t)(following - end - 1) : strlen(end + 1);
                    if (subtag_length == 1)
                        break;
                    end = following;
                }
                if (end)
                    memmove(part, end, strlen(end) + 1);
                else
                    *part = 0;
                return 1;
            }
        }
        part = next;
    }
    return 0;
}

static int list_has(const JSIntlLocaleList *l, const char *s)
{
    size_t i; for (i = 0; i < l->count; i++) if (!strcmp(l->items[i], s)) return 1; return 0;
}
static int canonicalize_element(JSContext *ctx, JSValueConst value, JSIntlLocaleList *list)
{
    const char *tag, *cstr = NULL; char *canonical; size_t length; int r = 0;
    if (!JS_IsString(value) && !JS_IsObject(value)) { JS_ThrowTypeError(ctx, "locale must be a string or object"); return -1; }
    tag = js_intl_locale_tag(value);
    if (tag) length = strlen(tag);
    else { tag = cstr = JS_ToCStringLen(ctx, &length, value); if (!tag) return -1; }
    canonical = js_intl_canonicalize_tag(ctx, tag, length); JS_FreeCString(ctx, cstr);
    if (!canonical) return -1;
    if (!list_has(list, canonical)) r = js_intl_locale_list_append(ctx, list, canonical);
    js_free(ctx, canonical); return r;
}
int js_intl_canonicalize_locale_list(JSContext *ctx, JSValueConst locales, JSIntlLocaleList *result)
{
    JSValue object = JS_UNDEFINED, value; int64_t length, k; int present;
    memset(result, 0, sizeof(*result));
    if (JS_IsUndefined(locales)) return 0;
    if (JS_IsString(locales) || js_intl_locale_tag(locales)) return canonicalize_element(ctx, locales, result);
    object = JS_ToObject(ctx, locales); if (JS_IsException(object)) return -1;
    if (js_get_length64(ctx, &length, object) < 0) goto fail;
    for (k = 0; k < length; k++) {
        char name[32]; JSAtom key;
        snprintf(name, sizeof(name), "%lld", (long long)k);
        key = JS_NewAtom(ctx, name); if (key == JS_ATOM_NULL) goto fail;
        present = JS_HasProperty(ctx, object, key);
        if (present < 0) { JS_FreeAtom(ctx, key); goto fail; }
        if (present) {
            value = JS_GetProperty(ctx, object, key); JS_FreeAtom(ctx, key);
            if (JS_IsException(value)) goto fail;
            present = canonicalize_element(ctx, value, result); JS_FreeValue(ctx, value);
            if (present < 0) goto fail;
        } else JS_FreeAtom(ctx, key);
    }
    JS_FreeValue(ctx, object); return 0;
fail:
    JS_FreeValue(ctx, object); js_intl_locale_list_free(ctx, result); return -1;
}
static int available_locale_append(JSContext *ctx, JSIntlLocaleList *list,
                                   const char *tag)
{
    IntlTag parsed = { 0 };
    char *canonical = js_intl_strdup(ctx, tag);
    char scriptless[16];
    int result = -1;

    if (!canonical)
        return -1;
    native_remove_unicode_extension(canonical);
    if (intl_parse_tag(ctx, canonical, strlen(canonical), &parsed) < 0 ||
        js_intl_locale_list_append(ctx, list, canonical) < 0)
        goto done;
    /* Every compound tag has a language fallback, including DefaultLocale. */
    if (strcmp(canonical, parsed.base.language) &&
        js_intl_locale_list_append(ctx, list, parsed.base.language) < 0)
        goto done;
    if (parsed.base.script && parsed.base.region) {
        /* The grammar limits language to eight and region to three bytes. */
        snprintf(scriptless, sizeof(scriptless), "%s-%s",
                 parsed.base.language, parsed.base.region);
        /* The parsed grammar stores subtags in lowercase. */
        for (size_t i = strlen(parsed.base.language) + 1; scriptless[i]; i++) {
            if (scriptless[i] >= 'a' && scriptless[i] <= 'z')
                scriptless[i] -= 'a' - 'A';
        }
        if (js_intl_locale_list_append(ctx, list, scriptless) < 0)
            goto done;
    }
    result = 0;
done:
    intl_tag_free(ctx, &parsed);
    js_free(ctx, canonical);
    return result;
}
static QJSIntlService native_service_id(JSIntlService service)
{
    /* Explicit mapping: the frontend, provider, and wire enums differ. */
    switch (service) {
    case JS_INTL_COLLATOR: return QJS_INTL_COLLATOR;
    case JS_INTL_COLLATOR_SEARCH: return QJS_INTL_COLLATOR_SEARCH;
    case JS_INTL_SEGMENTER: return QJS_INTL_SEGMENTER;
    case JS_INTL_NUMBER_FORMAT: return QJS_INTL_NUMBER_FORMAT;
    case JS_INTL_DATE_TIME_FORMAT: return QJS_INTL_DATE_TIME_FORMAT;
    case JS_INTL_PLURAL_RULES: return QJS_INTL_PLURAL_RULES;
    case JS_INTL_LIST_FORMAT: return QJS_INTL_LIST_FORMAT;
    case JS_INTL_RELATIVE_TIME_FORMAT: return QJS_INTL_RELATIVE_TIME_FORMAT;
    case JS_INTL_DISPLAY_NAMES: return QJS_INTL_DISPLAY_NAMES;
    case JS_INTL_DURATION_FORMAT: return QJS_INTL_DURATION_FORMAT;
    default: return QJS_INTL_SERVICE_COUNT;
    }
}
static const JSIntlLocaleList *available_locales(JSContext *ctx,
                                                 JSIntlService service)
{
    JSIntlLocaleList result = { 0 };
    JSIntlLocaleList *cached = js_intl_available_locale_cache(ctx, service);
    QJSIntlProvider *provider;
    QJSIntlTagList native = { 0 };
    QJSIntlStatus status;
    size_t i;
    if (!cached) return NULL;
    if (cached->items) return cached;
    provider = js_intl_native_provider(ctx);
    if (!provider) return NULL;
    status = qjs_intl_locale_available(provider, native_service_id(service), &native);
    if (js_intl_native_error(ctx, status, "AvailableLocales")) goto fail;
    for (i = 0; i < native.count; i++) {
        /* Native tag-list contract promises separately owned NUL termination.
         * Bounds are still checked before use by the frontend parser. */
        if (!native.items[i].data ||
            strlen(native.items[i].data) != native.items[i].length) {
            JS_ThrowInternalError(ctx, "invalid native AvailableLocales tag");
            goto fail;
        }
        if (available_locale_append(ctx, &result, native.items[i].data) < 0)
            goto fail;
    }
    if (available_locale_append(ctx, &result, js_intl_default_locale(ctx)) < 0 ||
        intl_list_sort_unique(ctx, &result) < 0) goto fail;
    qjs_intl_tag_list_clear(provider, &native);
    /* Publish a detached, complete cache only after every allocation succeeds. */
    *cached = result;
    return cached;
fail:
    qjs_intl_tag_list_clear(provider, &native);
    js_intl_locale_list_free(ctx, &result);
    return NULL;
}
static char *matching_locale(JSContext *ctx, const JSIntlLocaleList *available, const char *requested)
{
    char *candidate = js_intl_strdup(ctx, requested), *dash;
    if (!candidate) return NULL;
    native_remove_unicode_extension(candidate);
    while (*candidate) {
        if (list_has(available, candidate)) return candidate;
        dash = strrchr(candidate, '-'); if (!dash) break;
        while (dash - candidate >= 2 && dash[-2] == '-') dash -= 2;
        *dash = 0;
    }
    js_free(ctx, candidate); return NULL;
}
char *intl_lookup_locale(JSContext *ctx, JSIntlService service, const char *requested)
{
    const JSIntlLocaleList *available = available_locales(ctx, service);
    if (!available)
        return NULL;
    return matching_locale(ctx, available, requested);
}
/* qjs_intl_locale_key_values returns the default first. A null first
 * item represents the spec's null default; all nonnull items are supported
 * canonical Unicode types. This query never reads JS options. */
static int native_key_data(JSContext *ctx, JSIntlService service,
                           const char *locale, const char *key,
                           JSIntlLocaleList *supported, char **default_value)
{
    QJSIntlProvider *p = js_intl_native_provider(ctx);
    QJSIntlTagList native = {0};
    QJSIntlStatus status;
    size_t i;
    int ret = -1;
    *default_value = NULL;
    if (!p) return -1;
    status = qjs_intl_locale_key_values(p, native_service_id(service),
        (QJSIntlBytes){locale, strlen(locale)}, (QJSIntlBytes){key, strlen(key)}, &native);
    if (js_intl_native_error(ctx, status, "LocaleData keys")) goto done;
    if (!native.count || !native.items) {
        JS_ThrowInternalError(ctx, "empty native LocaleData key list"); goto done;
    }
    for (i = 0; i < native.count; i++) {
        const QJSIntlBytes *value = &native.items[i];
        if (!value->data) {
            if (i || value->length) {
                JS_ThrowInternalError(ctx, "invalid native null LocaleData key"); goto done;
            }
            continue;
        }
        if (!value->length || strlen(value->data) != value->length ||
            (!js_intl_is_unicode_type(value->data) && strcmp(value->data, "true") && strcmp(value->data, "false"))) {
            JS_ThrowInternalError(ctx, "invalid native LocaleData key"); goto done;
        }
        if (!i) {
            *default_value = js_intl_strdup(ctx, value->data);
            if (!*default_value) goto done;
        }
        if (js_intl_locale_list_append(ctx, supported, value->data) < 0) goto done;
    }
    ret = 0;
 done:
    qjs_intl_tag_list_clear(p, &native);
    return ret;
}
int js_intl_resolve_locale(JSContext *ctx, JSIntlService service,
                           const JSIntlLocaleList *requested, const char *matcher,
                           const JSIntlResolutionKey *keys, int count, JSIntlResolvedLocale *result)
{
    const JSIntlLocaleList *available; IntlTag request = { 0 }, public_tag = { 0 };
    char *matched = NULL, *public_string = NULL; size_t i; int k, r = -1;
    memset(result, 0, sizeof(*result));
    if (count < 0 || count > JS_INTL_MAX_RESOLUTION_KEYS) { JS_ThrowInternalError(ctx, "too many Intl resolution keys"); return -1; }
    (void)matcher;
    available = available_locales(ctx, service);
    if (!available) goto done;
    for (i = 0; i < requested->count; i++) {
        matched = matching_locale(ctx, available, requested->items[i]);
        if (matched) { if (intl_parse_tag(ctx, requested->items[i], strlen(requested->items[i]), &request) < 0) goto done; break; }
        if (JS_HasException(ctx)) goto done;
    }
    if (!matched) matched = js_intl_strdup(ctx, js_intl_default_locale(ctx));
    if (!matched) goto done;
    result->data_locale = js_intl_strdup(ctx, matched); if (!result->data_locale) goto done;
    if (intl_parse_tag(ctx, matched, strlen(matched), &public_tag) < 0) goto done;
    result->key_count = count;
    for (k = 0; k < count; k++) {
        JSIntlLocaleList supported = { 0 }; char *value = NULL, *option = NULL; const char *extension; BOOL retain = FALSE;
        if (native_key_data(ctx, service, matched, keys[k].key, &supported, &value) < 0) { js_intl_locale_list_free(ctx, &supported); js_free(ctx, value); goto done; }
        extension = intl_tag_keyword(&request, keys[k].key);
        if (extension && !keys[k].suppress_extension) {
            const char *candidate = *extension ? extension : "true";
            if (list_has(&supported, candidate)) {
                js_free(ctx, value); value = js_intl_strdup(ctx, candidate); if (!value) { js_intl_locale_list_free(ctx, &supported); goto done; } retain = TRUE;
            }
        }
        if (keys[k].option) {
            option = intl_canonicalize_uvalue(ctx, keys[k].key, keys[k].option);
            if (!option) { js_intl_locale_list_free(ctx, &supported); js_free(ctx, value); goto done; }
            if (!*option) { js_free(ctx, option); option = js_intl_strdup(ctx, "true"); if (!option) { js_intl_locale_list_free(ctx, &supported); js_free(ctx, value); goto done; } }
            if ((!value || strcmp(option, value)) && list_has(&supported, option)) {
                js_free(ctx, value); value = option; option = NULL; retain = FALSE;
            }
            js_free(ctx, option);
        }
        js_intl_locale_list_free(ctx, &supported);
        result->values[k] = value;
        if (retain && intl_tag_set_keyword(ctx, &public_tag, keys[k].key, !strcmp(value, "true") ? "" : value) < 0) goto done;
    }
    public_string = intl_tag_string(ctx, &public_tag);
    if (!public_string) goto done;
    result->locale = js_intl_canonicalize_tag(ctx, public_string, strlen(public_string));
    if (!result->locale) goto done;
    r = 0;
done:
    intl_tag_free(ctx, &request); intl_tag_free(ctx, &public_tag);
    js_free(ctx, matched); js_free(ctx, public_string);
    if (r < 0)
        js_intl_resolved_locale_free(ctx, result);
    return r;
}
JSValue js_intl_supported_locales(JSContext *ctx, JSIntlService service, JSValueConst locales, JSValueConst options)
{
    static const char *const matchers[] = { "lookup", "best fit" };
    JSIntlLocaleList requested = { 0 }, supported = { 0 }; const JSIntlLocaleList *available; JSValue object = JS_UNDEFINED, result = JS_EXCEPTION; size_t i; int matcher;
    if (js_intl_canonicalize_locale_list(ctx, locales, &requested) < 0) goto done;
    object = js_intl_coerce_options(ctx, options); if (JS_IsException(object)) goto done;
    if (js_intl_get_string_option(ctx, object, "localeMatcher", matchers, 2, 1, &matcher) < 0) goto done;
    available = available_locales(ctx, service);
    if (!available) goto done;
    for (i = 0; i < requested.count; i++) {
        char *match = matching_locale(ctx, available, requested.items[i]);
        if (match) { js_free(ctx, match); if (js_intl_locale_list_append(ctx, &supported, requested.items[i]) < 0) goto done; }
        else if (JS_HasException(ctx)) goto done;
    }
    result = intl_array_from_list(ctx, &supported);
done:
    JS_FreeValue(ctx, object); js_intl_locale_list_free(ctx, &requested); js_intl_locale_list_free(ctx, &supported); return result;
}
JSValue js_intl_get_canonical_locales(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSIntlLocaleList list = { 0 }; JSValue result;
    if (js_intl_canonicalize_locale_list(ctx, argc ? argv[0] : JS_UNDEFINED, &list) < 0) return JS_EXCEPTION;
    result = intl_array_from_list(ctx, &list); js_intl_locale_list_free(ctx, &list); return result;
}
#endif

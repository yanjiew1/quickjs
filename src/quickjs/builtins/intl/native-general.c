/* Native Intl general operations. ECMA402 7ae78cfdf8255468ffc8ebda33dafaea952808dd.
 * Reviewed 2026-10-09. Unicode18 data stays with libunicode; CLDR49 with
 * the one native provider blob; timezone names with the native TZ owner. */
#include "locale-private.h"
#if defined(CONFIG_INTL) && defined(CONFIG_INTL_NATIVE)
#include "../../internal/string-buffer.h"
#include "../../../unicode/libunicode.h"
#include "../../../intl/number-unit.h"
#include "../../../intl/native-locale-id.h"
#include "../../../timezone/timezone.h"

static int enumeration_error(JSContext *ctx)
{
    JS_ThrowInternalError(ctx, "invalid native Intl enumeration data");
    return -1;
}
static int append_slice(JSContext *ctx, JSIntlLocaleList *list,
                         QJSIntlDataSlice value)
{
    /* The reader validates pool references, including their trailing NUL. */
    if (!value.data || !value.length) return enumeration_error(ctx);
    return js_intl_locale_list_append(ctx, list, (const char *)value.data);
}
static int key_inventory(JSContext *ctx, QJSIntlProvider *provider,
                          QJSIntlService service, const char *key,
                          JSIntlLocaleList *list)
{
    QJSIntlTagList locales = {0}, values = {0};
    QJSIntlStatus status;
    size_t i, j;
    int result = -1;
    status = qjs_intl_locale_available(provider, service, &locales);
    if (js_intl_native_error(ctx, status, "enumeration AvailableLocales")) goto done;
    for (i = 0; i < locales.count; i++) {
        status = qjs_intl_locale_key_values(provider, service, locales.items[i],
            (QJSIntlBytes){key, strlen(key)}, &values);
        if (js_intl_native_error(ctx, status, "enumeration LocaleData")) goto done;
        for (j = 0; j < values.count; j++) {
            QJSIntlBytes v = values.items[j];
            if (!v.data) {
                if (j || v.length) { enumeration_error(ctx); goto done; }
                continue;
            }
            if (!v.length || strlen(v.data) != v.length ||
                !js_intl_is_unicode_type(v.data)) {
                enumeration_error(ctx); goto done;
            }
            if (!strcmp(key, "co") &&
                (!strcmp(v.data, "standard") || !strcmp(v.data, "search"))) continue;
            if (js_intl_locale_list_append(ctx, list, v.data) < 0) goto done;
        }
        qjs_intl_tag_list_clear(provider, &values);
    }
    result = 0;
done:
    qjs_intl_tag_list_clear(provider, &values);
    qjs_intl_tag_list_clear(provider, &locales);
    return result;
}
static int number_inventory(JSContext *ctx, QJSIntlProvider *provider,
                             int currency, JSIntlLocaleList *list)
{
    const QJSIntlDataView *view = qjs_intl_native_provider_view(provider);
    QJSIntlDataSection rows, locale_rows;
    QJSIntlDataSlice tag, code, name;
    QJSIntlTagList locales = {0};
    QJSIntlStatus status;
    uint32_t i, locale_index;
    size_t j;
    int result = -1;
    status = qjs_intl_locale_available(provider, QJS_INTL_NUMBER_FORMAT, &locales);
    if (js_intl_native_error(ctx, status, "NumberFormat enumeration")) goto done;
    if (!view || qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locale_rows) != QJS_INTL_DATA_OK ||
        qjs_intl_data_section(view, currency ? QJS_INTL_DATA_NUMBER_CURRENCY :
            QJS_INTL_DATA_NUMBER_UNIT, &rows) != QJS_INTL_DATA_OK) {
        enumeration_error(ctx); goto done;
    }
    for (i = 0; i < rows.record_count; i++) {
        if (qjs_intl_data_record_u32(&rows, i, 0, &locale_index) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_string(view, &locale_rows, locale_index, 0, &tag) != QJS_INTL_DATA_OK ||
            qjs_intl_data_record_string(view, &rows, i, 4, &code) != QJS_INTL_DATA_OK) {
            enumeration_error(ctx); goto done;
        }
        for (j = 0; j < locales.count; j++)
            if (tag.length == locales.items[j].length &&
                !memcmp(tag.data, locales.items[j].data, tag.length)) break;
        if (j == locales.count) continue;
        if (currency) {
            if (code.length != 3 || code.data[0] < 'A' || code.data[0] > 'Z' ||
                code.data[1] < 'A' || code.data[1] > 'Z' || code.data[2] < 'A' || code.data[2] > 'Z') {
                enumeration_error(ctx); goto done;
            }
            /* A substituted code is not an installed currency display name. */
            if (qjs_intl_data_record_string(view, &rows, i, 68, &name) != QJS_INTL_DATA_OK) {
                enumeration_error(ctx); goto done;
            }
            if (!name.length || (name.length == code.length && !memcmp(name.data, code.data, code.length))) continue;
        } else if (!qjs_intl_number_unit_validate((QJSIntlBytes){(const char *)code.data, code.length}) ||
                   strstr((const char *)code.data, "-per-")) continue;
        if (append_slice(ctx, list, code) < 0) goto done;
    }
    result = 0;
done:
    qjs_intl_tag_list_clear(provider, &locales);
    return result;
}
static int calendar_inventory(JSContext *ctx, QJSIntlProvider *provider,
                               JSIntlLocaleList *list)
{
    const QJSIntlDataView *view = qjs_intl_native_provider_view(provider);
    const QJSIntlAllocator *a = qjs_intl_native_provider_allocator(provider);
    QJSIntlDataSection rows;
    QJSIntlDataSlice id;
    uint32_t i;
    if (!view || !a || qjs_intl_data_section(view, QJS_INTL_DATA_AVAILABLE_CALENDAR, &rows) != QJS_INTL_DATA_OK ||
        !rows.record_count) return enumeration_error(ctx);
    for (i = 0; i < rows.record_count; i++) {
        char *canonical = NULL;
        QJSIntlStatus status;
        int result;
        if (qjs_intl_data_record_string(view, &rows, i, 0, &id) != QJS_INTL_DATA_OK)
            return enumeration_error(ctx);
        status = qjs_intl_native_locale_canonicalize_uvalue(a, view,
            (QJSIntlBytes){"ca", 2}, (QJSIntlBytes){(const char *)id.data, id.length}, &canonical);
        if (js_intl_native_error(ctx, status, "calendar enumeration canonicalization")) return -1;
        result = strlen(canonical) == id.length && !memcmp(canonical, id.data, id.length) ?
            append_slice(ctx, list, id) : 0;
        a->free(a->opaque, canonical);
        if (result < 0) return -1;
    }
    return 0;
}
int intl_values_list(JSContext *ctx, const char *key, JSIntlLocaleList *list)
{
    QJSIntlProvider *provider;
    size_t i;
    int result;
    memset(list, 0, sizeof(*list));
    if (!strcmp(key, "timeZone")) {
        for (i = 0; i < qjs_tz_identifier_count(); i++) {
            const char *name = qjs_tz_identifier_at(i), *identifier, *primary;
            if (!name || qjs_tz_resolve(name, strlen(name), &identifier, &primary) != QJS_TZ_OK ||
                !identifier || !primary) return enumeration_error(ctx);
            if (!strcmp(name, primary) && js_intl_locale_list_append(ctx, list, primary) < 0) return -1;
        }
        return intl_list_sort_unique(ctx, list);
    }
    if (strcmp(key, "calendar") && strcmp(key, "collation") && strcmp(key, "currency") &&
        strcmp(key, "numberingSystem") && strcmp(key, "unit")) {
        JS_ThrowRangeError(ctx, "invalid Intl enumeration key"); return -1;
    }
    provider = js_intl_native_provider(ctx);
    if (!provider) return -1;
    if (!strcmp(key, "calendar"))
        result = calendar_inventory(ctx, provider, list);
    else if (!strcmp(key, "collation")) {
        result = key_inventory(ctx, provider, QJS_INTL_COLLATOR, "co", list);
        if (!result) result = key_inventory(ctx, provider, QJS_INTL_COLLATOR_SEARCH, "co", list);
    } else if (!strcmp(key, "numberingSystem"))
        result = key_inventory(ctx, provider, QJS_INTL_NUMBER_FORMAT, "nu", list);
    else result = number_inventory(ctx, provider, !strcmp(key, "currency"), list);
    return result < 0 ? -1 : intl_list_sort_unique(ctx, list);
}
JSValue js_intl_supported_values_of(JSContext *ctx, JSValueConst receiver,
                                    int argc, JSValueConst *argv)
{
    JSValue key_value, result = JS_EXCEPTION;
    const char *key;
    size_t length;
    JSIntlLocaleList list = {0};
    key_value = JS_ToString(ctx, argc ? argv[0] : JS_UNDEFINED);
    if (JS_IsException(key_value)) return key_value;
    key = JS_ToCStringLen(ctx, &length, key_value);
    JS_FreeValue(ctx, key_value);
    if (!key) return JS_EXCEPTION;
    if (strlen(key) != length) JS_ThrowRangeError(ctx, "invalid Intl enumeration key");
    else if (!intl_values_list(ctx, key, &list)) result = intl_array_from_list(ctx, &list);
    JS_FreeCString(ctx, key);
    js_intl_locale_list_free(ctx, &list);
    return result;
}

static uint32_t case_previous(const JSString *s, int *position)
{
    uint32_t c;
    int i = *position;
    if (!i) return 0;
    c = s->is_wide_char ? s->u.str16[--i] : s->u.str8[--i];
    if (s->is_wide_char && c >= 0xdc00 && c <= 0xdfff && i &&
        s->u.str16[i - 1] >= 0xd800 && s->u.str16[i - 1] <= 0xdbff) {
        c = 0x10000 + ((s->u.str16[--i] - 0xd800) << 10) + c - 0xdc00;
    }
    *position = i;
    return c;
}
static int case_final_sigma(const JSString *s, int before, int after)
{
    uint32_t c = 0;
    while (before) {
        c = case_previous(s, &before);
        if (!lre_is_case_ignorable(c)) break;
    }
    if (lre_is_case_ignorable(c) || !lre_is_cased(c)) return 0;
    while (after < s->len) {
        c = string_getc(s, &after);
        if (!lre_is_case_ignorable(c)) return !lre_is_cased(c);
    }
    return 1;
}
static int case_before_dot(const JSString *s, int after)
{
    while (after < s->len) {
        uint32_t c = string_getc(s, &after);
        int cc;
        if (c == 0x0307) return 1;
        cc = unicode_get_combining_class(c);
        if (!cc || cc == 230) return 0;
    }
    return 0;
}
static int case_more_above(const JSString *s, int after)
{
    while (after < s->len) {
        int cc = unicode_get_combining_class(string_getc(s, &after));
        if (cc == 230) return 1;
        if (!cc) return 0;
    }
    return 0;
}
static int case_in_range(const CharRange *r, uint32_t c)
{
    int lo = 0, hi = r->len / 2;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (c < r->points[mid * 2]) hi = mid;
        else if (c >= r->points[mid * 2 + 1]) lo = mid + 1;
        else return 1;
    }
    return 0;
}
static int case_after(const JSString *s, int before, const CharRange *soft, int dotted)
{
    while (before) {
        uint32_t c = case_previous(s, &before);
        int cc;
        if (dotted ? case_in_range(soft, c) : c == 'I') return 1;
        cc = unicode_get_combining_class(c);
        if (!cc || cc == 230) return 0;
    }
    return 0;
}
static void *case_realloc(void *opaque, void *ptr, size_t size)
{
    if (!size) { js_free((JSContext *)opaque, ptr); return NULL; }
    return js_realloc((JSContext *)opaque, ptr, size);
}
JSValue js_intl_string_locale_case(JSContext *ctx, JSValueConst value,
                                    int argc, JSValueConst *argv, int lower)
{
    JSValue string, result = JS_EXCEPTION;
    JSIntlLocaleList requested = {0};
    StringBuffer buffer;
    JSString *s;
    CharRange soft;
    const char *tag;
    int tailoring = 0, ready = 0, i, j, n;
    uint32_t mapping[LRE_CC_RES_LEN_MAX];
    string = JS_ToStringCheckObject(ctx, value);
    if (JS_IsException(string)) return string;
    cr_init(&soft, ctx, case_realloc);
    if (js_intl_ensure_context(ctx) || js_intl_canonicalize_locale_list(ctx,
        argc ? argv[0] : JS_UNDEFINED, &requested)) goto done;
    /* TransformCase uses only the first canonical request and prefix lookup
     * in the tailoring inventory; constructor ResolveLocale is not applied. */
    tag = requested.count ? requested.items[0] : js_intl_default_locale(ctx);
    if (!tag) goto done;
    if (strlen(tag) >= 2 && (!tag[2] || tag[2] == '-')) {
        if (!strncmp(tag, "tr", 2) || !strncmp(tag, "az", 2)) tailoring = 1;
        else if (!strncmp(tag, "lt", 2)) tailoring = 2;
    }
    s = JS_VALUE_GET_STRING(string);
    if (tailoring == 2 && !lower && unicode_prop(&soft, "Soft_Dotted") < 0) {
        if (!JS_HasException(ctx)) JS_ThrowOutOfMemory(ctx);
        goto done;
    }
    if (string_buffer_init(ctx, &buffer, s->len)) goto done;
    ready = 1;
    for (i = 0; i < s->len;) {
        int before = i;
        uint32_t c = string_getc(s, &i);
        if (lower && c == 0x03a3 && case_final_sigma(s, before, i)) {
            mapping[0] = 0x03c2; n = 1;
        } else if (tailoring == 1 && lower && c == 0x0130) {
            mapping[0] = 'i'; n = 1;
        } else if (tailoring == 1 && lower && c == 'I' && !case_before_dot(s, i)) {
            mapping[0] = 0x0131; n = 1;
        } else if (tailoring == 1 && lower && c == 0x0307 && case_after(s, before, &soft, 0)) {
            n = 0;
        } else if (tailoring == 1 && !lower && c == 'i') {
            mapping[0] = 0x0130; n = 1;
        } else if (tailoring == 2 && !lower && c == 0x0307 && case_after(s, before, &soft, 1)) {
            n = 0;
        } else if (tailoring == 2 && lower && (c == 'I' || c == 'J' || c == 0x012e) &&
                   case_more_above(s, i)) {
            mapping[0] = c == 0x012e ? 0x012f : c + 32;
            mapping[1] = 0x0307; n = 2;
        } else if (tailoring == 2 && lower && (c == 0x00cc || c == 0x00cd || c == 0x0128)) {
            mapping[0] = 'i'; mapping[1] = 0x0307;
            mapping[2] = c == 0x00cc ? 0x0300 : c == 0x00cd ? 0x0301 : 0x0303; n = 3;
        } else n = lre_case_conv(mapping, c, lower);
        for (j = 0; j < n; j++) if (string_buffer_putc(&buffer, mapping[j])) goto done;
    }
    result = string_buffer_end(&buffer);
    ready = 0;
done:
    if (ready) string_buffer_free(&buffer);
    cr_free(&soft);
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, string);
    return result;
}
#endif

/* Native Unicode and locale-sensitive casing, ECMA-402 TransformCase. */
#include "intl-internal.h"
#include "locale-integration.h"
#ifdef CONFIG_INTL
#include <unicode/ustring.h>

JSValue js_intl_string_locale_case(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int lower)
{
    JSValue string, result = JS_EXCEPTION;
    JSIntlLocaleList requested = { 0 };
    UChar *input = NULL, *output = NULL;
    int32_t input_length, output_length;
    const char *tag, *locale = "und";
    UErrorCode status = U_ZERO_ERROR;

    string = JS_ToStringCheckObject(ctx, this_val);
    if (JS_IsException(string))
        return string;
    if (js_intl_ensure_context(ctx))
        goto done;
    if (js_intl_canonicalize_locale_list(ctx,
            argc > 0 ? argv[0] : JS_UNDEFINED, &requested))
        goto done;
    tag = requested.count ? requested.items[0] : js_intl_default_locale(ctx);
    /* LookupMatchingLocaleByPrefix with the supported tailoring locales.
       Only the first requested locale participates, without negotiation. */
    if (tag && strlen(tag) >= 2 && (tag[2] == 0 || tag[2] == '-')) {
        if (tag[0] == 't' && tag[1] == 'r')
            locale = "tr";
        else if (tag[0] == 'a' && tag[1] == 'z')
            locale = "az";
        else if (tag[0] == 'l' && tag[1] == 't')
            locale = "lt";
        else if (tag[0] == 'e' && tag[1] == 'l')
            locale = "el";
    }
    if (js_intl_to_uchar(ctx, string, &input, &input_length))
        goto done;
    if (lower)
        output_length = u_strToLower(NULL, 0, input, input_length, locale, &status);
    else
        output_length = u_strToUpper(NULL, 0, input, input_length, locale, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(status)) {
        js_intl_icu_error(ctx, status, "locale case mapping");
        goto done;
    }
    output = js_intl_alloc_uchar(ctx, output_length);
    if (!output)
        goto done;
    status = U_ZERO_ERROR;
    if (lower)
        output_length = u_strToLower(output, output_length + 1,
                                    input, input_length, locale, &status);
    else
        output_length = u_strToUpper(output, output_length + 1,
                                    input, input_length, locale, &status);
    if (js_intl_icu_error(ctx, status, "locale case mapping"))
        goto done;
    result = js_intl_from_uchar(ctx, output, output_length);
 done:
    js_free(ctx, input);
    js_free(ctx, output);
    js_intl_locale_list_free(ctx, &requested);
    JS_FreeValue(ctx, string);
    return result;
}
#endif

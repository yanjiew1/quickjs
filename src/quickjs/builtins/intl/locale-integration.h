/* Native ECMA-402 integration with existing builtins. */
#ifndef QUICKJS_INTL_LOCALE_INTEGRATION_H
#define QUICKJS_INTL_LOCALE_INTEGRATION_H
#include "../../internal/base.h"
#ifdef CONFIG_INTL
#define JS_INTL_DTF_ANY 0
#define JS_INTL_DTF_DATE 1
#define JS_INTL_DTF_TIME 2
#define JS_INTL_DTF_ALL 3
JSValue js_intl_date_format(JSContext *ctx, double time,
                            JSValueConst locales, JSValueConst options,
                            int required, int defaults);
JSValue js_intl_string_locale_case(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv, int lower);
#endif
#endif

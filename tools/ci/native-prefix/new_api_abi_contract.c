/* Header-only signature checks; root compiles normal and checked JSValue forms. */
#include "quickjs.h"
#define SAME(symbol,type) _Static_assert(__builtin_types_compatible_p(__typeof__(&(symbol)),type), #symbol " ABI")
SAME(JS_SetIntlDefaultLocale,int (*)(JSContext *,const char *));
SAME(JS_GetIntlDefaultLocale,JSValue (*)(JSContext *));
#ifdef CHECK_UTF16_API
SAME(JS_NewStringUTF16,JSValue (*)(JSContext *,const uint16_t *,size_t));
SAME(JS_ToCStringLenUTF16,const uint16_t *(*)(JSContext *,size_t *,JSValueConst));
SAME(JS_ToCStringUTF16,const uint16_t *(*)(JSContext *,JSValueConst));
SAME(JS_FreeCStringRT,void (*)(JSRuntime *,const char *));
SAME(JS_FreeCStringUTF16,void (*)(JSContext *,const uint16_t *));
SAME(JS_FreeCStringRT_UTF16,void (*)(JSRuntime *,const uint16_t *));
#endif

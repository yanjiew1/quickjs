#include "quickjs.h"
_Static_assert(JS_CFUNC_generic == 0, "callback selector changed");
_Static_assert(JS_CFUNC_generic_magic == 1, "callback selector changed");
_Static_assert(JS_CFUNC_constructor == 2, "callback selector changed");
_Static_assert(JS_CFUNC_constructor_magic == 3, "callback selector changed");
_Static_assert(JS_CFUNC_constructor_or_func == 4, "callback selector changed");
_Static_assert(JS_CFUNC_constructor_or_func_magic == 5, "callback selector changed");
_Static_assert(JS_CFUNC_f_f == 6, "callback selector changed");
_Static_assert(JS_CFUNC_f_f_f == 7, "callback selector changed");
_Static_assert(JS_CFUNC_getter == 8, "callback selector changed");
_Static_assert(JS_CFUNC_setter == 9, "callback selector changed");
_Static_assert(JS_CFUNC_getter_magic == 10, "callback selector changed");
_Static_assert(JS_CFUNC_setter_magic == 11, "callback selector changed");
_Static_assert(JS_CFUNC_iterator_next == 12, "callback selector changed");
_Static_assert(JS_CFUNC_constructor_or_func_flags == 13, "callback selector changed");
_Static_assert(JS_CFUNC_constructor_or_func_receiver == 14, "callback selector changed");
_Static_assert(_Generic(((JSCFunctionType *)0)->constructor_or_func_receiver, JSValue (*)(JSContext *, JSValueConst, JSValueConst, int, JSValueConst *): 1, default: 0), "receiver callback type changed");
#ifdef CHECK_INTL_API
_Static_assert(_Generic(&JS_AddIntrinsicIntl, int (*)(JSContext *): 1, default: 0), "Intl API type changed");
#endif
typedef union LegacyCFunctionType {
    JSCFunction *generic;
    JSValue (*generic_magic)(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic);
    JSCFunction *constructor;
    JSValue (*constructor_magic)(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv, int magic);
    JSCFunction *constructor_or_func;
    double (*f_f)(double);
    double (*f_f_f)(double, double);
    JSValue (*getter)(JSContext *ctx, JSValueConst this_val);
    JSValue (*setter)(JSContext *ctx, JSValueConst this_val, JSValueConst val);
    JSValue (*getter_magic)(JSContext *ctx, JSValueConst this_val, int magic);
    JSValue (*setter_magic)(JSContext *ctx, JSValueConst this_val, JSValueConst val, int magic);
    JSValue (*iterator_next)(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv, int *pdone, int magic);
} LegacyCFunctionType;
_Static_assert(sizeof(JSCFunctionType) == sizeof(LegacyCFunctionType), "callback union size changed");
_Static_assert(_Alignof(JSCFunctionType) == _Alignof(LegacyCFunctionType), "callback union alignment changed");
_Static_assert(_Generic(&JS_AddIntrinsicTemporal, int (*)(JSContext *): 1, default: 0), "Temporal intrinsic signature changed");
_Static_assert(_Generic(&JS_SetConstructor, int (*)(JSContext *, JSValueConst, JSValueConst): 1, default: 0), "constructor setter signature changed");
_Static_assert(_Generic(&JS_SetPropertyFunctionList, int (*)(JSContext *, JSValueConst, const JSCFunctionListEntry *, int): 1, default: 0), "property setter signature changed");
int main(void) { return 0; }

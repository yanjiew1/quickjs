/*
 * QuickJS Function Internal Interface
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
#ifndef QJS_FUNCTION_H
#define QJS_FUNCTION_H

#include "base.h"

typedef enum JSFunctionKindEnum {
    JS_FUNC_NORMAL = 0,
    JS_FUNC_GENERATOR = (1 << 0),
    JS_FUNC_ASYNC = (1 << 1),
    JS_FUNC_ASYNC_GENERATOR = (JS_FUNC_GENERATOR | JS_FUNC_ASYNC),
} JSFunctionKindEnum;


typedef struct JSCFunctionDataRecord {
    JSCFunctionData *func;
    uint8_t length;
    uint8_t data_len;
    uint16_t magic;
    JSValue data[0];
} JSCFunctionDataRecord;


#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* don't create a global binding */
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* the prototype class is 'class_id' instead of JS_CLASS_OBJECT */
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* the prototype is already defined */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor field */

QJS_INTERNAL JSValue JS_CallFree(JSContext *ctx, JSValue func_obj,
                                 JSValueConst this_obj, int argc,
                                 JSValueConst *argv);
QJS_INTERNAL JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val,
                                   JSAtom atom, int argc, JSValueConst *argv);
QJS_INTERNAL JSValueConst JS_GetActiveFunction(JSContext *ctx);
QJS_INTERNAL JSValue JS_SpeciesConstructor(JSContext *ctx, JSValueConst obj,
                                            JSValueConst defaultConstructor);
QJS_INTERNAL BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val,
                                  JSCFunction *func, int magic);
QJS_INTERNAL JSValue js_function_apply(JSContext *ctx, JSValueConst this_val,
                                        int argc, JSValueConst *argv, int magic);
QJS_INTERNAL JSValue js_get_this(JSContext *ctx, JSValueConst this_val);
QJS_INTERNAL JSValue *build_arg_list(JSContext *ctx, uint32_t *plen,
                                     JSValueConst array_arg);
QJS_INTERNAL void free_arg_list(JSContext *ctx, JSValue *tab, uint32_t len);
QJS_INTERNAL JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                                       const char *name, int length,
                                       JSCFunctionEnum cproto, int magic,
                                       JSValueConst proto_val, int n_fields);
QJS_INTERNAL JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx,
                                                        JSValueConst func_obj);
QJS_INTERNAL JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                                         int class_id);
QJS_INTERNAL JSValue JS_NewCConstructor(JSContext *ctx, int class_id,
                                        const char *name, JSCFunction *func,
                                        int length, JSCFunctionEnum cproto, int magic,
                                        JSValueConst parent_ctor,
                                        const JSCFunctionListEntry *ctor_fields,
                                        int n_ctor_fields,
                                        const JSCFunctionListEntry *proto_fields,
                                        int n_proto_fields, int flags);

QJS_INTERNAL int check_function(JSContext *ctx, JSValueConst obj);

QJS_INTERNAL void js_function_set_properties(JSContext *ctx, JSValueConst func_obj, JSAtom name_atom, int length);
QJS_INTERNAL void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
QJS_INTERNAL void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func);
QJS_INTERNAL JSValue js_function_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv, int func_kind);
QJS_INTERNAL int JS_SetConstructor2(JSContext *ctx, JSValueConst func_obj, JSValueConst proto, int proto_flags, int ctor_flags);

#endif /* QJS_FUNCTION_H */

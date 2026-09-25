/*
 * QuickJS function and VM types
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
#ifndef QUICKJS_FUNCTION_H
#define QUICKJS_FUNCTION_H

#include "runtime.h"

typedef enum {
    JS_CLOSURE_LOCAL, /* 'var_idx' is the index of a local variable in the parent function */
    JS_CLOSURE_ARG, /* 'var_idx' is the index of a argument variable in the parent function */
    JS_CLOSURE_REF, /* 'var_idx' is the index of a closure variable in the parent function */
    JS_CLOSURE_GLOBAL_REF, /* 'var_idx' in the index of a closure
                              variable in the parent function
                              referencing a global variable */
    JS_CLOSURE_GLOBAL_DECL, /* global variable declaration (eval code only) */
    JS_CLOSURE_GLOBAL, /* global variable (eval code only) */
    JS_CLOSURE_MODULE_DECL, /* definition of a module variable (eval code only) */
    JS_CLOSURE_MODULE_IMPORT, /* definition of a module import (eval code only) */ 
} JSClosureTypeEnum;

typedef struct JSClosureVar {
    JSClosureTypeEnum closure_type : 3;
    uint8_t is_lexical : 1; /* lexical variable */
    uint8_t is_const : 1; /* const variable (is_lexical = 1 if is_const = 1 */
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    uint16_t var_idx; /* is_local = TRUE: index to a normal variable of the
                    parent function. otherwise: index to a closure
                    variable of the parent function */
    JSAtom var_name;
} JSClosureVar;

#define ARG_SCOPE_END (-2)

typedef enum {
    /* XXX: add more variable kinds here instead of using bit fields */
    JS_VAR_NORMAL,
    JS_VAR_FUNCTION_DECL, /* lexical var with function declaration */
    JS_VAR_NEW_FUNCTION_DECL, /* lexical var with async/generator
                                 function declaration */
    JS_VAR_CATCH,
    JS_VAR_FUNCTION_NAME, /* function expression name */
    JS_VAR_PRIVATE_FIELD,
    JS_VAR_PRIVATE_METHOD,
    JS_VAR_PRIVATE_GETTER,
    JS_VAR_PRIVATE_SETTER, /* must come after JS_VAR_PRIVATE_GETTER */
    JS_VAR_PRIVATE_GETTER_SETTER, /* must come after JS_VAR_PRIVATE_SETTER */
    JS_VAR_GLOBAL_FUNCTION_DECL, /* global function definition, only in JSVarDef */
} JSVarKindEnum;

typedef struct JSBytecodeVarDef {
    JSAtom var_name;
    /* index into JSFunctionBytecode.vars of the next variable in the same or
       enclosing lexical scope
    */
    int scope_next; /* XXX: store on 16 bits */
    uint8_t is_const : 1;
    uint8_t is_lexical : 1;
    uint8_t is_captured : 1; /* XXX: could remove and use a var_ref_idx value */
    uint8_t has_scope: 1; /* true if JSVarDef.scope_level != 0 */
    uint8_t var_kind : 4; /* see JSVarKindEnum */
    /* If is_captured = TRUE, provides, the index of the corresponding
       JSVarRef on stack. It would be more compact to have a separate
       table with the corresponding inverted table but it requires
       more modifications in the code. */
    uint16_t var_ref_idx;
} JSBytecodeVarDef;

/* for the encoding of the pc2line table */
#define PC2LINE_BASE     (-1)
#define PC2LINE_RANGE    5
#define PC2LINE_OP_FIRST 1

typedef enum JSFunctionKindEnum {
    JS_FUNC_NORMAL = 0,
    JS_FUNC_GENERATOR = (1 << 0),
    JS_FUNC_ASYNC = (1 << 1),
    JS_FUNC_ASYNC_GENERATOR = (JS_FUNC_GENERATOR | JS_FUNC_ASYNC),
} JSFunctionKindEnum;

typedef struct JSFunctionBytecode {
    JSGCObjectHeader header; /* must come first */
    uint8_t js_mode;
    uint8_t has_prototype : 1; /* true if a prototype field is necessary */
    uint8_t has_simple_parameter_list : 1;
    uint8_t is_derived_class_constructor : 1;
    /* true if home_object needs to be initialized */
    uint8_t need_home_object : 1;
    uint8_t func_kind : 2;
    uint8_t new_target_allowed : 1;
    uint8_t super_call_allowed : 1;
    uint8_t super_allowed : 1;
    uint8_t arguments_allowed : 1;
    uint8_t has_debug : 1;
    uint8_t read_only_bytecode : 1;
    uint8_t is_direct_or_indirect_eval : 1; /* used by JS_GetScriptOrModuleName() */
    /* XXX: 10 bits available */
    uint8_t *byte_code_buf; /* (self pointer) */
    int byte_code_len;
    JSAtom func_name;
    JSBytecodeVarDef *vardefs; /* arguments + local variables (arg_count + var_count) (self pointer) */
    JSClosureVar *closure_var; /* list of variables in the closure (self pointer) */
    uint16_t arg_count;
    uint16_t var_count;
    uint16_t defined_arg_count; /* for length function property */
    uint16_t stack_size; /* maximum stack size */
    uint16_t var_ref_count; /* number of local variable references */
    JSContext *realm; /* function realm */
    JSValue *cpool; /* constant pool (self pointer) */
    int cpool_count;
    int closure_var_count;
    struct {
        /* debug info, move to separate structure to save memory? */
        JSAtom filename;
        int source_len; 
        int pc2line_len;
        uint8_t *pc2line_buf;
        char *source;
    } debug;
} JSFunctionBytecode;

typedef struct JSBoundFunction {
    JSValue func_obj;
    JSValue this_val;
    int argc;
    JSValue argv[0];
} JSBoundFunction;

typedef struct JSForInIterator {
    JSValue obj;
    uint32_t idx;
    uint32_t atom_count;
    uint8_t in_prototype_chain;
    uint8_t is_array;
    JSPropertyEnum *tab_atom; /* is_array = FALSE */
} JSForInIterator;

JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                    int argc, JSValueConst *argv);
JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor,
                            int class_id);

int check_function(JSContext *ctx, JSValueConst obj);

JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func,
                         const char *name,
                         int length, JSCFunctionEnum cproto, int magic,
                         JSValueConst proto_val, int n_fields);

static inline BOOL is_strict_mode(JSContext *ctx)
{
    JSStackFrame *sf = ctx->rt->current_stack_frame;
    return (sf && (sf->js_mode & JS_MODE_STRICT));
}

JSValueConst JS_GetActiveFunction(JSContext *ctx);

BOOL JS_IsCFunction(JSContext *ctx, JSValueConst val, JSCFunction *func, int magic);

void free_var_ref(JSRuntime *rt, JSVarRef *var_ref);
JSValue js_closure2(JSContext *ctx, JSValue func_obj,
                    JSFunctionBytecode *b,
                    JSVarRef **cur_var_refs,
                    JSStackFrame *sf,
                    BOOL is_eval, JSModuleDef *m);
JSVarRef *js_create_var_ref(JSContext *ctx, BOOL is_lexical);
BOOL js_class_has_bytecode(JSClassID class_id);
JSValue js_closure(JSContext *ctx, JSValue bfunc,
                   JSVarRef **cur_var_refs,
                   JSStackFrame *sf, BOOL is_eval);


typedef struct JSCFunctionDataRecord {
    JSCFunctionData *func;
    uint8_t length;
    uint8_t data_len;
    uint16_t magic;
    JSValue data[0];
} JSCFunctionDataRecord;

JSFunctionBytecode *JS_GetFunctionBytecode(JSValueConst val);
void js_bound_function_finalizer(JSRuntime *rt, JSValue val);
void js_bound_function_mark(JSRuntime *rt, JSValueConst val,
                                JS_MarkFunc *mark_func);
JSValue js_c_function_data_call(JSContext *ctx, JSValueConst func_obj,
                                JSValueConst this_val,
                                int argc, JSValueConst *argv, int flags);
void js_c_function_data_finalizer(JSRuntime *rt, JSValue val);
void js_c_function_data_mark(JSRuntime *rt, JSValueConst val,
                             JS_MarkFunc *mark_func);
void js_c_function_finalizer(JSRuntime *rt, JSValue val);
void js_c_function_mark(JSRuntime *rt, JSValueConst val,
                        JS_MarkFunc *mark_func);
void js_method_set_home_object(JSContext *ctx, JSValueConst func_obj,
                               JSValueConst home_obj);
int js_method_set_properties(JSContext *ctx, JSValueConst func_obj,
                             JSAtom name, int flags, JSValueConst home_obj);
JSValue js_get_function_name(JSContext *ctx, JSAtom name);
void js_function_set_properties(JSContext *ctx, JSValueConst func_obj,
                                JSAtom name, int len);
JSValue JS_InvokeFree(JSContext *ctx, JSValue this_val, JSAtom atom,
                      int argc, JSValueConst *argv);

void js_bytecode_function_finalizer(JSRuntime *rt, JSValue val);
void js_bytecode_function_mark(JSRuntime *rt, JSValueConst val,
                               JS_MarkFunc *mark_func);


JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj);

JSValue js_instantiate_prototype(JSContext *ctx, JSObject *p, JSAtom atom, void *opaque);

extern const uint16_t func_kind_to_class_id[JS_FUNC_ASYNC_GENERATOR + 1];

JSValue js_call_bound_function(JSContext *ctx, JSValueConst func_obj,
                               JSValueConst this_obj,
                               int argc, JSValueConst *argv, int flags);

JSValue js_call_c_function(JSContext *ctx, JSValueConst func_obj,
                           JSValueConst this_obj,
                           int argc, JSValueConst *argv, int flags);

void js_mapped_arguments_finalizer(JSRuntime *rt, JSValue val);

void js_mapped_arguments_mark(JSRuntime *rt, JSValueConst val,
                              JS_MarkFunc *mark_func);

extern const JSClassExoticMethods js_arguments_exotic_methods;

#endif

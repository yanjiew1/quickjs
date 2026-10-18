/* Internal compiled-value representation.
 * Copyright (c) 2026 Yan-Jie Wang; SPDX-License-Identifier: MIT */
#ifndef QUICKJS_COMPILED_VALUE_H
#define QUICKJS_COMPILED_VALUE_H

#include "runtime.h"

struct JSFunctionBytecode;

/* Only bytecode and module records use this tag. Their allocation metadata
   supplies the native kind without exposing the allocator layout publicly. */
static inline BOOL js_is_bytecode_value(JSValueConst val)
{
    return JS_VALUE_GET_TAG(val) == JS_TAG_INTERNAL_GC &&
           js_rc(JS_VALUE_GET_PTR(val))->gc_obj_type ==
           JS_GC_OBJ_TYPE_FUNCTION_BYTECODE;
}

static inline BOOL js_is_module_value(JSValueConst val)
{
    return JS_VALUE_GET_TAG(val) == JS_TAG_INTERNAL_GC &&
           js_rc(JS_VALUE_GET_PTR(val))->gc_obj_type == JS_GC_OBJ_TYPE_MODULE;
}

/* These views do not retain the record. A caller either transfers an existing
   reference or borrows the view for an immediate marking/release operation. */
static inline JSValue js_make_bytecode_value(struct JSFunctionBytecode *b)
{
    assert(js_rc(b)->gc_obj_type == JS_GC_OBJ_TYPE_FUNCTION_BYTECODE);
    return JS_MKPTR(JS_TAG_INTERNAL_GC, b);
}

static inline JSValue js_make_module_value(JSModuleDef *m)
{
    assert(js_rc(m)->gc_obj_type == JS_GC_OBJ_TYPE_MODULE);
    return JS_MKPTR(JS_TAG_INTERNAL_GC, m);
}

#endif

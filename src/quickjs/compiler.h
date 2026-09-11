#ifndef QUICKJS_COMPILER_H
#define QUICKJS_COMPILER_H

#include "quickjs.h"
#include "quickjs/parser.h"

static inline void *js_realloc_bytecode_rt(void *opaque, void *ptr, size_t size)
{
    JSRuntime *rt = opaque;
    if (size > (INT32_MAX / 2)) {
        return NULL;
    } else {
        return js_realloc_rt(rt, ptr, size);
    }
}

static inline void js_dbuf_bytecode_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, js_realloc_bytecode_rt);
}

JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                        const char *input, size_t input_len,
                        const char *filename, int flags, int scope_idx);
JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                          const char *input, size_t input_len,
                          const char *filename, int flags, int scope_idx);
JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                      JSValueConst val, int flags, int scope_idx);
JSValue JS_EvalThis(JSContext *ctx, JSValueConst this_obj,
                    const char *input, size_t input_len,
                    const char *filename, int eval_flags);
void js_parse_init(JSContext *ctx, JSParseState *s,
                   const char *input, size_t input_len,
                   const char *filename);

#endif /* QUICKJS_COMPILER_H */

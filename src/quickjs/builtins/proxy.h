/*
 * QuickJS internal proxy interfaces
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
#ifndef QUICKJS_PRIVATE_BUILTIN_PROXY_H
#define QUICKJS_PRIVATE_BUILTIN_PROXY_H

typedef struct JSProxyData {
    JSValue target;
    JSValue handler;
    uint8_t is_func;
    uint8_t is_revoked;
} JSProxyData;

/* Internal implementation detail; not part of the public QuickJS API. */
int js_resolve_proxy(JSContext *ctx, JSValueConst *pval, BOOL throw_exception);

/* Internal implementation detail; not part of the public QuickJS API. */
JSValue JS_ThrowTypeErrorRevokedProxy(JSContext *ctx);

/* Internal implementation details; not part of the public QuickJS API. */
extern const JSCFunctionListEntry js_reflect_obj[1];
extern const JSCFunctionListEntry js_symbol_funcs[15];
extern const JSCFunctionListEntry js_symbol_proto_funcs[5];
JSValue js_symbol_constructor(JSContext *ctx, JSValueConst new_target,
                              int argc, JSValueConst *argv);

#endif /* QUICKJS_PRIVATE_BUILTIN_PROXY_H */

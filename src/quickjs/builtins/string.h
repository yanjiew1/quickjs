/*
 * QuickJS String Builtin Interface
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
#ifndef QUICKJS_BUILTINS_STRING_H
#define QUICKJS_BUILTINS_STRING_H

#include "internal/object.h"
#include "internal/string.h"

int64_t string_advance_index(JSString *p, int64_t index, BOOL unicode);

int string_indexof_char(JSString *p, int c, int from);

int js_string_GetSubstitution(JSContext *ctx,
                                     StringBuffer *b,
                                     JSValueConst matched,
                                     JSString *sp,
                                     uint32_t position,
                                     JSValueConst captures_val,
                                     JSValueConst namedCaptures,
                                     JSValueConst rep,
                                     uint8_t **captures,
                                     uint32_t captures_len);

JSValue js_string_constructor(JSContext *ctx, JSValueConst new_target,
                                     int argc, JSValueConst *argv);

int js_string_find_invalid_codepoint(JSString *p);

extern const JSClassExoticMethods js_string_exotic_methods;
extern const JSCFunctionListEntry js_string_funcs[3];
extern const JSCFunctionListEntry js_string_proto_funcs[50];
extern const JSCFunctionListEntry js_string_iterator_proto_funcs[2];

#endif /* QUICKJS_BUILTINS_STRING_H */

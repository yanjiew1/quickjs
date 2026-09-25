/*
 * QuickJS compiler internals shared by the parser and bytecode compiler
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
#ifndef QUICKJS_COMPILER_INTERNAL_H
#define QUICKJS_COMPILER_INTERNAL_H

#include "frontend-state.h"

#define OPTIMIZE         1
#define ARG_SCOPE_INDEX 1

static inline void dbuf_set_error(DynBuf *s)
{
    s->error = TRUE;
}

static inline int update_label(JSFunctionDef *s, int label, int delta)
{
    LabelSlot *ls;

    assert(label >= 0 && label < s->label_count);
    ls = &s->label_slots[label];
    ls->ref_count += delta;
    assert(ls->ref_count >= 0);
    return ls->ref_count;
}

int new_label_fd(JSFunctionDef *fd);
int find_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int add_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int add_func_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int add_arguments_var(JSContext *ctx, JSFunctionDef *fd);
int add_arguments_arg(JSContext *ctx, JSFunctionDef *fd);
JSAtom get_private_setter_name(JSContext *ctx, JSAtom name);

JSFunctionDef *js_new_function_def(JSContext *ctx, JSFunctionDef *parent,
                                   BOOL is_eval, BOOL is_func_expr,
                                   const char *filename,
                                   const uint8_t *source_ptr,
                                   GetLineColCache *get_line_col_cache);
void js_free_function_def(JSContext *ctx, JSFunctionDef *fd);
int add_closure_var(JSContext *ctx, JSFunctionDef *s,
                    JSClosureTypeEnum closure_type, int var_idx,
                    JSAtom var_name, BOOL is_const, BOOL is_lexical,
                    JSVarKindEnum var_kind);
__exception int add_closure_variables(JSContext *ctx, JSFunctionDef *s,
                                      JSFunctionBytecode *b, int scope_idx);
JSValue js_create_function(JSContext *ctx, JSFunctionDef *fd);

#endif

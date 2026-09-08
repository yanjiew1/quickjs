/*
 * QuickJS Javascript Engine: Codegen Definitions
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

#ifndef QUICKJS_COMPILER_JS_CODEGEN_H
#define QUICKJS_COMPILER_JS_CODEGEN_H

#include "quickjs-internal.h"
#include "cutils.h"
#include "compiler/js_opcode.h"
#include "compiler/js_lexer.h"
#include "compiler/js_parser.h"

static inline int get_prev_opcode(JSFunctionDef *fd) {
    if (fd->last_opcode_pos < 0 || dbuf_error(&fd->byte_code))
        return OP_invalid;
    else
        return fd->byte_code.buf[fd->last_opcode_pos];
}

static inline BOOL js_is_live_code(JSParseState *s) {
    switch (get_prev_opcode(s->cur_func)) {
    case OP_tail_call:
    case OP_tail_call_method:
    case OP_return:
    case OP_return_undef:
    case OP_return_async:
    case OP_throw:
    case OP_throw_error:
    case OP_goto:
#if SHORT_OPCODES
    case OP_goto8:
    case OP_goto16:
#endif
    case OP_ret:
        return FALSE;
    default:
        return TRUE;
    }
}

static inline void emit_u8(JSParseState *s, uint8_t val)
{
    dbuf_putc(&s->cur_func->byte_code, val);
}

static inline void emit_u16(JSParseState *s, uint16_t val)
{
    dbuf_put_u16(&s->cur_func->byte_code, val);
}

static inline void emit_u32(JSParseState *s, uint32_t val)
{
    dbuf_put_u32(&s->cur_func->byte_code, val);
}

void emit_source_pos(JSParseState *s, const uint8_t *source_ptr);
void emit_op(JSParseState *s, uint8_t val);
void emit_atom(JSParseState *s, JSAtom name);
int update_label(JSFunctionDef *s, int label, int delta);
int new_label_fd(JSFunctionDef *fd);
int new_label(JSParseState *s);
void emit_label_raw(JSParseState *s, int label);
int emit_label(JSParseState *s, int label);
int emit_goto(JSParseState *s, int opcode, int label);
int cpool_add(JSParseState *s, JSValue val);
__exception int emit_push_const(JSParseState *s, JSValueConst val, BOOL as_atom);

int find_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int find_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int find_var_in_scope(JSContext *ctx, JSFunctionDef *fd, JSAtom name, int scope_level);
JSGlobalVar *find_global_var(JSFunctionDef *fd, JSAtom name);
JSGlobalVar *find_lexical_global_var(JSFunctionDef *fd, JSAtom name);
int find_lexical_decl(JSContext *ctx, JSFunctionDef *fd, JSAtom name, int scope_idx, BOOL check_catch_var);
int push_scope(JSParseState *s);
void pop_scope(JSParseState *s);
void close_scopes(JSParseState *s, int scope, int scope_stop);
int add_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int add_scope_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name, JSVarKindEnum var_kind);
int add_func_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
int add_arguments_var(JSContext *ctx, JSFunctionDef *fd);
int add_arguments_arg(JSContext *ctx, JSFunctionDef *fd);
int add_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name);
JSGlobalVar *add_global_var(JSContext *ctx, JSFunctionDef *s, JSAtom name);
int define_var(JSParseState *s, JSFunctionDef *fd, JSAtom name, JSVarDefEnum var_def_type);
int add_private_class_field(JSParseState *s, JSFunctionDef *fd, JSAtom name, JSVarKindEnum var_kind, BOOL is_static);

void free_bytecode_atoms(JSRuntime *rt, const uint8_t *bc_buf, int bc_len, BOOL use_short_opcodes);
__exception int add_closure_variables(JSContext *ctx, JSFunctionDef *s,
                                      JSFunctionBytecode *b, int scope_idx);

JSFunctionDef *js_new_function_def(JSContext *ctx, JSFunctionDef *parent,
                                   BOOL is_eval, BOOL is_func_expr,
                                   const char *filename,
                                   const uint8_t *source_ptr,
                                   GetLineColCache *get_line_col_cache);
void js_free_function_def(JSContext *ctx, JSFunctionDef *fd);
JSValue js_create_function(JSContext *ctx, JSFunctionDef *fd);
int add_closure_var(JSContext *ctx, JSFunctionDef *s,
                    JSClosureTypeEnum closure_type,
                    int var_idx, JSAtom var_name,
                    BOOL is_const, BOOL is_lexical,
                    JSVarKindEnum var_kind);

#endif /* QUICKJS_COMPILER_JS_CODEGEN_H */

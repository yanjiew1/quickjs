/*
 * QuickJS parser and initial bytecode emission
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
#include "../internal/base.h"
#include "../internal/value-print.h"
#include "../internal/runtime.h"
#include "../internal/allocator.h"
#include "../internal/gc.h"
#include "../internal/number.h"
#include "../internal/string.h"
#include "../internal/object.h"
#include "../internal/error.h"
#include "../internal/module.h"
#include "../internal/bytecode.h"
#include "../internal/frontend.h"
#include "../internal/parse-state.h"
#include "compiler-internal.h"
#include "lexer.h"

/* JS parser */

struct BlockEnv {
    struct BlockEnv *prev;
    JSAtom label_name; /* JS_ATOM_NULL if none */
    int label_break; /* -1 if none */
    int label_cont; /* -1 if none */
    int drop_count; /* number of stack elements to drop */
    int label_finally; /* -1 if none */
    int scope_level;
    uint8_t has_iterator : 1;
    uint8_t is_regular_stmt : 1; /* i.e. not a loop statement */
};

typedef enum JSParseExportEnum {
    JS_PARSE_EXPORT_NONE,
    JS_PARSE_EXPORT_NAMED,
    JS_PARSE_EXPORT_DEFAULT,
} JSParseExportEnum;

static int js_parse_expect(JSParseState *s, int tok)
{
    if (s->token.val != tok) {
        /* XXX: dump token correctly in all cases */
        return js_parse_error(s, "expecting '%c'", tok);
    }
    return next_token(s);
}

static int js_parse_expect_semi(JSParseState *s)
{
    if (s->token.val != ';') {
        /* automatic insertion of ';' */
        if (s->token.val == TOK_EOF || s->token.val == '}' || s->got_lf) {
            return 0;
        }
        return js_parse_error(s, "expecting '%c'", ';');
    }
    return next_token(s);
}

static int js_parse_error_reserved_identifier(JSParseState *s)
{
    char buf1[ATOM_GET_STR_BUF_SIZE];
    return js_parse_error(s, "'%s' is a reserved identifier",
                          JS_AtomGetStr(s->ctx, buf1, sizeof(buf1),
                                        s->token.u.ident.atom));
}

static inline BOOL token_is_pseudo_keyword(JSParseState *s, JSAtom atom) {
    return s->token.val == TOK_IDENT && s->token.u.ident.atom == atom &&
        !s->token.u.ident.has_escape;
}

static inline int get_prev_opcode(JSFunctionDef *fd) {
    if (fd->last_opcode_pos < 0 || dbuf_error(&fd->byte_code))
        return OP_invalid;
    else
        return fd->byte_code.buf[fd->last_opcode_pos];
}

static BOOL js_is_live_code(JSParseState *s) {
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

static void emit_u8(JSParseState *s, uint8_t val)
{
    dbuf_putc(&s->cur_func->byte_code, val);
}

static void emit_u16(JSParseState *s, uint16_t val)
{
    dbuf_put_u16(&s->cur_func->byte_code, val);
}

static void emit_u32(JSParseState *s, uint32_t val)
{
    dbuf_put_u32(&s->cur_func->byte_code, val);
}

static void emit_source_pos(JSParseState *s, const uint8_t *source_ptr)
{
    JSFunctionDef *fd = s->cur_func;
    DynBuf *bc = &fd->byte_code;

    if (unlikely(fd->last_opcode_source_ptr != source_ptr)) {
        dbuf_putc(bc, OP_line_num);
        dbuf_put_u32(bc, source_ptr - s->buf_start);
        fd->last_opcode_source_ptr = source_ptr;
    }
}

static void emit_op(JSParseState *s, uint8_t val)
{
    JSFunctionDef *fd = s->cur_func;
    DynBuf *bc = &fd->byte_code;

    fd->last_opcode_pos = bc->size;
    dbuf_putc(bc, val);
}

static void emit_atom(JSParseState *s, JSAtom name)
{
    DynBuf *bc = &s->cur_func->byte_code;
    if (dbuf_claim(bc, 4))
        return; /* not enough memory : don't duplicate the atom */
    put_u32(bc->buf + bc->size, JS_DupAtom(s->ctx, name));
    bc->size += 4;
}

int new_label_fd(JSFunctionDef *fd)
{
    int label;
    LabelSlot *ls;

    if (js_resize_array(fd->ctx, (void *)&fd->label_slots,
                        sizeof(fd->label_slots[0]),
                        &fd->label_size, fd->label_count + 1))
        return -1;
    label = fd->label_count++;
    ls = &fd->label_slots[label];
    ls->ref_count = 0;
    ls->pos = -1;
    ls->pos2 = -1;
    ls->addr = -1;
    ls->first_reloc = NULL;
    return label;
}

static int new_label(JSParseState *s)
{
    int label;
    label = new_label_fd(s->cur_func);
    if (unlikely(label < 0)) {
        dbuf_set_error(&s->cur_func->byte_code);
    }
    return label;
}

/* don't update the last opcode and don't emit line number info */
static void emit_label_raw(JSParseState *s, int label)
{
    emit_u8(s, OP_label);
    emit_u32(s, label);
    s->cur_func->label_slots[label].pos = s->cur_func->byte_code.size;
}

/* return the label ID offset */
static int emit_label(JSParseState *s, int label)
{
    if (label >= 0) {
        emit_op(s, OP_label);
        emit_u32(s, label);
        s->cur_func->label_slots[label].pos = s->cur_func->byte_code.size;
        return s->cur_func->byte_code.size - 4;
    } else {
        return -1;
    }
}

/* return label or -1 if dead code */
static int emit_goto(JSParseState *s, int opcode, int label)
{
    if (js_is_live_code(s)) {
        if (label < 0) {
            label = new_label(s);
            if (label < 0)
                return -1;
        }
        emit_op(s, opcode);
        emit_u32(s, label);
        s->cur_func->label_slots[label].ref_count++;
        return label;
    }
    return -1;
}

/* return the constant pool index. 'val' is not duplicated. */
static int cpool_add(JSParseState *s, JSValue val)
{
    JSFunctionDef *fd = s->cur_func;

    if (js_resize_array(s->ctx, (void *)&fd->cpool, sizeof(fd->cpool[0]),
                        &fd->cpool_size, fd->cpool_count + 1)) {
        JS_FreeValue(s->ctx, val);
        return -1;
    }
    fd->cpool[fd->cpool_count++] = val;
    return fd->cpool_count - 1;
}

static __exception int emit_push_const(JSParseState *s, JSValueConst val,
                                       BOOL as_atom)
{
    int idx;

    if (JS_VALUE_GET_TAG(val) == JS_TAG_STRING && as_atom) {
        JSAtom atom;
        /* warning: JS_NewAtomStr frees the string value */
        JS_DupValue(s->ctx, val);
        atom = JS_NewAtomStr(s->ctx, JS_VALUE_GET_STRING(val));
        if (atom != JS_ATOM_NULL && !__JS_AtomIsTaggedInt(atom)) {
            emit_op(s, OP_push_atom_value);
            emit_u32(s, atom);
            return 0;
        }
    }

    idx = cpool_add(s, JS_DupValue(s->ctx, val));
    if (idx < 0)
        return -1;
    emit_op(s, OP_push_const);
    emit_u32(s, idx);
    return 0;
}

/* return the variable index or -1 if not found,
   add ARGUMENT_VAR_OFFSET for argument variables */
static int find_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    int i;
    for(i = fd->arg_count; i-- > 0;) {
        if (fd->args[i].var_name == name)
            return i | ARGUMENT_VAR_OFFSET;
    }
    return -1;
}

int find_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    int i;
    for(i = fd->var_count; i-- > 0;) {
        if (fd->vars[i].var_name == name && fd->vars[i].scope_level == 0)
            return i;
    }
    return find_arg(ctx, fd, name);
}

/* find a variable declaration in a given scope */
static int find_var_in_scope(JSContext *ctx, JSFunctionDef *fd,
                             JSAtom name, int scope_level)
{
    int scope_idx;
    for(scope_idx = fd->scopes[scope_level].first; scope_idx >= 0;
        scope_idx = fd->vars[scope_idx].scope_next) {
        if (fd->vars[scope_idx].scope_level != scope_level)
            break;
        if (fd->vars[scope_idx].var_name == name)
            return scope_idx;
    }
    return -1;
}

/* return true if scope == parent_scope or if scope is a child of
   parent_scope */
static BOOL is_child_scope(JSContext *ctx, JSFunctionDef *fd,
                           int scope, int parent_scope)
{
    while (scope >= 0) {
        if (scope == parent_scope)
            return TRUE;
        scope = fd->scopes[scope].parent;
    }
    return FALSE;
}

/* find a 'var' declaration in the same scope or a child scope */
static int find_var_in_child_scope(JSContext *ctx, JSFunctionDef *fd,
                                   JSAtom name, int scope_level)
{
    int i;
    for(i = 0; i < fd->var_count; i++) {
        JSVarDef *vd = &fd->vars[i];
        if (vd->var_name == name && vd->scope_level == 0) {
            if (is_child_scope(ctx, fd, vd->scope_next,
                               scope_level))
                return i;
        }
    }
    return -1;
}


static JSGlobalVar *find_global_var(JSFunctionDef *fd, JSAtom name)
{
    int i;
    for(i = 0; i < fd->global_var_count; i++) {
        JSGlobalVar *hf = &fd->global_vars[i];
        if (hf->var_name == name)
            return hf;
    }
    return NULL;

}

static JSGlobalVar *find_lexical_global_var(JSFunctionDef *fd, JSAtom name)
{
    JSGlobalVar *hf = find_global_var(fd, name);
    if (hf && hf->is_lexical)
        return hf;
    else
        return NULL;
}

static int find_lexical_decl(JSContext *ctx, JSFunctionDef *fd, JSAtom name,
                             int scope_idx, BOOL check_catch_var)
{
    while (scope_idx >= 0) {
        JSVarDef *vd = &fd->vars[scope_idx];
        if (vd->var_name == name &&
            (vd->is_lexical || (vd->var_kind == JS_VAR_CATCH &&
                                check_catch_var)))
            return scope_idx;
        scope_idx = vd->scope_next;
    }

    if (fd->is_eval && fd->eval_type == JS_EVAL_TYPE_GLOBAL) {
        if (find_lexical_global_var(fd, name))
            return GLOBAL_VAR_OFFSET;
    }
    return -1;
}

static int push_scope(JSParseState *s) {
    if (s->cur_func) {
        JSFunctionDef *fd = s->cur_func;
        int scope = fd->scope_count;
        /* XXX: should check for scope overflow */
        if ((fd->scope_count + 1) > fd->scope_size) {
            int new_size;
            size_t slack;
            JSVarScope *new_buf;
            /* XXX: potential arithmetic overflow */
            new_size = max_int(fd->scope_count + 1, fd->scope_size * 3 / 2);
            if (fd->scopes == fd->def_scope_array) {
                new_buf = js_realloc2(s->ctx, NULL, new_size * sizeof(*fd->scopes), &slack);
                if (!new_buf)
                    return -1;
                memcpy(new_buf, fd->scopes, fd->scope_count * sizeof(*fd->scopes));
            } else {
                new_buf = js_realloc2(s->ctx, fd->scopes, new_size * sizeof(*fd->scopes), &slack);
                if (!new_buf)
                    return -1;
            }
            new_size += slack / sizeof(*new_buf);
            fd->scopes = new_buf;
            fd->scope_size = new_size;
        }
        fd->scope_count++;
        fd->scopes[scope].parent = fd->scope_level;
        fd->scopes[scope].first = fd->scope_first;
        emit_op(s, OP_enter_scope);
        emit_u16(s, scope);
        return fd->scope_level = scope;
    }
    return 0;
}

static int get_first_lexical_var(JSFunctionDef *fd, int scope)
{
    while (scope >= 0) {
        int scope_idx = fd->scopes[scope].first;
        if (scope_idx >= 0)
            return scope_idx;
        scope = fd->scopes[scope].parent;
    }
    return -1;
}

static void pop_scope(JSParseState *s) {
    if (s->cur_func) {
        /* disable scoped variables */
        JSFunctionDef *fd = s->cur_func;
        int scope = fd->scope_level;
        emit_op(s, OP_leave_scope);
        emit_u16(s, scope);
        fd->scope_level = fd->scopes[scope].parent;
        fd->scope_first = get_first_lexical_var(fd, fd->scope_level);
    }
}

static void close_scopes(JSParseState *s, int scope, int scope_stop)
{
    while (scope > scope_stop) {
        emit_op(s, OP_leave_scope);
        emit_u16(s, scope);
        scope = s->cur_func->scopes[scope].parent;
    }
}

/* return the variable index or -1 if error */
int add_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    JSVarDef *vd;

    /* the local variable indexes are currently stored on 16 bits */
    if (fd->var_count >= JS_MAX_LOCAL_VARS) {
        JS_ThrowInternalError(ctx, "too many local variables");
        return -1;
    }
    if (js_resize_array(ctx, (void **)&fd->vars, sizeof(fd->vars[0]),
                        &fd->var_size, fd->var_count + 1))
        return -1;
    vd = &fd->vars[fd->var_count++];
    memset(vd, 0, sizeof(*vd));
    vd->var_name = JS_DupAtom(ctx, name);
    vd->func_pool_idx = -1;
    return fd->var_count - 1;
}

static int add_scope_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name,
                         JSVarKindEnum var_kind)
{
    int idx = add_var(ctx, fd, name);
    if (idx >= 0) {
        JSVarDef *vd = &fd->vars[idx];
        vd->var_kind = var_kind;
        vd->scope_level = fd->scope_level;
        vd->scope_next = fd->scope_first;
        fd->scopes[fd->scope_level].first = idx;
        fd->scope_first = idx;
    }
    return idx;
}

int add_func_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    int idx = fd->func_var_idx;
    if (idx < 0 && (idx = add_var(ctx, fd, name)) >= 0) {
        fd->func_var_idx = idx;
        fd->vars[idx].var_kind = JS_VAR_FUNCTION_NAME;
        if (fd->js_mode & JS_MODE_STRICT)
            fd->vars[idx].is_const = TRUE;
    }
    return idx;
}

int add_arguments_var(JSContext *ctx, JSFunctionDef *fd)
{
    int idx = fd->arguments_var_idx;
    if (idx < 0 && (idx = add_var(ctx, fd, JS_ATOM_arguments)) >= 0) {
        fd->arguments_var_idx = idx;
    }
    return idx;
}

/* add an argument definition in the argument scope. Only needed when
   "eval()" may be called in the argument scope. Return 0 if OK. */
int add_arguments_arg(JSContext *ctx, JSFunctionDef *fd)
{
    int idx;
    if (fd->arguments_arg_idx < 0) {
        idx = find_var_in_scope(ctx, fd, JS_ATOM_arguments, ARG_SCOPE_INDEX);
        if (idx < 0) {
            /* XXX: the scope links are not fully updated. May be an
               issue if there are child scopes of the argument
               scope */
            idx = add_var(ctx, fd, JS_ATOM_arguments);
            if (idx < 0)
                return -1;
            fd->vars[idx].scope_next = fd->scopes[ARG_SCOPE_INDEX].first;
            fd->scopes[ARG_SCOPE_INDEX].first = idx;
            fd->vars[idx].scope_level = ARG_SCOPE_INDEX;
            fd->vars[idx].is_lexical = TRUE;

            fd->arguments_arg_idx = idx;
        }
    }
    return 0;
}

static int add_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
{
    JSVarDef *vd;

    /* the local variable indexes are currently stored on 16 bits */
    if (fd->arg_count >= JS_MAX_LOCAL_VARS) {
        JS_ThrowInternalError(ctx, "too many arguments");
        return -1;
    }
    if (js_resize_array(ctx, (void **)&fd->args, sizeof(fd->args[0]),
                        &fd->arg_size, fd->arg_count + 1))
        return -1;
    vd = &fd->args[fd->arg_count++];
    memset(vd, 0, sizeof(*vd));
    vd->var_name = JS_DupAtom(ctx, name);
    vd->func_pool_idx = -1;
    return fd->arg_count - 1;
}

/* add a global variable definition */
static JSGlobalVar *add_global_var(JSContext *ctx, JSFunctionDef *s,
                                     JSAtom name)
{
    JSGlobalVar *hf;

    if (js_resize_array(ctx, (void **)&s->global_vars,
                        sizeof(s->global_vars[0]),
                        &s->global_var_size, s->global_var_count + 1))
        return NULL;
    hf = &s->global_vars[s->global_var_count++];
    hf->cpool_idx = -1;
    hf->force_init = FALSE;
    hf->is_lexical = FALSE;
    hf->is_const = FALSE;
    hf->scope_level = s->scope_level;
    hf->var_name = JS_DupAtom(ctx, name);
    return hf;
}

typedef enum {
    JS_VAR_DEF_WITH,
    JS_VAR_DEF_LET,
    JS_VAR_DEF_CONST,
    JS_VAR_DEF_FUNCTION_DECL, /* function declaration */
    JS_VAR_DEF_NEW_FUNCTION_DECL, /* async/generator function declaration */
    JS_VAR_DEF_CATCH,
    JS_VAR_DEF_VAR,
} JSVarDefEnum;

static int define_var(JSParseState *s, JSFunctionDef *fd, JSAtom name,
                      JSVarDefEnum var_def_type)
{
    JSContext *ctx = s->ctx;
    JSVarDef *vd;
    int idx;

    switch (var_def_type) {
    case JS_VAR_DEF_WITH:
        idx = add_scope_var(ctx, fd, name, JS_VAR_NORMAL);
        break;

    case JS_VAR_DEF_LET:
    case JS_VAR_DEF_CONST:
    case JS_VAR_DEF_FUNCTION_DECL:
    case JS_VAR_DEF_NEW_FUNCTION_DECL:
        idx = find_lexical_decl(ctx, fd, name, fd->scope_first, TRUE);
        if (idx >= 0) {
            if (idx < GLOBAL_VAR_OFFSET) {
                if (fd->vars[idx].scope_level == fd->scope_level) {
                    /* same scope: in non strict mode, functions
                       can be redefined (annex B.3.3.4). */
                    if (!(!(fd->js_mode & JS_MODE_STRICT) &&
                          var_def_type == JS_VAR_DEF_FUNCTION_DECL &&
                          fd->vars[idx].var_kind == JS_VAR_FUNCTION_DECL)) {
                        goto redef_lex_error;
                    }
                } else if (fd->vars[idx].var_kind == JS_VAR_CATCH && (fd->vars[idx].scope_level + 2) == fd->scope_level) {
                    goto redef_lex_error;
                }
            } else {
                if (fd->scope_level == fd->body_scope) {
                redef_lex_error:
                    /* redefining a scoped var in the same scope: error */
                    return js_parse_error(s, "invalid redefinition of lexical identifier");
                }
            }
        }
        if (var_def_type != JS_VAR_DEF_FUNCTION_DECL &&
            var_def_type != JS_VAR_DEF_NEW_FUNCTION_DECL &&
            fd->scope_level == fd->body_scope &&
            find_arg(ctx, fd, name) >= 0) {
            /* lexical variable redefines a parameter name */
            return js_parse_error(s, "invalid redefinition of parameter name");
        }

        if (find_var_in_child_scope(ctx, fd, name, fd->scope_level) >= 0) {
            return js_parse_error(s, "invalid redefinition of a variable");
        }

        if (fd->is_global_var) {
            JSGlobalVar *hf;
            hf = find_global_var(fd, name);
            if (hf && is_child_scope(ctx, fd, hf->scope_level,
                                     fd->scope_level)) {
                return js_parse_error(s, "invalid redefinition of global identifier");
            }
        }

        if (fd->is_eval &&
            (fd->eval_type == JS_EVAL_TYPE_GLOBAL ||
             fd->eval_type == JS_EVAL_TYPE_MODULE) &&
            fd->scope_level == fd->body_scope) {
            JSGlobalVar *hf;
            hf = add_global_var(s->ctx, fd, name);
            if (!hf)
                return -1;
            hf->is_lexical = TRUE;
            hf->is_const = (var_def_type == JS_VAR_DEF_CONST);
            idx = GLOBAL_VAR_OFFSET;
        } else {
            JSVarKindEnum var_kind;
            if (var_def_type == JS_VAR_DEF_FUNCTION_DECL)
                var_kind = JS_VAR_FUNCTION_DECL;
            else if (var_def_type == JS_VAR_DEF_NEW_FUNCTION_DECL)
                var_kind = JS_VAR_NEW_FUNCTION_DECL;
            else
                var_kind = JS_VAR_NORMAL;
            idx = add_scope_var(ctx, fd, name, var_kind);
            if (idx >= 0) {
                vd = &fd->vars[idx];
                vd->is_lexical = 1;
                vd->is_const = (var_def_type == JS_VAR_DEF_CONST);
            }
        }
        break;

    case JS_VAR_DEF_CATCH:
        idx = add_scope_var(ctx, fd, name, JS_VAR_CATCH);
        break;

    case JS_VAR_DEF_VAR:
        if (find_lexical_decl(ctx, fd, name, fd->scope_first,
                              FALSE) >= 0) {
       invalid_lexical_redefinition:
            /* error to redefine a var that inside a lexical scope */
            return js_parse_error(s, "invalid redefinition of lexical identifier");
        }
        if (fd->is_global_var) {
            JSGlobalVar *hf;
            hf = find_global_var(fd, name);
            if (hf && hf->is_lexical && hf->scope_level == fd->scope_level &&
                fd->eval_type == JS_EVAL_TYPE_MODULE) {
                goto invalid_lexical_redefinition;
            }
            hf = add_global_var(s->ctx, fd, name);
            if (!hf)
                return -1;
            idx = GLOBAL_VAR_OFFSET;
        } else {
            /* if the variable already exists, don't add it again  */
            idx = find_var(ctx, fd, name);
            if (idx >= 0)
                break;
            idx = add_var(ctx, fd, name);
            if (idx >= 0) {
                if (name == JS_ATOM_arguments && fd->has_arguments_binding)
                    fd->arguments_var_idx = idx;
                fd->vars[idx].scope_next = fd->scope_level;
            }
        }
        break;
    default:
        abort();
    }
    return idx;
}

/* add a private field variable in the current scope */
static int add_private_class_field(JSParseState *s, JSFunctionDef *fd,
                                   JSAtom name, JSVarKindEnum var_kind, BOOL is_static)
{
    JSContext *ctx = s->ctx;
    JSVarDef *vd;
    int idx;

    idx = add_scope_var(ctx, fd, name, var_kind);
    if (idx < 0)
        return idx;
    vd = &fd->vars[idx];
    vd->is_lexical = 1;
    vd->is_const = 1;
    vd->is_static_private = is_static;
    return idx;
}

static __exception int js_parse_expr(JSParseState *s);
static __exception int js_parse_function_decl(JSParseState *s,
                                              JSParseFunctionEnum func_type,
                                              JSFunctionKindEnum func_kind,
                                              JSAtom func_name, const uint8_t *ptr);
static JSFunctionDef *js_parse_function_class_fields_init(JSParseState *s);
static __exception int js_parse_function_decl2(JSParseState *s,
                                               JSParseFunctionEnum func_type,
                                               JSFunctionKindEnum func_kind,
                                               JSAtom func_name,
                                               const uint8_t *ptr,
                                               JSParseExportEnum export_flag,
                                               JSFunctionDef **pfd);
static __exception int js_parse_assign_expr2(JSParseState *s, int parse_flags);
static __exception int js_parse_assign_expr(JSParseState *s);
static __exception int js_parse_unary(JSParseState *s, int parse_flags);
static void push_break_entry(JSFunctionDef *fd, BlockEnv *be,
                             JSAtom label_name,
                             int label_break, int label_cont,
                             int drop_count);
static void pop_break_entry(JSFunctionDef *fd);
/* Note: all the fields are already sealed except length */
static int seal_template_obj(JSContext *ctx, JSValueConst obj)
{
    JSObject *p;
    JSShapeProperty *prs;

    p = JS_VALUE_GET_OBJ(obj);
    prs = find_own_property1(p, JS_ATOM_length);
    if (prs) {
        if (js_update_property_flags(ctx, p, &prs,
                                     prs->flags & ~(JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)))
            return -1;
    }
    p->extensible = FALSE;
    return 0;
}

static __exception int js_parse_template(JSParseState *s, int call, int *argc)
{
    JSContext *ctx = s->ctx;
    JSValue raw_array, template_object;
    JSToken cooked;
    int depth, ret;

    raw_array = JS_UNDEFINED; /* avoid warning */
    template_object = JS_UNDEFINED; /* avoid warning */
    if (call) {
        /* Create a template object: an array of cooked strings */
        /* Create an array of raw strings and store it to the raw property */
        template_object = JS_NewArray(ctx);
        if (JS_IsException(template_object))
            return -1;
        //        pool_idx = s->cur_func->cpool_count;
        ret = emit_push_const(s, template_object, 0);
        JS_FreeValue(ctx, template_object);
        if (ret)
            return -1;
        raw_array = JS_NewArray(ctx);
        if (JS_IsException(raw_array))
            return -1;
        if (JS_DefinePropertyValue(ctx, template_object, JS_ATOM_raw,
                                   raw_array, JS_PROP_THROW) < 0) {
            return -1;
        }
    }

    depth = 0;
    while (s->token.val == TOK_TEMPLATE) {
        const uint8_t *p = s->token.ptr + 1;
        cooked = s->token;
        if (call) {
            if (JS_DefinePropertyValueUint32(ctx, raw_array, depth,
                                             JS_DupValue(ctx, s->token.u.str.str),
                                             JS_PROP_ENUMERABLE | JS_PROP_THROW) < 0) {
                return -1;
            }
            /* re-parse the string with escape sequences but do not throw a
               syntax error if it contains invalid sequences
             */
            if (js_parse_string(s, '`', FALSE, p, &cooked, &p)) {
                cooked.u.str.str = JS_UNDEFINED;
            }
            if (JS_DefinePropertyValueUint32(ctx, template_object, depth,
                                             cooked.u.str.str,
                                             JS_PROP_ENUMERABLE | JS_PROP_THROW) < 0) {
                return -1;
            }
        } else {
            JSString *str;
            /* re-parse the string with escape sequences and throw a
               syntax error if it contains invalid sequences
             */
            JS_FreeValue(ctx, s->token.u.str.str);
            s->token.u.str.str = JS_UNDEFINED;
            if (js_parse_string(s, '`', TRUE, p, &cooked, &p))
                return -1;
            str = JS_VALUE_GET_STRING(cooked.u.str.str);
            if (str->len != 0 || depth == 0) {
                ret = emit_push_const(s, cooked.u.str.str, 1);
                JS_FreeValue(s->ctx, cooked.u.str.str);
                if (ret)
                    return -1;
                if (depth == 0) {
                    if (s->token.u.str.sep == '`')
                        goto done1;
                    emit_op(s, OP_get_field2);
                    emit_atom(s, JS_ATOM_concat);
                }
                depth++;
            } else {
                JS_FreeValue(s->ctx, cooked.u.str.str);
            }
        }
        if (s->token.u.str.sep == '`')
            goto done;
        if (next_token(s))
            return -1;
        if (js_parse_expr(s))
            return -1;
        depth++;
        if (s->token.val != '}') {
            return js_parse_error(s, "expected '}' after template expression");
        }
        /* XXX: should convert to string at this stage? */
        free_token(s, &s->token);
        /* Resume TOK_TEMPLATE parsing (s->token.line_num and
         * s->token.ptr are OK) */
        s->got_lf = FALSE;
        if (js_parse_template_part(s, s->buf_ptr))
            return -1;
    }
    return js_parse_expect(s, TOK_TEMPLATE);

 done:
    if (call) {
        /* Seal the objects */
        seal_template_obj(ctx, raw_array);
        seal_template_obj(ctx, template_object);
        *argc = depth + 1;
    } else {
        emit_op(s, OP_call_method);
        emit_u16(s, depth - 1);
    }
 done1:
    return next_token(s);
}


#define PROP_TYPE_IDENT 0
#define PROP_TYPE_VAR   1
#define PROP_TYPE_GET   2
#define PROP_TYPE_SET   3
#define PROP_TYPE_STAR  4
#define PROP_TYPE_ASYNC 5
#define PROP_TYPE_ASYNC_STAR 6

#define PROP_TYPE_PRIVATE (1 << 4)

static BOOL token_is_ident(int tok)
{
    /* Accept keywords and reserved words as property names */
    return (tok == TOK_IDENT ||
            (tok >= TOK_FIRST_KEYWORD &&
             tok <= TOK_LAST_KEYWORD));
}

/* if the property is an expression, name = JS_ATOM_NULL */
static int __exception js_parse_property_name(JSParseState *s,
                                              JSAtom *pname,
                                              BOOL allow_method, BOOL allow_var,
                                              BOOL allow_private)
{
    int is_private = 0;
    BOOL is_non_reserved_ident;
    JSAtom name;
    int prop_type;

    prop_type = PROP_TYPE_IDENT;
    if (allow_method) {
        /* if allow_private is true (for class field parsing) and
           get/set is following by ';' (or LF with ASI), then it
           is a field name */
        if ((token_is_pseudo_keyword(s, JS_ATOM_get) ||
             token_is_pseudo_keyword(s, JS_ATOM_set)) &&
            (!allow_private || peek_token(s, TRUE) != '\n')) {
            /* get x(), set x() */
            name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
            if (next_token(s))
                goto fail1;
            if (s->token.val == ':' || s->token.val == ',' ||
                s->token.val == '}' || s->token.val == '(' ||
                s->token.val == '=' ||
                (s->token.val == ';' && allow_private)) {
                is_non_reserved_ident = TRUE;
                goto ident_found;
            }
            prop_type = PROP_TYPE_GET + (name == JS_ATOM_set);
            JS_FreeAtom(s->ctx, name);
        } else if (s->token.val == '*') {
            if (next_token(s))
                goto fail;
            prop_type = PROP_TYPE_STAR;
        } else if (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                   peek_token(s, TRUE) != '\n') {
            name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
            if (next_token(s))
                goto fail1;
            if (s->token.val == ':' || s->token.val == ',' ||
                s->token.val == '}' || s->token.val == '(' ||
                s->token.val == '=') {
                is_non_reserved_ident = TRUE;
                goto ident_found;
            }
            JS_FreeAtom(s->ctx, name);
            if (s->token.val == '*') {
                if (next_token(s))
                    goto fail;
                prop_type = PROP_TYPE_ASYNC_STAR;
            } else {
                prop_type = PROP_TYPE_ASYNC;
            }
        }
    }

    if (token_is_ident(s->token.val)) {
        /* variable can only be a non-reserved identifier */
        is_non_reserved_ident =
            (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved);
        /* keywords and reserved words have a valid atom */
        name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
        if (next_token(s))
            goto fail1;
    ident_found:
        if (is_non_reserved_ident &&
            prop_type == PROP_TYPE_IDENT && allow_var) {
            if (!(s->token.val == ':' ||
                  (s->token.val == '(' && allow_method))) {
                prop_type = PROP_TYPE_VAR;
            }
        }
    } else if (s->token.val == TOK_STRING) {
        name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
        if (name == JS_ATOM_NULL)
            goto fail;
        if (next_token(s))
            goto fail1;
    } else if (s->token.val == TOK_NUMBER) {
        JSValue val;
        val = s->token.u.num.val;
        name = JS_ValueToAtom(s->ctx, val);
        if (name == JS_ATOM_NULL)
            goto fail;
        if (next_token(s))
            goto fail1;
    } else if (s->token.val == '[') {
        if (next_token(s))
            goto fail;
        if (js_parse_assign_expr(s))
            goto fail;
        if (js_parse_expect(s, ']'))
            goto fail;
        name = JS_ATOM_NULL;
    } else if (s->token.val == TOK_PRIVATE_NAME && allow_private) {
        name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
        if (next_token(s))
            goto fail1;
        is_private = PROP_TYPE_PRIVATE;
    } else {
        goto invalid_prop;
    }
    if (prop_type != PROP_TYPE_IDENT && prop_type != PROP_TYPE_VAR &&
        s->token.val != '(') {
        JS_FreeAtom(s->ctx, name);
    invalid_prop:
        js_parse_error(s, "invalid property name");
        goto fail;
    }
    *pname = name;
    return prop_type | is_private;
 fail1:
    JS_FreeAtom(s->ctx, name);
 fail:
    *pname = JS_ATOM_NULL;
    return -1;
}

typedef struct JSParsePos {
    BOOL got_lf;
    const uint8_t *ptr;
} JSParsePos;

static int js_parse_get_pos(JSParseState *s, JSParsePos *sp)
{
    sp->ptr = s->token.ptr;
    sp->got_lf = s->got_lf;
    return 0;
}

static __exception int js_parse_seek_token(JSParseState *s, const JSParsePos *sp)
{
    s->buf_ptr = sp->ptr;
    s->got_lf = sp->got_lf;
    return next_token(s);
}

/* return TRUE if a regexp literal is allowed after this token */
static BOOL is_regexp_allowed(int tok)
{
    switch (tok) {
    case TOK_NUMBER:
    case TOK_STRING:
    case TOK_REGEXP:
    case TOK_DEC:
    case TOK_INC:
    case TOK_NULL:
    case TOK_FALSE:
    case TOK_TRUE:
    case TOK_THIS:
    case ')':
    case ']':
    case '}': /* XXX: regexp may occur after */
    case TOK_IDENT:
        return FALSE;
    default:
        return TRUE;
    }
}

#define SKIP_HAS_SEMI       (1 << 0)
#define SKIP_HAS_ELLIPSIS   (1 << 1)
#define SKIP_HAS_ASSIGNMENT (1 << 2)

static BOOL has_lf_in_range(const uint8_t *p1, const uint8_t *p2)
{
    const uint8_t *tmp;
    if (p1 > p2) {
        tmp = p1;
        p1 = p2;
        p2 = tmp;
    }
    return (memchr(p1, '\n', p2 - p1) != NULL);
}

/* XXX: improve speed with early bailout */
/* XXX: no longer works if regexps are present. Could use previous
   regexp parsing heuristics to handle most cases */
static int js_parse_skip_parens_token(JSParseState *s, int *pbits, BOOL no_line_terminator)
{
    char state[256];
    size_t level = 0;
    JSParsePos pos;
    int last_tok, tok = TOK_EOF;
    int c, tok_len, bits = 0;
    const uint8_t *last_token_ptr;
    
    /* protect from underflow */
    state[level++] = 0;

    js_parse_get_pos(s, &pos);
    last_tok = 0;
    for (;;) {
        switch(s->token.val) {
        case '(':
        case '[':
        case '{':
            if (level >= sizeof(state))
                goto done;
            state[level++] = s->token.val;
            break;
        case ')':
            if (state[--level] != '(')
                goto done;
            break;
        case ']':
            if (state[--level] != '[')
                goto done;
            break;
        case '}':
            c = state[--level];
            if (c == '`') {
                /* continue the parsing of the template */
                free_token(s, &s->token);
                /* Resume TOK_TEMPLATE parsing (s->token.line_num and
                 * s->token.ptr are OK) */
                s->got_lf = FALSE;
                if (js_parse_template_part(s, s->buf_ptr))
                    goto done;
                goto handle_template;
            } else if (c != '{') {
                goto done;
            }
            break;
        case TOK_TEMPLATE:
        handle_template:
            if (s->token.u.str.sep != '`') {
                /* '${' inside the template : closing '}' and continue
                   parsing the template */
                if (level >= sizeof(state))
                    goto done;
                state[level++] = '`';
            }
            break;
        case TOK_EOF:
            goto done;
        case ';':
            if (level == 2) {
                bits |= SKIP_HAS_SEMI;
            }
            break;
        case TOK_ELLIPSIS:
            if (level == 2) {
                bits |= SKIP_HAS_ELLIPSIS;
            }
            break;
        case '=':
            bits |= SKIP_HAS_ASSIGNMENT;
            break;

        case TOK_DIV_ASSIGN:
            tok_len = 2;
            goto parse_regexp;
        case '/':
            tok_len = 1;
        parse_regexp:
            if (is_regexp_allowed(last_tok)) {
                s->buf_ptr -= tok_len;
                if (js_parse_regexp(s)) {
                    /* XXX: should clear the exception */
                    goto done;
                }
            }
            break;
        }
        /* last_tok is only used to recognize regexps */
        if (s->token.val == TOK_IDENT &&
            (token_is_pseudo_keyword(s, JS_ATOM_of) ||
             token_is_pseudo_keyword(s, JS_ATOM_yield))) {
            last_tok = TOK_OF;
        } else {
            last_tok = s->token.val;
        }
        last_token_ptr = s->token.ptr;
        if (next_token(s)) {
            /* XXX: should clear the exception generated by next_token() */
            break;
        }
        if (level <= 1) {
            tok = s->token.val;
            if (token_is_pseudo_keyword(s, JS_ATOM_of))
                tok = TOK_OF;
            if (no_line_terminator && has_lf_in_range(last_token_ptr, s->token.ptr))
                tok = '\n';
            break;
        }
    }
 done:
    if (pbits) {
        *pbits = bits;
    }
    if (js_parse_seek_token(s, &pos))
        return -1;
    return tok;
}

static void set_object_name(JSParseState *s, JSAtom name)
{
    JSFunctionDef *fd = s->cur_func;
    int opcode;

    opcode = get_prev_opcode(fd);
    if (opcode == OP_set_name) {
        /* XXX: should free atom after OP_set_name? */
        fd->byte_code.size = fd->last_opcode_pos;
        fd->last_opcode_pos = -1;
        emit_op(s, OP_set_name);
        emit_atom(s, name);
    } else if (opcode == OP_set_class_name) {
        int define_class_pos;
        JSAtom atom;
        define_class_pos = fd->last_opcode_pos + 1 -
            get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        assert(fd->byte_code.buf[define_class_pos] == OP_define_class);
        /* for consistency we free the previous atom which is
           JS_ATOM_empty_string */
        atom = get_u32(fd->byte_code.buf + define_class_pos + 1);
        JS_FreeAtom(s->ctx, atom);
        put_u32(fd->byte_code.buf + define_class_pos + 1,
                JS_DupAtom(s->ctx, name));
        fd->last_opcode_pos = -1;
    }
}

static void set_object_name_computed(JSParseState *s)
{
    JSFunctionDef *fd = s->cur_func;
    int opcode;

    opcode = get_prev_opcode(fd);
    if (opcode == OP_set_name) {
        /* XXX: should free atom after OP_set_name? */
        fd->byte_code.size = fd->last_opcode_pos;
        fd->last_opcode_pos = -1;
        emit_op(s, OP_set_name_computed);
    } else if (opcode == OP_set_class_name) {
        int define_class_pos;
        define_class_pos = fd->last_opcode_pos + 1 -
            get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        assert(fd->byte_code.buf[define_class_pos] == OP_define_class);
        fd->byte_code.buf[define_class_pos] = OP_define_class_computed;
        fd->last_opcode_pos = -1;
    }
}

static __exception int js_parse_object_literal(JSParseState *s)
{
    JSAtom name = JS_ATOM_NULL;
    const uint8_t *start_ptr;
    int prop_type;
    BOOL has_proto;

    if (next_token(s))
        goto fail;
    /* XXX: add an initial length that will be patched back */
    emit_op(s, OP_object);
    has_proto = FALSE;
    while (s->token.val != '}') {
        /* specific case for getter/setter */
        start_ptr = s->token.ptr;

        if (s->token.val == TOK_ELLIPSIS) {
            if (next_token(s))
                return -1;
            if (js_parse_assign_expr(s))
                return -1;
            emit_op(s, OP_null);  /* dummy excludeList */
            emit_op(s, OP_copy_data_properties);
            emit_u8(s, 2 | (1 << 2) | (0 << 5));
            emit_op(s, OP_drop); /* pop excludeList */
            emit_op(s, OP_drop); /* pop src object */
            goto next;
        }

        prop_type = js_parse_property_name(s, &name, TRUE, TRUE, FALSE);
        if (prop_type < 0)
            goto fail;

        if (prop_type == PROP_TYPE_VAR) {
            /* shortcut for x: x */
            emit_op(s, OP_scope_get_var);
            emit_atom(s, name);
            emit_u16(s, s->cur_func->scope_level);
            emit_op(s, OP_define_field);
            emit_atom(s, name);
        } else if (s->token.val == '(') {
            BOOL is_getset = (prop_type == PROP_TYPE_GET ||
                              prop_type == PROP_TYPE_SET);
            JSParseFunctionEnum func_type;
            JSFunctionKindEnum func_kind;
            int op_flags;

            func_kind = JS_FUNC_NORMAL;
            if (is_getset) {
                func_type = JS_PARSE_FUNC_GETTER + prop_type - PROP_TYPE_GET;
            } else {
                func_type = JS_PARSE_FUNC_METHOD;
                if (prop_type == PROP_TYPE_STAR)
                    func_kind = JS_FUNC_GENERATOR;
                else if (prop_type == PROP_TYPE_ASYNC)
                    func_kind = JS_FUNC_ASYNC;
                else if (prop_type == PROP_TYPE_ASYNC_STAR)
                    func_kind = JS_FUNC_ASYNC_GENERATOR;
            }
            if (js_parse_function_decl(s, func_type, func_kind, JS_ATOM_NULL,
                                       start_ptr))
                goto fail;
            if (name == JS_ATOM_NULL) {
                emit_op(s, OP_define_method_computed);
            } else {
                emit_op(s, OP_define_method);
                emit_atom(s, name);
            }
            if (is_getset) {
                op_flags = OP_DEFINE_METHOD_GETTER +
                    prop_type - PROP_TYPE_GET;
            } else {
                op_flags = OP_DEFINE_METHOD_METHOD;
            }
            emit_u8(s, op_flags | OP_DEFINE_METHOD_ENUMERABLE);
        } else {
            if (name == JS_ATOM_NULL) {
                /* must be done before evaluating expr */
                emit_op(s, OP_to_propkey);
            }
            if (js_parse_expect(s, ':'))
                goto fail;
            if (js_parse_assign_expr(s))
                goto fail;
            if (name == JS_ATOM_NULL) {
                set_object_name_computed(s);
                emit_op(s, OP_define_array_el);
                emit_op(s, OP_drop);
            } else if (name == JS_ATOM___proto__) {
                if (has_proto) {
                    js_parse_error(s, "duplicate __proto__ property name");
                    goto fail;
                }
                emit_op(s, OP_set_proto);
                has_proto = TRUE;
            } else {
                set_object_name(s, name);
                emit_op(s, OP_define_field);
                emit_atom(s, name);
            }
        }
        JS_FreeAtom(s->ctx, name);
    next:
        name = JS_ATOM_NULL;
        if (s->token.val != ',')
            break;
        if (next_token(s))
            goto fail;
    }
    if (js_parse_expect(s, '}'))
        goto fail;
    return 0;
 fail:
    JS_FreeAtom(s->ctx, name);
    return -1;
}

/* allow the 'in' binary operator */
#define PF_IN_ACCEPTED  (1 << 0)
/* allow function calls parsing in js_parse_postfix_expr() */
#define PF_POSTFIX_CALL (1 << 1)
/* allow the exponentiation operator in js_parse_unary() */
#define PF_POW_ALLOWED  (1 << 2)
/* forbid the exponentiation operator in js_parse_unary() */
#define PF_POW_FORBIDDEN (1 << 3)

static __exception int js_parse_postfix_expr(JSParseState *s, int parse_flags);
static void emit_class_field_init(JSParseState *s);
static void emit_return(JSParseState *s, BOOL hasval);

static __exception int js_parse_left_hand_side_expr(JSParseState *s)
{
    return js_parse_postfix_expr(s, PF_POSTFIX_CALL);
}

static __exception int js_parse_class_default_ctor(JSParseState *s,
                                                   BOOL has_super,
                                                   JSFunctionDef **pfd)
{
    JSParseFunctionEnum func_type;
    JSFunctionDef *fd = s->cur_func;
    int idx;

    fd = js_new_function_def(s->ctx, fd, FALSE, FALSE, s->filename,
                             s->token.ptr, &s->get_line_col_cache);
    if (!fd)
        return -1;

    s->cur_func = fd;
    fd->has_home_object = TRUE;
    fd->super_allowed = TRUE;
    fd->has_prototype = FALSE;
    fd->has_this_binding = TRUE;
    fd->new_target_allowed = TRUE;

    push_scope(s);  /* enter body scope */
    fd->body_scope = fd->scope_level;
    if (has_super) {
        fd->is_derived_class_constructor = TRUE;
        fd->super_call_allowed = TRUE;
        fd->arguments_allowed = TRUE;
        fd->has_arguments_binding = TRUE;
        func_type = JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR;
        emit_op(s, OP_init_ctor);
        // TODO(bnoordhuis) roll into OP_init_ctor
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, JS_ATOM_this);
        emit_u16(s, 0);
        emit_class_field_init(s);
    } else {
        func_type = JS_PARSE_FUNC_CLASS_CONSTRUCTOR;
        /* error if not invoked as a constructor */
        emit_op(s, OP_check_ctor);
        emit_class_field_init(s);
    }

    fd->func_kind = JS_FUNC_NORMAL;
    fd->func_type = func_type;
    emit_return(s, FALSE);

    s->cur_func = fd->parent;
    if (pfd)
        *pfd = fd;

    /* the real object will be set at the end of the compilation */
    idx = cpool_add(s, JS_NULL);
    fd->parent_cpool_idx = idx;

    return 0;
}

/* find field in the current scope */
static int find_private_class_field(JSContext *ctx, JSFunctionDef *fd,
                                    JSAtom name, int scope_level)
{
    int idx;
    idx = fd->scopes[scope_level].first;
    while (idx != -1) {
        if (fd->vars[idx].scope_level != scope_level)
            break;
        if (fd->vars[idx].var_name == name)
            return idx;
        idx = fd->vars[idx].scope_next;
    }
    return -1;
}

/* initialize the class fields, called by the constructor. Note:
   super() can be called in an arrow function, so <this> and
   <class_fields_init> can be variable references */
static void emit_class_field_init(JSParseState *s)
{
    int label_next;

    emit_op(s, OP_scope_get_var);
    emit_atom(s, JS_ATOM_class_fields_init);
    emit_u16(s, s->cur_func->scope_level);

    /* no need to call the class field initializer if not defined */
    emit_op(s, OP_dup);
    label_next = emit_goto(s, OP_if_false, -1);

    emit_op(s, OP_scope_get_var);
    emit_atom(s, JS_ATOM_this);
    emit_u16(s, 0);

    emit_op(s, OP_swap);

    emit_op(s, OP_call_method);
    emit_u16(s, 0);

    emit_label(s, label_next);
    emit_op(s, OP_drop);
}

/* build a private setter function name from the private getter name */
JSAtom get_private_setter_name(JSContext *ctx, JSAtom name)
{
    return js_atom_concat_str(ctx, name, "<set>");
}

typedef struct {
    JSFunctionDef *fields_init_fd;
    int computed_fields_count;
    BOOL need_brand;
    int brand_push_pos;
    BOOL is_static;
} ClassFieldsDef;

static __exception int emit_class_init_start(JSParseState *s,
                                             ClassFieldsDef *cf)
{
    int label_add_brand;

    cf->fields_init_fd = js_parse_function_class_fields_init(s);
    if (!cf->fields_init_fd)
        return -1;

    s->cur_func = cf->fields_init_fd;

    if (!cf->is_static) {
        /* add the brand to the newly created instance */
        /* XXX: would be better to add the code only if needed, maybe in a
           later pass */
        emit_op(s, OP_push_false); /* will be patched later */
        cf->brand_push_pos = cf->fields_init_fd->last_opcode_pos;
        label_add_brand = emit_goto(s, OP_if_false, -1);

        emit_op(s, OP_scope_get_var);
        emit_atom(s, JS_ATOM_this);
        emit_u16(s, 0);

        emit_op(s, OP_scope_get_var);
        emit_atom(s, JS_ATOM_home_object);
        emit_u16(s, 0);

        emit_op(s, OP_add_brand);

        emit_label(s, label_add_brand);
    }
    s->cur_func = s->cur_func->parent;
    return 0;
}

static void emit_class_init_end(JSParseState *s, ClassFieldsDef *cf)
{
    int cpool_idx;

    s->cur_func = cf->fields_init_fd;
    emit_op(s, OP_return_undef);
    s->cur_func = s->cur_func->parent;

    cpool_idx = cpool_add(s, JS_NULL);
    cf->fields_init_fd->parent_cpool_idx = cpool_idx;
    emit_op(s, OP_fclosure);
    emit_u32(s, cpool_idx);
    emit_op(s, OP_set_home_object);
}


static __exception int js_parse_class(JSParseState *s, BOOL is_class_expr,
                                      JSParseExportEnum export_flag)
{
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    JSAtom name = JS_ATOM_NULL, class_name = JS_ATOM_NULL, class_name1;
    JSAtom class_var_name = JS_ATOM_NULL;
    JSFunctionDef *method_fd, *ctor_fd;
    int saved_js_mode, class_name_var_idx, prop_type, ctor_cpool_offset;
    int class_flags = 0, i, define_class_offset;
    BOOL is_static, is_private;
    const uint8_t *class_start_ptr = s->token.ptr;
    const uint8_t *start_ptr;
    ClassFieldsDef class_fields[2];

    /* classes are parsed and executed in strict mode */
    saved_js_mode = fd->js_mode;
    fd->js_mode |= JS_MODE_STRICT;
    if (next_token(s))
        goto fail;
    if (s->token.val == TOK_IDENT) {
        if (s->token.u.ident.is_reserved) {
            js_parse_error_reserved_identifier(s);
            goto fail;
        }
        class_name = JS_DupAtom(ctx, s->token.u.ident.atom);
        if (next_token(s))
            goto fail;
    } else if (!is_class_expr && export_flag != JS_PARSE_EXPORT_DEFAULT) {
        js_parse_error(s, "class statement requires a name");
        goto fail;
    }
    if (!is_class_expr) {
        if (class_name == JS_ATOM_NULL)
            class_var_name = JS_ATOM__default_; /* export default */
        else
            class_var_name = class_name;
        class_var_name = JS_DupAtom(ctx, class_var_name);
    }

    push_scope(s);

    if (s->token.val == TOK_EXTENDS) {
        class_flags = JS_DEFINE_CLASS_HAS_HERITAGE;
        if (next_token(s))
            goto fail;
        if (js_parse_left_hand_side_expr(s))
            goto fail;
    } else {
        emit_op(s, OP_undefined);
    }

    /* add a 'const' definition for the class name */
    if (class_name != JS_ATOM_NULL) {
        class_name_var_idx = define_var(s, fd, class_name, JS_VAR_DEF_CONST);
        if (class_name_var_idx < 0)
            goto fail;
    }

    if (js_parse_expect(s, '{'))
        goto fail;

    /* this scope contains the private fields */
    push_scope(s);

    emit_op(s, OP_push_const);
    ctor_cpool_offset = fd->byte_code.size;
    emit_u32(s, 0); /* will be patched at the end of the class parsing */

    if (class_name == JS_ATOM_NULL) {
        if (class_var_name != JS_ATOM_NULL)
            class_name1 = JS_ATOM_default;
        else
            class_name1 = JS_ATOM_empty_string;
    } else {
        class_name1 = class_name;
    }

    emit_op(s, OP_define_class);
    emit_atom(s, class_name1);
    emit_u8(s, class_flags);
    define_class_offset = fd->last_opcode_pos;

    for(i = 0; i < 2; i++) {
        ClassFieldsDef *cf = &class_fields[i];
        cf->fields_init_fd = NULL;
        cf->computed_fields_count = 0;
        cf->need_brand = FALSE;
        cf->is_static = i;
    }

    ctor_fd = NULL;
    while (s->token.val != '}') {
        if (s->token.val == ';') {
            if (next_token(s))
                goto fail;
            continue;
        }
        is_static = FALSE;
        if (s->token.val == TOK_STATIC) {
            int next = peek_token(s, TRUE);
            if (!(next == ';' || next == '}' || next == '(' || next == '='))
                is_static = TRUE;
        }
        prop_type = -1;
        if (is_static) {
            if (next_token(s))
                goto fail;
            if (s->token.val == '{') {
                ClassFieldsDef *cf = &class_fields[is_static];
                JSFunctionDef *init;
                if (!cf->fields_init_fd) {
                    if (emit_class_init_start(s, cf))
                        goto fail;
                }
                s->cur_func = cf->fields_init_fd;
                /* XXX: could try to avoid creating a new function and
                   reuse 'fields_init_fd' with a specific 'var'
                   scope */
                // stack is now: <empty>
                if (js_parse_function_decl2(s, JS_PARSE_FUNC_CLASS_STATIC_INIT,
                                            JS_FUNC_NORMAL, JS_ATOM_NULL,
                                            s->token.ptr,
                                            JS_PARSE_EXPORT_NONE, &init) < 0) {
                    goto fail;
                }
                // stack is now: fclosure
                push_scope(s);
                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_this);
                emit_u16(s, 0);
                // stack is now: fclosure this
                emit_op(s, OP_swap);
                // stack is now: this fclosure
                emit_op(s, OP_call_method);
                emit_u16(s, 0);
                // stack is now: returnvalue
                emit_op(s, OP_drop);
                // stack is now: <empty>
                pop_scope(s);
                s->cur_func = s->cur_func->parent;
                continue;
            }
            /* allow "static" field name */
            if (s->token.val == ';' || s->token.val == '=') {
                is_static = FALSE;
                name = JS_DupAtom(ctx, JS_ATOM_static);
                prop_type = PROP_TYPE_IDENT;
            }
        }
        if (is_static)
            emit_op(s, OP_swap);
        start_ptr = s->token.ptr;
        if (prop_type < 0) {
            prop_type = js_parse_property_name(s, &name, TRUE, FALSE, TRUE);
            if (prop_type < 0)
                goto fail;
        }
        is_private = prop_type & PROP_TYPE_PRIVATE;
        prop_type &= ~PROP_TYPE_PRIVATE;

        if ((name == JS_ATOM_constructor && !is_static &&
             prop_type != PROP_TYPE_IDENT) ||
            (name == JS_ATOM_prototype && is_static) ||
            name == JS_ATOM_hash_constructor) {
            js_parse_error(s, "invalid method name");
            goto fail;
        }
        if (prop_type == PROP_TYPE_GET || prop_type == PROP_TYPE_SET) {
            BOOL is_set = prop_type - PROP_TYPE_GET;
            JSFunctionDef *method_fd;

            if (is_private) {
                int idx, var_kind, is_static1;
                idx = find_private_class_field(ctx, fd, name, fd->scope_level);
                if (idx >= 0) {
                    var_kind = fd->vars[idx].var_kind;
                    is_static1 = fd->vars[idx].is_static_private;
                    if (var_kind == JS_VAR_PRIVATE_FIELD ||
                        var_kind == JS_VAR_PRIVATE_METHOD ||
                        var_kind == JS_VAR_PRIVATE_GETTER_SETTER ||
                        var_kind == (JS_VAR_PRIVATE_GETTER + is_set) ||
                        (var_kind == (JS_VAR_PRIVATE_GETTER + 1 - is_set) &&
                         is_static != is_static1)) {
                        goto private_field_already_defined;
                    }
                    fd->vars[idx].var_kind = JS_VAR_PRIVATE_GETTER_SETTER;
                } else {
                    if (add_private_class_field(s, fd, name,
                                                JS_VAR_PRIVATE_GETTER + is_set, is_static) < 0)
                        goto fail;
                }
                class_fields[is_static].need_brand = TRUE;
            }

            if (js_parse_function_decl2(s, JS_PARSE_FUNC_GETTER + is_set,
                                        JS_FUNC_NORMAL, JS_ATOM_NULL,
                                        start_ptr,
                                        JS_PARSE_EXPORT_NONE, &method_fd))
                goto fail;
            if (is_private) {
                method_fd->need_home_object = TRUE; /* needed for brand check */
                emit_op(s, OP_set_home_object);
                /* XXX: missing function name */
                emit_op(s, OP_scope_put_var_init);
                if (is_set) {
                    JSAtom setter_name;
                    int ret;

                    setter_name = get_private_setter_name(ctx, name);
                    if (setter_name == JS_ATOM_NULL)
                        goto fail;
                    emit_atom(s, setter_name);
                    ret = add_private_class_field(s, fd, setter_name,
                                                  JS_VAR_PRIVATE_SETTER, is_static);
                    JS_FreeAtom(ctx, setter_name);
                    if (ret < 0)
                        goto fail;
                } else {
                    emit_atom(s, name);
                }
                emit_u16(s, s->cur_func->scope_level);
            } else {
                if (name == JS_ATOM_NULL) {
                    emit_op(s, OP_define_method_computed);
                } else {
                    emit_op(s, OP_define_method);
                    emit_atom(s, name);
                }
                emit_u8(s, OP_DEFINE_METHOD_GETTER + is_set);
            }
        } else if (prop_type == PROP_TYPE_IDENT && s->token.val != '(') {
            ClassFieldsDef *cf = &class_fields[is_static];
            JSAtom field_var_name = JS_ATOM_NULL;

            /* class field */

            /* XXX: spec: not consistent with method name checks */
            if (name == JS_ATOM_constructor || name == JS_ATOM_prototype) {
                js_parse_error(s, "invalid field name");
                goto fail;
            }

            if (is_private) {
                if (find_private_class_field(ctx, fd, name,
                                             fd->scope_level) >= 0) {
                    goto private_field_already_defined;
                }
                if (add_private_class_field(s, fd, name,
                                            JS_VAR_PRIVATE_FIELD, is_static) < 0)
                    goto fail;
                emit_op(s, OP_private_symbol);
                emit_atom(s, name);
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, name);
                emit_u16(s, s->cur_func->scope_level);
            }

            if (!cf->fields_init_fd) {
                if (emit_class_init_start(s, cf))
                    goto fail;
            }
            if (name == JS_ATOM_NULL ) {
                /* save the computed field name into a variable */
                field_var_name = js_atom_concat_num(ctx, JS_ATOM_computed_field + is_static, cf->computed_fields_count);
                if (field_var_name == JS_ATOM_NULL)
                    goto fail;
                if (define_var(s, fd, field_var_name, JS_VAR_DEF_CONST) < 0) {
                    JS_FreeAtom(ctx, field_var_name);
                    goto fail;
                }
                emit_op(s, OP_to_propkey);
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, field_var_name);
                emit_u16(s, s->cur_func->scope_level);
            }
            s->cur_func = cf->fields_init_fd;
            emit_op(s, OP_scope_get_var);
            emit_atom(s, JS_ATOM_this);
            emit_u16(s, 0);

            if (name == JS_ATOM_NULL) {
                emit_op(s, OP_scope_get_var);
                emit_atom(s, field_var_name);
                emit_u16(s, s->cur_func->scope_level);
                cf->computed_fields_count++;
                JS_FreeAtom(ctx, field_var_name);
            } else if (is_private) {
                emit_op(s, OP_scope_get_var);
                emit_atom(s, name);
                emit_u16(s, s->cur_func->scope_level);
            }

            if (s->token.val == '=') {
                if (next_token(s))
                    goto fail;
                if (js_parse_assign_expr(s))
                    goto fail;
            } else {
                emit_op(s, OP_undefined);
            }
            if (is_private) {
                set_object_name_computed(s);
                emit_op(s, OP_define_private_field);
            } else if (name == JS_ATOM_NULL) {
                set_object_name_computed(s);
                emit_op(s, OP_define_array_el);
                emit_op(s, OP_drop);
            } else {
                set_object_name(s, name);
                emit_op(s, OP_define_field);
                emit_atom(s, name);
            }
            s->cur_func = s->cur_func->parent;
            if (js_parse_expect_semi(s))
                goto fail;
        } else {
            JSParseFunctionEnum func_type;
            JSFunctionKindEnum func_kind;

            func_type = JS_PARSE_FUNC_METHOD;
            func_kind = JS_FUNC_NORMAL;
            if (prop_type == PROP_TYPE_STAR) {
                func_kind = JS_FUNC_GENERATOR;
            } else if (prop_type == PROP_TYPE_ASYNC) {
                func_kind = JS_FUNC_ASYNC;
            } else if (prop_type == PROP_TYPE_ASYNC_STAR) {
                func_kind = JS_FUNC_ASYNC_GENERATOR;
            } else if (name == JS_ATOM_constructor && !is_static) {
                if (ctor_fd) {
                    js_parse_error(s, "property constructor appears more than once");
                    goto fail;
                }
                if (class_flags & JS_DEFINE_CLASS_HAS_HERITAGE)
                    func_type = JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR;
                else
                    func_type = JS_PARSE_FUNC_CLASS_CONSTRUCTOR;
            }
            if (is_private) {
                class_fields[is_static].need_brand = TRUE;
            }
            if (js_parse_function_decl2(s, func_type, func_kind, JS_ATOM_NULL, start_ptr, JS_PARSE_EXPORT_NONE, &method_fd))
                goto fail;
            if (func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR ||
                func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR) {
                ctor_fd = method_fd;
            } else if (is_private) {
                method_fd->need_home_object = TRUE; /* needed for brand check */
                if (find_private_class_field(ctx, fd, name,
                                             fd->scope_level) >= 0) {
                private_field_already_defined:
                    js_parse_error(s, "private class field is already defined");
                    goto fail;
                }
                if (add_private_class_field(s, fd, name,
                                            JS_VAR_PRIVATE_METHOD, is_static) < 0)
                    goto fail;
                emit_op(s, OP_set_home_object);
                emit_op(s, OP_set_name);
                emit_atom(s, name);
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, name);
                emit_u16(s, s->cur_func->scope_level);
            } else {
                if (name == JS_ATOM_NULL) {
                    emit_op(s, OP_define_method_computed);
                } else {
                    emit_op(s, OP_define_method);
                    emit_atom(s, name);
                }
                emit_u8(s, OP_DEFINE_METHOD_METHOD);
            }
        }
        if (is_static)
            emit_op(s, OP_swap);
        JS_FreeAtom(ctx, name);
        name = JS_ATOM_NULL;
    }

    if (s->token.val != '}') {
        js_parse_error(s, "expecting '%c'", '}');
        goto fail;
    }

    if (!ctor_fd) {
        if (js_parse_class_default_ctor(s, class_flags & JS_DEFINE_CLASS_HAS_HERITAGE, &ctor_fd))
            goto fail;
    }
    /* patch the constant pool index for the constructor */
    put_u32(fd->byte_code.buf + ctor_cpool_offset, ctor_fd->parent_cpool_idx);

    /* store the class source code in the constructor. */
    if (!fd->strip_source) {
        js_free(ctx, ctor_fd->source);
        ctor_fd->source_len = s->buf_ptr - class_start_ptr;
        ctor_fd->source = js_strndup(ctx, (const char *)class_start_ptr,
                                     ctor_fd->source_len);
        if (!ctor_fd->source)
            goto fail;
    }

    /* consume the '}' */
    if (next_token(s))
        goto fail;

    {
        ClassFieldsDef *cf = &class_fields[0];
        int var_idx;

        if (cf->need_brand) {
            /* add a private brand to the prototype */
            emit_op(s, OP_dup);
            emit_op(s, OP_null);
            emit_op(s, OP_swap);
            emit_op(s, OP_add_brand);

            /* define the brand field in 'this' of the initializer */
            if (!cf->fields_init_fd) {
                if (emit_class_init_start(s, cf))
                    goto fail;
            }
            /* patch the start of the function to enable the
               OP_add_brand_instance code */
            cf->fields_init_fd->byte_code.buf[cf->brand_push_pos] = OP_push_true;
        }

        /* store the function to initialize the fields to that it can be
           referenced by the constructor */
        var_idx = define_var(s, fd, JS_ATOM_class_fields_init,
                             JS_VAR_DEF_CONST);
        if (var_idx < 0)
            goto fail;
        if (cf->fields_init_fd) {
            emit_class_init_end(s, cf);
        } else {
            emit_op(s, OP_undefined);
        }
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, JS_ATOM_class_fields_init);
        emit_u16(s, s->cur_func->scope_level);
    }

    /* drop the prototype */
    emit_op(s, OP_drop);

    if (class_fields[1].need_brand) {
        /* add a private brand to the class */
        emit_op(s, OP_dup);
        emit_op(s, OP_dup);
        emit_op(s, OP_add_brand);
    }

    if (class_name != JS_ATOM_NULL) {
        /* store the class name in the scoped class name variable (it
           is independent from the class statement variable
           definition) */
        emit_op(s, OP_dup);
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, class_name);
        emit_u16(s, fd->scope_level);
    }

    /* initialize the static fields */
    if (class_fields[1].fields_init_fd != NULL) {
        ClassFieldsDef *cf = &class_fields[1];
        emit_op(s, OP_dup);
        emit_class_init_end(s, cf);
        emit_op(s, OP_call_method);
        emit_u16(s, 0);
        emit_op(s, OP_drop);
    }

    pop_scope(s);
    pop_scope(s);

    /* the class statements have a block level scope */
    if (class_var_name != JS_ATOM_NULL) {
        if (define_var(s, fd, class_var_name, JS_VAR_DEF_LET) < 0)
            goto fail;
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, class_var_name);
        emit_u16(s, fd->scope_level);
    } else {
        if (class_name == JS_ATOM_NULL) {
            /* cannot use OP_set_name because the name of the class
               must be defined before the static initializers are
               executed */
            emit_op(s, OP_set_class_name);
            emit_u32(s, fd->last_opcode_pos + 1 - define_class_offset);
        }
    }

    if (export_flag != JS_PARSE_EXPORT_NONE) {
        if (!add_export_entry(s, fd->module,
                              class_var_name,
                              export_flag == JS_PARSE_EXPORT_NAMED ? class_var_name : JS_ATOM_default,
                              JS_EXPORT_TYPE_LOCAL))
            goto fail;
    }

    JS_FreeAtom(ctx, class_name);
    JS_FreeAtom(ctx, class_var_name);
    fd->js_mode = saved_js_mode;
    return 0;
 fail:
    JS_FreeAtom(ctx, name);
    JS_FreeAtom(ctx, class_name);
    JS_FreeAtom(ctx, class_var_name);
    fd->js_mode = saved_js_mode;
    return -1;
}

static __exception int js_parse_array_literal(JSParseState *s)
{
    uint32_t idx;
    BOOL need_length;

    if (next_token(s))
        return -1;
    /* small regular arrays are created on the stack */
    idx = 0;
    while (s->token.val != ']' && idx < 32) {
        if (s->token.val == ',' || s->token.val == TOK_ELLIPSIS)
            break;
        if (js_parse_assign_expr(s))
            return -1;
        idx++;
        /* accept trailing comma */
        if (s->token.val == ',') {
            if (next_token(s))
                return -1;
        } else
        if (s->token.val != ']')
            goto done;
    }
    emit_op(s, OP_array_from);
    emit_u16(s, idx);

    /* larger arrays and holes are handled with explicit indices */
    need_length = FALSE;
    while (s->token.val != ']' && idx < 0x7fffffff) {
        if (s->token.val == TOK_ELLIPSIS)
            break;
        need_length = TRUE;
        if (s->token.val != ',') {
            if (js_parse_assign_expr(s))
                return -1;
            emit_op(s, OP_define_field);
            emit_u32(s, __JS_AtomFromUInt32(idx));
            need_length = FALSE;
        }
        idx++;
        /* accept trailing comma */
        if (s->token.val == ',') {
            if (next_token(s))
                return -1;
        }
    }
    if (s->token.val == ']') {
        if (need_length) {
            /* Set the length: Cannot use OP_define_field because
               length is not configurable */
            emit_op(s, OP_dup);
            emit_op(s, OP_push_i32);
            emit_u32(s, idx);
            emit_op(s, OP_put_field);
            emit_atom(s, JS_ATOM_length);
        }
        goto done;
    }

    /* huge arrays and spread elements require a dynamic index on the stack */
    emit_op(s, OP_push_i32);
    emit_u32(s, idx);

    /* stack has array, index */
    while (s->token.val != ']') {
        if (s->token.val == TOK_ELLIPSIS) {
            if (next_token(s))
                return -1;
            if (js_parse_assign_expr(s))
                return -1;
#if 1
            emit_op(s, OP_append);
#else
            int label_next, label_done;
            label_next = new_label(s);
            label_done = new_label(s);
            /* enumerate object */
            emit_op(s, OP_for_of_start);
            emit_op(s, OP_rot5l);
            emit_op(s, OP_rot5l);
            emit_label(s, label_next);
            /* on stack: enum_rec array idx */
            emit_op(s, OP_for_of_next);
            emit_u8(s, 2);
            emit_goto(s, OP_if_true, label_done);
            /* append element */
            /* enum_rec array idx val -> enum_rec array new_idx */
            emit_op(s, OP_define_array_el);
            emit_op(s, OP_inc);
            emit_goto(s, OP_goto, label_next);
            emit_label(s, label_done);
            /* close enumeration */
            emit_op(s, OP_drop); /* drop undef val */
            emit_op(s, OP_nip1); /* drop enum_rec */
            emit_op(s, OP_nip1);
            emit_op(s, OP_nip1);
#endif
        } else {
            need_length = TRUE;
            if (s->token.val != ',') {
                if (js_parse_assign_expr(s))
                    return -1;
                /* a idx val */
                emit_op(s, OP_define_array_el);
                need_length = FALSE;
            }
            emit_op(s, OP_inc);
        }
        if (s->token.val != ',')
            break;
        if (next_token(s))
            return -1;
    }
    if (need_length) {
        /* Set the length: cannot use OP_define_field because
           length is not configurable */
        emit_op(s, OP_dup1);    /* array length - array array length */
        emit_op(s, OP_put_field);
        emit_atom(s, JS_ATOM_length);
    } else {
        emit_op(s, OP_drop);    /* array length - array */
    }
done:
    return js_parse_expect(s, ']');
}

/* check if scope chain contains a with statement */
static BOOL has_with_scope(JSFunctionDef *s, int scope_level)
{
    while (s) {
        /* no with in strict mode */
        if (!(s->js_mode & JS_MODE_STRICT)) {
            int scope_idx = s->scopes[scope_level].first;
            while (scope_idx >= 0) {
                JSVarDef *vd = &s->vars[scope_idx];
                if (vd->var_name == JS_ATOM__with_)
                    return TRUE;
                scope_idx = vd->scope_next;
            }
        }
        /* check parent scopes */
        scope_level = s->parent_scope_level;
        s = s->parent;
    }
    return FALSE;
}

static __exception int get_lvalue(JSParseState *s, int *popcode, int *pscope,
                                  JSAtom *pname, int *plabel, int *pdepth, BOOL keep,
                                  int tok)
{
    JSFunctionDef *fd;
    int opcode, scope, label, depth;
    JSAtom name;

    /* we check the last opcode to get the lvalue type */
    fd = s->cur_func;
    scope = 0;
    name = JS_ATOM_NULL;
    label = -1;
    depth = 0;
    switch(opcode = get_prev_opcode(fd)) {
    case OP_scope_get_var:
        name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        scope = get_u16(fd->byte_code.buf + fd->last_opcode_pos + 5);
        if ((name == JS_ATOM_arguments || name == JS_ATOM_eval) &&
            (fd->js_mode & JS_MODE_STRICT)) {
            return js_parse_error(s, "invalid lvalue in strict mode");
        }
        if (name == JS_ATOM_this || name == JS_ATOM_new_target)
            goto invalid_lvalue;
        if (has_with_scope(fd, scope)) {
            depth = 2;  /* will generate OP_get_ref_value */
        } else {
            depth = 0;
        }
        break;
    case OP_get_field:
        name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        depth = 1;
        break;
    case OP_scope_get_private_field:
        name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        scope = get_u16(fd->byte_code.buf + fd->last_opcode_pos + 5);
        depth = 1;
        break;
    case OP_get_array_el:
        depth = 2;
        break;
    case OP_get_super_value:
        depth = 3;
        break;
    default:
    invalid_lvalue:
        if (tok == TOK_FOR) {
            return js_parse_error(s, "invalid for in/of left hand-side");
        } else if (tok == TOK_INC || tok == TOK_DEC) {
            return js_parse_error(s, "invalid increment/decrement operand");
        } else if (tok == '[' || tok == '{') {
            return js_parse_error(s, "invalid destructuring target");
        } else {
            return js_parse_error(s, "invalid assignment left-hand side");
        }
    }
    /* remove the last opcode */
    fd->byte_code.size = fd->last_opcode_pos;
    fd->last_opcode_pos = -1;

    if (keep) {
        /* get the value but keep the object/fields on the stack */
        switch(opcode) {
        case OP_scope_get_var:
            if (depth != 0) {
                label = new_label(s);
                if (label < 0)
                    return -1;
                emit_op(s, OP_scope_make_ref);
                emit_atom(s, name);
                emit_u32(s, label);
                emit_u16(s, scope);
                update_label(fd, label, 1);
                emit_op(s, OP_get_ref_value);
                opcode = OP_get_ref_value;
            } else {
                emit_op(s, OP_scope_get_var);
                emit_atom(s, name);
                emit_u16(s, scope);
            }
            break;
        case OP_get_field:
            emit_op(s, OP_get_field2);
            emit_atom(s, name);
            break;
        case OP_scope_get_private_field:
            emit_op(s, OP_scope_get_private_field2);
            emit_atom(s, name);
            emit_u16(s, scope);
            break;
        case OP_get_array_el:
            emit_op(s, OP_get_array_el3);
            break;
        case OP_get_super_value:
            emit_op(s, OP_to_propkey);
            emit_op(s, OP_dup3);
            emit_op(s, OP_get_super_value);
            break;
        default:
            abort();
        }
    } else {
        switch(opcode) {
        case OP_scope_get_var:
            if (depth != 0) {
                label = new_label(s);
                if (label < 0)
                    return -1;
                emit_op(s, OP_scope_make_ref);
                emit_atom(s, name);
                emit_u32(s, label);
                emit_u16(s, scope);
                update_label(fd, label, 1);
                opcode = OP_get_ref_value;
            }
            break;
        default:
            break;
        }
    }

    *popcode = opcode;
    *pscope = scope;
    /* name has refcount for OP_get_field and OP_get_ref_value,
       and JS_ATOM_NULL for other opcodes */
    *pname = name;
    *plabel = label;
    if (pdepth)
        *pdepth = depth;
    return 0;
}

typedef enum {
    PUT_LVALUE_NOKEEP, /* [depth] v -> */
    PUT_LVALUE_NOKEEP_DEPTH, /* [depth] v -> , keep depth (currently
                                just disable optimizations) */
    PUT_LVALUE_KEEP_TOP,  /* [depth] v -> v */
    PUT_LVALUE_KEEP_SECOND, /* [depth] v0 v -> v0 */
    PUT_LVALUE_NOKEEP_BOTTOM, /* v [depth] -> */
} PutLValueEnum;

/* name has a live reference. 'is_let' is only used with opcode =
   OP_scope_get_var which is never generated by get_lvalue(). */
static void put_lvalue(JSParseState *s, int opcode, int scope,
                       JSAtom name, int label, PutLValueEnum special,
                       BOOL is_let)
{
    switch(opcode) {
    case OP_scope_get_var:
        /* depth = 0 */
        switch(special) {
        case PUT_LVALUE_NOKEEP:
        case PUT_LVALUE_NOKEEP_DEPTH:
        case PUT_LVALUE_KEEP_SECOND:
        case PUT_LVALUE_NOKEEP_BOTTOM:
            break;
        case PUT_LVALUE_KEEP_TOP:
            emit_op(s, OP_dup);
            break;
        default:
            abort();
        }
        break;
    case OP_get_field:
    case OP_scope_get_private_field:
        /* depth = 1 */
        switch(special) {
        case PUT_LVALUE_NOKEEP:
        case PUT_LVALUE_NOKEEP_DEPTH:
            break;
        case PUT_LVALUE_KEEP_TOP:
            emit_op(s, OP_insert2); /* obj v -> v obj v */
            break;
        case PUT_LVALUE_KEEP_SECOND:
            emit_op(s, OP_perm3); /* obj v0 v -> v0 obj v */
            break;
        case PUT_LVALUE_NOKEEP_BOTTOM:
            emit_op(s, OP_swap);
            break;
        default:
            abort();
        }
        break;
    case OP_get_array_el:
    case OP_get_ref_value:
        /* depth = 2 */
        if (opcode == OP_get_ref_value) {
            JS_FreeAtom(s->ctx, name);
            emit_label(s, label);
        }
        switch(special) {
        case PUT_LVALUE_NOKEEP:
            emit_op(s, OP_nop); /* will trigger optimization */
            break;
        case PUT_LVALUE_NOKEEP_DEPTH:
            break;
        case PUT_LVALUE_KEEP_TOP:
            emit_op(s, OP_insert3); /* obj prop v -> v obj prop v */
            break;
        case PUT_LVALUE_KEEP_SECOND:
            emit_op(s, OP_perm4); /* obj prop v0 v -> v0 obj prop v */
            break;
        case PUT_LVALUE_NOKEEP_BOTTOM:
            emit_op(s, OP_rot3l);
            break;
        default:
            abort();
        }
        break;
    case OP_get_super_value:
        /* depth = 3 */
        switch(special) {
        case PUT_LVALUE_NOKEEP:
        case PUT_LVALUE_NOKEEP_DEPTH:
            break;
        case PUT_LVALUE_KEEP_TOP:
            emit_op(s, OP_insert4); /* this obj prop v -> v this obj prop v */
            break;
        case PUT_LVALUE_KEEP_SECOND:
            emit_op(s, OP_perm5); /* this obj prop v0 v -> v0 this obj prop v */
            break;
        case PUT_LVALUE_NOKEEP_BOTTOM:
            emit_op(s, OP_rot4l);
            break;
        default:
            abort();
        }
        break;
    default:
        break;
    }

    switch(opcode) {
    case OP_scope_get_var:  /* val -- */
        emit_op(s, is_let ? OP_scope_put_var_init : OP_scope_put_var);
        emit_u32(s, name);  /* has refcount */
        emit_u16(s, scope);
        break;
    case OP_get_field:
        emit_op(s, OP_put_field);
        emit_u32(s, name);  /* name has refcount */
        break;
    case OP_scope_get_private_field:
        emit_op(s, OP_scope_put_private_field);
        emit_u32(s, name);  /* name has refcount */
        emit_u16(s, scope);
        break;
    case OP_get_array_el:
        emit_op(s, OP_put_array_el);
        break;
    case OP_get_ref_value:
        emit_op(s, OP_put_ref_value);
        break;
    case OP_get_super_value:
        emit_op(s, OP_put_super_value);
        break;
    default:
        abort();
    }
}

static __exception int js_parse_expr_paren(JSParseState *s)
{
    if (js_parse_expect(s, '('))
        return -1;
    if (js_parse_expr(s))
        return -1;
    if (js_parse_expect(s, ')'))
        return -1;
    return 0;
}

static int js_unsupported_keyword(JSParseState *s, JSAtom atom)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return js_parse_error(s, "unsupported keyword: %s",
                          JS_AtomGetStr(s->ctx, buf, sizeof(buf), atom));
}

static __exception int js_define_var(JSParseState *s, JSAtom name, int tok)
{
    JSFunctionDef *fd = s->cur_func;
    JSVarDefEnum var_def_type;

    if (name == JS_ATOM_yield && fd->func_kind == JS_FUNC_GENERATOR) {
        return js_parse_error(s, "yield is a reserved identifier");
    }
    if ((name == JS_ATOM_arguments || name == JS_ATOM_eval)
    &&  (fd->js_mode & JS_MODE_STRICT)) {
        return js_parse_error(s, "invalid variable name in strict mode");
    }
    if (name == JS_ATOM_let
    &&  (tok == TOK_LET || tok == TOK_CONST)) {
        return js_parse_error(s, "invalid lexical variable name");
    }
    switch(tok) {
    case TOK_LET:
        var_def_type = JS_VAR_DEF_LET;
        break;
    case TOK_CONST:
        var_def_type = JS_VAR_DEF_CONST;
        break;
    case TOK_VAR:
        var_def_type = JS_VAR_DEF_VAR;
        break;
    case TOK_CATCH:
        var_def_type = JS_VAR_DEF_CATCH;
        break;
    default:
        abort();
    }
    if (define_var(s, fd, name, var_def_type) < 0)
        return -1;
    return 0;
}

static void js_emit_spread_code(JSParseState *s, int depth)
{
    int label_rest_next, label_rest_done;

    /* XXX: could check if enum object is an actual array and optimize
       slice extraction. enumeration record and target array are in a
       different order from OP_append case. */
    /* enum_rec xxx -- enum_rec xxx array 0 */
    emit_op(s, OP_array_from);
    emit_u16(s, 0);
    emit_op(s, OP_push_i32);
    emit_u32(s, 0);
    emit_label(s, label_rest_next = new_label(s));
    emit_op(s, OP_for_of_next);
    emit_u8(s, 2 + depth);
    label_rest_done = emit_goto(s, OP_if_true, -1);
    /* array idx val -- array idx */
    emit_op(s, OP_define_array_el);
    emit_op(s, OP_inc);
    emit_goto(s, OP_goto, label_rest_next);
    emit_label(s, label_rest_done);
    /* enum_rec xxx array idx undef -- enum_rec xxx array */
    emit_op(s, OP_drop);
    emit_op(s, OP_drop);
}

static int js_parse_check_duplicate_parameter(JSParseState *s, JSAtom name)
{
    /* Check for duplicate parameter names */
    JSFunctionDef *fd = s->cur_func;
    int i;
    for (i = 0; i < fd->arg_count; i++) {
        if (fd->args[i].var_name == name)
            goto duplicate;
    }
    for (i = 0; i < fd->var_count; i++) {
        if (fd->vars[i].var_name == name)
            goto duplicate;
    }
    return 0;

duplicate:
    return js_parse_error(s, "duplicate parameter names not allowed in this context");
}

/* tok = TOK_VAR, TOK_LET or TOK_CONST. Return whether a reference
   must be taken to the variable for proper 'with' or global variable
   evaluation */
/* Note: this function is needed only because variable references are
   not yet optimized in destructuring */
static BOOL need_var_reference(JSParseState *s, int tok)
{
    JSFunctionDef *fd = s->cur_func;
    if (tok != TOK_VAR)
        return FALSE; /* no reference for let/const */
    if (fd->js_mode & JS_MODE_STRICT) {
        if (!fd->is_global_var)
            return FALSE; /* local definitions in strict mode in function or direct eval */
        if (s->is_module)
            return FALSE; /* in a module global variables are like closure variables */
    }
    return TRUE;
}

static JSAtom js_parse_destructuring_var(JSParseState *s, int tok, int is_arg)
{
    JSAtom name;

    if (!(s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)
    ||  ((s->cur_func->js_mode & JS_MODE_STRICT) &&
         (s->token.u.ident.atom == JS_ATOM_eval || s->token.u.ident.atom == JS_ATOM_arguments))) {
        js_parse_error(s, "invalid destructuring target");
        return JS_ATOM_NULL;
    }
    name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
    if (is_arg && js_parse_check_duplicate_parameter(s, name))
        goto fail;
    if (next_token(s))
        goto fail;

    return name;
fail:
    JS_FreeAtom(s->ctx, name);
    return JS_ATOM_NULL;
}

/* Return -1 if error, 0 if no initializer, 1 if an initializer is
   present at the top level. */
static int js_parse_destructuring_element(JSParseState *s, int tok, int is_arg,
                                        int hasval, int has_ellipsis,
                                        BOOL allow_initializer, BOOL export_flag)
{
    int label_parse, label_assign, label_done, label_lvalue, depth_lvalue;
    int start_addr, assign_addr;
    JSAtom prop_name, var_name;
    int opcode, scope, tok1, skip_bits;
    BOOL has_initializer;

    if (has_ellipsis < 0) {
        /* pre-parse destructuration target for spread detection */
        js_parse_skip_parens_token(s, &skip_bits, FALSE);
        has_ellipsis = skip_bits & SKIP_HAS_ELLIPSIS;
    }

    label_parse = new_label(s);
    label_assign = new_label(s);

    start_addr = s->cur_func->byte_code.size;
    if (hasval) {
        /* consume value from the stack */
        emit_op(s, OP_dup);
        emit_op(s, OP_undefined);
        emit_op(s, OP_strict_eq);
        emit_goto(s, OP_if_true, label_parse);
        emit_label(s, label_assign);
    } else {
        emit_goto(s, OP_goto, label_parse);
        emit_label(s, label_assign);
        /* leave value on the stack */
        emit_op(s, OP_dup);
    }
    assign_addr = s->cur_func->byte_code.size;
    if (s->token.val == '{') {
        if (next_token(s))
            return -1;
        /* throw an exception if the value cannot be converted to an object */
        emit_op(s, OP_to_object);
        if (has_ellipsis) {
            /* add excludeList on stack just below src object */
            emit_op(s, OP_object);
            emit_op(s, OP_swap);
        }
        while (s->token.val != '}') {
            int prop_type;
            if (s->token.val == TOK_ELLIPSIS) {
                if (!has_ellipsis) {
                    JS_ThrowInternalError(s->ctx, "unexpected ellipsis token");
                    return -1;
                }
                if (next_token(s))
                    return -1;
                if (tok) {
                    var_name = js_parse_destructuring_var(s, tok, is_arg);
                    if (var_name == JS_ATOM_NULL)
                        return -1;
                    if (need_var_reference(s, tok)) {
                        /* Must make a reference for proper `with` semantics */
                        emit_op(s, OP_scope_get_var);
                        emit_atom(s, var_name);
                        emit_u16(s, s->cur_func->scope_level);
                        JS_FreeAtom(s->ctx, var_name);
                        goto lvalue0;
                    } else {
                        opcode = OP_scope_get_var;
                        scope = s->cur_func->scope_level;
                        label_lvalue = -1;
                        depth_lvalue = 0;
                    }
                } else {
                    if (js_parse_left_hand_side_expr(s))
                        return -1;
                lvalue0:
                    if (get_lvalue(s, &opcode, &scope, &var_name,
                                   &label_lvalue, &depth_lvalue, FALSE, '{'))
                        return -1;
                }
                if (s->token.val != '}') {
                    js_parse_error(s, "assignment rest property must be last");
                    goto var_error;
                }
                emit_op(s, OP_object);  /* target */
                emit_op(s, OP_copy_data_properties);
                emit_u8(s, 0 | ((depth_lvalue + 1) << 2) | ((depth_lvalue + 2) << 5));
                goto set_val;
            }
            prop_type = js_parse_property_name(s, &prop_name, FALSE, TRUE, FALSE);
            if (prop_type < 0)
                return -1;
            var_name = JS_ATOM_NULL;
            if (prop_type == PROP_TYPE_IDENT) {
                if (next_token(s))
                    goto prop_error;
                if ((s->token.val == '[' || s->token.val == '{')
                    &&  ((tok1 = js_parse_skip_parens_token(s, &skip_bits, FALSE)) == ',' ||
                         tok1 == '=' || tok1 == '}')) {
                    if (prop_name == JS_ATOM_NULL) {
                        /* computed property name on stack */
                        if (has_ellipsis) {
                            /* define the property in excludeList */
                            emit_op(s, OP_to_propkey); /* avoid calling ToString twice */
                            emit_op(s, OP_perm3); /* TOS: src excludeList prop */
                            emit_op(s, OP_null); /* TOS: src excludeList prop null */
                            emit_op(s, OP_define_array_el); /* TOS: src excludeList prop */
                            emit_op(s, OP_perm3); /* TOS: excludeList src prop */
                        }
                        /* get the computed property from the source object */
                        emit_op(s, OP_get_array_el2);
                    } else {
                        /* named property */
                        if (has_ellipsis) {
                            /* define the property in excludeList */
                            emit_op(s, OP_swap); /* TOS: src excludeList */
                            emit_op(s, OP_null); /* TOS: src excludeList null */
                            emit_op(s, OP_define_field); /* TOS: src excludeList */
                            emit_atom(s, prop_name);
                            emit_op(s, OP_swap); /* TOS: excludeList src */
                        }
                        /* get the named property from the source object */
                        emit_op(s, OP_get_field2);
                        emit_u32(s, prop_name);
                    }
                    if (js_parse_destructuring_element(s, tok, is_arg, TRUE, -1, TRUE, export_flag) < 0)
                        return -1;
                    if (s->token.val == '}')
                        break;
                    /* accept a trailing comma before the '}' */
                    if (js_parse_expect(s, ','))
                        return -1;
                    continue;
                }
                if (prop_name == JS_ATOM_NULL) {
                    emit_op(s, OP_to_propkey);
                    if (has_ellipsis) {
                        /* define the property in excludeList */
                        emit_op(s, OP_perm3);
                        emit_op(s, OP_null);
                        emit_op(s, OP_define_array_el);
                        emit_op(s, OP_perm3);
                    }
                    /* source prop -- source source prop */
                    emit_op(s, OP_dup1);
                } else {
                    if (has_ellipsis) {
                        /* define the property in excludeList */
                        emit_op(s, OP_swap);
                        emit_op(s, OP_null);
                        emit_op(s, OP_define_field);
                        emit_atom(s, prop_name);
                        emit_op(s, OP_swap);
                    }
                    /* source -- source source */
                    emit_op(s, OP_dup);
                }
                if (tok) {
                    var_name = js_parse_destructuring_var(s, tok, is_arg);
                    if (var_name == JS_ATOM_NULL)
                        goto prop_error;
                    if (need_var_reference(s, tok)) {
                        /* Must make a reference for proper `with` semantics */
                        emit_op(s, OP_scope_get_var);
                        emit_atom(s, var_name);
                        emit_u16(s, s->cur_func->scope_level);
                        JS_FreeAtom(s->ctx, var_name);
                        goto lvalue1;
                    } else {
                        /* no need to make a reference for let/const */
                        opcode = OP_scope_get_var;
                        scope = s->cur_func->scope_level;
                        label_lvalue = -1;
                        depth_lvalue = 0;
                    }
                } else {
                    if (js_parse_left_hand_side_expr(s))
                        goto prop_error;
                lvalue1:
                    if (get_lvalue(s, &opcode, &scope, &var_name,
                                   &label_lvalue, &depth_lvalue, FALSE, '{'))
                        goto prop_error;
                    /* swap ref and lvalue object if any */
                    if (prop_name == JS_ATOM_NULL) {
                        switch(depth_lvalue) {
                        case 0:
                            break;
                        case 1:
                            /* source prop x -> x source prop */
                            emit_op(s, OP_rot3r);
                            break;
                        case 2:
                            /* source prop x y -> x y source prop */
                            emit_op(s, OP_swap2);   /* t p2 s p1 */
                            break;
                        case 3:
                            /* source prop x y z -> x y z source prop */
                            emit_op(s, OP_rot5l);
                            emit_op(s, OP_rot5l);
                            break;
                        default:
                            abort();
                        }
                    } else {
                        switch(depth_lvalue) {
                        case 0:
                            break;
                        case 1:
                            /* source x -> x source */
                            emit_op(s, OP_swap);
                            break;
                        case 2:
                            /* source x y -> x y source */
                            emit_op(s, OP_rot3l);
                            break;
                        case 3:
                            /* source x y z -> x y z source */
                            emit_op(s, OP_rot4l);
                            break;
                        default:
                            abort();
                        }
                    }
                }
                if (prop_name == JS_ATOM_NULL) {
                    /* computed property name on stack */
                    /* XXX: should have OP_get_array_el2x with depth */
                    /* source prop -- val */
                    emit_op(s, OP_get_array_el);
                } else {
                    /* named property */
                    /* XXX: should have OP_get_field2x with depth */
                    /* source -- val */
                    emit_op(s, OP_get_field);
                    emit_u32(s, prop_name);
                }
            } else {
                /* prop_type = PROP_TYPE_VAR, cannot be a computed property */
                if (is_arg && js_parse_check_duplicate_parameter(s, prop_name))
                    goto prop_error;
                if ((s->cur_func->js_mode & JS_MODE_STRICT) &&
                    (prop_name == JS_ATOM_eval || prop_name == JS_ATOM_arguments)) {
                    js_parse_error(s, "invalid destructuring target");
                    goto prop_error;
                }
                if (has_ellipsis) {
                    /* define the property in excludeList */
                    emit_op(s, OP_swap);
                    emit_op(s, OP_null);
                    emit_op(s, OP_define_field);
                    emit_atom(s, prop_name);
                    emit_op(s, OP_swap);
                }
                if (!tok || need_var_reference(s, tok)) {
                    /* generate reference */
                    /* source -- source source */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_scope_get_var);
                    emit_atom(s, prop_name);
                    emit_u16(s, s->cur_func->scope_level);
                    goto lvalue1;
                } else {
                    /* no need to make a reference for let/const */
                    var_name = JS_DupAtom(s->ctx, prop_name);
                    opcode = OP_scope_get_var;
                    scope = s->cur_func->scope_level;
                    label_lvalue = -1;
                    depth_lvalue = 0;
                    
                    /* source -- source val */
                    emit_op(s, OP_get_field2);
                    emit_u32(s, prop_name);
                }
            }
        set_val:
            if (tok) {
                if (js_define_var(s, var_name, tok))
                    goto var_error;
                if (export_flag) {
                    if (!add_export_entry(s, s->cur_func->module, var_name, var_name,
                                          JS_EXPORT_TYPE_LOCAL))
                        goto var_error;
                }
                scope = s->cur_func->scope_level; /* XXX: check */
            }
            if (s->token.val == '=') {  /* handle optional default value */
                int label_hasval;
                emit_op(s, OP_dup);
                emit_op(s, OP_undefined);
                emit_op(s, OP_strict_eq);
                label_hasval = emit_goto(s, OP_if_false, -1);
                if (next_token(s))
                    goto var_error;
                emit_op(s, OP_drop);
                if (js_parse_assign_expr(s))
                    goto var_error;
                if (opcode == OP_scope_get_var || opcode == OP_get_ref_value)
                    set_object_name(s, var_name);
                emit_label(s, label_hasval);
            }
            /* store value into lvalue object */
            put_lvalue(s, opcode, scope, var_name, label_lvalue,
                       PUT_LVALUE_NOKEEP_DEPTH,
                       (tok == TOK_CONST || tok == TOK_LET));
            if (s->token.val == '}')
                break;
            /* accept a trailing comma before the '}' */
            if (js_parse_expect(s, ','))
                return -1;
        }
        /* drop the source object */
        emit_op(s, OP_drop);
        if (has_ellipsis) {
            emit_op(s, OP_drop); /* pop excludeList */
        }
        if (next_token(s))
            return -1;
    } else if (s->token.val == '[') {
        BOOL has_spread;
        int enum_depth;
        BlockEnv block_env;

        if (next_token(s))
            return -1;
        /* the block environment is only needed in generators in case
           'yield' triggers a 'return' */
        push_break_entry(s->cur_func, &block_env,
                         JS_ATOM_NULL, -1, -1, 2);
        block_env.has_iterator = TRUE;
        emit_op(s, OP_for_of_start);
        has_spread = FALSE;
        while (s->token.val != ']') {
            /* get the next value */
            if (s->token.val == TOK_ELLIPSIS) {
                if (next_token(s))
                    return -1;
                if (s->token.val == ',' || s->token.val == ']')
                    return js_parse_error(s, "missing binding pattern...");
                has_spread = TRUE;
            }
            if (s->token.val == ',') {
                /* do nothing, skip the value, has_spread is false */
                emit_op(s, OP_for_of_next);
                emit_u8(s, 0);
                emit_op(s, OP_drop);
                emit_op(s, OP_drop);
            } else if ((s->token.val == '[' || s->token.val == '{')
                   &&  ((tok1 = js_parse_skip_parens_token(s, &skip_bits, FALSE)) == ',' ||
                        tok1 == '=' || tok1 == ']')) {
                if (has_spread) {
                    if (tok1 == '=')
                        return js_parse_error(s, "rest element cannot have a default value");
                    js_emit_spread_code(s, 0);
                } else {
                    emit_op(s, OP_for_of_next);
                    emit_u8(s, 0);
                    emit_op(s, OP_drop);
                }
                if (js_parse_destructuring_element(s, tok, is_arg, TRUE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE, export_flag) < 0)
                    return -1;
            } else {
                var_name = JS_ATOM_NULL;
                if (tok) {
                    var_name = js_parse_destructuring_var(s, tok, is_arg);
                    if (var_name == JS_ATOM_NULL)
                        goto var_error;
                    if (js_define_var(s, var_name, tok))
                        goto var_error;
                    if (need_var_reference(s, tok)) {
                        /* Must make a reference for proper `with` semantics */
                        emit_op(s, OP_scope_get_var);
                        emit_atom(s, var_name);
                        emit_u16(s, s->cur_func->scope_level);
                        JS_FreeAtom(s->ctx, var_name);
                        goto lvalue2;
                    } else {
                        /* no need to make a reference for let/const */
                        opcode = OP_scope_get_var;
                        scope = s->cur_func->scope_level;
                        label_lvalue = -1;
                        enum_depth = 0;
                    }
                } else {
                    if (js_parse_left_hand_side_expr(s))
                        return -1;
                lvalue2:
                    if (get_lvalue(s, &opcode, &scope, &var_name,
                                   &label_lvalue, &enum_depth, FALSE, '[')) {
                        return -1;
                    }
                }
                if (has_spread) {
                    js_emit_spread_code(s, enum_depth);
                } else {
                    emit_op(s, OP_for_of_next);
                    emit_u8(s, enum_depth);
                    emit_op(s, OP_drop);
                }
                if (s->token.val == '=' && !has_spread) {
                    /* handle optional default value */
                    int label_hasval;
                    emit_op(s, OP_dup);
                    emit_op(s, OP_undefined);
                    emit_op(s, OP_strict_eq);
                    label_hasval = emit_goto(s, OP_if_false, -1);
                    if (next_token(s))
                        goto var_error;
                    emit_op(s, OP_drop);
                    if (js_parse_assign_expr(s))
                        goto var_error;
                    if (opcode == OP_scope_get_var || opcode == OP_get_ref_value)
                        set_object_name(s, var_name);
                    emit_label(s, label_hasval);
                }
                /* store value into lvalue object */
                put_lvalue(s, opcode, scope, var_name,
                           label_lvalue, PUT_LVALUE_NOKEEP_DEPTH,
                           (tok == TOK_CONST || tok == TOK_LET));
            }
            if (s->token.val == ']')
                break;
            if (has_spread)
                return js_parse_error(s, "rest element must be the last one");
            /* accept a trailing comma before the ']' */
            if (js_parse_expect(s, ','))
                return -1;
        }
        /* close iterator object:
           if completed, enum_obj has been replaced by undefined */
        emit_op(s, OP_iterator_close);
        pop_break_entry(s->cur_func);
        if (next_token(s))
            return -1;
    } else {
        return js_parse_error(s, "invalid assignment syntax");
    }
    if (s->token.val == '=' && allow_initializer) {
        label_done = emit_goto(s, OP_goto, -1);
        if (next_token(s))
            return -1;
        emit_label(s, label_parse);
        if (hasval)
            emit_op(s, OP_drop);
        if (js_parse_assign_expr(s))
            return -1;
        emit_goto(s, OP_goto, label_assign);
        emit_label(s, label_done);
        has_initializer = TRUE;
    } else {
        /* normally hasval is true except if
           js_parse_skip_parens_token() was wrong in the parsing */
        //        assert(hasval);
        if (!hasval) {
            js_parse_error(s, "too complicated destructuring expression");
            return -1;
        }
        /* remove test and decrement label ref count */
        memset(s->cur_func->byte_code.buf + start_addr, OP_nop,
               assign_addr - start_addr);
        s->cur_func->label_slots[label_parse].ref_count--;
        has_initializer = FALSE;
    }
    return has_initializer;

 prop_error:
    JS_FreeAtom(s->ctx, prop_name);
 var_error:
    JS_FreeAtom(s->ctx, var_name);
    return -1;
}

typedef enum FuncCallType {
    FUNC_CALL_NORMAL,
    FUNC_CALL_NEW,
    FUNC_CALL_SUPER_CTOR,
    FUNC_CALL_TEMPLATE,
} FuncCallType;

static void optional_chain_test(JSParseState *s, int *poptional_chaining_label,
                                int drop_count)
{
    int label_next, i;
    if (*poptional_chaining_label < 0)
        *poptional_chaining_label = new_label(s);
   /* XXX: could be more efficient with a specific opcode */
    emit_op(s, OP_dup);
    emit_op(s, OP_is_undefined_or_null);
    label_next = emit_goto(s, OP_if_false, -1);
    for(i = 0; i < drop_count; i++)
        emit_op(s, OP_drop);
    emit_op(s, OP_undefined);
    emit_goto(s, OP_goto, *poptional_chaining_label);
    emit_label(s, label_next);
}

/* allowed parse_flags: PF_POSTFIX_CALL */
static __exception int js_parse_postfix_expr(JSParseState *s, int parse_flags)
{
    FuncCallType call_type;
    int optional_chaining_label;
    BOOL accept_lparen = (parse_flags & PF_POSTFIX_CALL) != 0;
    const uint8_t *op_token_ptr;
    
    call_type = FUNC_CALL_NORMAL;
    switch(s->token.val) {
    case TOK_NUMBER:
        {
            JSValue val;
            val = s->token.u.num.val;

            if (JS_VALUE_GET_TAG(val) == JS_TAG_INT) {
                emit_op(s, OP_push_i32);
                emit_u32(s, JS_VALUE_GET_INT(val));
            } else if (JS_VALUE_GET_TAG(val) == JS_TAG_SHORT_BIG_INT) {
                int64_t v;
                v = JS_VALUE_GET_SHORT_BIG_INT(val);
                if (v >= INT32_MIN && v <= INT32_MAX) {
                    emit_op(s, OP_push_bigint_i32);
                    emit_u32(s, v);
                } else {
                    goto large_number;
                }
            } else {
            large_number:
                if (emit_push_const(s, val, 0) < 0)
                    return -1;
            }
        }
        if (next_token(s))
            return -1;
        break;
    case TOK_TEMPLATE:
        if (js_parse_template(s, 0, NULL))
            return -1;
        break;
    case TOK_STRING:
        if (emit_push_const(s, s->token.u.str.str, 1))
            return -1;
        if (next_token(s))
            return -1;
        break;

    case TOK_DIV_ASSIGN:
        s->buf_ptr -= 2;
        goto parse_regexp;
    case '/':
        s->buf_ptr--;
    parse_regexp:
        {
            JSValue str;
            int ret;
            if (!s->ctx->compile_regexp)
                return js_parse_error(s, "RegExp are not supported");
            /* the previous token is '/' or '/=', so no need to free */
            if (js_parse_regexp(s))
                return -1;
            ret = emit_push_const(s, s->token.u.regexp.body, 0);
            str = s->ctx->compile_regexp(s->ctx, s->token.u.regexp.body,
                                         s->token.u.regexp.flags);
            if (JS_IsException(str)) {
                /* add the line number info */
                int line_num, col_num;
                line_num = get_line_col(&col_num, s->buf_start, s->token.ptr - s->buf_start);
                build_backtrace(s->ctx, s->ctx->rt->current_exception,
                                s->filename, line_num + 1, col_num + 1, 0);
                return -1;
            }
            ret = emit_push_const(s, str, 0);
            JS_FreeValue(s->ctx, str);
            if (ret)
                return -1;
            /* we use a specific opcode to be sure the correct
               function is called (otherwise the bytecode would have
               to be verified by the RegExp constructor) */
            emit_op(s, OP_regexp);
            if (next_token(s))
                return -1;
        }
        break;
    case '(':
        if (js_parse_expr_paren(s))
            return -1;
        break;
    case TOK_FUNCTION:
        if (js_parse_function_decl(s, JS_PARSE_FUNC_EXPR,
                                   JS_FUNC_NORMAL, JS_ATOM_NULL,
                                   s->token.ptr))
            return -1;
        break;
    case TOK_CLASS:
        if (js_parse_class(s, TRUE, JS_PARSE_EXPORT_NONE))
            return -1;
        break;
    case TOK_NULL:
        if (next_token(s))
            return -1;
        emit_op(s, OP_null);
        break;
    case TOK_THIS:
        if (next_token(s))
            return -1;
        emit_op(s, OP_scope_get_var);
        emit_atom(s, JS_ATOM_this);
        emit_u16(s, 0);
        break;
    case TOK_FALSE:
        if (next_token(s))
            return -1;
        emit_op(s, OP_push_false);
        break;
    case TOK_TRUE:
        if (next_token(s))
            return -1;
        emit_op(s, OP_push_true);
        break;
    case TOK_IDENT:
        {
            JSAtom name;
            const uint8_t *source_ptr;
            if (s->token.u.ident.is_reserved) {
                return js_parse_error_reserved_identifier(s);
            }
            source_ptr = s->token.ptr;
            if (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                peek_token(s, TRUE) != '\n') {
                if (next_token(s))
                    return -1;
                if (s->token.val == TOK_FUNCTION) {
                    if (js_parse_function_decl(s, JS_PARSE_FUNC_EXPR,
                                               JS_FUNC_ASYNC, JS_ATOM_NULL,
                                               source_ptr))
                        return -1;
                } else {
                    name = JS_DupAtom(s->ctx, JS_ATOM_async);
                    goto do_get_var;
                }
            } else {
                if (s->token.u.ident.atom == JS_ATOM_arguments &&
                    !s->cur_func->arguments_allowed) {
                    js_parse_error(s, "'arguments' identifier is not allowed in class field initializer");
                    return -1;
                }
                name = JS_DupAtom(s->ctx, s->token.u.ident.atom);
                if (next_token(s)) {
                    JS_FreeAtom(s->ctx, name);
                    return -1;
                }
            do_get_var:
                emit_source_pos(s, source_ptr);
                emit_op(s, OP_scope_get_var);
                emit_u32(s, name);
                emit_u16(s, s->cur_func->scope_level);
            }
        }
        break;
    case '{':
    case '[':
        if (s->token.val == '{') {
            if (js_parse_object_literal(s))
                return -1;
        } else {
            if (js_parse_array_literal(s))
                return -1;
        }
        break;
    case TOK_NEW:
        if (next_token(s))
            return -1;
        if (s->token.val == '.') {
            if (next_token(s))
                return -1;
            if (!token_is_pseudo_keyword(s, JS_ATOM_target))
                return js_parse_error(s, "expecting target");
            if (!s->cur_func->new_target_allowed)
                return js_parse_error(s, "new.target only allowed within functions");
            if (next_token(s))
                return -1;
            emit_op(s, OP_scope_get_var);
            emit_atom(s, JS_ATOM_new_target);
            emit_u16(s, 0);
        } else {
            if (js_parse_postfix_expr(s, 0))
                return -1;
            accept_lparen = TRUE;
            if (s->token.val != '(') {
                /* new operator on an object */
                emit_source_pos(s, s->token.ptr);
                emit_op(s, OP_dup);
                emit_op(s, OP_call_constructor);
                emit_u16(s, 0);
            } else {
                call_type = FUNC_CALL_NEW;
            }
        }
        break;
    case TOK_SUPER:
        if (next_token(s))
            return -1;
        if (s->token.val == '(') {
            if (!s->cur_func->super_call_allowed)
                return js_parse_error(s, "super() is only valid in a derived class constructor");
            call_type = FUNC_CALL_SUPER_CTOR;
        } else if (s->token.val == '.' || s->token.val == '[') {
            if (!s->cur_func->super_allowed)
                return js_parse_error(s, "'super' is only valid in a method");
            emit_op(s, OP_scope_get_var);
            emit_atom(s, JS_ATOM_this);
            emit_u16(s, 0);
            emit_op(s, OP_scope_get_var);
            emit_atom(s, JS_ATOM_home_object);
            emit_u16(s, 0);
            emit_op(s, OP_get_super);
        } else {
            return js_parse_error(s, "invalid use of 'super'");
        }
        break;
    case TOK_IMPORT:
        if (next_token(s))
            return -1;
        if (s->token.val == '.') {
            if (next_token(s))
                return -1;
            if (!token_is_pseudo_keyword(s, JS_ATOM_meta))
                return js_parse_error(s, "meta expected");
            if (!s->is_module)
                return js_parse_error(s, "import.meta only valid in module code");
            if (next_token(s))
                return -1;
            emit_op(s, OP_special_object);
            emit_u8(s, OP_SPECIAL_OBJECT_IMPORT_META);
        } else {
            if (js_parse_expect(s, '('))
                return -1;
            if (!accept_lparen)
                return js_parse_error(s, "invalid use of 'import()'");
            if (js_parse_assign_expr(s))
                return -1;
            if (s->token.val == ',') {
                if (next_token(s))
                    return -1;
                if (s->token.val != ')') {
                    if (js_parse_assign_expr(s))
                        return -1;
                    /* accept a trailing comma */
                    if (s->token.val == ',') {
                        if (next_token(s))
                            return -1;
                    }
                } else {
                    emit_op(s, OP_undefined);
                }
            } else {
                emit_op(s, OP_undefined);
            }
            if (js_parse_expect(s, ')'))
                return -1;
            emit_op(s, OP_import);
        }
        break;
    default:
        return js_parse_error(s, "unexpected token in expression: '%.*s'",
                              (int)(s->buf_ptr - s->token.ptr), s->token.ptr);
    }

    optional_chaining_label = -1;
    for(;;) {
        JSFunctionDef *fd = s->cur_func;
        BOOL has_optional_chain = FALSE;

        if (s->token.val == TOK_QUESTION_MARK_DOT) {
            if ((parse_flags & PF_POSTFIX_CALL) == 0)
                return js_parse_error(s, "new keyword cannot be used with an optional chain");
            op_token_ptr = s->token.ptr;
            /* optional chaining */
            if (next_token(s))
                return -1;
            has_optional_chain = TRUE;
            if (s->token.val == '(' && accept_lparen) {
                goto parse_func_call;
            } else if (s->token.val == '[') {
                goto parse_array_access;
            } else {
                goto parse_property;
            }
        } else if (s->token.val == TOK_TEMPLATE &&
                   call_type == FUNC_CALL_NORMAL) {
            if (optional_chaining_label >= 0) {
                return js_parse_error(s, "template literal cannot appear in an optional chain");
            }
            call_type = FUNC_CALL_TEMPLATE;
            op_token_ptr = s->token.ptr; /* XXX: check if right position */
            goto parse_func_call2;
        } else if (s->token.val == '(' && accept_lparen) {
            int opcode, arg_count, drop_count;

            /* function call */
        parse_func_call:
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                return -1;

            if (call_type == FUNC_CALL_NORMAL) {
            parse_func_call2:
                switch(opcode = get_prev_opcode(fd)) {
                case OP_get_field:
                    /* keep the object on the stack */
                    fd->byte_code.buf[fd->last_opcode_pos] = OP_get_field2;
                    drop_count = 2;
                    break;
                case OP_get_field_opt_chain:
                    {
                        int opt_chain_label, next_label;
                        opt_chain_label = get_u32(fd->byte_code.buf +
                                                  fd->last_opcode_pos + 1 + 4 + 1);
                        /* keep the object on the stack */
                        fd->byte_code.buf[fd->last_opcode_pos] = OP_get_field2;
                        fd->byte_code.size = fd->last_opcode_pos + 1 + 4;
                        next_label = emit_goto(s, OP_goto, -1);
                        emit_label(s, opt_chain_label);
                        /* need an additional undefined value for the
                           case where the optional field does not
                           exists */
                        emit_op(s, OP_undefined);
                        emit_label(s, next_label);
                        drop_count = 2;
                        opcode = OP_get_field;
                    }
                    break;
                case OP_scope_get_private_field:
                    /* keep the object on the stack */
                    fd->byte_code.buf[fd->last_opcode_pos] = OP_scope_get_private_field2;
                    drop_count = 2;
                    break;
                case OP_get_array_el:
                    /* keep the object on the stack */
                    fd->byte_code.buf[fd->last_opcode_pos] = OP_get_array_el2;
                    drop_count = 2;
                    break;
                case OP_get_array_el_opt_chain:
                    {
                        int opt_chain_label, next_label;
                        opt_chain_label = get_u32(fd->byte_code.buf +
                                                  fd->last_opcode_pos + 1 + 1);
                        /* keep the object on the stack */
                        fd->byte_code.buf[fd->last_opcode_pos] = OP_get_array_el2;
                        fd->byte_code.size = fd->last_opcode_pos + 1;
                        next_label = emit_goto(s, OP_goto, -1);
                        emit_label(s, opt_chain_label);
                        /* need an additional undefined value for the
                           case where the optional field does not
                           exists */
                        emit_op(s, OP_undefined);
                        emit_label(s, next_label);
                        drop_count = 2;
                        opcode = OP_get_array_el;
                    }
                    break;
                case OP_scope_get_var:
                    {
                        JSAtom name;
                        int scope;
                        name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
                        scope = get_u16(fd->byte_code.buf + fd->last_opcode_pos + 5);
                        if (name == JS_ATOM_eval && call_type == FUNC_CALL_NORMAL && !has_optional_chain) {
                            /* direct 'eval' */
                            opcode = OP_eval;
                        } else {
                            /* verify if function name resolves to a simple
                               get_loc/get_arg: a function call inside a `with`
                               statement can resolve to a method call of the
                               `with` context object
                             */
                            /* XXX: always generate the OP_scope_get_ref
                               and remove it in variable resolution
                               pass ? */
                            if (has_with_scope(fd, scope)) {
                                opcode = OP_scope_get_ref;
                                fd->byte_code.buf[fd->last_opcode_pos] = opcode;
                            }
                        }
                        drop_count = 1;
                    }
                    break;
                case OP_get_super_value:
                    fd->byte_code.buf[fd->last_opcode_pos] = OP_get_array_el;
                    /* on stack: this func_obj */
                    opcode = OP_get_array_el;
                    drop_count = 2;
                    break;
                default:
                    opcode = OP_invalid;
                    drop_count = 1;
                    break;
                }
                if (has_optional_chain) {
                    optional_chain_test(s, &optional_chaining_label,
                                        drop_count);
                }
            } else {
                opcode = OP_invalid;
            }

            if (call_type == FUNC_CALL_TEMPLATE) {
                if (js_parse_template(s, 1, &arg_count))
                    return -1;
                goto emit_func_call;
            } else if (call_type == FUNC_CALL_SUPER_CTOR) {
                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_this_active_func);
                emit_u16(s, 0);

                emit_op(s, OP_get_super);

                emit_op(s, OP_scope_get_var);
                emit_atom(s, JS_ATOM_new_target);
                emit_u16(s, 0);
            } else if (call_type == FUNC_CALL_NEW) {
                emit_op(s, OP_dup); /* new.target = function */
            }

            /* parse arguments */
            arg_count = 0;
            while (s->token.val != ')') {
                if (arg_count >= 65535) {
                    return js_parse_error(s, "Too many call arguments");
                }
                if (s->token.val == TOK_ELLIPSIS)
                    break;
                if (js_parse_assign_expr(s))
                    return -1;
                arg_count++;
                if (s->token.val == ')')
                    break;
                /* accept a trailing comma before the ')' */
                if (js_parse_expect(s, ','))
                    return -1;
            }
            if (s->token.val == TOK_ELLIPSIS) {
                emit_op(s, OP_array_from);
                emit_u16(s, arg_count);
                emit_op(s, OP_push_i32);
                emit_u32(s, arg_count);

                /* on stack: array idx */
                while (s->token.val != ')') {
                    if (s->token.val == TOK_ELLIPSIS) {
                        if (next_token(s))
                            return -1;
                        if (js_parse_assign_expr(s))
                            return -1;
#if 1
                        /* XXX: could pass is_last indicator? */
                        emit_op(s, OP_append);
#else
                        int label_next, label_done;
                        label_next = new_label(s);
                        label_done = new_label(s);
                        /* push enumerate object below array/idx pair */
                        emit_op(s, OP_for_of_start);
                        emit_op(s, OP_rot5l);
                        emit_op(s, OP_rot5l);
                        emit_label(s, label_next);
                        /* on stack: enum_rec array idx */
                        emit_op(s, OP_for_of_next);
                        emit_u8(s, 2);
                        emit_goto(s, OP_if_true, label_done);
                        /* append element */
                        /* enum_rec array idx val -> enum_rec array new_idx */
                        emit_op(s, OP_define_array_el);
                        emit_op(s, OP_inc);
                        emit_goto(s, OP_goto, label_next);
                        emit_label(s, label_done);
                        /* close enumeration, drop enum_rec and idx */
                        emit_op(s, OP_drop); /* drop undef */
                        emit_op(s, OP_nip1); /* drop enum_rec */
                        emit_op(s, OP_nip1);
                        emit_op(s, OP_nip1);
#endif
                    } else {
                        if (js_parse_assign_expr(s))
                            return -1;
                        /* array idx val */
                        emit_op(s, OP_define_array_el);
                        emit_op(s, OP_inc);
                    }
                    if (s->token.val == ')')
                        break;
                    /* accept a trailing comma before the ')' */
                    if (js_parse_expect(s, ','))
                        return -1;
                }
                if (next_token(s))
                    return -1;
                /* drop the index */
                emit_op(s, OP_drop);

                emit_source_pos(s, op_token_ptr);
                /* apply function call */
                switch(opcode) {
                case OP_get_field:
                case OP_scope_get_private_field:
                case OP_get_array_el:
                case OP_scope_get_ref:
                    /* obj func array -> func obj array */
                    emit_op(s, OP_perm3);
                    emit_op(s, OP_apply);
                    emit_u16(s, call_type == FUNC_CALL_NEW);
                    break;
                case OP_eval:
                    emit_op(s, OP_apply_eval);
                    emit_u16(s, fd->scope_level);
                    fd->has_eval_call = TRUE;
                    break;
                default:
                    if (call_type == FUNC_CALL_SUPER_CTOR) {
                        emit_op(s, OP_apply);
                        emit_u16(s, 1);
                        /* set the 'this' value */
                        emit_op(s, OP_dup);
                        emit_op(s, OP_scope_put_var_init);
                        emit_atom(s, JS_ATOM_this);
                        emit_u16(s, 0);

                        emit_class_field_init(s);
                    } else if (call_type == FUNC_CALL_NEW) {
                        /* obj func array -> func obj array */
                        emit_op(s, OP_perm3);
                        emit_op(s, OP_apply);
                        emit_u16(s, 1);
                    } else {
                        /* func array -> func undef array */
                        emit_op(s, OP_undefined);
                        emit_op(s, OP_swap);
                        emit_op(s, OP_apply);
                        emit_u16(s, 0);
                    }
                    break;
                }
            } else {
                if (next_token(s))
                    return -1;
            emit_func_call:
                emit_source_pos(s, op_token_ptr);
                switch(opcode) {
                case OP_get_field:
                case OP_scope_get_private_field:
                case OP_get_array_el:
                case OP_scope_get_ref:
                    emit_op(s, OP_call_method);
                    emit_u16(s, arg_count);
                    break;
                case OP_eval:
                    emit_op(s, OP_eval);
                    emit_u16(s, arg_count);
                    emit_u16(s, fd->scope_level);
                    fd->has_eval_call = TRUE;
                    break;
                default:
                    if (call_type == FUNC_CALL_SUPER_CTOR) {
                        emit_op(s, OP_call_constructor);
                        emit_u16(s, arg_count);

                        /* set the 'this' value */
                        emit_op(s, OP_dup);
                        emit_op(s, OP_scope_put_var_init);
                        emit_atom(s, JS_ATOM_this);
                        emit_u16(s, 0);

                        emit_class_field_init(s);
                    } else if (call_type == FUNC_CALL_NEW) {
                        emit_op(s, OP_call_constructor);
                        emit_u16(s, arg_count);
                    } else {
                        emit_op(s, OP_call);
                        emit_u16(s, arg_count);
                    }
                    break;
                }
            }
            call_type = FUNC_CALL_NORMAL;
        } else if (s->token.val == '.') {
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                return -1;
        parse_property:
            emit_source_pos(s, op_token_ptr);
            if (s->token.val == TOK_PRIVATE_NAME) {
                /* private class field */
                if (get_prev_opcode(fd) == OP_get_super) {
                    return js_parse_error(s, "private class field forbidden after super");
                }
                if (has_optional_chain) {
                    optional_chain_test(s, &optional_chaining_label, 1);
                }
                emit_op(s, OP_scope_get_private_field);
                emit_atom(s, s->token.u.ident.atom);
                emit_u16(s, s->cur_func->scope_level);
            } else {
                if (!token_is_ident(s->token.val)) {
                    return js_parse_error(s, "expecting field name");
                }
                if (get_prev_opcode(fd) == OP_get_super) {
                    JSValue val;
                    int ret;
                    val = JS_AtomToValue(s->ctx, s->token.u.ident.atom);
                    ret = emit_push_const(s, val, 1);
                    JS_FreeValue(s->ctx, val);
                    if (ret)
                        return -1;
                    emit_op(s, OP_get_super_value);
                } else {
                    if (has_optional_chain) {
                        optional_chain_test(s, &optional_chaining_label, 1);
                    }
                    emit_op(s, OP_get_field);
                    emit_atom(s, s->token.u.ident.atom);
                }
            }
            if (next_token(s))
                return -1;
        } else if (s->token.val == '[') {
            int prev_op;
            op_token_ptr = s->token.ptr;
        parse_array_access:
            prev_op = get_prev_opcode(fd);
            if (has_optional_chain) {
                optional_chain_test(s, &optional_chaining_label, 1);
            }
            if (next_token(s))
                return -1;
            if (js_parse_expr(s))
                return -1;
            if (js_parse_expect(s, ']'))
                return -1;
            emit_source_pos(s, op_token_ptr);
            if (prev_op == OP_get_super) {
                emit_op(s, OP_get_super_value);
            } else {
                emit_op(s, OP_get_array_el);
            }
        } else {
            break;
        }
    }
    if (optional_chaining_label >= 0) {
        JSFunctionDef *fd = s->cur_func;
        int opcode;
        emit_label_raw(s, optional_chaining_label);
        /* modify the last opcode so that it is an indicator of an
           optional chain */
        opcode = get_prev_opcode(fd);
        if (opcode == OP_get_field || opcode == OP_get_array_el) {
            if (opcode == OP_get_field)
                opcode = OP_get_field_opt_chain;
            else
                opcode = OP_get_array_el_opt_chain;
            fd->byte_code.buf[fd->last_opcode_pos] = opcode;
        } else {
            fd->last_opcode_pos = -1;
        }
    }
    return 0;
}

static __exception int js_parse_delete(JSParseState *s)
{
    JSFunctionDef *fd = s->cur_func;
    JSAtom name;
    int opcode;

    if (next_token(s))
        return -1;
    if (js_parse_unary(s, PF_POW_FORBIDDEN))
        return -1;
    switch(opcode = get_prev_opcode(fd)) {
    case OP_get_field:
    case OP_get_field_opt_chain:
        {
            JSValue val;
            int ret, opt_chain_label, next_label;
            if (opcode == OP_get_field_opt_chain) {
                opt_chain_label = get_u32(fd->byte_code.buf +
                                          fd->last_opcode_pos + 1 + 4 + 1);
            } else {
                opt_chain_label = -1;
            }
            name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
            fd->byte_code.size = fd->last_opcode_pos;
            val = JS_AtomToValue(s->ctx, name);
            ret = emit_push_const(s, val, 1);
            JS_FreeValue(s->ctx, val);
            JS_FreeAtom(s->ctx, name);
            if (ret)
                return ret;
            emit_op(s, OP_delete);
            if (opt_chain_label >= 0) {
                next_label = emit_goto(s, OP_goto, -1);
                emit_label(s, opt_chain_label);
                /* if the optional chain is not taken, return 'true' */
                emit_op(s, OP_drop);
                emit_op(s, OP_push_true);
                emit_label(s, next_label);
            }
            fd->last_opcode_pos = -1;
        }
        break;
    case OP_get_array_el:
        fd->byte_code.size = fd->last_opcode_pos;
        fd->last_opcode_pos = -1;
        emit_op(s, OP_delete);
        break;
    case OP_get_array_el_opt_chain:
        {
            int opt_chain_label, next_label;
            opt_chain_label = get_u32(fd->byte_code.buf +
                                      fd->last_opcode_pos + 1 + 1);
            fd->byte_code.size = fd->last_opcode_pos;
            emit_op(s, OP_delete);
            next_label = emit_goto(s, OP_goto, -1);
            emit_label(s, opt_chain_label);
            /* if the optional chain is not taken, return 'true' */
            emit_op(s, OP_drop);
            emit_op(s, OP_push_true);
            emit_label(s, next_label);
            fd->last_opcode_pos = -1;
        }
        break;
    case OP_scope_get_var:
        /* 'delete this': this is not a reference */
        name = get_u32(fd->byte_code.buf + fd->last_opcode_pos + 1);
        if (name == JS_ATOM_this || name == JS_ATOM_new_target)
            goto ret_true;
        if (fd->js_mode & JS_MODE_STRICT) {
            return js_parse_error(s, "cannot delete a direct reference in strict mode");
        } else {
            fd->byte_code.buf[fd->last_opcode_pos] = OP_scope_delete_var;
        }
        break;
    case OP_scope_get_private_field:
        return js_parse_error(s, "cannot delete a private class field");
    case OP_get_super_value:
        fd->byte_code.size = fd->last_opcode_pos;
        fd->last_opcode_pos = -1;
        emit_op(s, OP_throw_error);
        emit_atom(s, JS_ATOM_NULL);
        emit_u8(s, JS_THROW_ERROR_DELETE_SUPER);
        break;
    default:
    ret_true:
        emit_op(s, OP_drop);
        emit_op(s, OP_push_true);
        break;
    }
    return 0;
}

/* allowed parse_flags: PF_POW_ALLOWED, PF_POW_FORBIDDEN */
static __exception int js_parse_unary(JSParseState *s, int parse_flags)
{
    int op;
    const uint8_t *op_token_ptr;

    switch(s->token.val) {
    case '+':
    case '-':
    case '!':
    case '~':
    case TOK_VOID:
        op_token_ptr = s->token.ptr;
        op = s->token.val;
        if (next_token(s))
            return -1;
        if (js_parse_unary(s, PF_POW_FORBIDDEN))
            return -1;
        switch(op) {
        case '-':
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_neg);
            break;
        case '+':
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_plus);
            break;
        case '!':
            emit_op(s, OP_lnot);
            break;
        case '~':
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_not);
            break;
        case TOK_VOID:
            emit_op(s, OP_drop);
            emit_op(s, OP_undefined);
            break;
        default:
            abort();
        }
        parse_flags = 0;
        break;
    case TOK_DEC:
    case TOK_INC:
        {
            int opcode, op, scope, label;
            JSAtom name;
            op = s->token.val;
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, 0))
                return -1;
            if (get_lvalue(s, &opcode, &scope, &name, &label, NULL, TRUE, op))
                return -1;
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_dec + op - TOK_DEC);
            put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_KEEP_TOP,
                       FALSE);
        }
        break;
    case TOK_TYPEOF:
        {
            JSFunctionDef *fd;
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, PF_POW_FORBIDDEN))
                return -1;
            /* reference access should not return an exception, so we
               patch the get_var */
            fd = s->cur_func;
            if (get_prev_opcode(fd) == OP_scope_get_var) {
                fd->byte_code.buf[fd->last_opcode_pos] = OP_scope_get_var_undef;
            }
            emit_op(s, OP_typeof);
            parse_flags = 0;
        }
        break;
    case TOK_DELETE:
        if (js_parse_delete(s))
            return -1;
        parse_flags = 0;
        break;
    case TOK_AWAIT:
        if (!(s->cur_func->func_kind & JS_FUNC_ASYNC))
            return js_parse_error(s, "unexpected 'await' keyword");
        if (!s->cur_func->in_function_body)
            return js_parse_error(s, "await in default expression");
        if (next_token(s))
            return -1;
        if (js_parse_unary(s, PF_POW_FORBIDDEN))
            return -1;
        s->cur_func->has_await = TRUE;
        emit_op(s, OP_await);
        parse_flags = 0;
        break;
    default:
        if (js_parse_postfix_expr(s, PF_POSTFIX_CALL))
            return -1;
        if (!s->got_lf &&
            (s->token.val == TOK_DEC || s->token.val == TOK_INC)) {
            int opcode, op, scope, label;
            JSAtom name;
            op = s->token.val;
            op_token_ptr = s->token.ptr;
            if (get_lvalue(s, &opcode, &scope, &name, &label, NULL, TRUE, op))
                return -1;
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_post_dec + op - TOK_DEC);
            put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_KEEP_SECOND,
                       FALSE);
            if (next_token(s))
                return -1;
        }
        break;
    }
    if (parse_flags & (PF_POW_ALLOWED | PF_POW_FORBIDDEN)) {
        if (s->token.val == TOK_POW) {
            /* Strict ES7 exponentiation syntax rules: To solve
               conficting semantics between different implementations
               regarding the precedence of prefix operators and the
               postifx exponential, ES7 specifies that -2**2 is a
               syntax error. */
            if (parse_flags & PF_POW_FORBIDDEN) {
                JS_ThrowSyntaxError(s->ctx, "unparenthesized unary expression can't appear on the left-hand side of '**'");
                return -1;
            }
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                return -1;
            if (js_parse_unary(s, PF_POW_ALLOWED))
                return -1;
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_pow);
        }
    }
    return 0;
}

/* allowed parse_flags: PF_IN_ACCEPTED */
static __exception int js_parse_expr_binary(JSParseState *s, int level,
                                            int parse_flags)
{
    int op, opcode;
    const uint8_t *op_token_ptr;
    
    if (level == 0) {
        return js_parse_unary(s, PF_POW_ALLOWED);
    } else if (s->token.val == TOK_PRIVATE_NAME &&
               (parse_flags & PF_IN_ACCEPTED) && level == 4 &&
               peek_token(s, FALSE) == TOK_IN) {
        JSAtom atom;

        atom = JS_DupAtom(s->ctx, s->token.u.ident.atom);
        if (next_token(s))
            goto fail_private_in;
        if (s->token.val != TOK_IN)
            goto fail_private_in;
        if (next_token(s))
            goto fail_private_in;
        if (js_parse_expr_binary(s, level - 1, parse_flags)) {
        fail_private_in:
            JS_FreeAtom(s->ctx, atom);
            return -1;
        }
        emit_op(s, OP_scope_in_private_field);
        emit_atom(s, atom);
        emit_u16(s, s->cur_func->scope_level);
        JS_FreeAtom(s->ctx, atom);
        return 0;
    } else {
        if (js_parse_expr_binary(s, level - 1, parse_flags))
            return -1;
    }
    for(;;) {
        op = s->token.val;
        op_token_ptr = s->token.ptr;
        switch(level) {
        case 1:
            switch(op) {
            case '*':
                opcode = OP_mul;
                break;
            case '/':
                opcode = OP_div;
                break;
            case '%':
                opcode = OP_mod;
                break;
            default:
                return 0;
            }
            break;
        case 2:
            switch(op) {
            case '+':
                opcode = OP_add;
                break;
            case '-':
                opcode = OP_sub;
                break;
            default:
                return 0;
            }
            break;
        case 3:
            switch(op) {
            case TOK_SHL:
                opcode = OP_shl;
                break;
            case TOK_SAR:
                opcode = OP_sar;
                break;
            case TOK_SHR:
                opcode = OP_shr;
                break;
            default:
                return 0;
            }
            break;
        case 4:
            switch(op) {
            case '<':
                opcode = OP_lt;
                break;
            case '>':
                opcode = OP_gt;
                break;
            case TOK_LTE:
                opcode = OP_lte;
                break;
            case TOK_GTE:
                opcode = OP_gte;
                break;
            case TOK_INSTANCEOF:
                opcode = OP_instanceof;
                break;
            case TOK_IN:
                if (parse_flags & PF_IN_ACCEPTED) {
                    opcode = OP_in;
                } else {
                    return 0;
                }
                break;
            default:
                return 0;
            }
            break;
        case 5:
            switch(op) {
            case TOK_EQ:
                opcode = OP_eq;
                break;
            case TOK_NEQ:
                opcode = OP_neq;
                break;
            case TOK_STRICT_EQ:
                opcode = OP_strict_eq;
                break;
            case TOK_STRICT_NEQ:
                opcode = OP_strict_neq;
                break;
            default:
                return 0;
            }
            break;
        case 6:
            switch(op) {
            case '&':
                opcode = OP_and;
                break;
            default:
                return 0;
            }
            break;
        case 7:
            switch(op) {
            case '^':
                opcode = OP_xor;
                break;
            default:
                return 0;
            }
            break;
        case 8:
            switch(op) {
            case '|':
                opcode = OP_or;
                break;
            default:
                return 0;
            }
            break;
        default:
            abort();
        }
        if (next_token(s))
            return -1;
        if (js_parse_expr_binary(s, level - 1, parse_flags))
            return -1;
        emit_source_pos(s, op_token_ptr);
        emit_op(s, opcode);
    }
    return 0;
}

/* allowed parse_flags: PF_IN_ACCEPTED */
static __exception int js_parse_logical_and_or(JSParseState *s, int op,
                                               int parse_flags)
{
    int label1;

    if (op == TOK_LAND) {
        if (js_parse_expr_binary(s, 8, parse_flags))
            return -1;
    } else {
        if (js_parse_logical_and_or(s, TOK_LAND, parse_flags))
            return -1;
    }
    if (s->token.val == op) {
        label1 = new_label(s);

        for(;;) {
            if (next_token(s))
                return -1;
            emit_op(s, OP_dup);
            emit_goto(s, op == TOK_LAND ? OP_if_false : OP_if_true, label1);
            emit_op(s, OP_drop);

            if (op == TOK_LAND) {
                if (js_parse_expr_binary(s, 8, parse_flags))
                    return -1;
            } else {
                if (js_parse_logical_and_or(s, TOK_LAND,
                                            parse_flags))
                    return -1;
            }
            if (s->token.val != op) {
                if (s->token.val == TOK_DOUBLE_QUESTION_MARK)
                    return js_parse_error(s, "cannot mix ?? with && or ||");
                break;
            }
        }

        emit_label(s, label1);
    }
    return 0;
}

static __exception int js_parse_coalesce_expr(JSParseState *s, int parse_flags)
{
    int label1;

    if (js_parse_logical_and_or(s, TOK_LOR, parse_flags))
        return -1;
    if (s->token.val == TOK_DOUBLE_QUESTION_MARK) {
        label1 = new_label(s);
        for(;;) {
            if (next_token(s))
                return -1;

            emit_op(s, OP_dup);
            emit_op(s, OP_is_undefined_or_null);
            emit_goto(s, OP_if_false, label1);
            emit_op(s, OP_drop);

            if (js_parse_expr_binary(s, 8, parse_flags))
                return -1;
            if (s->token.val != TOK_DOUBLE_QUESTION_MARK)
                break;
        }
        emit_label(s, label1);
    }
    return 0;
}

/* allowed parse_flags: PF_IN_ACCEPTED */
static __exception int js_parse_cond_expr(JSParseState *s, int parse_flags)
{
    int label1, label2;

    if (js_parse_coalesce_expr(s, parse_flags))
        return -1;
    if (s->token.val == '?') {
        if (next_token(s))
            return -1;
        label1 = emit_goto(s, OP_if_false, -1);

        if (js_parse_assign_expr(s))
            return -1;
        if (js_parse_expect(s, ':'))
            return -1;

        label2 = emit_goto(s, OP_goto, -1);

        emit_label(s, label1);

        if (js_parse_assign_expr2(s, parse_flags & PF_IN_ACCEPTED))
            return -1;

        emit_label(s, label2);
    }
    return 0;
}

/* allowed parse_flags: PF_IN_ACCEPTED */
static __exception int js_parse_assign_expr2(JSParseState *s, int parse_flags)
{
    int opcode, op, scope, skip_bits;
    JSAtom name0 = JS_ATOM_NULL;
    JSAtom name;

    if (s->token.val == TOK_YIELD) {
        BOOL is_star = FALSE, is_async;

        if (!(s->cur_func->func_kind & JS_FUNC_GENERATOR))
            return js_parse_error(s, "unexpected 'yield' keyword");
        if (!s->cur_func->in_function_body)
            return js_parse_error(s, "yield in default expression");
        if (next_token(s))
            return -1;
        /* XXX: is there a better method to detect 'yield' without
           parameters ? */
        if (s->token.val != ';' && s->token.val != ')' &&
            s->token.val != ']' && s->token.val != '}' &&
            s->token.val != ',' && s->token.val != ':' && !s->got_lf) {
            if (s->token.val == '*') {
                is_star = TRUE;
                if (next_token(s))
                    return -1;
            }
            if (js_parse_assign_expr2(s, parse_flags))
                return -1;
        } else {
            emit_op(s, OP_undefined);
        }
        is_async = (s->cur_func->func_kind == JS_FUNC_ASYNC_GENERATOR);

        if (is_star) {
            int label_loop, label_return, label_next;
            int label_return1, label_yield, label_throw, label_throw1;
            int label_throw2;

            label_loop = new_label(s);
            label_yield = new_label(s);

            emit_op(s, is_async ? OP_for_await_of_start : OP_for_of_start);

            /* remove the catch offset (XXX: could avoid pushing back
               undefined) */
            emit_op(s, OP_drop);
            emit_op(s, OP_undefined);

            emit_op(s, OP_undefined); /* initial value */

            emit_label(s, label_loop);
            emit_op(s, OP_iterator_next);
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_check_object);
            emit_op(s, OP_get_field2);
            emit_atom(s, JS_ATOM_done);
            label_next = emit_goto(s, OP_if_true, -1); /* end of loop */
            emit_label(s, label_yield);
            if (is_async) {
                /* OP_async_yield_star takes the value as parameter */
                emit_op(s, OP_get_field);
                emit_atom(s, JS_ATOM_value);
                emit_op(s, OP_async_yield_star);
            } else {
                /* OP_yield_star takes (value, done) as parameter */
                emit_op(s, OP_yield_star);
            }
            emit_op(s, OP_dup);
            label_return = emit_goto(s, OP_if_true, -1);
            emit_op(s, OP_drop);
            emit_goto(s, OP_goto, label_loop);

            emit_label(s, label_return);
            emit_op(s, OP_push_i32);
            emit_u32(s, 2);
            emit_op(s, OP_strict_eq);
            label_throw = emit_goto(s, OP_if_true, -1);

            /* return handling */
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_call);
            emit_u8(s, 0);
            label_return1 = emit_goto(s, OP_if_true, -1);
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_check_object);
            emit_op(s, OP_get_field2);
            emit_atom(s, JS_ATOM_done);
            emit_goto(s, OP_if_false, label_yield);

            emit_op(s, OP_get_field);
            emit_atom(s, JS_ATOM_value);

            emit_label(s, label_return1);
            emit_op(s, OP_nip);
            emit_op(s, OP_nip);
            emit_op(s, OP_nip);
            emit_return(s, TRUE);

            /* throw handling */
            emit_label(s, label_throw);
            emit_op(s, OP_iterator_call);
            emit_u8(s, 1);
            label_throw1 = emit_goto(s, OP_if_true, -1);
            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_iterator_check_object);
            emit_op(s, OP_get_field2);
            emit_atom(s, JS_ATOM_done);
            emit_goto(s, OP_if_false, label_yield);
            emit_goto(s, OP_goto, label_next);
            /* close the iterator and throw a type error exception */
            emit_label(s, label_throw1);
            emit_op(s, OP_iterator_call);
            emit_u8(s, 2);
            label_throw2 = emit_goto(s, OP_if_true, -1);
            if (is_async)
                emit_op(s, OP_await);
            emit_label(s, label_throw2);

            emit_op(s, OP_throw_error);
            emit_atom(s, JS_ATOM_NULL);
            emit_u8(s, JS_THROW_ERROR_ITERATOR_THROW);

            emit_label(s, label_next);
            emit_op(s, OP_get_field);
            emit_atom(s, JS_ATOM_value);
            emit_op(s, OP_nip); /* keep the value associated with
                                   done = true */
            emit_op(s, OP_nip);
            emit_op(s, OP_nip);
        } else {
            int label_next;

            if (is_async)
                emit_op(s, OP_await);
            emit_op(s, OP_yield);
            label_next = emit_goto(s, OP_if_false, -1);
            emit_return(s, TRUE);
            emit_label(s, label_next);
        }
        return 0;
    } else if (s->token.val == '(' &&
               js_parse_skip_parens_token(s, NULL, TRUE) == TOK_ARROW) {
        return js_parse_function_decl(s, JS_PARSE_FUNC_ARROW,
                                      JS_FUNC_NORMAL, JS_ATOM_NULL,
                                      s->token.ptr);
    } else if (token_is_pseudo_keyword(s, JS_ATOM_async)) {
        const uint8_t *source_ptr;
        int tok;
        JSParsePos pos;

        /* fast test */
        tok = peek_token(s, TRUE);
        if (tok == TOK_FUNCTION || tok == '\n')
            goto next;

        source_ptr = s->token.ptr;
        js_parse_get_pos(s, &pos);
        if (next_token(s))
            return -1;
        if ((s->token.val == '(' &&
             js_parse_skip_parens_token(s, NULL, TRUE) == TOK_ARROW) ||
            (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved &&
             peek_token(s, TRUE) == TOK_ARROW)) {
            return js_parse_function_decl(s, JS_PARSE_FUNC_ARROW,
                                          JS_FUNC_ASYNC, JS_ATOM_NULL,
                                          source_ptr);
        } else {
            /* undo the token parsing */
            if (js_parse_seek_token(s, &pos))
                return -1;
        }
    } else if (s->token.val == TOK_IDENT &&
               peek_token(s, TRUE) == TOK_ARROW) {
        return js_parse_function_decl(s, JS_PARSE_FUNC_ARROW,
                                      JS_FUNC_NORMAL, JS_ATOM_NULL,
                                      s->token.ptr);
    } else if ((s->token.val == '{' || s->token.val == '[') &&
               js_parse_skip_parens_token(s, &skip_bits, FALSE) == '=') {
        if (js_parse_destructuring_element(s, 0, 0, FALSE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE, FALSE) < 0)
            return -1;
        return 0;
    }
 next:
    if (s->token.val == TOK_IDENT) {
        /* name0 is used to check for OP_set_name pattern, not duplicated */
        name0 = s->token.u.ident.atom;
    }
    if (js_parse_cond_expr(s, parse_flags))
        return -1;

    op = s->token.val;
    if (op == '=' || (op >= TOK_MUL_ASSIGN && op <= TOK_POW_ASSIGN)) {
        int label;
        const uint8_t *op_token_ptr;
        op_token_ptr = s->token.ptr;
        if (next_token(s))
            return -1;
        if (get_lvalue(s, &opcode, &scope, &name, &label, NULL, (op != '='), op) < 0)
            return -1;

        if (js_parse_assign_expr2(s, parse_flags)) {
            JS_FreeAtom(s->ctx, name);
            return -1;
        }

        if (op == '=') {
            if ((opcode == OP_get_ref_value || opcode == OP_scope_get_var) && name == name0) {
                set_object_name(s, name);
            }
        } else {
            static const uint8_t assign_opcodes[] = {
                OP_mul, OP_div, OP_mod, OP_add, OP_sub,
                OP_shl, OP_sar, OP_shr, OP_and, OP_xor, OP_or,
                OP_pow,
            };
            op = assign_opcodes[op - TOK_MUL_ASSIGN];
            emit_source_pos(s, op_token_ptr);
            emit_op(s, op);
        }
        put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_KEEP_TOP, FALSE);
    } else if (op >= TOK_LAND_ASSIGN && op <= TOK_DOUBLE_QUESTION_MARK_ASSIGN) {
        int label, label1, depth_lvalue, label2;

        if (next_token(s))
            return -1;
        if (get_lvalue(s, &opcode, &scope, &name, &label,
                       &depth_lvalue, TRUE, op) < 0)
            return -1;

        emit_op(s, OP_dup);
        if (op == TOK_DOUBLE_QUESTION_MARK_ASSIGN)
            emit_op(s, OP_is_undefined_or_null);
        label1 = emit_goto(s, op == TOK_LOR_ASSIGN ? OP_if_true : OP_if_false,
                           -1);
        emit_op(s, OP_drop);

        if (js_parse_assign_expr2(s, parse_flags)) {
            JS_FreeAtom(s->ctx, name);
            return -1;
        }

        if ((opcode == OP_get_ref_value || opcode == OP_scope_get_var) && name == name0) {
            set_object_name(s, name);
        }

        switch(depth_lvalue) {
        case 0:
            emit_op(s, OP_dup);
            break;
        case 1:
            emit_op(s, OP_insert2);
            break;
        case 2:
            emit_op(s, OP_insert3);
            break;
        case 3:
            emit_op(s, OP_insert4);
            break;
        default:
            abort();
        }

        /* XXX: we disable the OP_put_ref_value optimization by not
           using put_lvalue() otherwise depth_lvalue is not correct */
        put_lvalue(s, opcode, scope, name, label, PUT_LVALUE_NOKEEP_DEPTH,
                   FALSE);
        label2 = emit_goto(s, OP_goto, -1);

        emit_label(s, label1);

        /* remove the lvalue stack entries */
        while (depth_lvalue != 0) {
            emit_op(s, OP_nip);
            depth_lvalue--;
        }

        emit_label(s, label2);
    }
    return 0;
}

static __exception int js_parse_assign_expr(JSParseState *s)
{
    return js_parse_assign_expr2(s, PF_IN_ACCEPTED);
}

/* allowed parse_flags: PF_IN_ACCEPTED */
static __exception int js_parse_expr2(JSParseState *s, int parse_flags)
{
    BOOL comma = FALSE;
    for(;;) {
        if (js_parse_assign_expr2(s, parse_flags))
            return -1;
        if (comma) {
            /* prevent get_lvalue from using the last expression
               as an lvalue. This also prevents the conversion of
               of get_var to get_ref for method lookup in function
               call inside `with` statement.
             */
            s->cur_func->last_opcode_pos = -1;
        }
        if (s->token.val != ',')
            break;
        comma = TRUE;
        if (next_token(s))
            return -1;
        emit_op(s, OP_drop);
    }
    return 0;
}

static __exception int js_parse_expr(JSParseState *s)
{
    return js_parse_expr2(s, PF_IN_ACCEPTED);
}

static void push_break_entry(JSFunctionDef *fd, BlockEnv *be,
                             JSAtom label_name,
                             int label_break, int label_cont,
                             int drop_count)
{
    be->prev = fd->top_break;
    fd->top_break = be;
    be->label_name = label_name;
    be->label_break = label_break;
    be->label_cont = label_cont;
    be->drop_count = drop_count;
    be->label_finally = -1;
    be->scope_level = fd->scope_level;
    be->has_iterator = FALSE;
    be->is_regular_stmt = FALSE;
}

static void pop_break_entry(JSFunctionDef *fd)
{
    BlockEnv *be;
    be = fd->top_break;
    fd->top_break = be->prev;
}

static __exception int emit_break(JSParseState *s, JSAtom name, int is_cont)
{
    BlockEnv *top;
    int i, scope_level;

    scope_level = s->cur_func->scope_level;
    top = s->cur_func->top_break;
    while (top != NULL) {
        close_scopes(s, scope_level, top->scope_level);
        scope_level = top->scope_level;
        if (is_cont &&
            top->label_cont != -1 &&
            (name == JS_ATOM_NULL || top->label_name == name)) {
            /* continue stays inside the same block */
            emit_goto(s, OP_goto, top->label_cont);
            return 0;
        }
        if (!is_cont &&
            top->label_break != -1 &&
            ((name == JS_ATOM_NULL && !top->is_regular_stmt) ||
             top->label_name == name)) {
            emit_goto(s, OP_goto, top->label_break);
            return 0;
        }
        i = 0;
        if (top->has_iterator) {
            emit_op(s, OP_iterator_close);
            i += 3;
        }
        for(; i < top->drop_count; i++)
            emit_op(s, OP_drop);
        if (top->label_finally != -1) {
            /* must push dummy value to keep same stack depth */
            emit_op(s, OP_undefined);
            emit_goto(s, OP_gosub, top->label_finally);
            emit_op(s, OP_drop);
        }
        top = top->prev;
    }
    if (name == JS_ATOM_NULL) {
        if (is_cont)
            return js_parse_error(s, "continue must be inside loop");
        else
            return js_parse_error(s, "break must be inside loop or switch");
    } else {
        return js_parse_error(s, "break/continue label not found");
    }
}

/* execute the finally blocks before return */
static void emit_return(JSParseState *s, BOOL hasval)
{
    BlockEnv *top;

    if (s->cur_func->func_kind != JS_FUNC_NORMAL) {
        if (!hasval) {
            /* no value: direct return in case of async generator */
            emit_op(s, OP_undefined);
            hasval = TRUE;
        } else if (s->cur_func->func_kind == JS_FUNC_ASYNC_GENERATOR) {
            /* the await must be done before handling the "finally" in
               case it raises an exception */
            emit_op(s, OP_await);
        }
    }

    top = s->cur_func->top_break;
    while (top != NULL) {
        if (top->has_iterator || top->label_finally != -1) {
            if (!hasval) {
                emit_op(s, OP_undefined);
                hasval = TRUE;
            }
            /* Remove the stack elements up to and including the catch
               offset. When 'yield' is used in an expression we have
               no easy way to count them, so we use this specific
               instruction instead. */
            emit_op(s, OP_nip_catch);
            /* stack: iter_obj next ret_val */
            if (top->has_iterator) {
                if (s->cur_func->func_kind == JS_FUNC_ASYNC_GENERATOR) {
                    int label_next, label_next2;
                    emit_op(s, OP_nip); /* next */
                    emit_op(s, OP_swap);
                    emit_op(s, OP_get_field2);
                    emit_atom(s, JS_ATOM_return);
                    /* stack: iter_obj return_func */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_is_undefined_or_null);
                    label_next = emit_goto(s, OP_if_true, -1);
                    emit_op(s, OP_call_method);
                    emit_u16(s, 0);
                    emit_op(s, OP_iterator_check_object);
                    emit_op(s, OP_await);
                    label_next2 = emit_goto(s, OP_goto, -1);
                    emit_label(s, label_next);
                    emit_op(s, OP_drop);
                    emit_label(s, label_next2);
                    emit_op(s, OP_drop);
                } else {
                    emit_op(s, OP_rot3r);
                    emit_op(s, OP_undefined); /* dummy catch offset */
                    emit_op(s, OP_iterator_close);
                }
            } else {
                /* execute the "finally" block */
                emit_goto(s, OP_gosub, top->label_finally);
            }
        }
        top = top->prev;
    }
    if (s->cur_func->is_derived_class_constructor) {
        int label_return;

        /* 'this' can be uninitialized, so it may be accessed only if
           the derived class constructor does not return an object */
        if (hasval) {
            emit_op(s, OP_check_ctor_return);
            label_return = emit_goto(s, OP_if_false, -1);
            emit_op(s, OP_drop);
        } else {
            label_return = -1;
        }

        /* The error should be raised in the caller context, so we use
           a specific opcode */
        emit_op(s, OP_scope_get_var_checkthis);
        emit_atom(s, JS_ATOM_this);
        emit_u16(s, 0);

        emit_label(s, label_return);
        emit_op(s, OP_return);
    } else if (s->cur_func->func_kind != JS_FUNC_NORMAL) {
        emit_op(s, OP_return_async);
    } else {
        emit_op(s, hasval ? OP_return : OP_return_undef);
    }
}

#define DECL_MASK_FUNC  (1 << 0) /* allow normal function declaration */
/* ored with DECL_MASK_FUNC if function declarations are allowed with a label */
#define DECL_MASK_FUNC_WITH_LABEL (1 << 1)
#define DECL_MASK_OTHER (1 << 2) /* all other declarations */
#define DECL_MASK_ALL   (DECL_MASK_FUNC | DECL_MASK_FUNC_WITH_LABEL | DECL_MASK_OTHER)

static __exception int js_parse_statement_or_decl(JSParseState *s,
                                                  int decl_mask);

static __exception int js_parse_statement(JSParseState *s)
{
    return js_parse_statement_or_decl(s, 0);
}

static __exception int js_parse_block(JSParseState *s)
{
    if (js_parse_expect(s, '{'))
        return -1;
    if (s->token.val != '}') {
        push_scope(s);
        for(;;) {
            if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
                return -1;
            if (s->token.val == '}')
                break;
        }
        pop_scope(s);
    }
    if (next_token(s))
        return -1;
    return 0;
}

/* allowed parse_flags: PF_IN_ACCEPTED */
static __exception int js_parse_var(JSParseState *s, int parse_flags, int tok,
                                    BOOL export_flag)
{
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    JSAtom name = JS_ATOM_NULL;

    for (;;) {
        if (s->token.val == TOK_IDENT) {
            if (s->token.u.ident.is_reserved) {
                return js_parse_error_reserved_identifier(s);
            }
            name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (name == JS_ATOM_let && (tok == TOK_LET || tok == TOK_CONST)) {
                js_parse_error(s, "'let' is not a valid lexical identifier");
                goto var_error;
            }
            if (next_token(s))
                goto var_error;
            if (js_define_var(s, name, tok))
                goto var_error;
            if (export_flag) {
                if (!add_export_entry(s, s->cur_func->module, name, name,
                                      JS_EXPORT_TYPE_LOCAL))
                    goto var_error;
            }

            if (s->token.val == '=') {
                const uint8_t *source_ptr = s->token.ptr;
                if (next_token(s))
                    goto var_error;
                if (need_var_reference(s, tok)) {
                    /* Must make a reference for proper `with` semantics */
                    int opcode, scope, label;
                    JSAtom name1;

                    emit_op(s, OP_scope_get_var);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                    if (get_lvalue(s, &opcode, &scope, &name1, &label, NULL, FALSE, '=') < 0)
                        goto var_error;
                    if (js_parse_assign_expr2(s, parse_flags)) {
                        JS_FreeAtom(ctx, name1);
                        goto var_error;
                    }
                    set_object_name(s, name);
                    emit_source_pos(s, source_ptr);
                    put_lvalue(s, opcode, scope, name1, label,
                               PUT_LVALUE_NOKEEP, FALSE);
                } else {
                    if (js_parse_assign_expr2(s, parse_flags))
                        goto var_error;
                    set_object_name(s, name);
                    emit_source_pos(s, source_ptr);
                    emit_op(s, (tok == TOK_CONST || tok == TOK_LET) ?
                        OP_scope_put_var_init : OP_scope_put_var);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                }
            } else {
                if (tok == TOK_CONST) {
                    js_parse_error(s, "missing initializer for const variable");
                    goto var_error;
                }
                if (tok == TOK_LET) {
                    /* initialize lexical variable upon entering its scope */
                    emit_op(s, OP_undefined);
                    emit_op(s, OP_scope_put_var_init);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                }
            }
            JS_FreeAtom(ctx, name);
        } else {
            int skip_bits;
            if ((s->token.val == '[' || s->token.val == '{')
            &&  js_parse_skip_parens_token(s, &skip_bits, FALSE) == '=') {
                emit_op(s, OP_undefined);
                if (js_parse_destructuring_element(s, tok, 0, TRUE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE, export_flag) < 0)
                    return -1;
            } else {
                return js_parse_error(s, "variable name expected");
            }
        }
        if (s->token.val != ',')
            break;
        if (next_token(s))
            return -1;
    }
    return 0;

 var_error:
    JS_FreeAtom(ctx, name);
    return -1;
}

/* test if the current token is a label. Use simplistic look-ahead scanner */
static BOOL is_label(JSParseState *s)
{
    return (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved &&
            peek_token(s, FALSE) == ':');
}

/* test if the current token is a let keyword. Use simplistic look-ahead scanner */
static int is_let(JSParseState *s, int decl_mask)
{
    int res = FALSE;
    const uint8_t *last_token_ptr;
    
    if (token_is_pseudo_keyword(s, JS_ATOM_let)) {
        JSParsePos pos;
        js_parse_get_pos(s, &pos);
        for (;;) {
            last_token_ptr = s->token.ptr;
            if (next_token(s)) {
                res = -1;
                break;
            }
            if (s->token.val == '[') {
                /* let [ is a syntax restriction:
                   it never introduces an ExpressionStatement */
                res = TRUE;
                break;
            }
            if (s->token.val == '{' ||
                (s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved) ||
                s->token.val == TOK_LET ||
                s->token.val == TOK_YIELD ||
                s->token.val == TOK_AWAIT) {
                /* Check for possible ASI if not scanning for Declaration */
                /* XXX: should also check that `{` introduces a BindingPattern,
                   but Firefox does not and rejects eval("let=1;let\n{if(1)2;}") */
                if (!has_lf_in_range(last_token_ptr, s->token.ptr) ||
                    (decl_mask & DECL_MASK_OTHER)) {
                    res = TRUE;
                    break;
                }
                break;
            }
            break;
        }
        if (js_parse_seek_token(s, &pos)) {
            res = -1;
        }
    }
    return res;
}

/* XXX: handle IteratorClose when exiting the loop before the
   enumeration is done */
static __exception int js_parse_for_in_of(JSParseState *s, int label_name,
                                          BOOL is_async)
{
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    JSAtom var_name;
    BOOL has_initializer, is_for_of, has_destructuring;
    int tok, tok1, opcode, scope, block_scope_level;
    int label_next, label_expr, label_cont, label_body, label_break;
    int pos_next, pos_expr;
    BlockEnv break_entry;

    has_initializer = FALSE;
    has_destructuring = FALSE;
    is_for_of = FALSE;
    block_scope_level = fd->scope_level;
    label_cont = new_label(s);
    label_body = new_label(s);
    label_break = new_label(s);
    label_next = new_label(s);

    /* create scope for the lexical variables declared in the enumeration
       expressions. XXX: Not completely correct because of weird capturing
       semantics in `for (i of o) a.push(function(){return i})` */
    push_scope(s);

    /* local for_in scope starts here so individual elements
       can be closed in statement. */
    push_break_entry(s->cur_func, &break_entry,
                     label_name, label_break, label_cont, 1);
    break_entry.scope_level = block_scope_level;

    label_expr = emit_goto(s, OP_goto, -1);

    pos_next = s->cur_func->byte_code.size;
    emit_label(s, label_next);

    tok = s->token.val;
    switch (is_let(s, DECL_MASK_OTHER)) {
    case TRUE:
        tok = TOK_LET;
        break;
    case FALSE:
        break;
    default:
        return -1;
    }
    if (tok == TOK_VAR || tok == TOK_LET || tok == TOK_CONST) {
        if (next_token(s))
            return -1;

        if (!(s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)) {
            if (s->token.val == '[' || s->token.val == '{') {
                if (js_parse_destructuring_element(s, tok, 0, TRUE, -1, FALSE, FALSE) < 0)
                    return -1;
                has_destructuring = TRUE;
            } else {
                return js_parse_error(s, "variable name expected");
            }
            var_name = JS_ATOM_NULL;
        } else {
            var_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (next_token(s)) {
                JS_FreeAtom(s->ctx, var_name);
                return -1;
            }
            if (js_define_var(s, var_name, tok)) {
                JS_FreeAtom(s->ctx, var_name);
                return -1;
            }
            emit_op(s, (tok == TOK_CONST || tok == TOK_LET) ?
                    OP_scope_put_var_init : OP_scope_put_var);
            emit_atom(s, var_name);
            emit_u16(s, fd->scope_level);
        }
    } else if (!is_async && token_is_pseudo_keyword(s, JS_ATOM_async) &&
               peek_token(s, FALSE) == TOK_OF) {
        return js_parse_error(s, "'for of' expression cannot start with 'async'");
    } else {
        int skip_bits;
        if ((s->token.val == '[' || s->token.val == '{')
        &&  ((tok1 = js_parse_skip_parens_token(s, &skip_bits, FALSE)) == TOK_IN || tok1 == TOK_OF)) {
            if (js_parse_destructuring_element(s, 0, 0, TRUE, skip_bits & SKIP_HAS_ELLIPSIS, TRUE, FALSE) < 0)
                return -1;
        } else {
            int lvalue_label;
            if (js_parse_left_hand_side_expr(s))
                return -1;
            if (get_lvalue(s, &opcode, &scope, &var_name, &lvalue_label,
                           NULL, FALSE, TOK_FOR))
                return -1;
            put_lvalue(s, opcode, scope, var_name, lvalue_label,
                       PUT_LVALUE_NOKEEP_BOTTOM, FALSE);
        }
        var_name = JS_ATOM_NULL;
    }
    emit_goto(s, OP_goto, label_body);

    pos_expr = s->cur_func->byte_code.size;
    emit_label(s, label_expr);
    if (s->token.val == '=') {
        /* XXX: potential scoping issue if inside `with` statement */
        has_initializer = TRUE;
        /* parse and evaluate initializer prior to evaluating the
           object (only used with "for in" with a non lexical variable
           in non strict mode */
        if (next_token(s) || js_parse_assign_expr2(s, 0)) {
            JS_FreeAtom(ctx, var_name);
            return -1;
        }
        if (var_name != JS_ATOM_NULL) {
            emit_op(s, OP_scope_put_var);
            emit_atom(s, var_name);
            emit_u16(s, fd->scope_level);
        }
    }
    JS_FreeAtom(ctx, var_name);

    if (token_is_pseudo_keyword(s, JS_ATOM_of)) {
        is_for_of = TRUE;
        if (has_initializer)
            goto initializer_error;
    } else if (s->token.val == TOK_IN) {
        if (is_async)
            return js_parse_error(s, "'for await' loop should be used with 'of'");
        if (has_initializer &&
            (tok != TOK_VAR || (fd->js_mode & JS_MODE_STRICT) ||
             has_destructuring)) {
        initializer_error:
            return js_parse_error(s, "a declaration in the head of a for-%s loop can't have an initializer",
                                  is_for_of ? "of" : "in");
        }
    } else {
        return js_parse_error(s, "expected 'of' or 'in' in for control expression");
    }
    if (next_token(s))
        return -1;
    if (is_for_of) {
        if (js_parse_assign_expr(s))
            return -1;
    } else {
        if (js_parse_expr(s))
            return -1;
    }
    /* close the scope after having evaluated the expression so that
       the TDZ values are in the closures */
    close_scopes(s, s->cur_func->scope_level, block_scope_level);
    if (is_for_of) {
        /* set has_iterator after the iterable expression is parsed so
           that a yield in the expression does not try to close a
           not-yet-created iterator */
        break_entry.has_iterator = TRUE;
        break_entry.drop_count += 2;
        if (is_async)
            emit_op(s, OP_for_await_of_start);
        else
            emit_op(s, OP_for_of_start);
        /* on stack: enum_rec */
    } else {
        emit_op(s, OP_for_in_start);
        /* on stack: enum_obj */
    }
    emit_goto(s, OP_goto, label_cont);

    if (js_parse_expect(s, ')'))
        return -1;

    if (OPTIMIZE) {
        /* move the `next` code here */
        DynBuf *bc = &s->cur_func->byte_code;
        int chunk_size = pos_expr - pos_next;
        int offset = bc->size - pos_next;
        int i;
        if (dbuf_claim(bc, chunk_size))
            return -1;
        dbuf_put(bc, bc->buf + pos_next, chunk_size);
        memset(bc->buf + pos_next, OP_nop, chunk_size);
        /* `next` part ends with a goto */
        s->cur_func->last_opcode_pos = bc->size - 5;
        /* relocate labels */
        for (i = label_cont; i < s->cur_func->label_count; i++) {
            LabelSlot *ls = &s->cur_func->label_slots[i];
            if (ls->pos >= pos_next && ls->pos < pos_expr)
                ls->pos += offset;
        }
    }

    emit_label(s, label_body);
    if (js_parse_statement(s))
        return -1;

    close_scopes(s, s->cur_func->scope_level, block_scope_level);

    emit_label(s, label_cont);
    if (is_for_of) {
        if (is_async) {
            /* stack: iter_obj next catch_offset */
            /* call the next method */
            emit_op(s, OP_for_await_of_next); 
            /* get the result of the promise */
            emit_op(s, OP_await);
            /* unwrap the value and done values */
            emit_op(s, OP_iterator_get_value_done);
        } else {
            emit_op(s, OP_for_of_next);
            emit_u8(s, 0);
        }
    } else {
        emit_op(s, OP_for_in_next);
    }
    /* on stack: enum_rec / enum_obj value bool */
    emit_goto(s, OP_if_false, label_next);
    /* drop the undefined value from for_xx_next */
    emit_op(s, OP_drop);

    emit_label(s, label_break);
    if (is_for_of) {
        /* close and drop enum_rec */
        emit_op(s, OP_iterator_close);
    } else {
        emit_op(s, OP_drop);
    }
    pop_break_entry(s->cur_func);
    pop_scope(s);
    return 0;
}

static void set_eval_ret_undefined(JSParseState *s)
{
    if (s->cur_func->eval_ret_idx >= 0) {
        emit_op(s, OP_undefined);
        emit_op(s, OP_put_loc);
        emit_u16(s, s->cur_func->eval_ret_idx);
    }
}

static __exception int js_parse_statement_or_decl(JSParseState *s,
                                                  int decl_mask)
{
    JSContext *ctx = s->ctx;
    JSAtom label_name;
    int tok;

    /* specific label handling */
    /* XXX: support multiple labels on loop statements */
    label_name = JS_ATOM_NULL;
    if (is_label(s)) {
        BlockEnv *be;

        label_name = JS_DupAtom(ctx, s->token.u.ident.atom);

        for (be = s->cur_func->top_break; be; be = be->prev) {
            if (be->label_name == label_name) {
                js_parse_error(s, "duplicate label name");
                goto fail;
            }
        }

        if (next_token(s))
            goto fail;
        if (js_parse_expect(s, ':'))
            goto fail;
        if (s->token.val != TOK_FOR
        &&  s->token.val != TOK_DO
        &&  s->token.val != TOK_WHILE) {
            /* labelled regular statement */
            int label_break, mask;
            BlockEnv break_entry;

            label_break = new_label(s);
            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, -1, 0);
            break_entry.is_regular_stmt = TRUE;
            if (!(s->cur_func->js_mode & JS_MODE_STRICT) &&
                (decl_mask & DECL_MASK_FUNC_WITH_LABEL)) {
                mask = DECL_MASK_FUNC | DECL_MASK_FUNC_WITH_LABEL;
            } else {
                mask = 0;
            }
            if (js_parse_statement_or_decl(s, mask))
                goto fail;
            emit_label(s, label_break);
            pop_break_entry(s->cur_func);
            goto done;
        }
    }

    switch(tok = s->token.val) {
    case '{':
        if (js_parse_block(s))
            goto fail;
        break;
    case TOK_RETURN:
        {
            const uint8_t *op_token_ptr;
            if (s->cur_func->is_eval) {
                js_parse_error(s, "return not in a function");
                goto fail;
            }
            if (s->cur_func->func_type == JS_PARSE_FUNC_CLASS_STATIC_INIT) {
                js_parse_error(s, "return in a static initializer block");
                goto fail;
            }
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                goto fail;
            if (s->token.val != ';' && s->token.val != '}' && !s->got_lf) {
                if (js_parse_expr(s))
                    goto fail;
                emit_source_pos(s, op_token_ptr);
                emit_return(s, TRUE);
            } else {
                emit_source_pos(s, op_token_ptr);
                emit_return(s, FALSE);
            }
            if (js_parse_expect_semi(s))
                goto fail;
        }
        break;
    case TOK_THROW:
        {
            const uint8_t *op_token_ptr;
            op_token_ptr = s->token.ptr;
            if (next_token(s))
                goto fail;
            if (s->got_lf) {
                js_parse_error(s, "line terminator not allowed after throw");
                goto fail;
            }
            if (js_parse_expr(s))
                goto fail;
            emit_source_pos(s, op_token_ptr);
            emit_op(s, OP_throw);
            if (js_parse_expect_semi(s))
                goto fail;
        }
        break;
    case TOK_LET:
    case TOK_CONST:
    haslet:
        if (!(decl_mask & DECL_MASK_OTHER)) {
            js_parse_error(s, "lexical declarations can't appear in single-statement context");
            goto fail;
        }
        /* fall thru */
    case TOK_VAR:
        if (next_token(s))
            goto fail;
        if (js_parse_var(s, TRUE, tok, FALSE))
            goto fail;
        if (js_parse_expect_semi(s))
            goto fail;
        break;
    case TOK_IF:
        {
            int label1, label2, mask;
            if (next_token(s))
                goto fail;
            /* create a new scope for `let f;if(1) function f(){}` */
            push_scope(s);
            set_eval_ret_undefined(s);
            if (js_parse_expr_paren(s))
                goto fail;
            label1 = emit_goto(s, OP_if_false, -1);
            if (s->cur_func->js_mode & JS_MODE_STRICT)
                mask = 0;
            else
                mask = DECL_MASK_FUNC; /* Annex B.3.4 */

            if (js_parse_statement_or_decl(s, mask))
                goto fail;

            if (s->token.val == TOK_ELSE) {
                label2 = emit_goto(s, OP_goto, -1);
                if (next_token(s))
                    goto fail;

                emit_label(s, label1);
                if (js_parse_statement_or_decl(s, mask))
                    goto fail;

                label1 = label2;
            }
            emit_label(s, label1);
            pop_scope(s);
        }
        break;
    case TOK_WHILE:
        {
            int label_cont, label_break;
            BlockEnv break_entry;

            label_cont = new_label(s);
            label_break = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);

            emit_label(s, label_cont);
            if (js_parse_expr_paren(s))
                goto fail;
            emit_goto(s, OP_if_false, label_break);

            if (js_parse_statement(s))
                goto fail;
            emit_goto(s, OP_goto, label_cont);

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
        }
        break;
    case TOK_DO:
        {
            int label_cont, label_break, label1;
            BlockEnv break_entry;

            label_cont = new_label(s);
            label_break = new_label(s);
            label1 = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            if (next_token(s))
                goto fail;

            emit_label(s, label1);

            set_eval_ret_undefined(s);

            if (js_parse_statement(s))
                goto fail;

            emit_label(s, label_cont);
            if (js_parse_expect(s, TOK_WHILE))
                goto fail;
            if (js_parse_expr_paren(s))
                goto fail;
            /* Insert semicolon if missing */
            if (s->token.val == ';') {
                if (next_token(s))
                    goto fail;
            }
            emit_goto(s, OP_if_true, label1);

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
        }
        break;
    case TOK_FOR:
        {
            int label_cont, label_break, label_body, label_test;
            int pos_cont, pos_body, block_scope_level;
            BlockEnv break_entry;
            int tok, bits;
            BOOL is_async;

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);
            bits = 0;
            is_async = FALSE;
            if (s->token.val == '(') {
                js_parse_skip_parens_token(s, &bits, FALSE);
            } else if (s->token.val == TOK_AWAIT) {
                if (!(s->cur_func->func_kind & JS_FUNC_ASYNC)) {
                    js_parse_error(s, "for await is only valid in asynchronous functions");
                    goto fail;
                }
                is_async = TRUE;
                if (next_token(s))
                    goto fail;
                s->cur_func->has_await = TRUE;
            }
            if (js_parse_expect(s, '('))
                goto fail;

            if (!(bits & SKIP_HAS_SEMI)) {
                /* parse for/in or for/of */
                if (js_parse_for_in_of(s, label_name, is_async))
                    goto fail;
                break;
            }
            block_scope_level = s->cur_func->scope_level;

            /* create scope for the lexical variables declared in the initial,
               test and increment expressions */
            push_scope(s);
            /* initial expression */
            tok = s->token.val;
            if (tok != ';') {
                switch (is_let(s, DECL_MASK_OTHER)) {
                case TRUE:
                    tok = TOK_LET;
                    break;
                case FALSE:
                    break;
                default:
                    goto fail;
                }
                if (tok == TOK_VAR || tok == TOK_LET || tok == TOK_CONST) {
                    if (next_token(s))
                        goto fail;
                    if (js_parse_var(s, FALSE, tok, FALSE))
                        goto fail;
                } else {
                    if (js_parse_expr2(s, FALSE))
                        goto fail;
                    emit_op(s, OP_drop);
                }

                /* close the closures before the first iteration */
                close_scopes(s, s->cur_func->scope_level, block_scope_level);
            }
            if (js_parse_expect(s, ';'))
                goto fail;

            label_test = new_label(s);
            label_cont = new_label(s);
            label_body = new_label(s);
            label_break = new_label(s);

            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, label_cont, 0);

            /* test expression */
            if (s->token.val == ';') {
                /* no test expression */
                label_test = label_body;
            } else {
                emit_label(s, label_test);
                if (js_parse_expr(s))
                    goto fail;
                emit_goto(s, OP_if_false, label_break);
            }
            if (js_parse_expect(s, ';'))
                goto fail;

            if (s->token.val == ')') {
                /* no end expression */
                break_entry.label_cont = label_cont = label_test;
                pos_cont = 0; /* avoid warning */
            } else {
                /* skip the end expression */
                emit_goto(s, OP_goto, label_body);

                pos_cont = s->cur_func->byte_code.size;
                emit_label(s, label_cont);
                if (js_parse_expr(s))
                    goto fail;
                emit_op(s, OP_drop);
                if (label_test != label_body)
                    emit_goto(s, OP_goto, label_test);
            }
            if (js_parse_expect(s, ')'))
                goto fail;

            pos_body = s->cur_func->byte_code.size;
            emit_label(s, label_body);
            if (js_parse_statement(s))
                goto fail;

            /* close the closures before the next iteration */
            /* XXX: check continue case */
            close_scopes(s, s->cur_func->scope_level, block_scope_level);

            if (OPTIMIZE && label_test != label_body && label_cont != label_test) {
                /* move the increment code here */
                DynBuf *bc = &s->cur_func->byte_code;
                int chunk_size = pos_body - pos_cont;
                int offset = bc->size - pos_cont;
                int i;
                if (dbuf_claim(bc, chunk_size))
                    goto fail;
                dbuf_put(bc, bc->buf + pos_cont, chunk_size);
                memset(bc->buf + pos_cont, OP_nop, chunk_size);
                /* increment part ends with a goto */
                s->cur_func->last_opcode_pos = bc->size - 5;
                /* relocate labels */
                for (i = label_cont; i < s->cur_func->label_count; i++) {
                    LabelSlot *ls = &s->cur_func->label_slots[i];
                    if (ls->pos >= pos_cont && ls->pos < pos_body)
                        ls->pos += offset;
                }
            } else {
                emit_goto(s, OP_goto, label_cont);
            }

            emit_label(s, label_break);

            pop_break_entry(s->cur_func);
            pop_scope(s);
        }
        break;
    case TOK_BREAK:
    case TOK_CONTINUE:
        {
            int is_cont = s->token.val - TOK_BREAK;
            int label;

            if (next_token(s))
                goto fail;
            if (!s->got_lf && s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)
                label = s->token.u.ident.atom;
            else
                label = JS_ATOM_NULL;
            if (emit_break(s, label, is_cont))
                goto fail;
            if (label != JS_ATOM_NULL) {
                if (next_token(s))
                    goto fail;
            }
            if (js_parse_expect_semi(s))
                goto fail;
        }
        break;
    case TOK_SWITCH:
        {
            int label_case, label_break, label1;
            int default_label_pos;
            BlockEnv break_entry;

            if (next_token(s))
                goto fail;

            set_eval_ret_undefined(s);
            if (js_parse_expr_paren(s))
                goto fail;

            push_scope(s);
            label_break = new_label(s);
            push_break_entry(s->cur_func, &break_entry,
                             label_name, label_break, -1, 1);

            if (js_parse_expect(s, '{'))
                goto fail;

            default_label_pos = -1;
            label_case = -1;
            while (s->token.val != '}') {
                if (s->token.val == TOK_CASE) {
                    label1 = -1;
                    if (label_case >= 0) {
                        /* skip the case if needed */
                        label1 = emit_goto(s, OP_goto, -1);
                    }
                    emit_label(s, label_case);
                    label_case = -1;
                    for (;;) {
                        /* parse a sequence of case clauses */
                        if (next_token(s))
                            goto fail;
                        emit_op(s, OP_dup);
                        if (js_parse_expr(s))
                            goto fail;
                        if (js_parse_expect(s, ':'))
                            goto fail;
                        emit_op(s, OP_strict_eq);
                        if (s->token.val == TOK_CASE) {
                            label1 = emit_goto(s, OP_if_true, label1);
                        } else {
                            label_case = emit_goto(s, OP_if_false, -1);
                            emit_label(s, label1);
                            break;
                        }
                    }
                } else if (s->token.val == TOK_DEFAULT) {
                    if (next_token(s))
                        goto fail;
                    if (js_parse_expect(s, ':'))
                        goto fail;
                    if (default_label_pos >= 0) {
                        js_parse_error(s, "duplicate default");
                        goto fail;
                    }
                    if (label_case < 0) {
                        /* falling thru direct from switch expression */
                        label_case = emit_goto(s, OP_goto, -1);
                    }
                    /* Emit a dummy label opcode. Label will be patched after
                       the end of the switch body. Do not use emit_label(s, 0)
                       because it would clobber label 0 address, preventing
                       proper optimizer operation.
                     */
                    emit_op(s, OP_label);
                    emit_u32(s, 0);
                    default_label_pos = s->cur_func->byte_code.size - 4;
                } else {
                    if (label_case < 0) {
                        /* falling thru direct from switch expression */
                        js_parse_error(s, "invalid switch statement");
                        goto fail;
                    }
                    if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
                        goto fail;
                }
            }
            if (js_parse_expect(s, '}'))
                goto fail;
            if (default_label_pos >= 0) {
                /* Ugly patch for the `default` label, shameful and risky */
                put_u32(s->cur_func->byte_code.buf + default_label_pos,
                        label_case);
                s->cur_func->label_slots[label_case].pos = default_label_pos + 4;
            } else {
                emit_label(s, label_case);
            }
            emit_label(s, label_break);
            emit_op(s, OP_drop); /* drop the switch expression */

            pop_break_entry(s->cur_func);
            pop_scope(s);
        }
        break;
    case TOK_TRY:
        {
            int label_catch, label_catch2, label_finally, label_end;
            JSAtom name;
            BlockEnv block_env;

            set_eval_ret_undefined(s);
            if (next_token(s))
                goto fail;
            label_catch = new_label(s);
            label_catch2 = new_label(s);
            label_finally = new_label(s);
            label_end = new_label(s);

            emit_goto(s, OP_catch, label_catch);

            push_break_entry(s->cur_func, &block_env,
                             JS_ATOM_NULL, -1, -1, 1);
            block_env.label_finally = label_finally;

            if (js_parse_block(s))
                goto fail;

            pop_break_entry(s->cur_func);

            if (js_is_live_code(s)) {
                /* drop the catch offset */
                emit_op(s, OP_drop);
                /* must push dummy value to keep same stack size */
                emit_op(s, OP_undefined);
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_drop);

                emit_goto(s, OP_goto, label_end);
            }

            if (s->token.val == TOK_CATCH) {
                if (next_token(s))
                    goto fail;

                push_scope(s);  /* catch variable */
                emit_label(s, label_catch);

                if (s->token.val == '{') {
                    /* support optional-catch-binding feature */
                    emit_op(s, OP_drop);    /* pop the exception object */
                } else {
                    if (js_parse_expect(s, '('))
                        goto fail;
                    if (!(s->token.val == TOK_IDENT && !s->token.u.ident.is_reserved)) {
                        if (s->token.val == '[' || s->token.val == '{') {
                            /* XXX: TOK_LET is not completely correct */
                            if (js_parse_destructuring_element(s, TOK_LET, 0, TRUE, -1, TRUE, FALSE) < 0)
                                goto fail;
                        } else {
                            js_parse_error(s, "identifier expected");
                            goto fail;
                        }
                    } else {
                        name = JS_DupAtom(ctx, s->token.u.ident.atom);
                        if (next_token(s)
                        ||  js_define_var(s, name, TOK_CATCH) < 0) {
                            JS_FreeAtom(ctx, name);
                            goto fail;
                        }
                        /* store the exception value in the catch variable */
                        emit_op(s, OP_scope_put_var);
                        emit_u32(s, name);
                        emit_u16(s, s->cur_func->scope_level);
                    }
                    if (js_parse_expect(s, ')'))
                        goto fail;
                }
                /* XXX: should keep the address to nop it out if there is no finally block */
                emit_goto(s, OP_catch, label_catch2);

                push_scope(s);  /* catch block */
                push_break_entry(s->cur_func, &block_env, JS_ATOM_NULL,
                                 -1, -1, 1);
                block_env.label_finally = label_finally;

                if (js_parse_block(s))
                    goto fail;

                pop_break_entry(s->cur_func);
                pop_scope(s);  /* catch block */
                pop_scope(s);  /* catch variable */

                if (js_is_live_code(s)) {
                    /* drop the catch2 offset */
                    emit_op(s, OP_drop);
                    /* XXX: should keep the address to nop it out if there is no finally block */
                    /* must push dummy value to keep same stack size */
                    emit_op(s, OP_undefined);
                    emit_goto(s, OP_gosub, label_finally);
                    emit_op(s, OP_drop);
                    emit_goto(s, OP_goto, label_end);
                }
                /* catch exceptions thrown in the catch block to execute the
                 * finally clause and rethrow the exception */
                emit_label(s, label_catch2);
                /* catch value is at TOS, no need to push undefined */
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_throw);

            } else if (s->token.val == TOK_FINALLY) {
                /* finally without catch : execute the finally clause
                 * and rethrow the exception */
                emit_label(s, label_catch);
                /* catch value is at TOS, no need to push undefined */
                emit_goto(s, OP_gosub, label_finally);
                emit_op(s, OP_throw);
            } else {
                js_parse_error(s, "expecting catch or finally");
                goto fail;
            }
            emit_label(s, label_finally);
            if (s->token.val == TOK_FINALLY) {
                int saved_eval_ret_idx = 0; /* avoid warning */

                if (next_token(s))
                    goto fail;
                /* on the stack: ret_value gosub_ret_value */
                push_break_entry(s->cur_func, &block_env, JS_ATOM_NULL,
                                 -1, -1, 2);

                if (s->cur_func->eval_ret_idx >= 0) {
                    /* 'finally' updates eval_ret only if not a normal
                       termination */
                    saved_eval_ret_idx =
                        add_var(s->ctx, s->cur_func, JS_ATOM__ret_);
                    if (saved_eval_ret_idx < 0)
                        goto fail;
                    emit_op(s, OP_get_loc);
                    emit_u16(s, s->cur_func->eval_ret_idx);
                    emit_op(s, OP_put_loc);
                    emit_u16(s, saved_eval_ret_idx);
                    set_eval_ret_undefined(s);
                }

                if (js_parse_block(s))
                    goto fail;

                if (s->cur_func->eval_ret_idx >= 0) {
                    emit_op(s, OP_get_loc);
                    emit_u16(s, saved_eval_ret_idx);
                    emit_op(s, OP_put_loc);
                    emit_u16(s, s->cur_func->eval_ret_idx);
                }
                pop_break_entry(s->cur_func);
            }
            emit_op(s, OP_ret);
            emit_label(s, label_end);
        }
        break;
    case ';':
        /* empty statement */
        if (next_token(s))
            goto fail;
        break;
    case TOK_WITH:
        if (s->cur_func->js_mode & JS_MODE_STRICT) {
            js_parse_error(s, "invalid keyword: with");
            goto fail;
        } else {
            int with_idx;

            if (next_token(s))
                goto fail;

            if (js_parse_expr_paren(s))
                goto fail;

            push_scope(s);
            with_idx = define_var(s, s->cur_func, JS_ATOM__with_,
                                  JS_VAR_DEF_WITH);
            if (with_idx < 0)
                goto fail;
            emit_op(s, OP_to_object);
            emit_op(s, OP_put_loc);
            emit_u16(s, with_idx);

            set_eval_ret_undefined(s);
            if (js_parse_statement(s))
                goto fail;

            /* Popping scope drops lexical context for the with object variable */
            pop_scope(s);
        }
        break;
    case TOK_FUNCTION:
        /* ES6 Annex B.3.2 and B.3.3 semantics */
        if (!(decl_mask & DECL_MASK_FUNC))
            goto func_decl_error;
        if (!(decl_mask & DECL_MASK_OTHER) && peek_token(s, FALSE) == '*')
            goto func_decl_error;
        goto parse_func_var;
    case TOK_IDENT:
        if (s->token.u.ident.is_reserved) {
            js_parse_error_reserved_identifier(s);
            goto fail;
        }
        /* Determine if `let` introduces a Declaration or an ExpressionStatement */
        switch (is_let(s, decl_mask)) {
        case TRUE:
            tok = TOK_LET;
            goto haslet;
        case FALSE:
            break;
        default:
            goto fail;
        }
        if (token_is_pseudo_keyword(s, JS_ATOM_async) &&
            peek_token(s, TRUE) == TOK_FUNCTION) {
            if (!(decl_mask & DECL_MASK_OTHER)) {
            func_decl_error:
                js_parse_error(s, "function declarations can't appear in single-statement context");
                goto fail;
            }
        parse_func_var:
            if (js_parse_function_decl(s, JS_PARSE_FUNC_VAR,
                                       JS_FUNC_NORMAL, JS_ATOM_NULL,
                                       s->token.ptr))
                goto fail;
            break;
        }
        goto hasexpr;

    case TOK_CLASS:
        if (!(decl_mask & DECL_MASK_OTHER)) {
            js_parse_error(s, "class declarations can't appear in single-statement context");
            goto fail;
        }
        if (js_parse_class(s, FALSE, JS_PARSE_EXPORT_NONE))
            return -1;
        break;

    case TOK_DEBUGGER:
        /* currently no debugger, so just skip the keyword */
        if (next_token(s))
            goto fail;
        if (js_parse_expect_semi(s))
            goto fail;
        break;

    case TOK_ENUM:
    case TOK_EXPORT:
    case TOK_EXTENDS:
        js_unsupported_keyword(s, s->token.u.ident.atom);
        goto fail;

    default:
    hasexpr:
        emit_source_pos(s, s->token.ptr);
        if (js_parse_expr(s))
            goto fail;
        if (s->cur_func->eval_ret_idx >= 0) {
            /* store the expression value so that it can be returned
               by eval() */
            emit_op(s, OP_put_loc);
            emit_u16(s, s->cur_func->eval_ret_idx);
        } else {
            emit_op(s, OP_drop); /* drop the result */
        }
        if (js_parse_expect_semi(s))
            goto fail;
        break;
    }
done:
    JS_FreeAtom(ctx, label_name);
    return 0;
fail:
    JS_FreeAtom(ctx, label_name);
    return -1;
}

static __exception int js_parse_with_clause(JSParseState *s, JSReqModuleEntry *rme)
{
    JSContext *ctx = s->ctx;
    JSAtom key;
    int ret;
    const uint8_t *key_token_ptr;
    
    if (next_token(s))
        return -1;
    if (js_parse_expect(s, '{'))
        return -1;
    while (s->token.val != '}') {
        key_token_ptr = s->token.ptr;
        if (s->token.val == TOK_STRING) {
            key = JS_ValueToAtom(ctx, s->token.u.str.str);
            if (key == JS_ATOM_NULL)
                return -1;
        } else {
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            key = JS_DupAtom(ctx, s->token.u.ident.atom);
        }
        if (next_token(s))
            return -1;
        if (js_parse_expect(s, ':')) {
            JS_FreeAtom(ctx, key);
            return -1;
        }
        if (s->token.val != TOK_STRING) {
            js_parse_error_pos(s, key_token_ptr, "string expected");
            return -1;
        }
        if (JS_IsUndefined(rme->attributes)) {
            JSValue attributes = JS_NewObjectProto(ctx, JS_NULL);
            if (JS_IsException(attributes)) {
                JS_FreeAtom(ctx, key);
                return -1;
            }
            rme->attributes = attributes;
        }
        ret = JS_HasProperty(ctx, rme->attributes, key);
        if (ret != 0) {
            JS_FreeAtom(ctx, key);
            if (ret < 0)
                return -1;
            else
                return js_parse_error(s, "duplicate with key");
        }
        ret = JS_DefinePropertyValue(ctx, rme->attributes, key,
                                     JS_DupValue(ctx, s->token.u.str.str), JS_PROP_C_W_E);
        JS_FreeAtom(ctx, key);
        if (ret < 0)
            return -1;
        if (next_token(s))
            return -1;
        if (s->token.val != ',')
            break;
        if (next_token(s))
            return -1;
    }
    if (!JS_IsUndefined(rme->attributes) &&
        ctx->rt->module_check_attrs &&
        ctx->rt->module_check_attrs(ctx, ctx->rt->module_loader_opaque, rme->attributes) < 0) {
        return -1;
    }
    return js_parse_expect(s, '}');
}

/* return the module index in m->req_module_entries[] or < 0 if error */
static __exception int js_parse_from_clause(JSParseState *s, JSModuleDef *m)
{
    JSAtom module_name;
    int idx;

    if (!token_is_pseudo_keyword(s, JS_ATOM_from)) {
        js_parse_error(s, "from clause expected");
        return -1;
    }
    if (next_token(s))
        return -1;
    if (s->token.val != TOK_STRING) {
        js_parse_error(s, "string expected");
        return -1;
    }
    module_name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
    if (module_name == JS_ATOM_NULL)
        return -1;
    if (next_token(s)) {
        JS_FreeAtom(s->ctx, module_name);
        return -1;
    }

    idx = add_req_module_entry(s->ctx, m, module_name);
    JS_FreeAtom(s->ctx, module_name);
    if (idx < 0)
        return -1;
    if (s->token.val == TOK_WITH) {
        if (js_parse_with_clause(s, &m->req_module_entries[idx]))
            return -1;
    }
    return idx;
}

static __exception int js_parse_export(JSParseState *s)
{
    JSContext *ctx = s->ctx;
    JSModuleDef *m = s->cur_func->module;
    JSAtom local_name, export_name;
    int first_export, idx, i, tok;
    JSExportEntry *me;

    if (next_token(s))
        return -1;

    tok = s->token.val;
    if (tok == TOK_CLASS) {
        return js_parse_class(s, FALSE, JS_PARSE_EXPORT_NAMED);
    } else if (tok == TOK_FUNCTION ||
               (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                peek_token(s, TRUE) == TOK_FUNCTION)) {
        return js_parse_function_decl2(s, JS_PARSE_FUNC_STATEMENT,
                                       JS_FUNC_NORMAL, JS_ATOM_NULL,
                                       s->token.ptr,
                                       JS_PARSE_EXPORT_NAMED, NULL);
    }

    if (next_token(s))
        return -1;

    switch(tok) {
    case '{':
        first_export = m->export_entries_count;
        while (s->token.val != '}') {
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            export_name = JS_ATOM_NULL;
            if (next_token(s))
                goto fail;
            if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
                if (next_token(s))
                    goto fail;
                if (s->token.val == TOK_STRING) {
                    if (js_string_find_invalid_codepoint(JS_VALUE_GET_STRING(s->token.u.str.str)) >= 0) {
                        js_parse_error(s, "contains unpaired surrogate");
                        goto fail;
                    }
                    export_name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
                    if (export_name == JS_ATOM_NULL)
                        goto fail;
                } else {
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        goto fail;
                    }
                    export_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                }
                if (next_token(s)) {
                fail:
                    JS_FreeAtom(ctx, local_name);
                fail1:
                    JS_FreeAtom(ctx, export_name);
                    return -1;
                }
            } else {
                export_name = JS_DupAtom(ctx, local_name);
            }
            me = add_export_entry(s, m, local_name, export_name,
                                  JS_EXPORT_TYPE_LOCAL);
            JS_FreeAtom(ctx, local_name);
            JS_FreeAtom(ctx, export_name);
            if (!me)
                return -1;
            if (s->token.val != ',')
                break;
            if (next_token(s))
                return -1;
        }
        if (js_parse_expect(s, '}'))
            return -1;
        if (token_is_pseudo_keyword(s, JS_ATOM_from)) {
            idx = js_parse_from_clause(s, m);
            if (idx < 0)
                return -1;
            for(i = first_export; i < m->export_entries_count; i++) {
                me = &m->export_entries[i];
                me->export_type = JS_EXPORT_TYPE_INDIRECT;
                me->u.req_module_idx = idx;
            }
        }
        break;
    case '*':
        if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
            /* export ns from */
            if (next_token(s))
                return -1;
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            export_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (next_token(s))
                goto fail1;
            idx = js_parse_from_clause(s, m);
            if (idx < 0)
                goto fail1;
            me = add_export_entry(s, m, JS_ATOM__star_, export_name,
                                  JS_EXPORT_TYPE_INDIRECT);
            JS_FreeAtom(ctx, export_name);
            if (!me)
                return -1;
            me->u.req_module_idx = idx;
        } else {
            idx = js_parse_from_clause(s, m);
            if (idx < 0)
                return -1;
            if (add_star_export_entry(ctx, m, idx) < 0)
                return -1;
        }
        break;
    case TOK_DEFAULT:
        if (s->token.val == TOK_CLASS) {
            return js_parse_class(s, FALSE, JS_PARSE_EXPORT_DEFAULT);
        } else if (s->token.val == TOK_FUNCTION ||
                   (token_is_pseudo_keyword(s, JS_ATOM_async) &&
                    peek_token(s, TRUE) == TOK_FUNCTION)) {
            return js_parse_function_decl2(s, JS_PARSE_FUNC_STATEMENT,
                                           JS_FUNC_NORMAL, JS_ATOM_NULL,
                                           s->token.ptr,
                                           JS_PARSE_EXPORT_DEFAULT, NULL);
        } else {
            if (js_parse_assign_expr(s))
                return -1;
        }
        /* set the name of anonymous functions */
        set_object_name(s, JS_ATOM_default);

        /* store the value in the _default_ global variable and export
           it */
        local_name = JS_ATOM__default_;
        if (define_var(s, s->cur_func, local_name, JS_VAR_DEF_LET) < 0)
            return -1;
        emit_op(s, OP_scope_put_var_init);
        emit_atom(s, local_name);
        emit_u16(s, 0);

        if (!add_export_entry(s, m, local_name, JS_ATOM_default,
                              JS_EXPORT_TYPE_LOCAL))
            return -1;
        break;
    case TOK_VAR:
    case TOK_LET:
    case TOK_CONST:
        return js_parse_var(s, TRUE, tok, TRUE);
    default:
        return js_parse_error(s, "invalid export syntax");
    }
    return js_parse_expect_semi(s);
}

static int add_import(JSParseState *s, JSModuleDef *m,
                      JSAtom local_name, JSAtom import_name, BOOL is_star)
{
    JSContext *ctx = s->ctx;
    int i, var_idx;
    JSImportEntry *mi;

    if (local_name == JS_ATOM_arguments || local_name == JS_ATOM_eval)
        return js_parse_error(s, "invalid import binding");

    if (local_name != JS_ATOM_default) {
        for (i = 0; i < s->cur_func->closure_var_count; i++) {
            if (s->cur_func->closure_var[i].var_name == local_name)
                return js_parse_error(s, "duplicate import binding");
        }
    }

    var_idx = add_closure_var(ctx, s->cur_func,
                              is_star ? JS_CLOSURE_MODULE_DECL : JS_CLOSURE_MODULE_IMPORT,
                              m->import_entries_count,
                              local_name, TRUE, TRUE, JS_VAR_NORMAL);
    if (var_idx < 0)
        return -1;
    if (js_resize_array(ctx, (void **)&m->import_entries,
                        sizeof(JSImportEntry),
                        &m->import_entries_size,
                        m->import_entries_count + 1))
        return -1;
    mi = &m->import_entries[m->import_entries_count++];
    mi->import_name = JS_DupAtom(ctx, import_name);
    mi->var_idx = var_idx;
    mi->is_star = is_star;
    return 0;
}

static __exception int js_parse_import(JSParseState *s)
{
    JSContext *ctx = s->ctx;
    JSModuleDef *m = s->cur_func->module;
    JSAtom local_name, import_name, module_name;
    int first_import, i, idx;

    if (next_token(s))
        return -1;

    first_import = m->import_entries_count;
    if (s->token.val == TOK_STRING) {
        module_name = JS_ValueToAtom(ctx, s->token.u.str.str);
        if (module_name == JS_ATOM_NULL)
            return -1;
        if (next_token(s)) {
            JS_FreeAtom(ctx, module_name);
            return -1;
        }
        idx = add_req_module_entry(ctx, m, module_name);
        JS_FreeAtom(ctx, module_name);
        if (idx < 0)
            return -1;
        if (s->token.val == TOK_WITH) {
            if (js_parse_with_clause(s, &m->req_module_entries[idx]))
                return -1;
        }
    } else {
        if (s->token.val == TOK_IDENT) {
            if (s->token.u.ident.is_reserved) {
                return js_parse_error_reserved_identifier(s);
            }
            /* "default" import */
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            import_name = JS_ATOM_default;
            if (next_token(s))
                goto fail;
            if (add_import(s, m, local_name, import_name, FALSE))
                goto fail;
            JS_FreeAtom(ctx, local_name);

            if (s->token.val != ',')
                goto end_import_clause;
            if (next_token(s))
                return -1;
        }

        if (s->token.val == '*') {
            /* name space import */
            if (next_token(s))
                return -1;
            if (!token_is_pseudo_keyword(s, JS_ATOM_as))
                return js_parse_error(s, "expecting 'as'");
            if (next_token(s))
                return -1;
            if (!token_is_ident(s->token.val)) {
                js_parse_error(s, "identifier expected");
                return -1;
            }
            local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            import_name = JS_ATOM__star_;
            if (next_token(s))
                goto fail;
            if (add_import(s, m, local_name, import_name, TRUE))
                goto fail;
            JS_FreeAtom(ctx, local_name);
        } else if (s->token.val == '{') {
            if (next_token(s))
                return -1;

            while (s->token.val != '}') {
                BOOL is_string;
                if (s->token.val == TOK_STRING) {
                    is_string = TRUE;
                    if (js_string_find_invalid_codepoint(JS_VALUE_GET_STRING(s->token.u.str.str)) >= 0) {
                        js_parse_error(s, "contains unpaired surrogate");
                        return -1;
                    }
                    import_name = JS_ValueToAtom(s->ctx, s->token.u.str.str);
                    if (import_name == JS_ATOM_NULL)
                        return -1;
                } else {
                    is_string = FALSE;
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        return -1;
                    }
                    import_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                }
                local_name = JS_ATOM_NULL;
                if (next_token(s))
                    goto fail;
                if (token_is_pseudo_keyword(s, JS_ATOM_as)) {
                    if (next_token(s))
                        goto fail;
                    if (!token_is_ident(s->token.val)) {
                        js_parse_error(s, "identifier expected");
                        goto fail;
                    }
                    local_name = JS_DupAtom(ctx, s->token.u.ident.atom);
                    if (next_token(s))
                        goto fail;
                } else {
                    if (is_string) {
                        js_parse_error(s, "expecting 'as'");
                    fail:
                        JS_FreeAtom(ctx, local_name);
                        JS_FreeAtom(ctx, import_name);
                        return -1;
                    }
                    local_name = JS_DupAtom(ctx, import_name);
                }
                if (add_import(s, m, local_name, import_name, FALSE))
                    goto fail;
                JS_FreeAtom(ctx, local_name);
                JS_FreeAtom(ctx, import_name);
                if (s->token.val != ',')
                    break;
                if (next_token(s))
                    return -1;
            }
            if (js_parse_expect(s, '}'))
                return -1;
        }
    end_import_clause:
        idx = js_parse_from_clause(s, m);
        if (idx < 0)
            return -1;
    }
    for(i = first_import; i < m->import_entries_count; i++)
        m->import_entries[i].req_module_idx = idx;

    return js_parse_expect_semi(s);
}

static __exception int js_parse_source_element(JSParseState *s)
{
    JSFunctionDef *fd = s->cur_func;
    int tok;

    if (s->token.val == TOK_FUNCTION ||
        (token_is_pseudo_keyword(s, JS_ATOM_async) &&
         peek_token(s, TRUE) == TOK_FUNCTION)) {
        if (js_parse_function_decl(s, JS_PARSE_FUNC_STATEMENT,
                                   JS_FUNC_NORMAL, JS_ATOM_NULL,
                                   s->token.ptr))
            return -1;
    } else if (s->token.val == TOK_EXPORT && fd->module) {
        if (js_parse_export(s))
            return -1;
    } else if (s->token.val == TOK_IMPORT && fd->module &&
               ((tok = peek_token(s, FALSE)) != '(' && tok != '.'))  {
        /* the peek_token is needed to avoid confusion with ImportCall
           (dynamic import) or import.meta */
        if (js_parse_import(s))
            return -1;
    } else {
        if (js_parse_statement_or_decl(s, DECL_MASK_ALL))
            return -1;
    }
    return 0;
}

static __exception int js_parse_directives(JSParseState *s)
{
    char str[20];
    JSParsePos pos;
    BOOL has_semi;

    if (s->token.val != TOK_STRING)
        return 0;

    js_parse_get_pos(s, &pos);

    while(s->token.val == TOK_STRING) {
        /* Copy actual source string representation */
        snprintf(str, sizeof str, "%.*s",
                 (int)(s->buf_ptr - s->token.ptr - 2), s->token.ptr + 1);

        if (next_token(s))
            return -1;

        has_semi = FALSE;
        switch (s->token.val) {
        case ';':
            if (next_token(s))
                return -1;
            has_semi = TRUE;
            break;
        case '}':
        case TOK_EOF:
            has_semi = TRUE;
            break;
        case TOK_NUMBER:
        case TOK_STRING:
        case TOK_TEMPLATE:
        case TOK_IDENT:
        case TOK_REGEXP:
        case TOK_DEC:
        case TOK_INC:
        case TOK_NULL:
        case TOK_FALSE:
        case TOK_TRUE:
        case TOK_IF:
        case TOK_RETURN:
        case TOK_VAR:
        case TOK_THIS:
        case TOK_DELETE:
        case TOK_TYPEOF:
        case TOK_NEW:
        case TOK_DO:
        case TOK_WHILE:
        case TOK_FOR:
        case TOK_SWITCH:
        case TOK_THROW:
        case TOK_TRY:
        case TOK_FUNCTION:
        case TOK_DEBUGGER:
        case TOK_WITH:
        case TOK_CLASS:
        case TOK_CONST:
        case TOK_ENUM:
        case TOK_EXPORT:
        case TOK_IMPORT:
        case TOK_SUPER:
        case TOK_INTERFACE:
        case TOK_LET:
        case TOK_PACKAGE:
        case TOK_PRIVATE:
        case TOK_PROTECTED:
        case TOK_PUBLIC:
        case TOK_STATIC:
            /* automatic insertion of ';' */
            if (s->got_lf)
                has_semi = TRUE;
            break;
        default:
            break;
        }
        if (!has_semi)
            break;
        if (!strcmp(str, "use strict")) {
            s->cur_func->has_use_strict = TRUE;
            s->cur_func->js_mode |= JS_MODE_STRICT;
        }
    }
    return js_parse_seek_token(s, &pos);
}

/* return TRUE if the keyword is forbidden only in strict mode */
static BOOL is_strict_future_keyword(JSAtom atom)
{
    return (atom >= JS_ATOM_LAST_KEYWORD + 1 && atom <= JS_ATOM_LAST_STRICT_KEYWORD);
}

static int js_parse_function_check_names(JSParseState *s, JSFunctionDef *fd,
                                         JSAtom func_name)
{
    JSAtom name;
    int i, idx;

    if (fd->js_mode & JS_MODE_STRICT) {
        if (!fd->has_simple_parameter_list && fd->has_use_strict) {
            return js_parse_error(s, "\"use strict\" not allowed in function with default or destructuring parameter");
        }
        if (func_name == JS_ATOM_eval || func_name == JS_ATOM_arguments ||
            is_strict_future_keyword(func_name)) {
            return js_parse_error(s, "invalid function name in strict code");
        }
        for (idx = 0; idx < fd->arg_count; idx++) {
            name = fd->args[idx].var_name;

            if (name == JS_ATOM_eval || name == JS_ATOM_arguments ||
                is_strict_future_keyword(name)) {
                return js_parse_error(s, "invalid argument name in strict code");
            }
        }
    }
    /* check async_generator case */
    if ((fd->js_mode & JS_MODE_STRICT)
    ||  !fd->has_simple_parameter_list
    ||  (fd->func_type == JS_PARSE_FUNC_METHOD && fd->func_kind == JS_FUNC_ASYNC)
    ||  fd->func_type == JS_PARSE_FUNC_ARROW
    ||  fd->func_type == JS_PARSE_FUNC_METHOD) {
        for (idx = 0; idx < fd->arg_count; idx++) {
            name = fd->args[idx].var_name;
            if (name != JS_ATOM_NULL) {
                for (i = 0; i < idx; i++) {
                    if (fd->args[i].var_name == name)
                        goto duplicate;
                }
                /* Check if argument name duplicates a destructuring parameter */
                /* XXX: should have a flag for such variables */
                for (i = 0; i < fd->var_count; i++) {
                    if (fd->vars[i].var_name == name &&
                        fd->vars[i].scope_level == 0)
                        goto duplicate;
                }
            }
        }
    }
    return 0;

duplicate:
    return js_parse_error(s, "duplicate argument names not allowed in this context");
}

/* create a function to initialize class fields */
static JSFunctionDef *js_parse_function_class_fields_init(JSParseState *s)
{
    JSFunctionDef *fd;

    fd = js_new_function_def(s->ctx, s->cur_func, FALSE, FALSE,
                             s->filename, s->buf_start,
                             &s->get_line_col_cache);
    if (!fd)
        return NULL;
    fd->func_name = JS_ATOM_NULL;
    fd->has_prototype = FALSE;
    fd->has_home_object = TRUE;

    fd->has_arguments_binding = FALSE;
    fd->has_this_binding = TRUE;
    fd->is_derived_class_constructor = FALSE;
    fd->new_target_allowed = TRUE;
    fd->super_call_allowed = FALSE;
    fd->super_allowed = fd->has_home_object;
    fd->arguments_allowed = FALSE;

    fd->func_kind = JS_FUNC_NORMAL;
    fd->func_type = JS_PARSE_FUNC_METHOD;
    return fd;
}

/* func_name must be JS_ATOM_NULL for JS_PARSE_FUNC_STATEMENT and
   JS_PARSE_FUNC_EXPR, JS_PARSE_FUNC_ARROW and JS_PARSE_FUNC_VAR */
static __exception int js_parse_function_decl2(JSParseState *s,
                                               JSParseFunctionEnum func_type,
                                               JSFunctionKindEnum func_kind,
                                               JSAtom func_name,
                                               const uint8_t *ptr,
                                               JSParseExportEnum export_flag,
                                               JSFunctionDef **pfd)
{
    JSContext *ctx = s->ctx;
    JSFunctionDef *fd = s->cur_func;
    BOOL is_expr;
    int func_idx, lexical_func_idx = -1;
    BOOL has_opt_arg;
    BOOL create_func_var = FALSE;

    is_expr = (func_type != JS_PARSE_FUNC_STATEMENT &&
               func_type != JS_PARSE_FUNC_VAR);

    if (func_type == JS_PARSE_FUNC_STATEMENT ||
        func_type == JS_PARSE_FUNC_VAR ||
        func_type == JS_PARSE_FUNC_EXPR) {
        if (func_kind == JS_FUNC_NORMAL &&
            token_is_pseudo_keyword(s, JS_ATOM_async) &&
            peek_token(s, TRUE) != '\n') {
            if (next_token(s))
                return -1;
            func_kind = JS_FUNC_ASYNC;
        }
        if (next_token(s))
            return -1;
        if (s->token.val == '*') {
            if (next_token(s))
                return -1;
            func_kind |= JS_FUNC_GENERATOR;
        }

        if (s->token.val == TOK_IDENT) {
            if (s->token.u.ident.is_reserved ||
                (s->token.u.ident.atom == JS_ATOM_yield &&
                 func_type == JS_PARSE_FUNC_EXPR &&
                 (func_kind & JS_FUNC_GENERATOR)) ||
                (s->token.u.ident.atom == JS_ATOM_await &&
                 ((func_type == JS_PARSE_FUNC_EXPR &&
                   (func_kind & JS_FUNC_ASYNC)) ||
                  func_type == JS_PARSE_FUNC_CLASS_STATIC_INIT))) {
                return js_parse_error_reserved_identifier(s);
            }
        }
        if (s->token.val == TOK_IDENT ||
            (((s->token.val == TOK_YIELD && !(fd->js_mode & JS_MODE_STRICT)) ||
             (s->token.val == TOK_AWAIT && !s->is_module)) &&
             func_type == JS_PARSE_FUNC_EXPR)) {
            func_name = JS_DupAtom(ctx, s->token.u.ident.atom);
            if (next_token(s)) {
                JS_FreeAtom(ctx, func_name);
                return -1;
            }
        } else {
            if (func_type != JS_PARSE_FUNC_EXPR &&
                export_flag != JS_PARSE_EXPORT_DEFAULT) {
                return js_parse_error(s, "function name expected");
            }
        }
    } else if (func_type != JS_PARSE_FUNC_ARROW) {
        func_name = JS_DupAtom(ctx, func_name);
    }

    if (fd->is_eval && fd->eval_type == JS_EVAL_TYPE_MODULE &&
        (func_type == JS_PARSE_FUNC_STATEMENT || func_type == JS_PARSE_FUNC_VAR)) {
        JSGlobalVar *hf;
        hf = find_global_var(fd, func_name);
        /* XXX: should check scope chain */
        if (hf && hf->scope_level == fd->scope_level) {
            js_parse_error(s, "invalid redefinition of global identifier in module code");
            JS_FreeAtom(ctx, func_name);
            return -1;
        }
    }

    if (func_type == JS_PARSE_FUNC_VAR) {
        if (!(fd->js_mode & JS_MODE_STRICT)
        && func_kind == JS_FUNC_NORMAL
        &&  find_lexical_decl(ctx, fd, func_name, fd->scope_first, FALSE) < 0
        &&  !((func_idx = find_var(ctx, fd, func_name)) >= 0 && (func_idx & ARGUMENT_VAR_OFFSET))
        &&  !(func_name == JS_ATOM_arguments && fd->has_arguments_binding)) {
            create_func_var = TRUE;
        }
        /* Create the lexical name here so that the function closure
           contains it */
        if (fd->is_eval &&
            (fd->eval_type == JS_EVAL_TYPE_GLOBAL ||
             fd->eval_type == JS_EVAL_TYPE_MODULE) &&
            fd->scope_level == fd->body_scope) {
            /* avoid creating a lexical variable in the global
               scope. XXX: check annex B */
            JSGlobalVar *hf;
            hf = find_global_var(fd, func_name);
            /* XXX: should check scope chain */
            if (hf && hf->scope_level == fd->scope_level) {
                js_parse_error(s, "invalid redefinition of global identifier");
                JS_FreeAtom(ctx, func_name);
                return -1;
            }
        } else {
            /* Always create a lexical name, fail if at the same scope as
               existing name */
            /* Lexical variable will be initialized upon entering scope */
            lexical_func_idx = define_var(s, fd, func_name,
                                          func_kind != JS_FUNC_NORMAL ?
                                          JS_VAR_DEF_NEW_FUNCTION_DECL :
                                          JS_VAR_DEF_FUNCTION_DECL);
            if (lexical_func_idx < 0) {
                JS_FreeAtom(ctx, func_name);
                return -1;
            }
        }
    }

    fd = js_new_function_def(ctx, fd, FALSE, is_expr,
                             s->filename, ptr,
                             &s->get_line_col_cache);
    if (!fd) {
        JS_FreeAtom(ctx, func_name);
        return -1;
    }
    if (pfd)
        *pfd = fd;
    s->cur_func = fd;
    fd->func_name = func_name;
    /* XXX: test !fd->is_generator is always false */
    fd->has_prototype = (func_type == JS_PARSE_FUNC_STATEMENT ||
                         func_type == JS_PARSE_FUNC_VAR ||
                         func_type == JS_PARSE_FUNC_EXPR) &&
                        func_kind == JS_FUNC_NORMAL;
    fd->has_home_object = (func_type == JS_PARSE_FUNC_METHOD ||
                           func_type == JS_PARSE_FUNC_GETTER ||
                           func_type == JS_PARSE_FUNC_SETTER ||
                           func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR ||
                           func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR);
    fd->has_arguments_binding = (func_type != JS_PARSE_FUNC_ARROW &&
                                 func_type != JS_PARSE_FUNC_CLASS_STATIC_INIT);
    fd->has_this_binding = fd->has_arguments_binding;
    fd->is_derived_class_constructor = (func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR);
    if (func_type == JS_PARSE_FUNC_ARROW) {
        fd->new_target_allowed = fd->parent->new_target_allowed;
        fd->super_call_allowed = fd->parent->super_call_allowed;
        fd->super_allowed = fd->parent->super_allowed;
        fd->arguments_allowed = fd->parent->arguments_allowed;
    } else if (func_type == JS_PARSE_FUNC_CLASS_STATIC_INIT) {
        fd->new_target_allowed = TRUE; // although new.target === undefined
        fd->super_call_allowed = FALSE;
        fd->super_allowed = TRUE;
        fd->arguments_allowed = FALSE;
    } else {
        fd->new_target_allowed = TRUE;
        fd->super_call_allowed = fd->is_derived_class_constructor;
        fd->super_allowed = fd->has_home_object;
        fd->arguments_allowed = TRUE;
    }

    /* fd->in_function_body == FALSE prevents yield/await during the parsing
       of the arguments in generator/async functions. They are parsed as
       regular identifiers for other function kinds. */
    fd->func_kind = func_kind;
    fd->func_type = func_type;

    if (func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR ||
        func_type == JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR) {
        /* error if not invoked as a constructor */
        emit_op(s, OP_check_ctor);
    }

    if (func_type == JS_PARSE_FUNC_CLASS_CONSTRUCTOR) {
        emit_class_field_init(s);
    }

    /* parse arguments */
    fd->has_simple_parameter_list = TRUE;
    fd->has_parameter_expressions = FALSE;
    has_opt_arg = FALSE;
    if (func_type == JS_PARSE_FUNC_ARROW && s->token.val == TOK_IDENT) {
        JSAtom name;
        if (s->token.u.ident.is_reserved) {
            js_parse_error_reserved_identifier(s);
            goto fail;
        }
        name = s->token.u.ident.atom;
        if (add_arg(ctx, fd, name) < 0)
            goto fail;
        fd->defined_arg_count = 1;
    } else if (func_type != JS_PARSE_FUNC_CLASS_STATIC_INIT) {
        if (s->token.val == '(') {
            int skip_bits;
            /* if there is an '=' inside the parameter list, we
               consider there is a parameter expression inside */
            js_parse_skip_parens_token(s, &skip_bits, FALSE);
            if (skip_bits & SKIP_HAS_ASSIGNMENT)
                fd->has_parameter_expressions = TRUE;
            if (next_token(s))
                goto fail;
        } else {
            if (js_parse_expect(s, '('))
                goto fail;
        }

        if (fd->has_parameter_expressions) {
            fd->scope_level = -1; /* force no parent scope */
            if (push_scope(s) < 0)
                return -1;
        }

        while (s->token.val != ')') {
            JSAtom name;
            BOOL rest = FALSE;
            int idx, has_initializer;

            if (s->token.val == TOK_ELLIPSIS) {
                if (func_type == JS_PARSE_FUNC_SETTER)
                    goto fail_accessor;
                fd->has_simple_parameter_list = FALSE;
                rest = TRUE;
                if (next_token(s))
                    goto fail;
            }
            if (s->token.val == '[' || s->token.val == '{') {
                fd->has_simple_parameter_list = FALSE;
                if (rest) {
                    emit_op(s, OP_rest);
                    emit_u16(s, fd->arg_count);
                } else {
                    /* unnamed arg for destructuring */
                    idx = add_arg(ctx, fd, JS_ATOM_NULL);
                    emit_op(s, OP_get_arg);
                    emit_u16(s, idx);
                }
                has_initializer = js_parse_destructuring_element(s, fd->has_parameter_expressions ? TOK_LET : TOK_VAR, 1, TRUE, -1, TRUE, FALSE);
                if (has_initializer < 0)
                    goto fail;
                if (has_initializer)
                    has_opt_arg = TRUE;
                if (!has_opt_arg)
                    fd->defined_arg_count++;
            } else if (s->token.val == TOK_IDENT) {
                if (s->token.u.ident.is_reserved) {
                    js_parse_error_reserved_identifier(s);
                    goto fail;
                }
                name = s->token.u.ident.atom;
                if (name == JS_ATOM_yield && fd->func_kind == JS_FUNC_GENERATOR) {
                    js_parse_error_reserved_identifier(s);
                    goto fail;
                }
                if (fd->has_parameter_expressions) {
                    if (js_parse_check_duplicate_parameter(s, name))
                        goto fail;
                    if (define_var(s, fd, name, JS_VAR_DEF_LET) < 0)
                        goto fail;
                }
                /* XXX: could avoid allocating an argument if rest is true */
                idx = add_arg(ctx, fd, name);
                if (idx < 0)
                    goto fail;
                if (next_token(s))
                    goto fail;
                if (rest) {
                    emit_op(s, OP_rest);
                    emit_u16(s, idx);
                    if (fd->has_parameter_expressions) {
                        emit_op(s, OP_dup);
                        emit_op(s, OP_scope_put_var_init);
                        emit_atom(s, name);
                        emit_u16(s, fd->scope_level);
                    }
                    emit_op(s, OP_put_arg);
                    emit_u16(s, idx);
                    fd->has_simple_parameter_list = FALSE;
                    has_opt_arg = TRUE;
                } else if (s->token.val == '=') {
                    int label;

                    fd->has_simple_parameter_list = FALSE;
                    has_opt_arg = TRUE;

                    if (next_token(s))
                        goto fail;

                    label = new_label(s);
                    emit_op(s, OP_get_arg);
                    emit_u16(s, idx);
                    emit_op(s, OP_dup);
                    emit_op(s, OP_undefined);
                    emit_op(s, OP_strict_eq);
                    emit_goto(s, OP_if_false, label);
                    emit_op(s, OP_drop);
                    if (js_parse_assign_expr(s))
                        goto fail;
                    set_object_name(s, name);
                    emit_op(s, OP_dup);
                    emit_op(s, OP_put_arg);
                    emit_u16(s, idx);
                    emit_label(s, label);
                    emit_op(s, OP_scope_put_var_init);
                    emit_atom(s, name);
                    emit_u16(s, fd->scope_level);
                } else {
                    if (!has_opt_arg) {
                        fd->defined_arg_count++;
                    }
                    if (fd->has_parameter_expressions) {
                        /* copy the argument to the argument scope */
                        emit_op(s, OP_get_arg);
                        emit_u16(s, idx);
                        emit_op(s, OP_scope_put_var_init);
                        emit_atom(s, name);
                        emit_u16(s, fd->scope_level);
                    }
                }
            } else {
                js_parse_error(s, "missing formal parameter");
                goto fail;
            }
            if (rest && s->token.val != ')') {
                js_parse_expect(s, ')');
                goto fail;
            }
            if (s->token.val == ')')
                break;
            if (js_parse_expect(s, ','))
                goto fail;
        }
        if ((func_type == JS_PARSE_FUNC_GETTER && fd->arg_count != 0) ||
            (func_type == JS_PARSE_FUNC_SETTER && fd->arg_count != 1)) {
        fail_accessor:
            js_parse_error(s, "invalid number of arguments for getter or setter");
            goto fail;
        }
    }

    if (fd->has_parameter_expressions) {
        int idx;

        /* Copy the variables in the argument scope to the variable
           scope (see FunctionDeclarationInstantiation() in spec). The
           normal arguments are already present, so no need to copy
           them. */
        idx = fd->scopes[fd->scope_level].first;
        while (idx >= 0) {
            JSVarDef *vd = &fd->vars[idx];
            if (vd->scope_level != fd->scope_level)
                break;
            if (find_var(ctx, fd, vd->var_name) < 0) {
                if (add_var(ctx, fd, vd->var_name) < 0)
                    goto fail;
                vd = &fd->vars[idx]; /* fd->vars may have been reallocated */
                emit_op(s, OP_scope_get_var);
                emit_atom(s, vd->var_name);
                emit_u16(s, fd->scope_level);
                emit_op(s, OP_scope_put_var);
                emit_atom(s, vd->var_name);
                emit_u16(s, 0);
            }
            idx = vd->scope_next;
        }

        /* the argument scope has no parent, hence we don't use pop_scope(s) */
        emit_op(s, OP_leave_scope);
        emit_u16(s, fd->scope_level);

        /* set the variable scope as the current scope */
        fd->scope_level = 0;
        fd->scope_first = fd->scopes[fd->scope_level].first;
    }

    if (next_token(s))
        goto fail;

    /* generator function: yield after the parameters are evaluated */
    if (func_kind == JS_FUNC_GENERATOR ||
        func_kind == JS_FUNC_ASYNC_GENERATOR)
        emit_op(s, OP_initial_yield);

    /* in generators, yield expression is forbidden during the parsing
       of the arguments */
    fd->in_function_body = TRUE;
    push_scope(s);  /* enter body scope */
    fd->body_scope = fd->scope_level;

    if (s->token.val == TOK_ARROW && func_type == JS_PARSE_FUNC_ARROW) {
        if (next_token(s))
            goto fail;

        if (s->token.val != '{') {
            if (js_parse_function_check_names(s, fd, func_name))
                goto fail;

            if (js_parse_assign_expr(s))
                goto fail;

            if (func_kind != JS_FUNC_NORMAL)
                emit_op(s, OP_return_async);
            else
                emit_op(s, OP_return);

            if (!fd->strip_source) {
                /* save the function source code */
                /* the end of the function source code is after the last
                   token of the function source stored into s->last_ptr */
                fd->source_len = s->last_ptr - ptr;
                fd->source = js_strndup(ctx, (const char *)ptr, fd->source_len);
                if (!fd->source)
                    goto fail;
            }
            goto done;
        }
    }

    if (func_type != JS_PARSE_FUNC_CLASS_STATIC_INIT) {
        if (js_parse_expect(s, '{'))
            goto fail;
    }

    if (js_parse_directives(s))
        goto fail;

    /* in strict_mode, check function and argument names */
    if (js_parse_function_check_names(s, fd, func_name))
        goto fail;

    while (s->token.val != '}') {
        if (js_parse_source_element(s))
            goto fail;
    }
    if (!fd->strip_source) {
        /* save the function source code */
        fd->source_len = s->buf_ptr - ptr;
        fd->source = js_strndup(ctx, (const char *)ptr, fd->source_len);
        if (!fd->source)
            goto fail;
    }

    if (next_token(s)) {
        /* consume the '}' */
        goto fail;
    }

    /* in case there is no return, add one */
    if (js_is_live_code(s)) {
        emit_return(s, FALSE);
    }
 done:
    s->cur_func = fd->parent;

    /* Reparse identifiers after the function is terminated so that
       the token is parsed in the englobing function. It could be done
       by just using next_token() here for normal functions, but it is
       necessary for arrow functions with an expression body. */
    reparse_ident_token(s);

    /* create the function object */
    {
        int idx;
        JSAtom func_name = fd->func_name;

        /* the real object will be set at the end of the compilation */
        idx = cpool_add(s, JS_NULL);
        fd->parent_cpool_idx = idx;

        if (is_expr) {
            /* for constructors, no code needs to be generated here */
            if (func_type != JS_PARSE_FUNC_CLASS_CONSTRUCTOR &&
                func_type != JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR) {
                /* OP_fclosure creates the function object from the bytecode
                   and adds the scope information */
                emit_op(s, OP_fclosure);
                emit_u32(s, idx);
                if (func_name == JS_ATOM_NULL) {
                    emit_op(s, OP_set_name);
                    emit_u32(s, JS_ATOM_NULL);
                }
            }
        } else if (func_type == JS_PARSE_FUNC_VAR) {
            emit_op(s, OP_fclosure);
            emit_u32(s, idx);
            if (create_func_var) {
                if (s->cur_func->is_global_var) {
                    JSGlobalVar *hf;
                    /* the global variable must be defined at the start of the
                       function */
                    hf = add_global_var(ctx, s->cur_func, func_name);
                    if (!hf)
                        goto fail;
                    /* it is considered as defined at the top level
                       (needed for annex B.3.3.4 and B.3.3.5
                       checks) */
                    hf->scope_level = 0;
                    hf->force_init = ((s->cur_func->js_mode & JS_MODE_STRICT) != 0);
                    /* store directly into global var, bypass lexical scope */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_scope_put_var);
                    emit_atom(s, func_name);
                    emit_u16(s, 0);
                } else {
                    /* do not call define_var to bypass lexical scope check */
                    func_idx = find_var(ctx, s->cur_func, func_name);
                    if (func_idx < 0) {
                        func_idx = add_var(ctx, s->cur_func, func_name);
                        if (func_idx < 0)
                            goto fail;
                    }
                    /* store directly into local var, bypass lexical catch scope */
                    emit_op(s, OP_dup);
                    emit_op(s, OP_scope_put_var);
                    emit_atom(s, func_name);
                    emit_u16(s, 0);
                }
            }
            if (lexical_func_idx >= 0) {
                /* lexical variable will be initialized upon entering scope */
                s->cur_func->vars[lexical_func_idx].func_pool_idx = idx;
                emit_op(s, OP_drop);
            } else {
                /* store function object into its lexical name */
                /* XXX: could use OP_put_loc directly */
                emit_op(s, OP_scope_put_var_init);
                emit_atom(s, func_name);
                emit_u16(s, s->cur_func->scope_level);
            }
        } else {
            if (!s->cur_func->is_global_var) {
                int var_idx = define_var(s, s->cur_func, func_name, JS_VAR_DEF_VAR);

                if (var_idx < 0)
                    goto fail;
                /* the variable will be assigned at the top of the function */
                if (var_idx & ARGUMENT_VAR_OFFSET) {
                    s->cur_func->args[var_idx - ARGUMENT_VAR_OFFSET].func_pool_idx = idx;
                } else {
                    s->cur_func->vars[var_idx].func_pool_idx = idx;
                }
            } else {
                JSAtom func_var_name;
                JSGlobalVar *hf;
                if (func_name == JS_ATOM_NULL)
                    func_var_name = JS_ATOM__default_; /* export default */
                else
                    func_var_name = func_name;
                /* the variable will be assigned at the top of the function */
                hf = add_global_var(ctx, s->cur_func, func_var_name);
                if (!hf)
                    goto fail;
                hf->cpool_idx = idx;
                if (export_flag != JS_PARSE_EXPORT_NONE) {
                    if (!add_export_entry(s, s->cur_func->module, func_var_name,
                                          export_flag == JS_PARSE_EXPORT_NAMED ? func_var_name : JS_ATOM_default, JS_EXPORT_TYPE_LOCAL))
                        goto fail;
                }
            }
        }
    }
    return 0;
 fail:
    s->cur_func = fd->parent;
    js_free_function_def(ctx, fd);
    if (pfd)
        *pfd = NULL;
    return -1;
}

static __exception int js_parse_function_decl(JSParseState *s,
                                              JSParseFunctionEnum func_type,
                                              JSFunctionKindEnum func_kind,
                                              JSAtom func_name,
                                              const uint8_t *ptr)
{
    return js_parse_function_decl2(s, func_type, func_kind, func_name, ptr,
                                   JS_PARSE_EXPORT_NONE, NULL);
}

static __exception int js_parse_program(JSParseState *s)
{
    JSFunctionDef *fd = s->cur_func;
    int idx;

    if (next_token(s))
        return -1;

    if (js_parse_directives(s))
        return -1;

    fd->is_global_var = (fd->eval_type == JS_EVAL_TYPE_GLOBAL) ||
        (fd->eval_type == JS_EVAL_TYPE_MODULE) ||
        !(fd->js_mode & JS_MODE_STRICT);

    if (!s->is_module) {
        /* hidden variable for the return value */
        fd->eval_ret_idx = idx = add_var(s->ctx, fd, JS_ATOM__ret_);
        if (idx < 0)
            return -1;
    }

    while (s->token.val != TOK_EOF) {
        if (js_parse_source_element(s))
            return -1;
    }

    if (!s->is_module) {
        /* return the value of the hidden variable eval_ret_idx  */
        if (fd->func_kind == JS_FUNC_ASYNC) {
            /* wrap the return value in an object so that promises can
               be safely returned */
            emit_op(s, OP_object);
            emit_op(s, OP_dup);

            emit_op(s, OP_get_loc);
            emit_u16(s, fd->eval_ret_idx);

            emit_op(s, OP_put_field);
            emit_atom(s, JS_ATOM_value);
        } else {
            emit_op(s, OP_get_loc);
            emit_u16(s, fd->eval_ret_idx);
        }
        emit_return(s, TRUE);
    } else {
        emit_return(s, FALSE);
    }

    return 0;
}

static JSValue JS_EvalFunctionInternal(JSContext *ctx, JSValue fun_obj,
                                       JSValueConst this_obj,
                                       JSVarRef **var_refs, JSStackFrame *sf)
{
    JSValue ret_val;
    uint32_t tag;

    tag = JS_VALUE_GET_TAG(fun_obj);
    if (tag == JS_TAG_FUNCTION_BYTECODE) {
        fun_obj = js_closure(ctx, fun_obj, var_refs, sf, TRUE);
        if (JS_IsException(fun_obj))
            return JS_EXCEPTION;
        ret_val = JS_CallFree(ctx, fun_obj, this_obj, 0, NULL);
    } else if (tag == JS_TAG_MODULE) {
        JSModuleDef *m;
        m = JS_VALUE_GET_PTR(fun_obj);
        /* the module refcount should be >= 2 */
        JS_FreeValue(ctx, fun_obj);
        if (js_create_module_function(ctx, m) < 0)
            goto fail;
        if (js_link_module(ctx, m) < 0)
            goto fail;
        ret_val = js_evaluate_module(ctx, m);
        if (JS_IsException(ret_val)) {
        fail:
            return JS_EXCEPTION;
        }
    } else {
        JS_FreeValue(ctx, fun_obj);
        ret_val = JS_ThrowTypeError(ctx, "bytecode function expected");
    }
    return ret_val;
}

JSValue JS_EvalFunction(JSContext *ctx, JSValue fun_obj)
{
    return JS_EvalFunctionInternal(ctx, fun_obj, ctx->global_obj, NULL, NULL);
}

/* 'input' must be zero terminated i.e. input[input_len] = '\0'. */
JSValue __JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                          const char *input, size_t input_len,
                          const char *filename, int flags, int scope_idx)
{
    JSParseState s1, *s = &s1;
    int err, js_mode, eval_type;
    JSValue fun_obj, ret_val;
    JSStackFrame *sf;
    JSVarRef **var_refs;
    JSFunctionBytecode *b;
    JSFunctionDef *fd;
    JSModuleDef *m;

    js_parse_init(ctx, s, input, input_len, filename);
    skip_shebang(&s->buf_ptr, s->buf_end);

    eval_type = flags & JS_EVAL_TYPE_MASK;
    m = NULL;
    if (eval_type == JS_EVAL_TYPE_DIRECT) {
        JSObject *p;
        sf = ctx->rt->current_stack_frame;
        assert(sf != NULL);
        assert(JS_VALUE_GET_TAG(sf->cur_func) == JS_TAG_OBJECT);
        p = JS_VALUE_GET_OBJ(sf->cur_func);
        assert(js_class_has_bytecode(p->class_id));
        b = p->u.func.function_bytecode;
        var_refs = p->u.func.var_refs;
        js_mode = b->js_mode;
    } else {
        sf = NULL;
        b = NULL;
        var_refs = NULL;
        js_mode = 0;
        if (flags & JS_EVAL_FLAG_STRICT)
            js_mode |= JS_MODE_STRICT;
        if (eval_type == JS_EVAL_TYPE_MODULE) {
            JSAtom module_name = JS_NewAtom(ctx, filename);
            if (module_name == JS_ATOM_NULL)
                return JS_EXCEPTION;
            m = js_new_module_def(ctx, module_name);
            if (!m)
                return JS_EXCEPTION;
            js_mode |= JS_MODE_STRICT;
        }
    }
    fd = js_new_function_def(ctx, NULL, TRUE, FALSE, filename,
                             s->buf_start, &s->get_line_col_cache);
    if (!fd)
        goto fail1;
    s->cur_func = fd;
    fd->eval_type = eval_type;
    fd->has_this_binding = (eval_type != JS_EVAL_TYPE_DIRECT);
    if (eval_type == JS_EVAL_TYPE_DIRECT) {
        fd->new_target_allowed = b->new_target_allowed;
        fd->super_call_allowed = b->super_call_allowed;
        fd->super_allowed = b->super_allowed;
        fd->arguments_allowed = b->arguments_allowed;
    } else {
        fd->new_target_allowed = FALSE;
        fd->super_call_allowed = FALSE;
        fd->super_allowed = FALSE;
        fd->arguments_allowed = TRUE;
    }
    fd->js_mode = js_mode;
    fd->func_name = JS_DupAtom(ctx, JS_ATOM__eval_);
    if (b) {
        if (add_closure_variables(ctx, fd, b, scope_idx))
            goto fail;
    }
    fd->module = m;
    if (m != NULL || (flags & JS_EVAL_FLAG_ASYNC)) {
        fd->in_function_body = TRUE;
        fd->func_kind = JS_FUNC_ASYNC;
    }
    s->is_module = (m != NULL);
    s->allow_html_comments = !s->is_module;

    push_scope(s); /* body scope */
    fd->body_scope = fd->scope_level;

    err = js_parse_program(s);
    if (err) {
    fail:
        free_token(s, &s->token);
        js_free_function_def(ctx, fd);
        goto fail1;
    }

    if (m != NULL)
        m->has_tla = fd->has_await;

    /* create the function object and all the enclosed functions */
    fun_obj = js_create_function(ctx, fd);
    if (JS_IsException(fun_obj))
        goto fail1;
    /* Could add a flag to avoid resolution if necessary */
    if (m) {
        m->func_obj = fun_obj;
        if (js_resolve_module(ctx, m) < 0)
            goto fail1;
        fun_obj = JS_NewModuleValue(ctx, m);
    }
    if (flags & JS_EVAL_FLAG_COMPILE_ONLY) {
        ret_val = fun_obj;
    } else {
        ret_val = JS_EvalFunctionInternal(ctx, fun_obj, this_obj, var_refs, sf);
    }
    return ret_val;
 fail1:
    /* XXX: should free all the unresolved dependencies */
    if (m)
        JS_FreeValue(ctx, JS_MKPTR(JS_TAG_MODULE, m));
    return JS_EXCEPTION;
}

/* the indirection is needed to make 'eval' optional */
static JSValue JS_EvalInternal(JSContext *ctx, JSValueConst this_obj,
                               const char *input, size_t input_len,
                               const char *filename, int flags, int scope_idx)
{
    BOOL backtrace_barrier = ((flags & JS_EVAL_FLAG_BACKTRACE_BARRIER) != 0);
    int saved_js_mode = 0;
    JSValue ret;
    
    if (unlikely(!ctx->eval_internal)) {
        return JS_ThrowTypeError(ctx, "eval is not supported");
    }
    if (backtrace_barrier && ctx->rt->current_stack_frame) {
        saved_js_mode = ctx->rt->current_stack_frame->js_mode;
        ctx->rt->current_stack_frame->js_mode |= JS_MODE_BACKTRACE_BARRIER;
    }
    ret = ctx->eval_internal(ctx, this_obj, input, input_len, filename,
                             flags, scope_idx);
    if (backtrace_barrier && ctx->rt->current_stack_frame)
        ctx->rt->current_stack_frame->js_mode = saved_js_mode;
    return ret;
}

JSValue JS_EvalObject(JSContext *ctx, JSValueConst this_obj,
                      JSValueConst val, int flags, int scope_idx)
{
    JSValue ret;
    const char *str;
    size_t len;

    if (!JS_IsString(val))
        return JS_DupValue(ctx, val);
    str = JS_ToCStringLen(ctx, &len, val);
    if (!str)
        return JS_EXCEPTION;
    ret = JS_EvalInternal(ctx, this_obj, str, len, "<input>", flags, scope_idx);
    JS_FreeCString(ctx, str);
    return ret;
}

JSValue JS_EvalThis(JSContext *ctx, JSValueConst this_obj,
                    const char *input, size_t input_len,
                    const char *filename, int eval_flags)
{
    int eval_type = eval_flags & JS_EVAL_TYPE_MASK;
    JSValue ret;

    assert(eval_type == JS_EVAL_TYPE_GLOBAL ||
           eval_type == JS_EVAL_TYPE_MODULE);
    ret = JS_EvalInternal(ctx, this_obj, input, input_len, filename,
                          eval_flags, -1);
    return ret;
}

JSValue JS_Eval(JSContext *ctx, const char *input, size_t input_len,
                const char *filename, int eval_flags)
{
    return JS_EvalThis(ctx, ctx->global_obj, input, input_len, filename,
                       eval_flags);
}

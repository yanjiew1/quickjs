/*
 * QuickJS Javascript Engine: Compiler / Codegen
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

#include "quickjs-internal.h"
#include "cutils.h"
#include "libregexp.h"
#include "compiler/js_opcode.h"
#include "compiler/js_lexer.h"
#include "compiler/js_parser.h"
#include "compiler/js_codegen.h"

const JSOpCode opcode_info[OP_COUNT + (OP_TEMP_END - OP_TEMP_START)] = {
#define FMT(f)
#ifdef DUMP_BYTECODE
#define DEF(id, size, n_pop, n_push, f) { #id, size, n_pop, n_push, OP_FMT_ ## f },
#else
#define DEF(id, size, n_pop, n_push, f) { size, n_pop, n_push, OP_FMT_ ## f },
#endif
#include "quickjs-opcode.h"
#undef DEF
#undef FMT
};

/* [short_opcode_info defined in compiler/js_opcode.h] */

/* [Lexer functions moved to src/compiler/js_lexer.c] */


/* [get_prev_opcode, js_is_live_code, emit_u8, emit_u16, emit_u32 defined in compiler/js_codegen.h] */

void emit_source_pos(JSParseState *s, const uint8_t *source_ptr)
{
    JSFunctionDef *fd = s->cur_func;
    DynBuf *bc = &fd->byte_code;

    if (unlikely(fd->last_opcode_source_ptr != source_ptr)) {
        dbuf_putc(bc, OP_line_num);
        dbuf_put_u32(bc, source_ptr - s->buf_start);
        fd->last_opcode_source_ptr = source_ptr;
    }
}

void emit_op(JSParseState *s, uint8_t val)
{
    JSFunctionDef *fd = s->cur_func;
    DynBuf *bc = &fd->byte_code;

    fd->last_opcode_pos = bc->size;
    dbuf_putc(bc, val);
}

void emit_atom(JSParseState *s, JSAtom name)
{
    DynBuf *bc = &s->cur_func->byte_code;
    if (dbuf_claim(bc, 4))
        return; /* not enough memory : don't duplicate the atom */
    put_u32(bc->buf + bc->size, JS_DupAtom(s->ctx, name));
    bc->size += 4;
}

int update_label(JSFunctionDef *s, int label, int delta)
{
    LabelSlot *ls;

    assert(label >= 0 && label < s->label_count);
    ls = &s->label_slots[label];
    ls->ref_count += delta;
    assert(ls->ref_count >= 0);
    return ls->ref_count;
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

int new_label(JSParseState *s)
{
    int label;
    label = new_label_fd(s->cur_func);
    if (unlikely(label < 0)) {
        dbuf_set_error(&s->cur_func->byte_code);
    }
    return label;
}

/* don't update the last opcode and don't emit line number info */
void emit_label_raw(JSParseState *s, int label)
{
    emit_u8(s, OP_label);
    emit_u32(s, label);
    s->cur_func->label_slots[label].pos = s->cur_func->byte_code.size;
}

/* return the label ID offset */
int emit_label(JSParseState *s, int label)
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
int emit_goto(JSParseState *s, int opcode, int label)
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
int cpool_add(JSParseState *s, JSValue val)
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

__exception int emit_push_const(JSParseState *s, JSValueConst val,
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
int find_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
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
int find_var_in_scope(JSContext *ctx, JSFunctionDef *fd,
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


JSGlobalVar *find_global_var(JSFunctionDef *fd, JSAtom name)
{
    int i;
    for(i = 0; i < fd->global_var_count; i++) {
        JSGlobalVar *hf = &fd->global_vars[i];
        if (hf->var_name == name)
            return hf;
    }
    return NULL;

}

JSGlobalVar *find_lexical_global_var(JSFunctionDef *fd, JSAtom name)
{
    JSGlobalVar *hf = find_global_var(fd, name);
    if (hf && hf->is_lexical)
        return hf;
    else
        return NULL;
}

int find_lexical_decl(JSContext *ctx, JSFunctionDef *fd, JSAtom name,
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

int push_scope(JSParseState *s) {
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

void pop_scope(JSParseState *s) {
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

void close_scopes(JSParseState *s, int scope, int scope_stop)
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

int add_scope_var(JSContext *ctx, JSFunctionDef *fd, JSAtom name,
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

int add_arg(JSContext *ctx, JSFunctionDef *fd, JSAtom name)
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
JSGlobalVar *add_global_var(JSContext *ctx, JSFunctionDef *s,
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

/* [JSVarDefEnum defined in compiler/js_parser.h] */
int define_var(JSParseState *s, JSFunctionDef *fd, JSAtom name,
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
int add_private_class_field(JSParseState *s, JSFunctionDef *fd,
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


JSFunctionDef *js_new_function_def(JSContext *ctx,
                                          JSFunctionDef *parent,
                                          BOOL is_eval,
                                          BOOL is_func_expr,
                                          const char *filename,
                                          const uint8_t *source_ptr,
                                          GetLineColCache *get_line_col_cache)
{
    JSFunctionDef *fd;

    fd = js_mallocz(ctx, sizeof(*fd));
    if (!fd)
        return NULL;

    fd->ctx = ctx;
    init_list_head(&fd->child_list);

    /* insert in parent list */
    fd->parent = parent;
    fd->parent_cpool_idx = -1;
    if (parent) {
        list_add_tail(&fd->link, &parent->child_list);
        fd->js_mode = parent->js_mode;
        fd->parent_scope_level = parent->scope_level;
    }
    fd->strip_debug = ((ctx->rt->strip_flags & JS_STRIP_DEBUG) != 0);
    fd->strip_source = ((ctx->rt->strip_flags & (JS_STRIP_DEBUG | JS_STRIP_SOURCE)) != 0);

    fd->is_eval = is_eval;
    fd->is_func_expr = is_func_expr;
    js_dbuf_bytecode_init(ctx, &fd->byte_code);
    fd->last_opcode_pos = -1;
    fd->func_name = JS_ATOM_NULL;
    fd->var_object_idx = -1;
    fd->arg_var_object_idx = -1;
    fd->arguments_var_idx = -1;
    fd->arguments_arg_idx = -1;
    fd->func_var_idx = -1;
    fd->eval_ret_idx = -1;
    fd->this_var_idx = -1;
    fd->new_target_var_idx = -1;
    fd->this_active_func_var_idx = -1;
    fd->home_object_var_idx = -1;

    /* XXX: should distinguish arg, var and var object and body scopes */
    fd->scopes = fd->def_scope_array;
    fd->scope_size = countof(fd->def_scope_array);
    fd->scope_count = 1;
    fd->scopes[0].first = -1;
    fd->scopes[0].parent = -1;
    fd->scope_level = 0;  /* 0: var/arg scope */
    fd->scope_first = -1;
    fd->body_scope = -1;

    fd->filename = JS_NewAtom(ctx, filename);
    fd->source_pos = source_ptr - get_line_col_cache->buf_start;
    fd->get_line_col_cache = get_line_col_cache;
    
    js_dbuf_init(ctx, &fd->pc2line);
    //fd->pc2line_last_line_num = line_num;
    //fd->pc2line_last_pc = 0;
    fd->last_opcode_source_ptr = source_ptr;
    return fd;
}

void free_bytecode_atoms(JSRuntime *rt,
                                const uint8_t *bc_buf, int bc_len,
                                BOOL use_short_opcodes)
{
    int pos, len, op;
    JSAtom atom;
    const JSOpCode *oi;

    pos = 0;
    while (pos < bc_len) {
        op = bc_buf[pos];
        if (use_short_opcodes)
            oi = &short_opcode_info(op);
        else
            oi = &opcode_info[op];

        len = oi->size;
        switch(oi->fmt) {
        case OP_FMT_atom:
        case OP_FMT_atom_u8:
        case OP_FMT_atom_u16:
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            if ((pos + 1 + 4) > bc_len)
                break; /* may happen if there is not enough memory when emiting bytecode */
            atom = get_u32(bc_buf + pos + 1);
            JS_FreeAtomRT(rt, atom);
            break;
        default:
            break;
        }
        pos += len;
    }
}

void js_free_function_def(JSContext *ctx, JSFunctionDef *fd)
{
    int i;
    struct list_head *el, *el1;

    /* free the child functions */
    list_for_each_safe(el, el1, &fd->child_list) {
        JSFunctionDef *fd1;
        fd1 = list_entry(el, JSFunctionDef, link);
        js_free_function_def(ctx, fd1);
    }

    free_bytecode_atoms(ctx->rt, fd->byte_code.buf, fd->byte_code.size,
                        fd->use_short_opcodes);
    dbuf_free(&fd->byte_code);
    js_free(ctx, fd->jump_slots);
    js_free(ctx, fd->label_slots);
    js_free(ctx, fd->line_number_slots);

    for(i = 0; i < fd->cpool_count; i++) {
        JS_FreeValue(ctx, fd->cpool[i]);
    }
    js_free(ctx, fd->cpool);

    JS_FreeAtom(ctx, fd->func_name);

    for(i = 0; i < fd->var_count; i++) {
        JS_FreeAtom(ctx, fd->vars[i].var_name);
    }
    js_free(ctx, fd->vars);
    for(i = 0; i < fd->arg_count; i++) {
        JS_FreeAtom(ctx, fd->args[i].var_name);
    }
    js_free(ctx, fd->args);

    for(i = 0; i < fd->global_var_count; i++) {
        JS_FreeAtom(ctx, fd->global_vars[i].var_name);
    }
    js_free(ctx, fd->global_vars);

    for(i = 0; i < fd->closure_var_count; i++) {
        JSClosureVar *cv = &fd->closure_var[i];
        JS_FreeAtom(ctx, cv->var_name);
    }
    js_free(ctx, fd->closure_var);

    if (fd->scopes != fd->def_scope_array)
        js_free(ctx, fd->scopes);

    JS_FreeAtom(ctx, fd->filename);
    dbuf_free(&fd->pc2line);

    js_free(ctx, fd->source);

    if (fd->parent) {
        /* remove in parent list */
        list_del(&fd->link);
    }
    js_free(ctx, fd);
}

#ifdef DUMP_BYTECODE
static const char *skip_lines(const char *p, int n) {
    while (n-- > 0 && *p) {
        while (*p && *p++ != '\n')
            continue;
    }
    return p;
}

static void print_lines(const char *source, int line, int line1) {
    const char *s = source;
    const char *p = skip_lines(s, line);
    if (*p) {
        while (line++ < line1) {
            p = skip_lines(s = p, 1);
            printf(";; %.*s", (int)(p - s), s);
            if (!*p) {
                if (p[-1] != '\n')
                    printf("\n");
                break;
            }
        }
    }
}

static void dump_byte_code(JSContext *ctx, int pass,
                           const uint8_t *tab, int len,
                           const JSBytecodeVarDef *vardefs, 
                           const JSVarDef *args, int arg_count,
                           const JSVarDef *vars, int var_count,
                           const JSClosureVar *closure_var, int closure_var_count,
                           const JSValue *cpool, uint32_t cpool_count,
                           const char *source,
                           const LabelSlot *label_slots, JSFunctionBytecode *b)
{
    const JSOpCode *oi;
    int pos, pos_next, op, size, idx, addr, line, line1, in_source, line_num;
    uint8_t *bits = js_mallocz(ctx, len * sizeof(*bits));
    BOOL use_short_opcodes = (b != NULL), dump_pc;

    if (b) {
        int col_num;
        line_num = find_line_num(ctx, b, -1, &col_num);
    }
    
    /* scan for jump targets */
    for (pos = 0; pos < len; pos = pos_next) {
        op = tab[pos];
        if (use_short_opcodes)
            oi = &short_opcode_info(op);
        else
            oi = &opcode_info[op];
        pos_next = pos + oi->size;
        if (op < OP_COUNT) {
            switch (oi->fmt) {
#if SHORT_OPCODES
            case OP_FMT_label8:
                pos++;
                addr = (int8_t)tab[pos];
                goto has_addr;
            case OP_FMT_label16:
                pos++;
                addr = (int16_t)get_u16(tab + pos);
                goto has_addr;
#endif
            case OP_FMT_atom_label_u8:
            case OP_FMT_atom_label_u16:
                pos += 4;
                /* fall thru */
            case OP_FMT_label:
            case OP_FMT_label_u16:
                pos++;
                addr = get_u32(tab + pos);
                goto has_addr;
            has_addr:
                if (pass == 1)
                    addr = label_slots[addr].pos;
                if (pass == 2)
                    addr = label_slots[addr].pos2;
                if (pass == 3)
                    addr += pos;
                if (addr >= 0 && addr < len)
                    bits[addr] |= 1;
                break;
            }
        }
    }
    in_source = 0;
    if (source) {
        /* Always print first line: needed if single line */
        print_lines(source, 0, 1);
        in_source = 1;
    }
    line1 = line = 1;
    pos = 0;
    while (pos < len) {
        op = tab[pos];
        if (source && b) {
            int col_num;
            if (b) {
                line1 = find_line_num(ctx, b, pos, &col_num) - line_num + 1;
            } else if (op == OP_line_num) {
                /* XXX: no longer works */
                line1 = get_u32(tab + pos + 1) - line_num + 1;
            }
            if (line1 > line) {
                if (!in_source)
                    printf("\n");
                in_source = 1;
                print_lines(source, line, line1);
                line = line1;
                //bits[pos] |= 2;
            }
        }
        if (in_source)
            printf("\n");
        in_source = 0;
        if (op >= OP_COUNT) {
            printf("invalid opcode (0x%02x)\n", op);
            pos++;
            continue;
        }
        if (use_short_opcodes)
            oi = &short_opcode_info(op);
        else
            oi = &opcode_info[op];
        size = oi->size;
        if (pos + size > len) {
            printf("truncated opcode (0x%02x)\n", op);
            break;
        }
#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 16)
        {
            int i, x, x0;
            x = x0 = printf("%5d ", pos);
            for (i = 0; i < size; i++) {
                if (i == 6) {
                    printf("\n%*s", x = x0, "");
                }
                x += printf(" %02X", tab[pos + i]);
            }
            printf("%*s", x0 + 20 - x, "");
        }
#endif
#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 32)
        dump_pc = TRUE;
#else
        dump_pc = bits[pos];
#endif
        if (dump_pc) {
            printf("%5d:  ", pos);
        } else {
            printf("        ");
        }
        printf("%s", oi->name);
        pos++;
        switch(oi->fmt) {
        case OP_FMT_none_int:
            printf(" %d", op - OP_push_0);
            break;
        case OP_FMT_npopx:
            printf(" %d", op - OP_call0);
            break;
        case OP_FMT_u8:
            printf(" %u", get_u8(tab + pos));
            break;
        case OP_FMT_i8:
            printf(" %d", get_i8(tab + pos));
            break;
        case OP_FMT_u16:
        case OP_FMT_npop:
            printf(" %u", get_u16(tab + pos));
            break;
        case OP_FMT_npop_u16:
            printf(" %u,%u", get_u16(tab + pos), get_u16(tab + pos + 2));
            break;
        case OP_FMT_i16:
            printf(" %d", get_i16(tab + pos));
            break;
        case OP_FMT_i32:
            printf(" %d", get_i32(tab + pos));
            break;
        case OP_FMT_u32:
            printf(" %u", get_u32(tab + pos));
            break;
#if SHORT_OPCODES
        case OP_FMT_label8:
            addr = get_i8(tab + pos);
            goto has_addr1;
        case OP_FMT_label16:
            addr = get_i16(tab + pos);
            goto has_addr1;
#endif
        case OP_FMT_label:
            addr = get_u32(tab + pos);
            goto has_addr1;
        has_addr1:
            if (pass == 1)
                printf(" %u:%u", addr, label_slots[addr].pos);
            if (pass == 2)
                printf(" %u:%u", addr, label_slots[addr].pos2);
            if (pass == 3)
                printf(" %u", addr + pos);
            break;
        case OP_FMT_label_u16:
            addr = get_u32(tab + pos);
            if (pass == 1)
                printf(" %u:%u", addr, label_slots[addr].pos);
            if (pass == 2)
                printf(" %u:%u", addr, label_slots[addr].pos2);
            if (pass == 3)
                printf(" %u", addr + pos);
            printf(",%u", get_u16(tab + pos + 4));
            break;
#if SHORT_OPCODES
        case OP_FMT_const8:
            idx = get_u8(tab + pos);
            goto has_pool_idx;
#endif
        case OP_FMT_const:
            idx = get_u32(tab + pos);
            goto has_pool_idx;
        has_pool_idx:
            printf(" %u: ", idx);
            if (idx < cpool_count) {
                JS_PrintValue(ctx, js_dump_value_write, stdout, cpool[idx], NULL);
            }
            break;
        case OP_FMT_atom:
            printf(" ");
            print_atom(ctx, get_u32(tab + pos));
            break;
        case OP_FMT_atom_u8:
            printf(" ");
            print_atom(ctx, get_u32(tab + pos));
            printf(",%d", get_u8(tab + pos + 4));
            break;
        case OP_FMT_atom_u16:
            printf(" ");
            print_atom(ctx, get_u32(tab + pos));
            printf(",%d", get_u16(tab + pos + 4));
            break;
        case OP_FMT_atom_label_u8:
        case OP_FMT_atom_label_u16:
            printf(" ");
            print_atom(ctx, get_u32(tab + pos));
            addr = get_u32(tab + pos + 4);
            if (pass == 1)
                printf(",%u:%u", addr, label_slots[addr].pos);
            if (pass == 2)
                printf(",%u:%u", addr, label_slots[addr].pos2);
            if (pass == 3)
                printf(",%u", addr + pos + 4);
            if (oi->fmt == OP_FMT_atom_label_u8)
                printf(",%u", get_u8(tab + pos + 8));
            else
                printf(",%u", get_u16(tab + pos + 8));
            break;
        case OP_FMT_none_loc:
            idx = (op - OP_get_loc0) % 4;
            goto has_loc;
        case OP_FMT_loc8:
            idx = get_u8(tab + pos);
            goto has_loc;
        case OP_FMT_loc:
            idx = get_u16(tab + pos);
        has_loc:
            printf(" %d: ", idx);
            if (idx < var_count) {
                print_atom(ctx, vars ? vars[idx].var_name : vardefs[arg_count + idx].var_name);
            }
            break;
        case OP_FMT_none_arg:
            idx = (op - OP_get_arg0) % 4;
            goto has_arg;
        case OP_FMT_arg:
            idx = get_u16(tab + pos);
        has_arg:
            printf(" %d: ", idx);
            if (idx < arg_count) {
                print_atom(ctx, args ? args[idx].var_name : vardefs[idx].var_name);
            }
            break;
        case OP_FMT_none_var_ref:
            idx = (op - OP_get_var_ref0) % 4;
            goto has_var_ref;
        case OP_FMT_var_ref:
            idx = get_u16(tab + pos);
        has_var_ref:
            printf(" %d: ", idx);
            if (idx < closure_var_count) {
                print_atom(ctx, closure_var[idx].var_name);
            }
            break;
        default:
            break;
        }
        printf("\n");
        pos += oi->size - 1;
    }
    if (source) {
        if (!in_source)
            printf("\n");
        print_lines(source, line, INT32_MAX);
    }
    js_free(ctx, bits);
}

static __maybe_unused void dump_pc2line(JSContext *ctx, const uint8_t *buf, int len)
{
    const uint8_t *p_end, *p;
    int pc, v, line_num, col_num, ret;
    unsigned int op;
    uint32_t val;
    
    if (len <= 0)
        return;

    printf("%5s %5s %5s\n", "PC", "LINE", "COL");

    p = buf;
    p_end = buf + len;
    
    /* get the function line and column numbers */
    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    line_num = val + 1;

    ret = get_leb128(&val, p, p_end);
    if (ret < 0)
        goto fail;
    p += ret;
    col_num = val + 1;

    printf("%5s %5d %5d\n", "-", line_num, col_num);
    
    pc = 0;
    while (p < p_end) {
        op = *p++;
        if (op == 0) {
            ret = get_leb128(&val, p, p_end);
            if (ret < 0)
                goto fail;
            pc += val;
            p += ret;
            ret = get_sleb128(&v, p, p_end);
            if (ret < 0)
                goto fail;
            p += ret;
            line_num += v;
        } else {
            op -= PC2LINE_OP_FIRST;
            pc += (op / PC2LINE_RANGE);
            line_num += (op % PC2LINE_RANGE) + PC2LINE_BASE;
        }
        ret = get_sleb128(&v, p, p_end);
        if (ret < 0)
            goto fail;
        p += ret;
        col_num += v;
        
        printf("%5d %5d %5d\n", pc, line_num, col_num);
    }
 fail: ;
}

static __maybe_unused void js_dump_function_bytecode(JSContext *ctx, JSFunctionBytecode *b)
{
    int i;
    char atom_buf[ATOM_GET_STR_BUF_SIZE];
    const char *str;

    if (b->has_debug && b->debug.filename != JS_ATOM_NULL) {
        int line_num, col_num;
        str = JS_AtomGetStr(ctx, atom_buf, sizeof(atom_buf), b->debug.filename);
        line_num = find_line_num(ctx, b, -1, &col_num);
        printf("%s:%d:%d: ", str, line_num, col_num);
    }

    str = JS_AtomGetStr(ctx, atom_buf, sizeof(atom_buf), b->func_name);
    printf("function: %s%s\n", &"*"[b->func_kind != JS_FUNC_GENERATOR], str);
    if (b->js_mode) {
        printf("  mode:");
        if (b->js_mode & JS_MODE_STRICT)
            printf(" strict");
        printf("\n");
    }
    if (b->arg_count && b->vardefs) {
        printf("  args:");
        for(i = 0; i < b->arg_count; i++) {
            printf(" %s", JS_AtomGetStr(ctx, atom_buf, sizeof(atom_buf),
                                        b->vardefs[i].var_name));
        }
        printf("\n");
    }
    if (b->var_count && b->vardefs) {
        printf("  locals:\n");
        for(i = 0; i < b->var_count; i++) {
            JSBytecodeVarDef *vd = &b->vardefs[b->arg_count + i];
            printf("%5d: %s %s", i,
                   vd->var_kind == JS_VAR_CATCH ? "catch" :
                   (vd->var_kind == JS_VAR_FUNCTION_DECL ||
                    vd->var_kind == JS_VAR_NEW_FUNCTION_DECL) ? "function" :
                   vd->is_const ? "const" :
                   vd->is_lexical ? "let" : "var",
                   JS_AtomGetStr(ctx, atom_buf, sizeof(atom_buf), vd->var_name));
            if (vd->has_scope)
                printf(" [next:%d]", vd->scope_next);
            printf("\n");
        }
    }
    if (b->closure_var_count) {
        printf("  closure vars:\n");
        for(i = 0; i < b->closure_var_count; i++) {
            JSClosureVar *cv = &b->closure_var[i];
            printf("%5d: %s %s", i,
                   cv->is_const ? "const" :
                   cv->is_lexical ? "let" : "var",
                   JS_AtomGetStr(ctx, atom_buf, sizeof(atom_buf), cv->var_name));
            switch(cv->closure_type) {
            case JS_CLOSURE_LOCAL:
                printf(" [loc%d]\n", cv->var_idx);
                break;
            case JS_CLOSURE_ARG:
                printf(" [arg%d]\n", cv->var_idx);
                break;
            case JS_CLOSURE_REF:
                printf(" [ref%d]\n", cv->var_idx);
                break;
            case JS_CLOSURE_GLOBAL_REF:
                printf(" [global_ref%d]\n", cv->var_idx);
                break;
            case JS_CLOSURE_GLOBAL_DECL:
                printf(" [global_decl]\n");
                break;
            case JS_CLOSURE_GLOBAL:
                printf(" [global]\n");
                break;
            case JS_CLOSURE_MODULE_DECL:
                printf(" [module_decl]\n");
                break;
            case JS_CLOSURE_MODULE_IMPORT:
                printf(" [module_import]\n");
                break;
            default:
                printf(" [?]\n");
                break;
            }
        }
    }
    printf("  stack_size: %d\n", b->stack_size);
    printf("  var_ref_count: %d\n", b->var_ref_count);
    printf("  opcodes:\n");
    dump_byte_code(ctx, 3, b->byte_code_buf, b->byte_code_len,
                   b->vardefs,
                   NULL, b->arg_count,
                   NULL, b->var_count,
                   b->closure_var, b->closure_var_count,
                   b->cpool, b->cpool_count,
                   b->has_debug ? b->debug.source : NULL,
                   NULL, b);
#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 32)
    if (b->has_debug)
        dump_pc2line(ctx, b->debug.pc2line_buf, b->debug.pc2line_len);
#endif
    printf("\n");
}
#endif

int add_closure_var(JSContext *ctx, JSFunctionDef *s,
                           JSClosureTypeEnum closure_type,
                           int var_idx, JSAtom var_name,
                           BOOL is_const, BOOL is_lexical,
                           JSVarKindEnum var_kind)
{
    JSClosureVar *cv;

    /* the closure variable indexes are currently stored on 16 bits */
    if (s->closure_var_count >= JS_MAX_LOCAL_VARS) {
        JS_ThrowInternalError(ctx, "too many closure variables");
        return -1;
    }

    if (js_resize_array(ctx, (void **)&s->closure_var,
                        sizeof(s->closure_var[0]),
                        &s->closure_var_size, s->closure_var_count + 1))
        return -1;
    cv = &s->closure_var[s->closure_var_count++];
    cv->closure_type = closure_type;
    cv->is_const = is_const;
    cv->is_lexical = is_lexical;
    cv->var_kind = var_kind;
    cv->var_idx = var_idx;
    cv->var_name = JS_DupAtom(ctx, var_name);
    return s->closure_var_count - 1;
}

static int find_closure_var(JSContext *ctx, JSFunctionDef *s,
                            JSAtom var_name)
{
    int i;
    for(i = 0; i < s->closure_var_count; i++) {
        JSClosureVar *cv = &s->closure_var[i];
        if (cv->var_name == var_name)
            return i;
    }
    return -1;
}

/* 'fd' must be a parent of 's'. Create in 's' a closure referencing
   another one in 'fd' */
static int get_closure_var(JSContext *ctx, JSFunctionDef *s,
                           JSFunctionDef *fd, JSClosureTypeEnum closure_type,
                           int var_idx, JSAtom var_name,
                           BOOL is_const, BOOL is_lexical,
                           JSVarKindEnum var_kind)
{
    int i;

    if (fd != s->parent) {
        var_idx = get_closure_var(ctx, s->parent, fd, closure_type,
                                  var_idx, var_name,
                                  is_const, is_lexical, var_kind);
        if (var_idx < 0)
            return -1;
        if (closure_type != JS_CLOSURE_GLOBAL_REF)
            closure_type = JS_CLOSURE_REF;
    }
    for(i = 0; i < s->closure_var_count; i++) {
        JSClosureVar *cv = &s->closure_var[i];
        if (cv->var_idx == var_idx && cv->closure_type == closure_type)
            return i;
    }
    return add_closure_var(ctx, s, closure_type, var_idx, var_name,
                           is_const, is_lexical, var_kind);
}

static int get_with_scope_opcode(int op)
{
    if (op == OP_scope_get_var_undef)
        return OP_with_get_var;
    else
        return OP_with_get_var + (op - OP_scope_get_var);
}

static BOOL can_opt_put_ref_value(const uint8_t *bc_buf, int pos)
{
    int opcode = bc_buf[pos];
    return (bc_buf[pos + 1] == OP_put_ref_value &&
            (opcode == OP_insert3 ||
             opcode == OP_perm4 ||
             opcode == OP_nop ||
             opcode == OP_rot3l));
}

static BOOL can_opt_put_global_ref_value(const uint8_t *bc_buf, int pos)
{
    int opcode = bc_buf[pos];
    return (bc_buf[pos + 1] == OP_put_ref_value &&
            (opcode == OP_insert3 ||
             opcode == OP_perm4 ||
             opcode == OP_nop ||
             opcode == OP_rot3l));
}

static int optimize_scope_make_ref(JSContext *ctx, JSFunctionDef *s,
                                   DynBuf *bc, uint8_t *bc_buf,
                                   LabelSlot *ls, int pos_next,
                                   int get_op, int var_idx)
{
    int label_pos, end_pos, pos;

    /* XXX: should optimize `loc(a) += expr` as `expr add_loc(a)`
       but only if expr does not modify `a`.
       should scan the code between pos_next and label_pos
       for operations that can potentially change `a`:
       OP_scope_make_ref(a), function calls, jumps and gosub.
     */
    /* replace the reference get/put with normal variable
       accesses */
    if (bc_buf[pos_next] == OP_get_ref_value) {
        dbuf_putc(bc, get_op);
        dbuf_put_u16(bc, var_idx);
        pos_next++;
    }
    /* remove the OP_label to make room for replacement */
    /* label should have a refcount of 0 anyway */
    /* XXX: should avoid this patch by inserting nops in phase 1 */
    label_pos = ls->pos;
    pos = label_pos - 5;
    assert(bc_buf[pos] == OP_label);
    /* label points to an instruction pair:
       - insert3 / put_ref_value
       - perm4 / put_ref_value
       - rot3l / put_ref_value
       - nop / put_ref_value
     */
    end_pos = label_pos + 2;
    if (bc_buf[label_pos] == OP_insert3)
        bc_buf[pos++] = OP_dup;
    bc_buf[pos] = get_op + 1;
    put_u16(bc_buf + pos + 1, var_idx);
    pos += 3;
    /* pad with OP_nop */
    while (pos < end_pos)
        bc_buf[pos++] = OP_nop;
    return pos_next;
}

static int add_var_this(JSContext *ctx, JSFunctionDef *fd)
{
    int idx;
    idx = add_var(ctx, fd, JS_ATOM_this);
    if (idx >= 0 && fd->is_derived_class_constructor) {
        JSVarDef *vd = &fd->vars[idx];
        /* XXX: should have is_this flag or var type */
        vd->is_lexical = 1; /* used to trigger 'uninitialized' checks
                               in a derived class constructor */
    }
    return idx;
}

static int resolve_pseudo_var(JSContext *ctx, JSFunctionDef *s,
                               JSAtom var_name)
{
    int var_idx;

    if (!s->has_this_binding)
        return -1;
    switch(var_name) {
    case JS_ATOM_home_object:
        /* 'home_object' pseudo variable */
        if (s->home_object_var_idx < 0)
            s->home_object_var_idx = add_var(ctx, s, var_name);
        var_idx = s->home_object_var_idx;
        break;
    case JS_ATOM_this_active_func:
        /* 'this.active_func' pseudo variable */
        if (s->this_active_func_var_idx < 0)
            s->this_active_func_var_idx = add_var(ctx, s, var_name);
        var_idx = s->this_active_func_var_idx;
        break;
    case JS_ATOM_new_target:
        /* 'new.target' pseudo variable */
        if (s->new_target_var_idx < 0)
            s->new_target_var_idx = add_var(ctx, s, var_name);
        var_idx = s->new_target_var_idx;
        break;
    case JS_ATOM_this:
        /* 'this' pseudo variable */
        if (s->this_var_idx < 0)
            s->this_var_idx = add_var_this(ctx, s);
        var_idx = s->this_var_idx;
        break;
    default:
        var_idx = -1;
        break;
    }
    return var_idx;
}

/* test if 'var_name' is in the variable object on the stack. If is it
   the case, handle it and jump to 'label_done' */
static void var_object_test(JSContext *ctx, JSFunctionDef *s,
                            JSAtom var_name, int op, DynBuf *bc,
                            int *plabel_done, BOOL is_with)
{
    dbuf_putc(bc, get_with_scope_opcode(op));
    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
    if (*plabel_done < 0) {
        *plabel_done = new_label_fd(s);
        if (*plabel_done < 0) {
            dbuf_set_error(bc);
            return;
        }
    }
    dbuf_put_u32(bc, *plabel_done);
    dbuf_putc(bc, is_with);
    update_label(s, *plabel_done, 1);
    s->jump_size++;
}

static inline void capture_var(JSFunctionDef *s, JSVarDef *vd)
{
    if (!vd->is_captured) {
        vd->is_captured = 1;
        vd->var_ref_idx = s->var_ref_count++;
    }
}

/* return the position of the next opcode or -1 if error */
static int resolve_scope_var(JSContext *ctx, JSFunctionDef *s,
                             JSAtom var_name, int scope_level, int op,
                             DynBuf *bc, uint8_t *bc_buf,
                             LabelSlot *ls, int pos_next)
{
    int idx, var_idx, is_put;
    int label_done;
    JSFunctionDef *fd;
    JSVarDef *vd;
    BOOL is_pseudo_var, is_arg_scope;

    label_done = -1;

    /* XXX: could be simpler to use a specific function to
       resolve the pseudo variables */
    is_pseudo_var = (var_name == JS_ATOM_home_object ||
                     var_name == JS_ATOM_this_active_func ||
                     var_name == JS_ATOM_new_target ||
                     var_name == JS_ATOM_this);

    /* resolve local scoped variables */
    var_idx = -1;
    for (idx = s->scopes[scope_level].first; idx >= 0;) {
        vd = &s->vars[idx];
        if (vd->var_name == var_name) {
            if (op == OP_scope_put_var || op == OP_scope_make_ref) {
                if (vd->is_const) {
                    dbuf_putc(bc, OP_throw_error);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    dbuf_putc(bc, JS_THROW_VAR_RO);
                    goto done;
                }
            }
            var_idx = idx;
            break;
        } else
        if (vd->var_name == JS_ATOM__with_ && !is_pseudo_var) {
            dbuf_putc(bc, OP_get_loc);
            dbuf_put_u16(bc, idx);
            var_object_test(ctx, s, var_name, op, bc, &label_done, 1);
        }
        idx = vd->scope_next;
    }
    is_arg_scope = (idx == ARG_SCOPE_END);
    if (var_idx < 0) {
        /* argument scope: variables are not visible but pseudo
           variables are visible */
        if (!is_arg_scope) {
            var_idx = find_var(ctx, s, var_name);
        }

        if (var_idx < 0 && is_pseudo_var)
            var_idx = resolve_pseudo_var(ctx, s, var_name);

        if (var_idx < 0 && var_name == JS_ATOM_arguments &&
            s->has_arguments_binding) {
            /* 'arguments' pseudo variable */
            var_idx = add_arguments_var(ctx, s);
        }
        if (var_idx < 0 && s->is_func_expr && var_name == s->func_name) {
            /* add a new variable with the function name */
            var_idx = add_func_var(ctx, s, var_name);
        }
    }
    if (var_idx >= 0) {
        if ((op == OP_scope_put_var || op == OP_scope_make_ref) &&
            !(var_idx & ARGUMENT_VAR_OFFSET) &&
            s->vars[var_idx].is_const) {
            /* only happens when assigning a function expression name
               in strict mode */
            dbuf_putc(bc, OP_throw_error);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            dbuf_putc(bc, JS_THROW_VAR_RO);
            goto done;
        }
        /* OP_scope_put_var_init is only used to initialize a
           lexical variable, so it is never used in a with or var object. It
           can be used with a closure (module global variable case). */
        switch (op) {
        case OP_scope_make_ref:
            if (!(var_idx & ARGUMENT_VAR_OFFSET) &&
                s->vars[var_idx].var_kind == JS_VAR_FUNCTION_NAME) {
                /* Create a dummy object reference for the func_var */
                dbuf_putc(bc, OP_object);
                dbuf_putc(bc, OP_get_loc);
                dbuf_put_u16(bc, var_idx);
                dbuf_putc(bc, OP_define_field);
                dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                dbuf_putc(bc, OP_push_atom_value);
                dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            } else
            if (label_done == -1 && can_opt_put_ref_value(bc_buf, ls->pos)) {
                int get_op;
                if (var_idx & ARGUMENT_VAR_OFFSET) {
                    get_op = OP_get_arg;
                    var_idx -= ARGUMENT_VAR_OFFSET;
                } else {
                    if (s->vars[var_idx].is_lexical)
                        get_op = OP_get_loc_check;
                    else
                        get_op = OP_get_loc;
                }
                pos_next = optimize_scope_make_ref(ctx, s, bc, bc_buf, ls,
                                                   pos_next, get_op, var_idx);
            } else {
                /* Create a dummy object with a named slot that is
                   a reference to the local variable */
                if (var_idx & ARGUMENT_VAR_OFFSET) {
                    capture_var(s, &s->args[var_idx - ARGUMENT_VAR_OFFSET]);
                    dbuf_putc(bc, OP_make_arg_ref);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    dbuf_put_u16(bc, var_idx - ARGUMENT_VAR_OFFSET);
                } else {
                    capture_var(s, &s->vars[var_idx]);
                    dbuf_putc(bc, OP_make_loc_ref);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    dbuf_put_u16(bc, var_idx);
                }
            }
            break;
        case OP_scope_put_var:
            if (!(var_idx & ARGUMENT_VAR_OFFSET) &&
                s->vars[var_idx].var_kind == JS_VAR_FUNCTION_NAME) {
                /* in non strict mode, modifying the function name is ignored */
                dbuf_putc(bc, OP_drop);
                goto done;
            }
            goto local_scope_var;
        case OP_scope_get_ref:
            dbuf_putc(bc, OP_undefined);
            goto local_scope_var;
        case OP_scope_get_var_checkthis:
        case OP_scope_get_var_undef:
        case OP_scope_get_var:
        case OP_scope_put_var_init:
        local_scope_var:
            is_put = (op == OP_scope_put_var || op == OP_scope_put_var_init);
            if (var_idx & ARGUMENT_VAR_OFFSET) {
                dbuf_putc(bc, OP_get_arg + is_put);
                dbuf_put_u16(bc, var_idx - ARGUMENT_VAR_OFFSET);
            } else {
                if (is_put) {
                    if (s->vars[var_idx].is_lexical) {
                        if (op == OP_scope_put_var_init) {
                            /* 'this' can only be initialized once */
                            if (var_name == JS_ATOM_this)
                                dbuf_putc(bc, OP_put_loc_check_init);
                            else
                                dbuf_putc(bc, OP_put_loc);
                        } else {
                            dbuf_putc(bc, OP_put_loc_check);
                        }
                    } else {
                        dbuf_putc(bc, OP_put_loc);
                    }
                } else {
                    if (s->vars[var_idx].is_lexical) {
                        if (op == OP_scope_get_var_checkthis) {
                            /* only used for 'this' return in derived class constructors */
                            dbuf_putc(bc, OP_get_loc_checkthis);
                        } else {
                            dbuf_putc(bc, OP_get_loc_check);
                        }
                    } else {
                        dbuf_putc(bc, OP_get_loc);
                    }
                }
                dbuf_put_u16(bc, var_idx);
            }
            break;
        case OP_scope_delete_var:
            dbuf_putc(bc, OP_push_false);
            break;
        }
        goto done;
    }
    /* check eval object */
    if (!is_arg_scope && s->var_object_idx >= 0 && !is_pseudo_var) {
        dbuf_putc(bc, OP_get_loc);
        dbuf_put_u16(bc, s->var_object_idx);
        var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
    }
    /* check eval object in argument scope */
    if (s->arg_var_object_idx >= 0 && !is_pseudo_var) {
        dbuf_putc(bc, OP_get_loc);
        dbuf_put_u16(bc, s->arg_var_object_idx);
        var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
    }

    /* check parent scopes */
    for (fd = s; fd->parent;) {
        scope_level = fd->parent_scope_level;
        fd = fd->parent;
        for (idx = fd->scopes[scope_level].first; idx >= 0;) {
            vd = &fd->vars[idx];
            if (vd->var_name == var_name) {
                if (op == OP_scope_put_var || op == OP_scope_make_ref) {
                    if (vd->is_const) {
                        dbuf_putc(bc, OP_throw_error);
                        dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                        dbuf_putc(bc, JS_THROW_VAR_RO);
                        goto done;
                    }
                }
                var_idx = idx;
                break;
            } else if (vd->var_name == JS_ATOM__with_ && !is_pseudo_var) {
                capture_var(fd, vd);
                idx = get_closure_var(ctx, s, fd, JS_CLOSURE_LOCAL, idx, vd->var_name, FALSE, FALSE, JS_VAR_NORMAL);
                if (idx >= 0) {
                    dbuf_putc(bc, OP_get_var_ref);
                    dbuf_put_u16(bc, idx);
                    var_object_test(ctx, s, var_name, op, bc, &label_done, 1);
                }
            }
            idx = vd->scope_next;
        }
        is_arg_scope = (idx == ARG_SCOPE_END);
        if (var_idx >= 0)
            break;

        if (!is_arg_scope) {
            var_idx = find_var(ctx, fd, var_name);
            if (var_idx >= 0)
                break;
        }
        if (is_pseudo_var) {
            var_idx = resolve_pseudo_var(ctx, fd, var_name);
            if (var_idx >= 0)
                break;
        }
        if (var_name == JS_ATOM_arguments && fd->has_arguments_binding) {
            var_idx = add_arguments_var(ctx, fd);
            break;
        }
        if (fd->is_func_expr && fd->func_name == var_name) {
            /* add a new variable with the function name */
            var_idx = add_func_var(ctx, fd, var_name);
            break;
        }

        /* check eval object */
        if (!is_arg_scope && fd->var_object_idx >= 0 && !is_pseudo_var) {
            vd = &fd->vars[fd->var_object_idx];
            capture_var(fd, vd);
            idx = get_closure_var(ctx, s, fd, JS_CLOSURE_LOCAL,
                                  fd->var_object_idx, vd->var_name,
                                  FALSE, FALSE, JS_VAR_NORMAL);
            dbuf_putc(bc, OP_get_var_ref);
            dbuf_put_u16(bc, idx);
            var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
        }

        /* check eval object in argument scope */
        if (fd->arg_var_object_idx >= 0 && !is_pseudo_var) {
            vd = &fd->vars[fd->arg_var_object_idx];
            capture_var(fd, vd);
            idx = get_closure_var(ctx, s, fd, JS_CLOSURE_LOCAL,
                                  fd->arg_var_object_idx, vd->var_name,
                                  FALSE, FALSE, JS_VAR_NORMAL);
            dbuf_putc(bc, OP_get_var_ref);
            dbuf_put_u16(bc, idx);
            var_object_test(ctx, s, var_name, op, bc, &label_done, 0);
        }

        if (fd->is_eval)
            break; /* it it necessarily the top level function */
    }

    /* check direct eval scope (in the closure of the eval function
       which is necessarily at the top level) */
    if (!fd)
        fd = s;
    if (var_idx < 0 && fd->is_eval) {
        int idx1;
        for (idx1 = 0; idx1 < fd->closure_var_count; idx1++) {
            JSClosureVar *cv = &fd->closure_var[idx1];
            if (var_name == cv->var_name) {
                if (fd != s) {
                    JSClosureTypeEnum closure_type;
                    if (cv->closure_type == JS_CLOSURE_GLOBAL ||
                        cv->closure_type == JS_CLOSURE_GLOBAL_DECL ||
                        cv->closure_type == JS_CLOSURE_GLOBAL_REF)
                        closure_type = JS_CLOSURE_GLOBAL_REF;
                    else
                        closure_type = JS_CLOSURE_REF;
                    idx = get_closure_var(ctx, s, fd,
                                          closure_type,
                                          idx1,
                                          cv->var_name, cv->is_const,
                                          cv->is_lexical, cv->var_kind);
                } else {
                    idx = idx1;
                }
                if (cv->closure_type == JS_CLOSURE_GLOBAL ||
                    cv->closure_type == JS_CLOSURE_GLOBAL_DECL ||
                    cv->closure_type == JS_CLOSURE_GLOBAL_REF)
                    goto has_global_idx;
                else
                    goto has_idx;
            } else if ((cv->var_name == JS_ATOM__var_ ||
                        cv->var_name == JS_ATOM__arg_var_ ||
                        cv->var_name == JS_ATOM__with_) && !is_pseudo_var) {
                int is_with = (cv->var_name == JS_ATOM__with_);
                if (fd != s) {
                    idx = get_closure_var(ctx, s, fd,
                                          JS_CLOSURE_REF,
                                          idx1,
                                          cv->var_name, FALSE, FALSE,
                                          JS_VAR_NORMAL);
                } else {
                    idx = idx1;
                }
                dbuf_putc(bc, OP_get_var_ref);
                dbuf_put_u16(bc, idx);
                var_object_test(ctx, s, var_name, op, bc, &label_done, is_with);
            }
        }

        /* not found: add a closure for a global variable access */
        idx1 = add_closure_var(ctx, fd, JS_CLOSURE_GLOBAL, 0, var_name,
                              FALSE, FALSE, JS_VAR_NORMAL);
        if (idx1 < 0)
            return -1;
        if (fd != s) {
            idx = get_closure_var(ctx, s, fd,
                                  JS_CLOSURE_GLOBAL_REF,
                                  idx1,
                                  var_name, FALSE, FALSE, 
                                  JS_VAR_NORMAL);
        } else {
            idx = idx1;
        }
    has_global_idx:
        /* global variable access */
        switch (op) {
        case OP_scope_make_ref:
            if (label_done == -1 && can_opt_put_global_ref_value(bc_buf, ls->pos)) {
                pos_next = optimize_scope_make_ref(ctx, s, bc, bc_buf, ls,
                                                   pos_next,
                                                   OP_get_var, idx);
            } else {
                dbuf_putc(bc, OP_make_var_ref);
                dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            }
            break;
        case OP_scope_get_ref:
            /* XXX: should create a dummy object with a named slot that is
               a reference to the global variable */
            dbuf_putc(bc, OP_undefined);
            dbuf_putc(bc, OP_get_var);
            dbuf_put_u16(bc, idx);
            break;
        case OP_scope_get_var_undef:
        case OP_scope_get_var:
        case OP_scope_put_var:
            dbuf_putc(bc, OP_get_var_undef + (op - OP_scope_get_var_undef));
            dbuf_put_u16(bc, idx);
            break;
        case OP_scope_put_var_init:
            dbuf_putc(bc, OP_put_var_init);
            dbuf_put_u16(bc, idx);
            break;
        case OP_scope_delete_var:
            dbuf_putc(bc, OP_delete_var);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            break;
        }
    } else {
        /* find the corresponding closure variable */
        if (var_idx & ARGUMENT_VAR_OFFSET) {
            capture_var(fd, &fd->args[var_idx - ARGUMENT_VAR_OFFSET]);
            idx = get_closure_var(ctx, s, fd,
                                  JS_CLOSURE_ARG, var_idx - ARGUMENT_VAR_OFFSET,
                                  var_name, FALSE, FALSE, JS_VAR_NORMAL);
        } else {
            capture_var(fd, &fd->vars[var_idx]);
            idx = get_closure_var(ctx, s, fd,
                                  JS_CLOSURE_LOCAL, var_idx,
                                  var_name,
                                  fd->vars[var_idx].is_const,
                                  fd->vars[var_idx].is_lexical,
                                  fd->vars[var_idx].var_kind);
        }
        if (idx >= 0) {
        has_idx:
            if ((op == OP_scope_put_var || op == OP_scope_make_ref) &&
                s->closure_var[idx].is_const) {
                dbuf_putc(bc, OP_throw_error);
                dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                dbuf_putc(bc, JS_THROW_VAR_RO);
                goto done;
            }
            switch (op) {
            case OP_scope_make_ref:
                if (s->closure_var[idx].var_kind == JS_VAR_FUNCTION_NAME) {
                    /* Create a dummy object reference for the func_var */
                    dbuf_putc(bc, OP_object);
                    dbuf_putc(bc, OP_get_var_ref);
                    dbuf_put_u16(bc, idx);
                    dbuf_putc(bc, OP_define_field);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    dbuf_putc(bc, OP_push_atom_value);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                } else
                if (label_done == -1 &&
                    can_opt_put_ref_value(bc_buf, ls->pos)) {
                    int get_op;
                    if (s->closure_var[idx].is_lexical)
                        get_op = OP_get_var_ref_check;
                    else
                        get_op = OP_get_var_ref;
                    pos_next = optimize_scope_make_ref(ctx, s, bc, bc_buf, ls,
                                                       pos_next,
                                                       get_op, idx);
                } else {
                    /* Create a dummy object with a named slot that is
                       a reference to the closure variable */
                    dbuf_putc(bc, OP_make_var_ref_ref);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
                    dbuf_put_u16(bc, idx);
                }
                break;
            case OP_scope_put_var:
                if (s->closure_var[idx].var_kind == JS_VAR_FUNCTION_NAME) {
                    /* in non strict mode, modifying the function name is ignored */
                    dbuf_putc(bc, OP_drop);
                    goto done;
                }
                goto closure_scope_var;
            case OP_scope_get_ref:
                /* XXX: should create a dummy object with a named slot that is
                   a reference to the closure variable */
                dbuf_putc(bc, OP_undefined);
                goto closure_scope_var;
            case OP_scope_get_var_undef:
            case OP_scope_get_var:
            case OP_scope_put_var_init:
            closure_scope_var:
                is_put = (op == OP_scope_put_var ||
                          op == OP_scope_put_var_init);
                if (is_put) {
                    if (s->closure_var[idx].is_lexical) {
                        if (op == OP_scope_put_var_init) {
                            /* 'this' can only be initialized once */
                            if (var_name == JS_ATOM_this)
                                dbuf_putc(bc, OP_put_var_ref_check_init);
                            else
                                dbuf_putc(bc, OP_put_var_ref);
                        } else {
                            dbuf_putc(bc, OP_put_var_ref_check);
                        }
                    } else {
                        dbuf_putc(bc, OP_put_var_ref);
                    }
                } else {
                    if (s->closure_var[idx].is_lexical) {
                        dbuf_putc(bc, OP_get_var_ref_check);
                    } else {
                        dbuf_putc(bc, OP_get_var_ref);
                    }
                }
                dbuf_put_u16(bc, idx);
                break;
            case OP_scope_delete_var:
                dbuf_putc(bc, OP_push_false);
                break;
            }
            goto done;
        }
    }

done:
    if (label_done >= 0) {
        dbuf_putc(bc, OP_label);
        dbuf_put_u32(bc, label_done);
        s->label_slots[label_done].pos2 = bc->size;
    }
    return pos_next;
}

/* search in all scopes */
static int find_private_class_field_all(JSContext *ctx, JSFunctionDef *fd,
                                        JSAtom name, int scope_level)
{
    int idx;

    idx = fd->scopes[scope_level].first;
    while (idx >= 0) {
        if (fd->vars[idx].var_name == name)
            return idx;
        idx = fd->vars[idx].scope_next;
    }
    return -1;
}

static void get_loc_or_ref(DynBuf *bc, BOOL is_ref, int idx)
{
    /* if the field is not initialized, the error is catched when
       accessing it */
    if (is_ref)
        dbuf_putc(bc, OP_get_var_ref);
    else
        dbuf_putc(bc, OP_get_loc);
    dbuf_put_u16(bc, idx);
}

static int resolve_scope_private_field1(JSContext *ctx,
                                        BOOL *pis_ref, int *pvar_kind,
                                        JSFunctionDef *s,
                                        JSAtom var_name, int scope_level)
{
    int idx, var_kind;
    JSFunctionDef *fd;
    BOOL is_ref;

    fd = s;
    is_ref = FALSE;
    for(;;) {
        idx = find_private_class_field_all(ctx, fd, var_name, scope_level);
        if (idx >= 0) {
            var_kind = fd->vars[idx].var_kind;
            if (is_ref) {
                capture_var(fd, &fd->vars[idx]);
                idx = get_closure_var(ctx, s, fd, JS_CLOSURE_LOCAL, idx, var_name,
                                      TRUE, TRUE, JS_VAR_NORMAL);
                if (idx < 0)
                    return -1;
            }
            break;
        }
        scope_level = fd->parent_scope_level;
        if (!fd->parent) {
            if (fd->is_eval) {
                /* closure of the eval function (top level) */
                for (idx = 0; idx < fd->closure_var_count; idx++) {
                    JSClosureVar *cv = &fd->closure_var[idx];
                    if (cv->var_name == var_name) {
                        var_kind = cv->var_kind;
                        is_ref = TRUE;
                        if (fd != s) {
                            idx = get_closure_var(ctx, s, fd,
                                                  JS_CLOSURE_REF,
                                                  idx,
                                                  cv->var_name, cv->is_const,
                                                  cv->is_lexical,
                                                  cv->var_kind);
                            if (idx < 0)
                                return -1;
                        }
                        goto done;
                    }
                }
            }
            /* XXX: no line number info */
            JS_ThrowSyntaxErrorAtom(ctx, "undefined private field '%s'",
                                    var_name);
            return -1;
        } else {
            fd = fd->parent;
        }
        is_ref = TRUE;
    }
 done:
    *pis_ref = is_ref;
    *pvar_kind = var_kind;
    return idx;
}

/* return 0 if OK or -1 if the private field could not be resolved */
static int resolve_scope_private_field(JSContext *ctx, JSFunctionDef *s,
                                       JSAtom var_name, int scope_level, int op,
                                       DynBuf *bc)
{
    int idx, var_kind;
    BOOL is_ref;

    idx = resolve_scope_private_field1(ctx, &is_ref, &var_kind, s,
                                       var_name, scope_level);
    if (idx < 0)
        return -1;
    assert(var_kind != JS_VAR_NORMAL);
    switch (op) {
    case OP_scope_get_private_field:
    case OP_scope_get_private_field2:
        switch(var_kind) {
        case JS_VAR_PRIVATE_FIELD:
            if (op == OP_scope_get_private_field2)
                dbuf_putc(bc, OP_dup);
            get_loc_or_ref(bc, is_ref, idx);
            dbuf_putc(bc, OP_get_private_field);
            break;
        case JS_VAR_PRIVATE_METHOD:
            get_loc_or_ref(bc, is_ref, idx);
            dbuf_putc(bc, OP_check_brand);
            if (op != OP_scope_get_private_field2)
                dbuf_putc(bc, OP_nip);
            break;
        case JS_VAR_PRIVATE_GETTER:
        case JS_VAR_PRIVATE_GETTER_SETTER:
            if (op == OP_scope_get_private_field2)
                dbuf_putc(bc, OP_dup);
            get_loc_or_ref(bc, is_ref, idx);
            dbuf_putc(bc, OP_check_brand);
            dbuf_putc(bc, OP_call_method);
            dbuf_put_u16(bc, 0);
            break;
        case JS_VAR_PRIVATE_SETTER:
            /* XXX: add clearer error message */
            dbuf_putc(bc, OP_throw_error);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            dbuf_putc(bc, JS_THROW_VAR_RO);
            break;
        default:
            abort();
        }
        break;
    case OP_scope_put_private_field:
        switch(var_kind) {
        case JS_VAR_PRIVATE_FIELD:
            get_loc_or_ref(bc, is_ref, idx);
            dbuf_putc(bc, OP_put_private_field);
            break;
        case JS_VAR_PRIVATE_METHOD:
        case JS_VAR_PRIVATE_GETTER:
            /* XXX: add clearer error message */
            dbuf_putc(bc, OP_throw_error);
            dbuf_put_u32(bc, JS_DupAtom(ctx, var_name));
            dbuf_putc(bc, JS_THROW_VAR_RO);
            break;
        case JS_VAR_PRIVATE_SETTER:
        case JS_VAR_PRIVATE_GETTER_SETTER:
            {
                JSAtom setter_name = get_private_setter_name(ctx, var_name);
                if (setter_name == JS_ATOM_NULL)
                    return -1;
                idx = resolve_scope_private_field1(ctx, &is_ref,
                                                   &var_kind, s,
                                                   setter_name, scope_level);
                JS_FreeAtom(ctx, setter_name);
                if (idx < 0)
                    return -1;
                assert(var_kind == JS_VAR_PRIVATE_SETTER);
                get_loc_or_ref(bc, is_ref, idx);
                dbuf_putc(bc, OP_swap);
                /* obj func value */
                dbuf_putc(bc, OP_rot3r);
                /* value obj func */
                dbuf_putc(bc, OP_check_brand);
                dbuf_putc(bc, OP_rot3l);
                /* obj func value */
                dbuf_putc(bc, OP_call_method);
                dbuf_put_u16(bc, 1);
                dbuf_putc(bc, OP_drop);
            }
            break;
        default:
            abort();
        }
        break;
    case OP_scope_in_private_field:
        get_loc_or_ref(bc, is_ref, idx);
        dbuf_putc(bc, OP_private_in);
        break;
    default:
        abort();
    }
    return 0;
}

static void mark_eval_captured_variables(JSContext *ctx, JSFunctionDef *s,
                                         int scope_level)
{
    int idx;
    JSVarDef *vd;

    for (idx = s->scopes[scope_level].first; idx >= 0;) {
        vd = &s->vars[idx];
        capture_var(s, vd);
        idx = vd->scope_next;
    }
}

/* XXX: should handle the argument scope generically */
static BOOL is_var_in_arg_scope(JSAtom var_name, JSVarKindEnum var_kind)
{
    return (var_name == JS_ATOM_home_object ||
            var_name == JS_ATOM_this_active_func ||
            var_name == JS_ATOM_new_target ||
            var_name == JS_ATOM_this ||
            var_name == JS_ATOM__arg_var_ ||
            var_kind == JS_VAR_FUNCTION_NAME);
}

static void add_eval_variables(JSContext *ctx, JSFunctionDef *s)
{
    JSFunctionDef *fd;
    JSVarDef *vd;
    int i, scope_level, scope_idx;
    BOOL has_arguments_binding, has_this_binding, is_arg_scope;

    /* in non strict mode, variables are created in the caller's
       environment object */
    if (!s->is_eval && !(s->js_mode & JS_MODE_STRICT)) {
        s->var_object_idx = add_var(ctx, s, JS_ATOM__var_);
        if (s->has_parameter_expressions) {
            /* an additional variable object is needed for the
               argument scope */
            s->arg_var_object_idx = add_var(ctx, s, JS_ATOM__arg_var_);
        }
    }

    /* eval can potentially use 'arguments' so we must define it */
    has_this_binding = s->has_this_binding;
    if (has_this_binding) {
        if (s->this_var_idx < 0)
            s->this_var_idx = add_var_this(ctx, s);
        if (s->new_target_var_idx < 0)
            s->new_target_var_idx = add_var(ctx, s, JS_ATOM_new_target);
        if (s->is_derived_class_constructor && s->this_active_func_var_idx < 0)
            s->this_active_func_var_idx = add_var(ctx, s, JS_ATOM_this_active_func);
        if (s->has_home_object && s->home_object_var_idx < 0)
            s->home_object_var_idx = add_var(ctx, s, JS_ATOM_home_object);
    }
    has_arguments_binding = s->has_arguments_binding;
    if (has_arguments_binding) {
        add_arguments_var(ctx, s);
        /* also add an arguments binding in the argument scope to
           raise an error if a direct eval in the argument scope tries
           to redefine it */
        if (s->has_parameter_expressions && !(s->js_mode & JS_MODE_STRICT))
            add_arguments_arg(ctx, s);
    }
    if (s->is_func_expr && s->func_name != JS_ATOM_NULL)
        add_func_var(ctx, s, s->func_name);

    for(i = 0; i < s->arg_count; i++) {
        vd = &s->args[i];
        capture_var(s, vd);
    }
    for(i = 0; i < s->var_count; i++) {
        vd = &s->vars[i];
        /* do not close top level last result */
        if (vd->scope_level == 0 &&
            vd->var_name != JS_ATOM__ret_ &&
            vd->var_name != JS_ATOM_NULL) {
            capture_var(s, vd);
        }
    }
    
    /* eval can use all the variables of the enclosing functions, so
       they must be all put in the closure. The closure variables are
       ordered by scope. It works only because no closure are created
       before. */
    assert(s->is_eval || s->closure_var_count == 0);

    /* XXX: inefficient, but eval performance is less critical */
    fd = s;
    for(;;) {
        scope_level = fd->parent_scope_level;
        fd = fd->parent;
        if (!fd)
            break;
        /* add 'this' if it was not previously added */
        if (!has_this_binding && fd->has_this_binding) {
            if (fd->this_var_idx < 0)
                fd->this_var_idx = add_var_this(ctx, fd);
            if (fd->new_target_var_idx < 0)
                fd->new_target_var_idx = add_var(ctx, fd, JS_ATOM_new_target);
            if (fd->is_derived_class_constructor && fd->this_active_func_var_idx < 0)
                fd->this_active_func_var_idx = add_var(ctx, fd, JS_ATOM_this_active_func);
            if (fd->has_home_object && fd->home_object_var_idx < 0)
                fd->home_object_var_idx = add_var(ctx, fd, JS_ATOM_home_object);
            has_this_binding = TRUE;
        }
        /* add 'arguments' if it was not previously added */
        if (!has_arguments_binding && fd->has_arguments_binding) {
            add_arguments_var(ctx, fd);
            has_arguments_binding = TRUE;
        }
        /* add function name */
        if (fd->is_func_expr && fd->func_name != JS_ATOM_NULL)
            add_func_var(ctx, fd, fd->func_name);

        /* add lexical variables */
        scope_idx = fd->scopes[scope_level].first;
        while (scope_idx >= 0) {
            vd = &fd->vars[scope_idx];
            capture_var(fd, vd);
            get_closure_var(ctx, s, fd, JS_CLOSURE_LOCAL, scope_idx,
                            vd->var_name, vd->is_const, vd->is_lexical, vd->var_kind);
            scope_idx = vd->scope_next;
        }
        is_arg_scope = (scope_idx == ARG_SCOPE_END);
        if (!is_arg_scope) {
            /* add unscoped variables */
            /* XXX: propagate is_const and var_kind too ? */
            for(i = 0; i < fd->arg_count; i++) {
                vd = &fd->args[i];
                if (vd->var_name != JS_ATOM_NULL) {
                    capture_var(fd, vd);
                    get_closure_var(ctx, s, fd,
                                    JS_CLOSURE_ARG, i, vd->var_name, FALSE,
                                    vd->is_lexical, JS_VAR_NORMAL);
                }
            }
            for(i = 0; i < fd->var_count; i++) {
                vd = &fd->vars[i];
                /* do not close top level last result */
                if (vd->scope_level == 0 &&
                    vd->var_name != JS_ATOM__ret_ &&
                    vd->var_name != JS_ATOM_NULL) {
                    capture_var(fd, vd);
                    get_closure_var(ctx, s, fd,
                                    JS_CLOSURE_LOCAL, i, vd->var_name, FALSE,
                                    vd->is_lexical, JS_VAR_NORMAL);
                }
            }
        } else {
            for(i = 0; i < fd->var_count; i++) {
                vd = &fd->vars[i];
                /* do not close top level last result */
                if (vd->scope_level == 0 && is_var_in_arg_scope(vd->var_name, vd->var_kind)) {
                    capture_var(fd, vd);
                    get_closure_var(ctx, s, fd,
                                    JS_CLOSURE_LOCAL, i, vd->var_name, FALSE,
                                    vd->is_lexical, JS_VAR_NORMAL);
                }
            }
        }
        if (fd->is_eval) {
            int idx;
            /* add direct eval variables (we are necessarily at the
               top level). */
            for (idx = 0; idx < fd->closure_var_count; idx++) {
                JSClosureVar *cv = &fd->closure_var[idx];
                /* Global variables are removed but module
                   definitions are kept. */
                if (cv->closure_type != JS_CLOSURE_GLOBAL_REF &&
                    cv->closure_type != JS_CLOSURE_GLOBAL_DECL &&
                    cv->closure_type != JS_CLOSURE_GLOBAL) {
                    get_closure_var(ctx, s, fd,
                                    JS_CLOSURE_REF,
                                    idx, cv->var_name, cv->is_const,
                                    cv->is_lexical, cv->var_kind);
                }
            }
        }
    }
}

static void set_closure_from_var(JSContext *ctx, JSClosureVar *cv,
                                 JSBytecodeVarDef *vd, int var_idx)
{
    cv->closure_type = JS_CLOSURE_LOCAL;
    cv->is_const = vd->is_const;
    cv->is_lexical = vd->is_lexical;
    cv->var_kind = vd->var_kind;
    cv->var_idx = var_idx;
    cv->var_name = JS_DupAtom(ctx, vd->var_name);
}

/* for direct eval compilation: add references to the variables of the
   calling function */
__exception int add_closure_variables(JSContext *ctx, JSFunctionDef *s,
                                             JSFunctionBytecode *b, int scope_idx)
{
    int i, count;
    JSBytecodeVarDef *vd;
    BOOL is_arg_scope;

    count = b->arg_count + b->var_count + b->closure_var_count;
    s->closure_var = NULL;
    s->closure_var_count = 0;
    s->closure_var_size = count;
    if (count == 0)
        return 0;
    s->closure_var = js_malloc(ctx, sizeof(s->closure_var[0]) * count);
    if (!s->closure_var)
        return -1;
    /* Add lexical variables in scope at the point of evaluation */
    for (i = scope_idx; i >= 0;) {
        vd = &b->vardefs[b->arg_count + i];
        if (vd->has_scope) {
            JSClosureVar *cv = &s->closure_var[s->closure_var_count++];
            set_closure_from_var(ctx, cv, vd, i);
        }
        i = vd->scope_next;
    }
    is_arg_scope = (i == ARG_SCOPE_END);
    if (!is_arg_scope) {
        /* Add argument variables */
        for(i = 0; i < b->arg_count; i++) {
            JSClosureVar *cv = &s->closure_var[s->closure_var_count++];
            vd = &b->vardefs[i];
            cv->closure_type = JS_CLOSURE_ARG;
            cv->is_const = FALSE;
            cv->is_lexical = FALSE;
            cv->var_kind = JS_VAR_NORMAL;
            cv->var_idx = i;
            cv->var_name = JS_DupAtom(ctx, vd->var_name);
        }
        /* Add local non lexical variables */
        for(i = 0; i < b->var_count; i++) {
            vd = &b->vardefs[b->arg_count + i];
            if (!vd->has_scope && vd->var_name != JS_ATOM__ret_) {
                JSClosureVar *cv = &s->closure_var[s->closure_var_count++];
                set_closure_from_var(ctx, cv, vd, i);
            }
        }
    } else {
        /* only add pseudo variables */
        for(i = 0; i < b->var_count; i++) {
            vd = &b->vardefs[b->arg_count + i];
            if (!vd->has_scope && is_var_in_arg_scope(vd->var_name, vd->var_kind)) {
                JSClosureVar *cv = &s->closure_var[s->closure_var_count++];
                set_closure_from_var(ctx, cv, vd, i);
            }
        }
    }
    for(i = 0; i < b->closure_var_count; i++) {
        JSClosureVar *cv0 = &b->closure_var[i];
        JSClosureVar *cv;

        switch(cv0->closure_type) {
        case JS_CLOSURE_LOCAL:
        case JS_CLOSURE_ARG:
        case JS_CLOSURE_REF:
        case JS_CLOSURE_MODULE_DECL:
        case JS_CLOSURE_MODULE_IMPORT:
            break;
        case JS_CLOSURE_GLOBAL_REF:
        case JS_CLOSURE_GLOBAL_DECL:
        case JS_CLOSURE_GLOBAL:
            continue; /* not necessary to add global variables */
        default:
            abort();
        }
        cv = &s->closure_var[s->closure_var_count++];
        cv->closure_type = JS_CLOSURE_REF;
        cv->is_const = cv0->is_const;
        cv->is_lexical = cv0->is_lexical;
        cv->var_kind = cv0->var_kind;
        cv->var_idx = i;
        cv->var_name = JS_DupAtom(ctx, cv0->var_name);
    }
    return 0;
}

typedef struct CodeContext {
    const uint8_t *bc_buf; /* code buffer */
    int bc_len;   /* length of the code buffer */
    int pos;      /* position past the matched code pattern */
    int line_num; /* last visited OP_line_num parameter or -1 */
    int op;
    int idx;
    int label;
    int val;
    JSAtom atom;
} CodeContext;

#define M2(op1, op2)            ((op1) | ((op2) << 8))
#define M3(op1, op2, op3)       ((op1) | ((op2) << 8) | ((op3) << 16))
#define M4(op1, op2, op3, op4)  ((op1) | ((op2) << 8) | ((op3) << 16) | ((op4) << 24))

static BOOL code_match(CodeContext *s, int pos, ...)
{
    const uint8_t *tab = s->bc_buf;
    int op, len, op1, line_num, pos_next;
    va_list ap;
    BOOL ret = FALSE;

    line_num = -1;
    va_start(ap, pos);

    for(;;) {
        op1 = va_arg(ap, int);
        if (op1 == -1) {
            s->pos = pos;
            s->line_num = line_num;
            ret = TRUE;
            break;
        }
        for (;;) {
            if (pos >= s->bc_len)
                goto done;
            op = tab[pos];
            len = opcode_info[op].size;
            pos_next = pos + len;
            if (pos_next > s->bc_len)
                goto done;
            if (op == OP_line_num) {
                line_num = get_u32(tab + pos + 1);
                pos = pos_next;
            } else {
                break;
            }
        }
        if (op != op1) {
            if (op1 == (uint8_t)op1 || !op)
                break;
            if (op != (uint8_t)op1
            &&  op != (uint8_t)(op1 >> 8)
            &&  op != (uint8_t)(op1 >> 16)
            &&  op != (uint8_t)(op1 >> 24)) {
                break;
            }
            s->op = op;
        }

        pos++;
        switch(opcode_info[op].fmt) {
        case OP_FMT_loc8:
        case OP_FMT_u8:
            {
                int idx = tab[pos];
                int arg = va_arg(ap, int);
                if (arg == -1) {
                    s->idx = idx;
                } else {
                    if (arg != idx)
                        goto done;
                }
                break;
            }
        case OP_FMT_u16:
        case OP_FMT_npop:
        case OP_FMT_loc:
        case OP_FMT_arg:
        case OP_FMT_var_ref:
            {
                int idx = get_u16(tab + pos);
                int arg = va_arg(ap, int);
                if (arg == -1) {
                    s->idx = idx;
                } else {
                    if (arg != idx)
                        goto done;
                }
                break;
            }
        case OP_FMT_i32:
        case OP_FMT_u32:
        case OP_FMT_label:
        case OP_FMT_const:
            {
                s->label = get_u32(tab + pos);
                break;
            }
        case OP_FMT_label_u16:
            {
                s->label = get_u32(tab + pos);
                s->val = get_u16(tab + pos + 4);
                break;
            }
        case OP_FMT_atom:
            {
                s->atom = get_u32(tab + pos);
                break;
            }
        case OP_FMT_atom_u8:
            {
                s->atom = get_u32(tab + pos);
                s->val = get_u8(tab + pos + 4);
                break;
            }
        case OP_FMT_atom_u16:
            {
                s->atom = get_u32(tab + pos);
                s->val = get_u16(tab + pos + 4);
                break;
            }
        case OP_FMT_atom_label_u8:
            {
                s->atom = get_u32(tab + pos);
                s->label = get_u32(tab + pos + 4);
                s->val = get_u8(tab + pos + 8);
                break;
            }
        default:
            break;
        }
        pos = pos_next;
    }
 done:
    va_end(ap);
    return ret;
}

static void instantiate_hoisted_definitions(JSContext *ctx, JSFunctionDef *s, DynBuf *bc)
{
    int i, idx, label_next = -1;

    /* add the hoisted functions in arguments and local variables */
    for(i = 0; i < s->arg_count; i++) {
        JSVarDef *vd = &s->args[i];
        if (vd->func_pool_idx >= 0) {
            dbuf_putc(bc, OP_fclosure);
            dbuf_put_u32(bc, vd->func_pool_idx);
            dbuf_putc(bc, OP_put_arg);
            dbuf_put_u16(bc, i);
        }
    }
    for(i = 0; i < s->var_count; i++) {
        JSVarDef *vd = &s->vars[i];
        if (vd->scope_level == 0 && vd->func_pool_idx >= 0) {
            dbuf_putc(bc, OP_fclosure);
            dbuf_put_u32(bc, vd->func_pool_idx);
            dbuf_putc(bc, OP_put_loc);
            dbuf_put_u16(bc, i);
        }
    }

    /* the module global variables must be initialized before
       evaluating the module so that the exported functions are
       visible if there are cyclic module references */
    if (s->module) {
        label_next = new_label_fd(s);
        if (label_next < 0) {
            dbuf_set_error(bc);
            return;
        }
        /* if 'this' is true, initialize the global variables and return */
        dbuf_putc(bc, OP_push_this);
        dbuf_putc(bc, OP_if_false);
        dbuf_put_u32(bc, label_next);
        update_label(s, label_next, 1);
        s->jump_size++;
    }

    /* add the global variables (only happens if s->is_global_var is
       true) */
    /* XXX: inefficient, add a closure index in JSGlobalVar */
    for(i = 0; i < s->global_var_count; i++) {
        JSGlobalVar *hf = &s->global_vars[i];
        BOOL has_var_obj = FALSE;
        BOOL force_init = hf->force_init;
        /* we are in an eval, so the closure contains all the
           enclosing variables */
        /* If the outer function has a variable environment,
           create a property for the variable there */
        for(idx = 0; idx < s->closure_var_count; idx++) {
            JSClosureVar *cv = &s->closure_var[idx];
            if (cv->var_name == hf->var_name) {
                force_init = FALSE;
                goto closure_found;
            }
            if (cv->var_name == JS_ATOM__var_ ||
                cv->var_name == JS_ATOM__arg_var_) {
                dbuf_putc(bc, OP_get_var_ref);
                dbuf_put_u16(bc, idx);
                has_var_obj = TRUE;
                force_init = TRUE;
                goto closure_found;
            }
        }
        abort();
    closure_found:
        if (hf->cpool_idx >= 0 || force_init) {
            if (hf->cpool_idx >= 0) {
                dbuf_putc(bc, OP_fclosure);
                dbuf_put_u32(bc, hf->cpool_idx);
                if (hf->var_name == JS_ATOM__default_) {
                    /* set default export function name */
                    dbuf_putc(bc, OP_set_name);
                    dbuf_put_u32(bc, JS_DupAtom(ctx, JS_ATOM_default));
                }
            } else {
                dbuf_putc(bc, OP_undefined);
            }
            if (!has_var_obj) {
                dbuf_putc(bc, OP_put_var_ref);
                dbuf_put_u16(bc, idx);
            } else {
                dbuf_putc(bc, OP_define_field);
                dbuf_put_u32(bc, JS_DupAtom(ctx, hf->var_name));
                dbuf_putc(bc, OP_drop);
            }
        }
        JS_FreeAtom(ctx, hf->var_name);
    }

    if (s->module) {
        dbuf_putc(bc, OP_return_undef);

        dbuf_putc(bc, OP_label);
        dbuf_put_u32(bc, label_next);
        s->label_slots[label_next].pos2 = bc->size;
    }

    js_free(ctx, s->global_vars);
    s->global_vars = NULL;
    s->global_var_count = 0;
    s->global_var_size = 0;
}

static int skip_dead_code(JSFunctionDef *s, const uint8_t *bc_buf, int bc_len,
                          int pos, int *linep)
{
    int op, len, label;

    for (; pos < bc_len; pos += len) {
        op = bc_buf[pos];
        len = opcode_info[op].size;
        if (op == OP_line_num) {
            *linep = get_u32(bc_buf + pos + 1);
        } else
        if (op == OP_label) {
            label = get_u32(bc_buf + pos + 1);
            if (update_label(s, label, 0) > 0)
                break;
#if 0
            if (s->label_slots[label].first_reloc) {
                printf("line %d: unreferenced label %d:%d has relocations\n",
                       *linep, label, s->label_slots[label].pos2);
            }
#endif
            assert(s->label_slots[label].first_reloc == NULL);
        } else {
            /* XXX: output a warning for unreachable code? */
            JSAtom atom;
            switch(opcode_info[op].fmt) {
            case OP_FMT_label:
            case OP_FMT_label_u16:
                label = get_u32(bc_buf + pos + 1);
                update_label(s, label, -1);
                break;
            case OP_FMT_atom_label_u8:
            case OP_FMT_atom_label_u16:
                label = get_u32(bc_buf + pos + 5);
                update_label(s, label, -1);
                /* fall thru */
            case OP_FMT_atom:
            case OP_FMT_atom_u8:
            case OP_FMT_atom_u16:
                atom = get_u32(bc_buf + pos + 1);
                JS_FreeAtom(s->ctx, atom);
                break;
            default:
                break;
            }
        }
    }
    return pos;
}

static int get_label_pos(JSFunctionDef *s, int label)
{
    int i, pos;
    for (i = 0; i < 20; i++) {
        pos = s->label_slots[label].pos;
        for (;;) {
            switch (s->byte_code.buf[pos]) {
            case OP_line_num:
            case OP_label:
                pos += 5;
                continue;
            case OP_goto:
                label = get_u32(s->byte_code.buf + pos + 1);
                break;
            default:
                return pos;
            }
            break;
        }
    }
    return pos;
}

/* convert global variable accesses to local variables or closure
   variables when necessary */
static __exception int resolve_variables(JSContext *ctx, JSFunctionDef *s)
{
    int pos, pos_next, bc_len, op, len, line_num, i, idx;
    uint8_t *bc_buf;
    JSAtom var_name;
    DynBuf bc_out;
    CodeContext cc;
    int scope;

    cc.bc_buf = bc_buf = s->byte_code.buf;
    cc.bc_len = bc_len = s->byte_code.size;
    js_dbuf_bytecode_init(ctx, &bc_out);

    /* first pass for runtime checks (must be done before the
       variables are created) */
    /* XXX: inefficient */
    for(i = 0; i < s->global_var_count; i++) {
        JSGlobalVar *hf = &s->global_vars[i];

        /* check if global variable (XXX: simplify) */
        for(idx = 0; idx < s->closure_var_count; idx++) {
            JSClosureVar *cv = &s->closure_var[idx];
            if (cv->closure_type == JS_CLOSURE_GLOBAL_REF ||
                cv->closure_type == JS_CLOSURE_GLOBAL_DECL ||
                cv->closure_type == JS_CLOSURE_GLOBAL ||
                cv->closure_type == JS_CLOSURE_MODULE_DECL ||
                cv->closure_type == JS_CLOSURE_MODULE_IMPORT)
                goto next; /* don't look at global variables (they are at the end) */
            if (cv->var_name == hf->var_name) {
                if (s->eval_type == JS_EVAL_TYPE_DIRECT &&
                    cv->is_lexical) {
                    /* Check if a lexical variable is
                       redefined as 'var'. XXX: Could abort
                       compilation here, but for consistency
                       with the other checks, we delay the
                       error generation. */
                    dbuf_putc(&bc_out, OP_throw_error);
                    dbuf_put_u32(&bc_out, JS_DupAtom(ctx, hf->var_name));
                    dbuf_putc(&bc_out, JS_THROW_VAR_REDECL);
                }
                goto next;
            }
            if (cv->var_name == JS_ATOM__var_ ||
                cv->var_name == JS_ATOM__arg_var_)
                goto next;
        }
    next: ;
    }

    line_num = 0; /* avoid warning */
    for (pos = 0; pos < bc_len; pos = pos_next) {
        op = bc_buf[pos];
        len = opcode_info[op].size;
        pos_next = pos + len;
        switch(op) {
        case OP_line_num:
            line_num = get_u32(bc_buf + pos + 1);
            s->line_number_size++;
            goto no_change;

        case OP_eval: /* convert scope index to adjusted variable index */
            {
                int call_argc = get_u16(bc_buf + pos + 1);
                scope = get_u16(bc_buf + pos + 1 + 2);
                mark_eval_captured_variables(ctx, s, scope);
                dbuf_putc(&bc_out, op);
                dbuf_put_u16(&bc_out, call_argc);
                dbuf_put_u16(&bc_out, s->scopes[scope].first - ARG_SCOPE_END);
            }
            break;
        case OP_apply_eval: /* convert scope index to adjusted variable index */
            scope = get_u16(bc_buf + pos + 1);
            mark_eval_captured_variables(ctx, s, scope);
            dbuf_putc(&bc_out, op);
            dbuf_put_u16(&bc_out, s->scopes[scope].first - ARG_SCOPE_END);
            break;
        case OP_scope_get_var_checkthis:
        case OP_scope_get_var_undef:
        case OP_scope_get_var:
        case OP_scope_put_var:
        case OP_scope_delete_var:
        case OP_scope_get_ref:
        case OP_scope_put_var_init:
            var_name = get_u32(bc_buf + pos + 1);
            scope = get_u16(bc_buf + pos + 5);
            pos_next = resolve_scope_var(ctx, s, var_name, scope, op, &bc_out,
                                         NULL, NULL, pos_next);
            JS_FreeAtom(ctx, var_name);
            break;
        case OP_scope_make_ref:
            {
                int label;
                LabelSlot *ls;
                var_name = get_u32(bc_buf + pos + 1);
                label = get_u32(bc_buf + pos + 5);
                scope = get_u16(bc_buf + pos + 9);
                ls = &s->label_slots[label];
                ls->ref_count--;  /* always remove label reference */
                pos_next = resolve_scope_var(ctx, s, var_name, scope, op, &bc_out,
                                             bc_buf, ls, pos_next);
                JS_FreeAtom(ctx, var_name);
            }
            break;
        case OP_scope_get_private_field:
        case OP_scope_get_private_field2:
        case OP_scope_put_private_field:
        case OP_scope_in_private_field:
            {
                int ret;
                var_name = get_u32(bc_buf + pos + 1);
                scope = get_u16(bc_buf + pos + 5);
                ret = resolve_scope_private_field(ctx, s, var_name, scope, op, &bc_out);
                if (ret < 0)
                    goto fail;
                JS_FreeAtom(ctx, var_name);
            }
            break;
        case OP_gosub:
            s->jump_size++;
            if (OPTIMIZE) {
                /* remove calls to empty finalizers  */
                int label;
                LabelSlot *ls;

                label = get_u32(bc_buf + pos + 1);
                assert(label >= 0 && label < s->label_count);
                ls = &s->label_slots[label];
                if (code_match(&cc, ls->pos, OP_ret, -1)) {
                    ls->ref_count--;
                    break;
                }
            }
            goto no_change;
        case OP_drop:
            if (0) {
                /* remove drops before return_undef */
                /* do not perform this optimization in pass2 because
                   it breaks patterns recognised in resolve_labels */
                int pos1 = pos_next;
                int line1 = line_num;
                while (code_match(&cc, pos1, OP_drop, -1)) {
                    if (cc.line_num >= 0) line1 = cc.line_num;
                    pos1 = cc.pos;
                }
                if (code_match(&cc, pos1, OP_return_undef, -1)) {
                    pos_next = pos1;
                    if (line1 != -1 && line1 != line_num) {
                        line_num = line1;
                        s->line_number_size++;
                        dbuf_putc(&bc_out, OP_line_num);
                        dbuf_put_u32(&bc_out, line_num);
                    }
                    break;
                }
            }
            goto no_change;
        case OP_insert3:
            if (OPTIMIZE) {
                /* Transformation: insert3 put_array_el|put_ref_value drop -> put_array_el|put_ref_value */
                if (code_match(&cc, pos_next, M2(OP_put_array_el, OP_put_ref_value), OP_drop, -1)) {
                    dbuf_putc(&bc_out, cc.op);
                    pos_next = cc.pos;
                    if (cc.line_num != -1 && cc.line_num != line_num) {
                        line_num = cc.line_num;
                        s->line_number_size++;
                        dbuf_putc(&bc_out, OP_line_num);
                        dbuf_put_u32(&bc_out, line_num);
                    }
                    break;
                }
            }
            goto no_change;

        case OP_goto:
            s->jump_size++;
            /* fall thru */
        case OP_tail_call:
        case OP_tail_call_method:
        case OP_return:
        case OP_return_undef:
        case OP_throw:
        case OP_throw_error:
        case OP_ret:
            if (OPTIMIZE) {
                /* remove dead code */
                int line = -1;
                dbuf_put(&bc_out, bc_buf + pos, len);
                pos = skip_dead_code(s, bc_buf, bc_len, pos + len, &line);
                pos_next = pos;
                if (pos < bc_len && line >= 0 && line_num != line) {
                    line_num = line;
                    s->line_number_size++;
                    dbuf_putc(&bc_out, OP_line_num);
                    dbuf_put_u32(&bc_out, line_num);
                }
                break;
            }
            goto no_change;

        case OP_label:
            {
                int label;
                LabelSlot *ls;

                label = get_u32(bc_buf + pos + 1);
                assert(label >= 0 && label < s->label_count);
                ls = &s->label_slots[label];
                ls->pos2 = bc_out.size + opcode_info[op].size;
            }
            goto no_change;

        case OP_enter_scope:
            {
                int scope_idx, scope = get_u16(bc_buf + pos + 1);

                if (scope == s->body_scope) {
                    instantiate_hoisted_definitions(ctx, s, &bc_out);
                }

                for(scope_idx = s->scopes[scope].first; scope_idx >= 0;) {
                    JSVarDef *vd = &s->vars[scope_idx];
                    if (vd->scope_level == scope) {
                        if (scope_idx != s->arguments_arg_idx) {
                            if (vd->var_kind == JS_VAR_FUNCTION_DECL ||
                                vd->var_kind == JS_VAR_NEW_FUNCTION_DECL) {
                                /* Initialize lexical variable upon entering scope */
                                dbuf_putc(&bc_out, OP_fclosure);
                                dbuf_put_u32(&bc_out, vd->func_pool_idx);
                                dbuf_putc(&bc_out, OP_put_loc);
                                dbuf_put_u16(&bc_out, scope_idx);
                            } else {
                                /* XXX: should check if variable can be used
                                   before initialization */
                                dbuf_putc(&bc_out, OP_set_loc_uninitialized);
                                dbuf_put_u16(&bc_out, scope_idx);
                            }
                        }
                        scope_idx = vd->scope_next;
                    } else {
                        break;
                    }
                }
            }
            break;

        case OP_leave_scope:
            {
                int scope_idx, scope = get_u16(bc_buf + pos + 1);

                for(scope_idx = s->scopes[scope].first; scope_idx >= 0;) {
                    JSVarDef *vd = &s->vars[scope_idx];
                    if (vd->scope_level == scope) {
                        if (vd->is_captured) {
                            dbuf_putc(&bc_out, OP_close_loc);
                            dbuf_put_u16(&bc_out, scope_idx);
                        }
                        scope_idx = vd->scope_next;
                    } else {
                        break;
                    }
                }
            }
            break;

        case OP_set_name:
            {
                /* remove dummy set_name opcodes */
                JSAtom name = get_u32(bc_buf + pos + 1);
                if (name == JS_ATOM_NULL)
                    break;
            }
            goto no_change;

        case OP_if_false:
        case OP_if_true:
        case OP_catch:
            s->jump_size++;
            goto no_change;

        case OP_dup:
            if (OPTIMIZE) {
                /* Transformation: dup if_false(l1) drop, l1: if_false(l2) -> if_false(l2) */
                /* Transformation: dup if_true(l1) drop, l1: if_true(l2) -> if_true(l2) */
                if (code_match(&cc, pos_next, M2(OP_if_false, OP_if_true), OP_drop, -1)) {
                    int lab0, lab1, op1, pos1, line1, pos2;
                    lab0 = lab1 = cc.label;
                    assert(lab1 >= 0 && lab1 < s->label_count);
                    op1 = cc.op;
                    pos1 = cc.pos;
                    line1 = cc.line_num;
                    while (code_match(&cc, (pos2 = get_label_pos(s, lab1)), OP_dup, op1, OP_drop, -1)) {
                        lab1 = cc.label;
                    }
                    if (code_match(&cc, pos2, op1, -1)) {
                        s->jump_size++;
                        update_label(s, lab0, -1);
                        update_label(s, cc.label, +1);
                        dbuf_putc(&bc_out, op1);
                        dbuf_put_u32(&bc_out, cc.label);
                        pos_next = pos1;
                        if (line1 != -1 && line1 != line_num) {
                            line_num = line1;
                            s->line_number_size++;
                            dbuf_putc(&bc_out, OP_line_num);
                            dbuf_put_u32(&bc_out, line_num);
                        }
                        break;
                    }
                }
            }
            goto no_change;

        case OP_nop:
            /* remove erased code */
            break;
        case OP_set_class_name:
            /* only used during parsing */
            break;

        case OP_get_field_opt_chain: /* equivalent to OP_get_field */
            {
                JSAtom name = get_u32(bc_buf + pos + 1);
                dbuf_putc(&bc_out, OP_get_field);
                dbuf_put_u32(&bc_out, name);
            }
            break;
        case OP_get_array_el_opt_chain: /* equivalent to OP_get_array_el */
            dbuf_putc(&bc_out, OP_get_array_el);
            break;

        default:
        no_change:
            dbuf_put(&bc_out, bc_buf + pos, len);
            break;
        }
    }

    /* set the new byte code */
    dbuf_free(&s->byte_code);
    s->byte_code = bc_out;
    if (dbuf_error(&s->byte_code)) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    return 0;
 fail:
    /* continue the copy to keep the atom refcounts consistent */
    /* XXX: find a better solution ? */
    for (; pos < bc_len; pos = pos_next) {
        op = bc_buf[pos];
        len = opcode_info[op].size;
        pos_next = pos + len;
        dbuf_put(&bc_out, bc_buf + pos, len);
    }
    dbuf_free(&s->byte_code);
    s->byte_code = bc_out;
    return -1;
}

/* the pc2line table gives a source position for each PC value */
static void add_pc2line_info(JSFunctionDef *s, uint32_t pc, uint32_t source_pos)
{
    if (s->line_number_slots != NULL
    &&  s->line_number_count < s->line_number_size
    &&  pc >= s->line_number_last_pc
    &&  source_pos != s->line_number_last) {
        s->line_number_slots[s->line_number_count].pc = pc;
        s->line_number_slots[s->line_number_count].source_pos = source_pos;
        s->line_number_count++;
        s->line_number_last_pc = pc;
        s->line_number_last = source_pos;
    }
}

/* XXX: could use a more compact storage */
/* XXX: get_line_col_cached() is slow. For more predictable
   performance, line/cols could be stored every N source
   bytes. Alternatively, get_line_col_cached() could be issued in
   emit_source_pos() so that the deltas are more likely to be
   small. */
static void compute_pc2line_info(JSFunctionDef *s)
{
    if (!s->strip_debug) {
        int last_line_num, last_col_num;
        uint32_t last_pc = 0;
        int i, line_num, col_num;
        const uint8_t *buf_start = s->get_line_col_cache->buf_start;
        js_dbuf_init(s->ctx, &s->pc2line);

        last_line_num = get_line_col_cached(s->get_line_col_cache,
                                            &last_col_num,
                                            buf_start + s->source_pos);
        dbuf_put_leb128(&s->pc2line, last_line_num); /* line number minus 1 */
        dbuf_put_leb128(&s->pc2line, last_col_num); /* column number minus 1 */

        for (i = 0; i < s->line_number_count; i++) {
            uint32_t pc = s->line_number_slots[i].pc;
            uint32_t source_pos = s->line_number_slots[i].source_pos;
            int diff_pc, diff_line, diff_col;

            if (source_pos == -1)
                continue;
            diff_pc = pc - last_pc;
            if (diff_pc < 0)
                continue;

            line_num = get_line_col_cached(s->get_line_col_cache, &col_num,
                                           buf_start + source_pos);
            diff_line = line_num - last_line_num;
            diff_col = col_num - last_col_num;
            if (diff_line == 0 && diff_col == 0)
                continue;

            if (diff_line >= PC2LINE_BASE &&
                diff_line < PC2LINE_BASE + PC2LINE_RANGE &&
                diff_pc <= PC2LINE_DIFF_PC_MAX) {
                dbuf_putc(&s->pc2line, (diff_line - PC2LINE_BASE) +
                          diff_pc * PC2LINE_RANGE + PC2LINE_OP_FIRST);
            } else {
                /* longer encoding */
                dbuf_putc(&s->pc2line, 0);
                dbuf_put_leb128(&s->pc2line, diff_pc);
                dbuf_put_sleb128(&s->pc2line, diff_line);
            }
            dbuf_put_sleb128(&s->pc2line, diff_col);
                
            last_pc = pc;
            last_line_num = line_num;
            last_col_num = col_num;
        }
    }
}

static RelocEntry *add_reloc(JSContext *ctx, LabelSlot *ls, uint32_t addr, int size)
{
    RelocEntry *re;
    re = js_malloc(ctx, sizeof(*re));
    if (!re)
        return NULL;
    re->addr = addr;
    re->size = size;
    re->next = ls->first_reloc;
    ls->first_reloc = re;
    return re;
}

static BOOL code_has_label(CodeContext *s, int pos, int label)
{
    while (pos < s->bc_len) {
        int op = s->bc_buf[pos];
        if (op == OP_line_num) {
            pos += 5;
            continue;
        }
        if (op == OP_label) {
            int lab = get_u32(s->bc_buf + pos + 1);
            if (lab == label)
                return TRUE;
            pos += 5;
            continue;
        }
        if (op == OP_goto) {
            int lab = get_u32(s->bc_buf + pos + 1);
            if (lab == label)
                return TRUE;
        }
        break;
    }
    return FALSE;
}

/* return the target label, following the OP_goto jumps
   the first opcode at destination is stored in *pop
 */
static int find_jump_target(JSFunctionDef *s, int label0, int *pop, int *pline)
{
    int i, pos, op, label;

    label = label0;
    update_label(s, label, -1);
    for (i = 0; i < 10; i++) {
        assert(label >= 0 && label < s->label_count);
        pos = s->label_slots[label].pos2;
        for (;;) {
            switch(op = s->byte_code.buf[pos]) {
            case OP_line_num:
                if (pline)
                    *pline = get_u32(s->byte_code.buf + pos + 1);
                /* fall thru */
            case OP_label:
                pos += opcode_info[op].size;
                continue;
            case OP_goto:
                label = get_u32(s->byte_code.buf + pos + 1);
                break;
            case OP_drop:
                /* ignore drop opcodes if followed by OP_return_undef */
                while (s->byte_code.buf[++pos] == OP_drop)
                    continue;
                if (s->byte_code.buf[pos] == OP_return_undef)
                    op = OP_return_undef;
                /* fall thru */
            default:
                goto done;
            }
            break;
        }
    }
    /* cycle detected, could issue a warning */
    /* XXX: the combination of find_jump_target() and skip_dead_code()
       seems incorrect with cyclic labels. See for exemple:

       for (;;) {
       l:break l;
       l:break l;
       l:break l;
       l:break l;
       }

       Avoiding changing the target is just a workaround and might not
       suffice to completely fix the problem. */
    label = label0;
 done:
    *pop = op;
    update_label(s, label, +1);
    return label;
}

static void push_short_int(DynBuf *bc_out, int val)
{
#if SHORT_OPCODES
    if (val >= -1 && val <= 7) {
        dbuf_putc(bc_out, OP_push_0 + val);
        return;
    }
    if (val == (int8_t)val) {
        dbuf_putc(bc_out, OP_push_i8);
        dbuf_putc(bc_out, val);
        return;
    }
    if (val == (int16_t)val) {
        dbuf_putc(bc_out, OP_push_i16);
        dbuf_put_u16(bc_out, val);
        return;
    }
#endif
    dbuf_putc(bc_out, OP_push_i32);
    dbuf_put_u32(bc_out, val);
}

static void put_short_code(DynBuf *bc_out, int op, int idx)
{
#if SHORT_OPCODES
    if (idx < 4) {
        switch (op) {
        case OP_get_loc:
            dbuf_putc(bc_out, OP_get_loc0 + idx);
            return;
        case OP_put_loc:
            dbuf_putc(bc_out, OP_put_loc0 + idx);
            return;
        case OP_set_loc:
            dbuf_putc(bc_out, OP_set_loc0 + idx);
            return;
        case OP_get_arg:
            dbuf_putc(bc_out, OP_get_arg0 + idx);
            return;
        case OP_put_arg:
            dbuf_putc(bc_out, OP_put_arg0 + idx);
            return;
        case OP_set_arg:
            dbuf_putc(bc_out, OP_set_arg0 + idx);
            return;
        case OP_get_var_ref:
            dbuf_putc(bc_out, OP_get_var_ref0 + idx);
            return;
        case OP_put_var_ref:
            dbuf_putc(bc_out, OP_put_var_ref0 + idx);
            return;
        case OP_set_var_ref:
            dbuf_putc(bc_out, OP_set_var_ref0 + idx);
            return;
        case OP_call:
            dbuf_putc(bc_out, OP_call0 + idx);
            return;
        }
    }
    if (idx < 256) {
        switch (op) {
        case OP_get_loc:
            dbuf_putc(bc_out, OP_get_loc8);
            dbuf_putc(bc_out, idx);
            return;
        case OP_put_loc:
            dbuf_putc(bc_out, OP_put_loc8);
            dbuf_putc(bc_out, idx);
            return;
        case OP_set_loc:
            dbuf_putc(bc_out, OP_set_loc8);
            dbuf_putc(bc_out, idx);
            return;
        }
    }
#endif
    dbuf_putc(bc_out, op);
    dbuf_put_u16(bc_out, idx);
}

/* peephole optimizations and resolve goto/labels */
static __exception int resolve_labels(JSContext *ctx, JSFunctionDef *s)
{
    int pos, pos_next, bc_len, op, op1, len, i, line_num;
    const uint8_t *bc_buf;
    DynBuf bc_out;
    LabelSlot *label_slots, *ls;
    RelocEntry *re, *re_next;
    CodeContext cc;
    int label;
#if SHORT_OPCODES
    JumpSlot *jp;
#endif

    label_slots = s->label_slots;

    line_num = s->source_pos;

    cc.bc_buf = bc_buf = s->byte_code.buf;
    cc.bc_len = bc_len = s->byte_code.size;
    js_dbuf_bytecode_init(ctx, &bc_out);

#if SHORT_OPCODES
    if (s->jump_size) {
        s->jump_slots = js_mallocz(s->ctx, sizeof(*s->jump_slots) * s->jump_size);
        if (s->jump_slots == NULL)
            return -1;
    }
#endif
    /* XXX: Should skip this phase if not generating SHORT_OPCODES */
    if (s->line_number_size && !s->strip_debug) {
        s->line_number_slots = js_mallocz(s->ctx, sizeof(*s->line_number_slots) * s->line_number_size);
        if (s->line_number_slots == NULL)
            return -1;
        s->line_number_last = s->source_pos;
        s->line_number_last_pc = 0;
    }

    /* initialize the 'home_object' variable if needed */
    if (s->home_object_var_idx >= 0) {
        dbuf_putc(&bc_out, OP_special_object);
        dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_HOME_OBJECT);
        put_short_code(&bc_out, OP_put_loc, s->home_object_var_idx);
    }
    /* initialize the 'this.active_func' variable if needed */
    if (s->this_active_func_var_idx >= 0) {
        dbuf_putc(&bc_out, OP_special_object);
        dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_THIS_FUNC);
        put_short_code(&bc_out, OP_put_loc, s->this_active_func_var_idx);
    }
    /* initialize the 'new.target' variable if needed */
    if (s->new_target_var_idx >= 0) {
        dbuf_putc(&bc_out, OP_special_object);
        dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_NEW_TARGET);
        put_short_code(&bc_out, OP_put_loc, s->new_target_var_idx);
    }
    /* initialize the 'this' variable if needed. In a derived class
       constructor, this is initially uninitialized. */
    if (s->this_var_idx >= 0) {
        if (s->is_derived_class_constructor) {
            dbuf_putc(&bc_out, OP_set_loc_uninitialized);
            dbuf_put_u16(&bc_out, s->this_var_idx);
        } else {
            dbuf_putc(&bc_out, OP_push_this);
            put_short_code(&bc_out, OP_put_loc, s->this_var_idx);
        }
    }
    /* initialize the 'arguments' variable if needed */
    if (s->arguments_var_idx >= 0) {
        if ((s->js_mode & JS_MODE_STRICT) || !s->has_simple_parameter_list) {
            dbuf_putc(&bc_out, OP_special_object);
            dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_ARGUMENTS);
        } else {
            dbuf_putc(&bc_out, OP_special_object);
            dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_MAPPED_ARGUMENTS);
            /* the arguments are implicitly captured because
               references to them are created with the 'argument'
               object */
            for(i = 0; i < s->arg_count; i++)
                capture_var(s, &s->args[i]);
        }
        if (s->arguments_arg_idx >= 0)
            put_short_code(&bc_out, OP_set_loc, s->arguments_arg_idx);
        put_short_code(&bc_out, OP_put_loc, s->arguments_var_idx);
    }
    /* initialize a reference to the current function if needed */
    if (s->func_var_idx >= 0) {
        dbuf_putc(&bc_out, OP_special_object);
        dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_THIS_FUNC);
        put_short_code(&bc_out, OP_put_loc, s->func_var_idx);
    }
    /* initialize the variable environment object if needed */
    if (s->var_object_idx >= 0) {
        dbuf_putc(&bc_out, OP_special_object);
        dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_VAR_OBJECT);
        put_short_code(&bc_out, OP_put_loc, s->var_object_idx);
    }
    if (s->arg_var_object_idx >= 0) {
        dbuf_putc(&bc_out, OP_special_object);
        dbuf_putc(&bc_out, OP_SPECIAL_OBJECT_VAR_OBJECT);
        put_short_code(&bc_out, OP_put_loc, s->arg_var_object_idx);
    }

    for (pos = 0; pos < bc_len; pos = pos_next) {
        int val;
        op = bc_buf[pos];
        len = opcode_info[op].size;
        pos_next = pos + len;
        switch(op) {
        case OP_line_num:
            /* line number info (for debug). We put it in a separate
               compressed table to reduce memory usage and get better
               performance */
            line_num = get_u32(bc_buf + pos + 1);
            break;

        case OP_label:
            {
                label = get_u32(bc_buf + pos + 1);
                assert(label >= 0 && label < s->label_count);
                ls = &label_slots[label];
                assert(ls->addr == -1);
                ls->addr = bc_out.size;
                /* resolve the relocation entries */
                for(re = ls->first_reloc; re != NULL; re = re_next) {
                    int diff = ls->addr - re->addr;
                    re_next = re->next;
                    switch (re->size) {
                    case 4:
                        put_u32(bc_out.buf + re->addr, diff);
                        break;
                    case 2:
                        assert(diff == (int16_t)diff);
                        put_u16(bc_out.buf + re->addr, diff);
                        break;
                    case 1:
                        assert(diff == (int8_t)diff);
                        put_u8(bc_out.buf + re->addr, diff);
                        break;
                    }
                    js_free(ctx, re);
                }
                ls->first_reloc = NULL;
            }
            break;

        case OP_call:
        case OP_call_method:
            {
                /* detect and transform tail calls */
                int argc;
                argc = get_u16(bc_buf + pos + 1);
                if (code_match(&cc, pos_next, OP_return, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    put_short_code(&bc_out, op + 1, argc);
                    pos_next = skip_dead_code(s, bc_buf, bc_len, cc.pos, &line_num);
                    break;
                }
                add_pc2line_info(s, bc_out.size, line_num);
                put_short_code(&bc_out, op, argc);
                break;
            }
            goto no_change;

        case OP_return:
        case OP_return_undef:
        case OP_return_async:
        case OP_throw:
        case OP_throw_error:
            pos_next = skip_dead_code(s, bc_buf, bc_len, pos_next, &line_num);
            goto no_change;

        case OP_goto:
            label = get_u32(bc_buf + pos + 1);
        has_goto:
            if (OPTIMIZE) {
                int line1 = -1;
                /* Use custom matcher because multiple labels can follow */
                label = find_jump_target(s, label, &op1, &line1);
                if (code_has_label(&cc, pos_next, label)) {
                    /* jump to next instruction: remove jump */
                    update_label(s, label, -1);
                    break;
                }
                if (op1 == OP_return || op1 == OP_return_undef || op1 == OP_throw) {
                    /* jump to return/throw: remove jump, append return/throw */
                    /* updating the line number obfuscates assembly listing */
                    //if (line1 != -1) line_num = line1;
                    update_label(s, label, -1);
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, op1);
                    pos_next = skip_dead_code(s, bc_buf, bc_len, pos_next, &line_num);
                    break;
                }
                /* XXX: should duplicate single instructions followed by goto or return */
                /* For example, can match one of these followed by return:
                   push_i32 / push_const / push_atom_value / get_var /
                   undefined / null / push_false / push_true / get_ref_value /
                   get_loc / get_arg / get_var_ref
                 */
            }
            goto has_label;

        case OP_gosub:
            label = get_u32(bc_buf + pos + 1);
            if (0 && OPTIMIZE) {
                label = find_jump_target(s, label, &op1, NULL);
                if (op1 == OP_ret) {
                    update_label(s, label, -1);
                    /* empty finally clause: remove gosub */
                    break;
                }
            }
            goto has_label;

        case OP_catch:
            label = get_u32(bc_buf + pos + 1);
            goto has_label;

        case OP_if_true:
        case OP_if_false:
            label = get_u32(bc_buf + pos + 1);
            if (OPTIMIZE) {
                label = find_jump_target(s, label, &op1, NULL);
                /* transform if_false/if_true(l1) label(l1) -> drop label(l1) */
                if (code_has_label(&cc, pos_next, label)) {
                    update_label(s, label, -1);
                    dbuf_putc(&bc_out, OP_drop);
                    break;
                }
                /* transform if_false(l1) goto(l2) label(l1) -> if_false(l2) label(l1) */
                if (code_match(&cc, pos_next, OP_goto, -1)) {
                    int pos1 = cc.pos;
                    int line1 = cc.line_num;
                    if (code_has_label(&cc, pos1, label)) {
                        if (line1 != -1) line_num = line1;
                        pos_next = pos1;
                        update_label(s, label, -1);
                        label = cc.label;
                        op ^= OP_if_true ^ OP_if_false;
                    }
                }
            }
        has_label:
            add_pc2line_info(s, bc_out.size, line_num);
            if (op == OP_goto) {
                pos_next = skip_dead_code(s, bc_buf, bc_len, pos_next, &line_num);
            }
            assert(label >= 0 && label < s->label_count);
            ls = &label_slots[label];
#if SHORT_OPCODES
            jp = &s->jump_slots[s->jump_count++];
            jp->op = op;
            jp->size = 4;
            jp->pos = bc_out.size + 1;
            jp->label = label;

            if (ls->addr == -1) {
                int diff = ls->pos2 - pos - 1;
                if (diff < 128 && (op == OP_if_false || op == OP_if_true || op == OP_goto)) {
                    jp->size = 1;
                    jp->op = OP_if_false8 + (op - OP_if_false);
                    dbuf_putc(&bc_out, OP_if_false8 + (op - OP_if_false));
                    dbuf_putc(&bc_out, 0);
                    if (!add_reloc(ctx, ls, bc_out.size - 1, 1))
                        goto fail;
                    break;
                }
                if (diff < 32768 && op == OP_goto) {
                    jp->size = 2;
                    jp->op = OP_goto16;
                    dbuf_putc(&bc_out, OP_goto16);
                    dbuf_put_u16(&bc_out, 0);
                    if (!add_reloc(ctx, ls, bc_out.size - 2, 2))
                        goto fail;
                    break;
                }
            } else {
                int diff = ls->addr - bc_out.size - 1;
                if (diff == (int8_t)diff && (op == OP_if_false || op == OP_if_true || op == OP_goto)) {
                    jp->size = 1;
                    jp->op = OP_if_false8 + (op - OP_if_false);
                    dbuf_putc(&bc_out, OP_if_false8 + (op - OP_if_false));
                    dbuf_putc(&bc_out, diff);
                    break;
                }
                if (diff == (int16_t)diff && op == OP_goto) {
                    jp->size = 2;
                    jp->op = OP_goto16;
                    dbuf_putc(&bc_out, OP_goto16);
                    dbuf_put_u16(&bc_out, diff);
                    break;
                }
            }
#endif
            dbuf_putc(&bc_out, op);
            dbuf_put_u32(&bc_out, ls->addr - bc_out.size);
            if (ls->addr == -1) {
                /* unresolved yet: create a new relocation entry */
                if (!add_reloc(ctx, ls, bc_out.size - 4, 4))
                    goto fail;
            }
            break;
        case OP_with_get_var:
        case OP_with_put_var:
        case OP_with_delete_var:
        case OP_with_make_ref:
        case OP_with_get_ref:
            {
                JSAtom atom;
                int is_with;

                atom = get_u32(bc_buf + pos + 1);
                label = get_u32(bc_buf + pos + 5);
                is_with = bc_buf[pos + 9];
                if (OPTIMIZE) {
                    label = find_jump_target(s, label, &op1, NULL);
                }
                assert(label >= 0 && label < s->label_count);
                ls = &label_slots[label];
                add_pc2line_info(s, bc_out.size, line_num);
#if SHORT_OPCODES
                jp = &s->jump_slots[s->jump_count++];
                jp->op = op;
                jp->size = 4;
                jp->pos = bc_out.size + 5;
                jp->label = label;
#endif
                dbuf_putc(&bc_out, op);
                dbuf_put_u32(&bc_out, atom);
                dbuf_put_u32(&bc_out, ls->addr - bc_out.size);
                if (ls->addr == -1) {
                    /* unresolved yet: create a new relocation entry */
                    if (!add_reloc(ctx, ls, bc_out.size - 4, 4))
                        goto fail;
                }
                dbuf_putc(&bc_out, is_with);
            }
            break;

        case OP_drop:
            if (OPTIMIZE) {
                /* remove useless drops before return */
                if (code_match(&cc, pos_next, OP_return_undef, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    break;
                }
            }
            goto no_change;

        case OP_null:
#if SHORT_OPCODES
            if (OPTIMIZE) {
                /* transform null strict_eq into is_null */
                if (code_match(&cc, pos_next, OP_strict_eq, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_is_null);
                    pos_next = cc.pos;
                    break;
                }
                /* transform null strict_neq if_false/if_true -> is_null if_true/if_false */
                if (code_match(&cc, pos_next, OP_strict_neq, M2(OP_if_false, OP_if_true), -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_is_null);
                    pos_next = cc.pos;
                    label = cc.label;
                    op = cc.op ^ OP_if_false ^ OP_if_true;
                    goto has_label;
                }
            }
#endif
            /* fall thru */
        case OP_push_false:
        case OP_push_true:
            if (OPTIMIZE) {
                val = (op == OP_push_true);
                if (code_match(&cc, pos_next, M2(OP_if_false, OP_if_true), -1)) {
                has_constant_test:
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    if (val == cc.op - OP_if_false) {
                        /* transform null if_false(l1) -> goto l1 */
                        /* transform false if_false(l1) -> goto l1 */
                        /* transform true if_true(l1) -> goto l1 */
                        pos_next = cc.pos;
                        op = OP_goto;
                        label = cc.label;
                        goto has_goto;
                    } else {
                        /* transform null if_true(l1) -> nop */
                        /* transform false if_true(l1) -> nop */
                        /* transform true if_false(l1) -> nop */
                        pos_next = cc.pos;
                        update_label(s, cc.label, -1);
                        break;
                    }
                }
            }
            goto no_change;

        case OP_push_i32:
            if (OPTIMIZE) {
                /* transform i32(val) neg -> i32(-val) */
                val = get_i32(bc_buf + pos + 1);
                if ((val != INT32_MIN && val != 0)
                &&  code_match(&cc, pos_next, OP_neg, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    if (code_match(&cc, cc.pos, OP_drop, -1)) {
                        if (cc.line_num >= 0) line_num = cc.line_num;
                    } else {
                        add_pc2line_info(s, bc_out.size, line_num);
                        push_short_int(&bc_out, -val);
                    }
                    pos_next = cc.pos;
                    break;
                }
                /* remove push/drop pairs generated by the parser */
                if (code_match(&cc, pos_next, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    pos_next = cc.pos;
                    break;
                }
                /* Optimize constant tests: `if (0)`, `if (1)`, `if (!0)`... */
                if (code_match(&cc, pos_next, M2(OP_if_false, OP_if_true), -1)) {
                    val = (val != 0);
                    goto has_constant_test;
                }
                add_pc2line_info(s, bc_out.size, line_num);
                push_short_int(&bc_out, val);
                break;
            }
            goto no_change;

        case OP_push_bigint_i32:
            if (OPTIMIZE) {
                /* transform i32(val) neg -> i32(-val) */
                val = get_i32(bc_buf + pos + 1);
                if (val != INT32_MIN
                &&  code_match(&cc, pos_next, OP_neg, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    if (code_match(&cc, cc.pos, OP_drop, -1)) {
                        if (cc.line_num >= 0) line_num = cc.line_num;
                    } else {
                        add_pc2line_info(s, bc_out.size, line_num);
                        dbuf_putc(&bc_out, OP_push_bigint_i32);
                        dbuf_put_u32(&bc_out, -val);
                    }
                    pos_next = cc.pos;
                    break;
                }
            }
            goto no_change;

#if SHORT_OPCODES
        case OP_push_const:
        case OP_fclosure:
            if (OPTIMIZE) {
                int idx = get_u32(bc_buf + pos + 1);
                if (idx < 256) {
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_push_const8 + op - OP_push_const);
                    dbuf_putc(&bc_out, idx);
                    break;
                }
            }
            goto no_change;

        case OP_get_field:
            if (OPTIMIZE) {
                JSAtom atom = get_u32(bc_buf + pos + 1);
                if (atom == JS_ATOM_length) {
                    JS_FreeAtom(ctx, atom);
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_get_length);
                    break;
                }
            }
            goto no_change;
#endif
        case OP_push_atom_value:
            if (OPTIMIZE) {
                JSAtom atom = get_u32(bc_buf + pos + 1);
                /* remove push/drop pairs generated by the parser */
                if (code_match(&cc, pos_next, OP_drop, -1)) {
                    JS_FreeAtom(ctx, atom);
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    pos_next = cc.pos;
                    break;
                }
#if SHORT_OPCODES
                if (atom == JS_ATOM_empty_string) {
                    JS_FreeAtom(ctx, atom);
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_push_empty_string);
                    break;
                }
#endif
            }
            goto no_change;

        case OP_to_propkey:
            if (OPTIMIZE) {
                /* remove redundant to_propkey opcodes when storing simple data */
                if (code_match(&cc, pos_next, M3(OP_get_loc, OP_get_arg, OP_get_var_ref), -1, OP_put_array_el, -1)
                ||  code_match(&cc, pos_next, M3(OP_push_i32, OP_push_const, OP_push_atom_value), OP_put_array_el, -1)
                ||  code_match(&cc, pos_next, M4(OP_undefined, OP_null, OP_push_true, OP_push_false), OP_put_array_el, -1)) {
                    break;
                }
            }
            goto no_change;

        case OP_undefined:
            if (OPTIMIZE) {
                /* remove push/drop pairs generated by the parser */
                if (code_match(&cc, pos_next, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    pos_next = cc.pos;
                    break;
                }
                /* transform undefined return -> return_undefined */
                if (code_match(&cc, pos_next, OP_return, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_return_undef);
                    pos_next = cc.pos;
                    break;
                }
                /* transform undefined if_true(l1)/if_false(l1) -> nop/goto(l1) */
                if (code_match(&cc, pos_next, M2(OP_if_false, OP_if_true), -1)) {
                    val = 0;
                    goto has_constant_test;
                }
#if SHORT_OPCODES
                /* transform undefined strict_eq -> is_undefined */
                if (code_match(&cc, pos_next, OP_strict_eq, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_is_undefined);
                    pos_next = cc.pos;
                    break;
                }
                /* transform undefined strict_neq if_false/if_true -> is_undefined if_true/if_false */
                if (code_match(&cc, pos_next, OP_strict_neq, M2(OP_if_false, OP_if_true), -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_is_undefined);
                    pos_next = cc.pos;
                    label = cc.label;
                    op = cc.op ^ OP_if_false ^ OP_if_true;
                    goto has_label;
                }
#endif
            }
            goto no_change;

        case OP_insert2:
            if (OPTIMIZE) {
                /* Transformation:
                   insert2 put_field(a) drop -> put_field(a)
                */
                if (code_match(&cc, pos_next, OP_put_field, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_put_field);
                    dbuf_put_u32(&bc_out, cc.atom);
                    pos_next = cc.pos;
                    break;
                }
            }
            goto no_change;

        case OP_dup:
            if (OPTIMIZE) {
                /* Transformation: dup put_x(n) drop -> put_x(n) */
                int op1, line2 = -1;
                /* Transformation: dup put_x(n) -> set_x(n) */
                if (code_match(&cc, pos_next, M4(OP_put_loc, OP_put_loc_check, OP_put_arg, OP_put_var_ref), -1, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    op1 = cc.op + 1;  /* put_x -> set_x */
                    pos_next = cc.pos;
                    if (code_match(&cc, cc.pos, OP_drop, -1)) {
                        if (cc.line_num >= 0) line_num = cc.line_num;
                        op1 -= 1; /* set_x drop -> put_x */
                        pos_next = cc.pos;
                        if (code_match(&cc, cc.pos, op1 - 1, cc.idx, -1)) {
                            line2 = cc.line_num; /* delay line number update */
                            op1 += 1;   /* put_x(n) get_x(n) -> set_x(n) */
                            pos_next = cc.pos;
                        }
                    }
                    add_pc2line_info(s, bc_out.size, line_num);
                    put_short_code(&bc_out, op1, cc.idx);
                    if (line2 >= 0) line_num = line2;
                    break;
                }
            }
            goto no_change;

        case OP_get_loc:
            if (OPTIMIZE) {
                /* transformation:
                   get_loc(n) post_dec put_loc(n) drop -> dec_loc(n)
                   get_loc(n) post_inc put_loc(n) drop -> inc_loc(n)
                   get_loc(n) dec dup put_loc(n) drop -> dec_loc(n)
                   get_loc(n) inc dup put_loc(n) drop -> inc_loc(n)
                 */
                int idx;
                idx = get_u16(bc_buf + pos + 1);
                if (idx >= 256)
                    goto no_change;
                if (code_match(&cc, pos_next, M2(OP_post_dec, OP_post_inc), OP_put_loc, idx, OP_drop, -1) ||
                    code_match(&cc, pos_next, M2(OP_dec, OP_inc), OP_dup, OP_put_loc, idx, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, (cc.op == OP_inc || cc.op == OP_post_inc) ? OP_inc_loc : OP_dec_loc);
                    dbuf_putc(&bc_out, idx);
                    pos_next = cc.pos;
                    break;
                }
                /* transformation:
                   get_loc(n) push_atom_value(x) add dup put_loc(n) drop -> push_atom_value(x) add_loc(n)
                 */
                if (code_match(&cc, pos_next, OP_push_atom_value, OP_add, OP_dup, OP_put_loc, idx, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
#if SHORT_OPCODES
                    if (cc.atom == JS_ATOM_empty_string) {
                        JS_FreeAtom(ctx, cc.atom);
                        dbuf_putc(&bc_out, OP_push_empty_string);
                    } else
#endif
                    {
                        dbuf_putc(&bc_out, OP_push_atom_value);
                        dbuf_put_u32(&bc_out, cc.atom);
                    }
                    dbuf_putc(&bc_out, OP_add_loc);
                    dbuf_putc(&bc_out, idx);
                    pos_next = cc.pos;
                    break;
                }
                /* transformation:
                   get_loc(n) push_i32(x) add dup put_loc(n) drop -> push_i32(x) add_loc(n)
                 */
                if (code_match(&cc, pos_next, OP_push_i32, OP_add, OP_dup, OP_put_loc, idx, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    push_short_int(&bc_out, cc.label);
                    dbuf_putc(&bc_out, OP_add_loc);
                    dbuf_putc(&bc_out, idx);
                    pos_next = cc.pos;
                    break;
                }
                /* transformation: XXX: also do these:
                   get_loc(n) get_loc(x) add dup put_loc(n) drop -> get_loc(x) add_loc(n)
                   get_loc(n) get_arg(x) add dup put_loc(n) drop -> get_arg(x) add_loc(n)
                   get_loc(n) get_var_ref(x) add dup put_loc(n) drop -> get_var_ref(x) add_loc(n)
                 */
                if (code_match(&cc, pos_next, M3(OP_get_loc, OP_get_arg, OP_get_var_ref), -1, OP_add, OP_dup, OP_put_loc, idx, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    put_short_code(&bc_out, cc.op, cc.idx);
                    dbuf_putc(&bc_out, OP_add_loc);
                    dbuf_putc(&bc_out, idx);
                    pos_next = cc.pos;
                    break;
                }
                add_pc2line_info(s, bc_out.size, line_num);
                put_short_code(&bc_out, op, idx);
                break;
            }
            goto no_change;
#if SHORT_OPCODES
        case OP_get_arg:
        case OP_get_var_ref:
            if (OPTIMIZE) {
                int idx;
                idx = get_u16(bc_buf + pos + 1);
                add_pc2line_info(s, bc_out.size, line_num);
                put_short_code(&bc_out, op, idx);
                break;
            }
            goto no_change;
#endif
        case OP_put_loc:
        case OP_put_loc_check:
        case OP_put_arg:
        case OP_put_var_ref:
            if (OPTIMIZE) {
                /* transformation: put_x(n) get_x(n) -> set_x(n) */
                int idx;
                idx = get_u16(bc_buf + pos + 1);
                if (code_match(&cc, pos_next, op - 1, idx, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    put_short_code(&bc_out, op + 1, idx);
                    pos_next = cc.pos;
                    break;
                }
                add_pc2line_info(s, bc_out.size, line_num);
                put_short_code(&bc_out, op, idx);
                break;
            }
            goto no_change;

        case OP_post_inc:
        case OP_post_dec:
            if (OPTIMIZE) {
                /* transformation:
                   post_inc put_x drop -> inc put_x
                   post_inc perm3 put_field drop -> inc put_field
                   post_inc perm4 put_array_el drop -> inc put_array_el
                 */
                int op1, idx;
                if (code_match(&cc, pos_next, M3(OP_put_loc, OP_put_arg, OP_put_var_ref), -1, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    op1 = cc.op;
                    idx = cc.idx;
                    pos_next = cc.pos;
                    if (code_match(&cc, cc.pos, op1 - 1, idx, -1)) {
                        if (cc.line_num >= 0) line_num = cc.line_num;
                        op1 += 1;   /* put_x(n) get_x(n) -> set_x(n) */
                        pos_next = cc.pos;
                    }
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_dec + (op - OP_post_dec));
                    put_short_code(&bc_out, op1, idx);
                    break;
                }
                if (code_match(&cc, pos_next, OP_perm3, OP_put_field, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_dec + (op - OP_post_dec));
                    dbuf_putc(&bc_out, OP_put_field);
                    dbuf_put_u32(&bc_out, cc.atom);
                    pos_next = cc.pos;
                    break;
                }
                if (code_match(&cc, pos_next, OP_perm4, OP_put_array_el, OP_drop, -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    add_pc2line_info(s, bc_out.size, line_num);
                    dbuf_putc(&bc_out, OP_dec + (op - OP_post_dec));
                    dbuf_putc(&bc_out, OP_put_array_el);
                    pos_next = cc.pos;
                    break;
                }
            }
            goto no_change;

#if SHORT_OPCODES
        case OP_typeof:
            if (OPTIMIZE) {
                /* simplify typeof tests */
                if (code_match(&cc, pos_next, OP_push_atom_value, M4(OP_strict_eq, OP_strict_neq, OP_eq, OP_neq), -1)) {
                    if (cc.line_num >= 0) line_num = cc.line_num;
                    int op1 = (cc.op == OP_strict_eq || cc.op == OP_eq) ? OP_strict_eq : OP_strict_neq;
                    int op2 = -1;
                    switch (cc.atom) {
                    case JS_ATOM_undefined:
                        op2 = OP_typeof_is_undefined;
                        break;
                    case JS_ATOM_function:
                        op2 = OP_typeof_is_function;
                        break;
                    }
                    if (op2 >= 0) {
                        /* transform typeof(s) == "<type>" into is_<type> */
                        if (op1 == OP_strict_eq) {
                            add_pc2line_info(s, bc_out.size, line_num);
                            dbuf_putc(&bc_out, op2);
                            JS_FreeAtom(ctx, cc.atom);
                            pos_next = cc.pos;
                            break;
                        }
                        if (op1 == OP_strict_neq && code_match(&cc, cc.pos, OP_if_false, -1)) {
                            /* transform typeof(s) != "<type>" if_false into is_<type> if_true */
                            if (cc.line_num >= 0) line_num = cc.line_num;
                            add_pc2line_info(s, bc_out.size, line_num);
                            dbuf_putc(&bc_out, op2);
                            JS_FreeAtom(ctx, cc.atom);
                            pos_next = cc.pos;
                            label = cc.label;
                            op = OP_if_true;
                            goto has_label;
                        }
                    }
                }
            }
            goto no_change;
#endif

        default:
        no_change:
            add_pc2line_info(s, bc_out.size, line_num);
            dbuf_put(&bc_out, bc_buf + pos, len);
            break;
        }
    }

    /* check that there were no missing labels */
    for(i = 0; i < s->label_count; i++) {
        assert(label_slots[i].first_reloc == NULL);
    }
#if SHORT_OPCODES
    if (OPTIMIZE) {
        /* more jump optimizations */
        int patch_offsets = 0;
        for (i = 0, jp = s->jump_slots; i < s->jump_count; i++, jp++) {
            LabelSlot *ls;
            JumpSlot *jp1;
            int j, pos, diff, delta;

            delta = 3;
            switch (op = jp->op) {
            case OP_goto16:
                delta = 1;
                /* fall thru */
            case OP_if_false:
            case OP_if_true:
            case OP_goto:
                pos = jp->pos;
                diff = s->label_slots[jp->label].addr - pos;
                if (diff >= -128 && diff <= 127 + delta) {
                    //put_u8(bc_out.buf + pos, diff);
                    jp->size = 1;
                    if (op == OP_goto16) {
                        bc_out.buf[pos - 1] = jp->op = OP_goto8;
                    } else {
                        bc_out.buf[pos - 1] = jp->op = OP_if_false8 + (op - OP_if_false);
                    }
                    goto shrink;
                } else
                if (diff == (int16_t)diff && op == OP_goto) {
                    //put_u16(bc_out.buf + pos, diff);
                    jp->size = 2;
                    delta = 2;
                    bc_out.buf[pos - 1] = jp->op = OP_goto16;
                shrink:
                    /* XXX: should reduce complexity, using 2 finger copy scheme */
                    memmove(bc_out.buf + pos + jp->size, bc_out.buf + pos + jp->size + delta,
                            bc_out.size - pos - jp->size - delta);
                    bc_out.size -= delta;
                    patch_offsets++;
                    for (j = 0, ls = s->label_slots; j < s->label_count; j++, ls++) {
                        if (ls->addr > pos)
                            ls->addr -= delta;
                    }
                    for (j = i + 1, jp1 = jp + 1; j < s->jump_count; j++, jp1++) {
                        if (jp1->pos > pos)
                            jp1->pos -= delta;
                    }
                    for (j = 0; j < s->line_number_count; j++) {
                        if (s->line_number_slots[j].pc > pos)
                            s->line_number_slots[j].pc -= delta;
                    }
                    continue;
                }
                break;
            }
        }
        if (patch_offsets) {
            JumpSlot *jp1;
            int j;
            for (j = 0, jp1 = s->jump_slots; j < s->jump_count; j++, jp1++) {
                int diff1 = s->label_slots[jp1->label].addr - jp1->pos;
                switch (jp1->size) {
                case 1:
                    put_u8(bc_out.buf + jp1->pos, diff1);
                    break;
                case 2:
                    put_u16(bc_out.buf + jp1->pos, diff1);
                    break;
                case 4:
                    put_u32(bc_out.buf + jp1->pos, diff1);
                    break;
                }
            }
        }
    }
    js_free(ctx, s->jump_slots);
    s->jump_slots = NULL;
#endif
    js_free(ctx, s->label_slots);
    s->label_slots = NULL;
    /* XXX: should delay until copying to runtime bytecode function */
    compute_pc2line_info(s);
    js_free(ctx, s->line_number_slots);
    s->line_number_slots = NULL;
    /* set the new byte code */
    dbuf_free(&s->byte_code);
    s->byte_code = bc_out;
    s->use_short_opcodes = TRUE;
    if (dbuf_error(&s->byte_code)) {
        JS_ThrowOutOfMemory(ctx);
        return -1;
    }
    return 0;
 fail:
    /* XXX: not safe */
    dbuf_free(&bc_out);
    return -1;
}

/* compute the maximum stack size needed by the function */

typedef struct StackSizeState {
    int bc_len;
    int stack_len_max;
    uint16_t *stack_level_tab;
    int32_t *catch_pos_tab;
    int *pc_stack;
    int pc_stack_len;
    int pc_stack_size;
} StackSizeState;

/* 'op' is only used for error indication */
static __exception int ss_check(JSContext *ctx, StackSizeState *s,
                                int pos, int op, int stack_len, int catch_pos)
{
    if ((unsigned)pos >= s->bc_len) {
        JS_ThrowInternalError(ctx, "bytecode buffer overflow (op=%d, pc=%d)", op, pos);
        return -1;
    }
    if (stack_len > s->stack_len_max) {
        s->stack_len_max = stack_len;
        if (s->stack_len_max > JS_STACK_SIZE_MAX) {
            JS_ThrowInternalError(ctx, "stack overflow (op=%d, pc=%d)", op, pos);
            return -1;
        }
    }
    if (s->stack_level_tab[pos] != 0xffff) {
        /* already explored: check that the stack size is consistent */
        if (s->stack_level_tab[pos] != stack_len) {
            JS_ThrowInternalError(ctx, "inconsistent stack size: %d %d (pc=%d)",
                                  s->stack_level_tab[pos], stack_len, pos);
            return -1;
        } else if (s->catch_pos_tab[pos] != catch_pos) {
            JS_ThrowInternalError(ctx, "inconsistent catch position: %d %d (pc=%d)",
                                  s->catch_pos_tab[pos], catch_pos, pos);
            return -1;
        } else {
            return 0;
        }
    }

    /* mark as explored and store the stack size */
    s->stack_level_tab[pos] = stack_len;
    s->catch_pos_tab[pos] = catch_pos;

    /* queue the new PC to explore */
    if (js_resize_array(ctx, (void **)&s->pc_stack, sizeof(s->pc_stack[0]),
                        &s->pc_stack_size, s->pc_stack_len + 1))
        return -1;
    s->pc_stack[s->pc_stack_len++] = pos;
    return 0;
}

static __exception int compute_stack_size(JSContext *ctx,
                                          JSFunctionDef *fd,
                                          int *pstack_size)
{
    StackSizeState s_s, *s = &s_s;
    int i, diff, n_pop, pos_next, stack_len, pos, op, catch_pos, catch_level;
    const JSOpCode *oi;
    const uint8_t *bc_buf;

    bc_buf = fd->byte_code.buf;
    s->bc_len = fd->byte_code.size;
    /* bc_len > 0 */
    s->stack_level_tab = js_malloc(ctx, sizeof(s->stack_level_tab[0]) *
                                   s->bc_len);
    if (!s->stack_level_tab)
        return -1;
    for(i = 0; i < s->bc_len; i++)
        s->stack_level_tab[i] = 0xffff;
    s->pc_stack = NULL;
    s->catch_pos_tab = js_malloc(ctx, sizeof(s->catch_pos_tab[0]) *
                                   s->bc_len);
    if (!s->catch_pos_tab)
        goto fail;

    s->stack_len_max = 0;
    s->pc_stack_len = 0;
    s->pc_stack_size = 0;

    /* breadth-first graph exploration */
    if (ss_check(ctx, s, 0, OP_invalid, 0, -1))
        goto fail;

    while (s->pc_stack_len > 0) {
        pos = s->pc_stack[--s->pc_stack_len];
        stack_len = s->stack_level_tab[pos];
        catch_pos = s->catch_pos_tab[pos];
        op = bc_buf[pos];
        if (op == 0 || op >= OP_COUNT) {
            JS_ThrowInternalError(ctx, "invalid opcode (op=%d, pc=%d)", op, pos);
            goto fail;
        }
        oi = &short_opcode_info(op);
#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 64)
        printf("%5d: %10s %5d %5d\n", pos, oi->name, stack_len, catch_pos);
#endif
        pos_next = pos + oi->size;
        if (pos_next > s->bc_len) {
            JS_ThrowInternalError(ctx, "bytecode buffer overflow (op=%d, pc=%d)", op, pos);
            goto fail;
        }
        n_pop = oi->n_pop;
        /* call pops a variable number of arguments */
        if (oi->fmt == OP_FMT_npop || oi->fmt == OP_FMT_npop_u16) {
            n_pop += get_u16(bc_buf + pos + 1);
        } else {
#if SHORT_OPCODES
            if (oi->fmt == OP_FMT_npopx) {
                n_pop += op - OP_call0;
            }
#endif
        }

        if (stack_len < n_pop) {
            JS_ThrowInternalError(ctx, "stack underflow (op=%d, pc=%d)", op, pos);
            goto fail;
        }
        stack_len += oi->n_push - n_pop;
        if (stack_len > s->stack_len_max) {
            s->stack_len_max = stack_len;
            if (s->stack_len_max > JS_STACK_SIZE_MAX) {
                JS_ThrowInternalError(ctx, "stack overflow (op=%d, pc=%d)", op, pos);
                goto fail;
            }
        }
        switch(op) {
        case OP_tail_call:
        case OP_tail_call_method:
        case OP_return:
        case OP_return_undef:
        case OP_return_async:
        case OP_throw:
        case OP_throw_error:
        case OP_ret:
            goto done_insn;
        case OP_goto:
            diff = get_u32(bc_buf + pos + 1);
            pos_next = pos + 1 + diff;
            break;
#if SHORT_OPCODES
        case OP_goto16:
            diff = (int16_t)get_u16(bc_buf + pos + 1);
            pos_next = pos + 1 + diff;
            break;
        case OP_goto8:
            diff = (int8_t)bc_buf[pos + 1];
            pos_next = pos + 1 + diff;
            break;
        case OP_if_true8:
        case OP_if_false8:
            diff = (int8_t)bc_buf[pos + 1];
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len, catch_pos))
                goto fail;
            break;
#endif
        case OP_if_true:
        case OP_if_false:
            diff = get_u32(bc_buf + pos + 1);
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len, catch_pos))
                goto fail;
            break;
        case OP_gosub:
            diff = get_u32(bc_buf + pos + 1);
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len + 1, catch_pos))
                goto fail;
            break;
        case OP_with_get_var:
        case OP_with_delete_var:
            diff = get_u32(bc_buf + pos + 5);
            if (ss_check(ctx, s, pos + 5 + diff, op, stack_len + 1, catch_pos))
                goto fail;
            break;
        case OP_with_make_ref:
        case OP_with_get_ref:
            diff = get_u32(bc_buf + pos + 5);
            if (ss_check(ctx, s, pos + 5 + diff, op, stack_len + 2, catch_pos))
                goto fail;
            break;
        case OP_with_put_var:
            diff = get_u32(bc_buf + pos + 5);
            if (ss_check(ctx, s, pos + 5 + diff, op, stack_len - 1, catch_pos))
                goto fail;
            break;
        case OP_catch:
            diff = get_u32(bc_buf + pos + 1);
            if (ss_check(ctx, s, pos + 1 + diff, op, stack_len, catch_pos))
                goto fail;
            catch_pos = pos;
            break;
        case OP_for_of_start:
        case OP_for_await_of_start:
            catch_pos = pos;
            break;
            /* we assume the catch offset entry is only removed with
               some op codes */
        case OP_drop:
            catch_level = stack_len;
            goto check_catch;
        case OP_nip:
            catch_level = stack_len - 1;
            goto check_catch;
        case OP_nip1:
            catch_level = stack_len - 1;
            goto check_catch;
        case OP_iterator_close:
            catch_level = stack_len + 2;
        check_catch:
            /* Note: for for_of_start/for_await_of_start we consider
               the catch offset is on the first stack entry instead of
               the thirst */
            if (catch_pos >= 0) {
                int level;
                level = s->stack_level_tab[catch_pos];
                if (bc_buf[catch_pos] != OP_catch)
                    level++; /* for_of_start, for_wait_of_start */
                /* catch_level = stack_level before op_catch is executed ? */
                if (catch_level == level) {
                    catch_pos = s->catch_pos_tab[catch_pos];
                }
            }
            break;
        case OP_nip_catch:
            if (catch_pos < 0) {
                JS_ThrowInternalError(ctx, "nip_catch: no catch op (pc=%d)", pos);
                goto fail;
            }
            stack_len = s->stack_level_tab[catch_pos];
            if (bc_buf[catch_pos] != OP_catch)
                stack_len++; /* for_of_start, for_wait_of_start */
            stack_len++; /* no stack overflow is possible by construction */
            catch_pos = s->catch_pos_tab[catch_pos];
            break;
        default:
            break;
        }
        if (ss_check(ctx, s, pos_next, op, stack_len, catch_pos))
            goto fail;
    done_insn: ;
    }
    js_free(ctx, s->pc_stack);
    js_free(ctx, s->catch_pos_tab);
    js_free(ctx, s->stack_level_tab);
    *pstack_size = s->stack_len_max;
    return 0;
 fail:
    js_free(ctx, s->pc_stack);
    js_free(ctx, s->catch_pos_tab);
    js_free(ctx, s->stack_level_tab);
    *pstack_size = 0;
    return -1;
}

static int add_global_variables(JSContext *ctx, JSFunctionDef *fd)
{
    int i, idx;
    JSModuleDef *m = fd->module;
    JSExportEntry *me;
    JSGlobalVar *hf;
    BOOL need_global_closures;
    
    /* Script: add the defined global variables. In the non strict
       direct eval not in global scope, the global variables are
       created in the enclosing scope so they are not created as
       variable references.

       In modules, the imported global variables were added as closure
       global variables in js_parse_import().
    */
    need_global_closures = TRUE;
    if (fd->eval_type == JS_EVAL_TYPE_DIRECT && !(fd->js_mode & JS_MODE_STRICT)) {
        /* XXX: add a flag ? */
        for(idx = 0; idx < fd->closure_var_count; idx++) {
            JSClosureVar *cv = &fd->closure_var[idx];
            if (cv->var_name == JS_ATOM__var_ ||
                cv->var_name == JS_ATOM__arg_var_) {
                need_global_closures = FALSE;
                break;
            }
        }
    }

    if (need_global_closures) {
        JSClosureTypeEnum closure_type;
        if (fd->module)
            closure_type = JS_CLOSURE_MODULE_DECL;
        else
            closure_type = JS_CLOSURE_GLOBAL_DECL;
        for(i = 0; i < fd->global_var_count; i++) {
            JSVarKindEnum var_kind;
            hf = &fd->global_vars[i];
            if (hf->cpool_idx >= 0 && !hf->is_lexical) {
                var_kind = JS_VAR_GLOBAL_FUNCTION_DECL;
            } else {
                var_kind = JS_VAR_NORMAL;
            }
            if (add_closure_var(ctx, fd, closure_type, i, hf->var_name, hf->is_const,
                                hf->is_lexical, var_kind) < 0)
                return -1;
        }
    }

    if (fd->module) {
        /* resolve the variable names of the local exports */
        for(i = 0; i < m->export_entries_count; i++) {
            me = &m->export_entries[i];
            if (me->export_type == JS_EXPORT_TYPE_LOCAL) {
                idx = find_closure_var(ctx, fd, me->local_name);
                if (idx < 0) {
                    JS_ThrowSyntaxErrorAtom(ctx, "exported variable '%s' does not exist",
                                            me->local_name);
                    return -1;
                }
                me->u.local.var_idx = idx;
            }
        }
    }
    return 0;
}

/* create a function object from a function definition. The function
   definition is freed. All the child functions are also created. It
   must be done this way to resolve all the variables. */
JSValue js_create_function(JSContext *ctx, JSFunctionDef *fd)
{
    JSValue func_obj;
    JSFunctionBytecode *b;
    struct list_head *el, *el1;
    int stack_size, scope, idx;
    int function_size, byte_code_offset, cpool_offset;
    int closure_var_offset, vardefs_offset;
    BOOL strip_var_debug;
    
    /* recompute scope linkage */
    for (scope = 0; scope < fd->scope_count; scope++) {
        fd->scopes[scope].first = -1;
    }
    if (fd->has_parameter_expressions) {
        /* special end of variable list marker for the argument scope */
        fd->scopes[ARG_SCOPE_INDEX].first = ARG_SCOPE_END;
    }
    for (idx = 0; idx < fd->var_count; idx++) {
        JSVarDef *vd = &fd->vars[idx];
        vd->scope_next = fd->scopes[vd->scope_level].first;
        fd->scopes[vd->scope_level].first = idx;
    }
    for (scope = 2; scope < fd->scope_count; scope++) {
        JSVarScope *sd = &fd->scopes[scope];
        if (sd->first < 0)
            sd->first = fd->scopes[sd->parent].first;
    }
    for (idx = 0; idx < fd->var_count; idx++) {
        JSVarDef *vd = &fd->vars[idx];
        if (vd->scope_next < 0 && vd->scope_level > 1) {
            scope = fd->scopes[vd->scope_level].parent;
            vd->scope_next = fd->scopes[scope].first;
        }
    }

    /* if the function contains an eval call, the closure variables
       are used to compile the eval and they must be ordered by scope,
       so it is necessary to create the closure variables before any
       other variable lookup is done. */
    if (fd->has_eval_call)
        add_eval_variables(ctx, fd);

    /* add the module global variables in the closure */
    if (fd->is_eval) {
        if (add_global_variables(ctx, fd))
            goto fail;
    } 

    /* first create all the child functions */
    list_for_each_safe(el, el1, &fd->child_list) {
        JSFunctionDef *fd1;
        int cpool_idx;

        fd1 = list_entry(el, JSFunctionDef, link);
        cpool_idx = fd1->parent_cpool_idx;
        func_obj = js_create_function(ctx, fd1);
        if (JS_IsException(func_obj))
            goto fail;
        /* save it in the constant pool */
        assert(cpool_idx >= 0);
        fd->cpool[cpool_idx] = func_obj;
    }

#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 4)
    if (!fd->strip_debug) {
        printf("pass 1\n");
        dump_byte_code(ctx, 1, fd->byte_code.buf, fd->byte_code.size,
                       NULL, fd->args, fd->arg_count, fd->vars, fd->var_count,
                       fd->closure_var, fd->closure_var_count,
                       fd->cpool, fd->cpool_count, fd->source,
                       fd->label_slots, NULL);
        printf("\n");
    }
#endif

    if (resolve_variables(ctx, fd))
        goto fail;

#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 2)
    if (!fd->strip_debug) {
        printf("pass 2\n");
        dump_byte_code(ctx, 2, fd->byte_code.buf, fd->byte_code.size,
                       NULL, fd->args, fd->arg_count, fd->vars, fd->var_count,
                       fd->closure_var, fd->closure_var_count,
                       fd->cpool, fd->cpool_count, fd->source,
                       fd->label_slots, NULL);
        printf("\n");
    }
#endif

    if (resolve_labels(ctx, fd))
        goto fail;

    if (compute_stack_size(ctx, fd, &stack_size) < 0)
        goto fail;

    if (fd->strip_debug) {
        function_size = offsetof(JSFunctionBytecode, debug);
    } else {
        function_size = sizeof(*b);
    }
    cpool_offset = function_size;
    function_size += fd->cpool_count * sizeof(*fd->cpool);
    vardefs_offset = function_size;
    function_size += (fd->arg_count + fd->var_count) * sizeof(*b->vardefs);
    closure_var_offset = function_size;
    function_size += fd->closure_var_count * sizeof(*fd->closure_var);
    byte_code_offset = function_size;
    function_size += fd->byte_code.size;

    b = js_mallocz(ctx, function_size);
    if (!b)
        goto fail;
    js_rc(b)->ref_count = 1;

    b->byte_code_buf = (void *)((uint8_t*)b + byte_code_offset);
    b->byte_code_len = fd->byte_code.size;
    memcpy(b->byte_code_buf, fd->byte_code.buf, fd->byte_code.size);
    js_free(ctx, fd->byte_code.buf);
    fd->byte_code.buf = NULL;

    strip_var_debug = fd->strip_debug && !fd->has_eval_call; /* XXX: check */
    b->func_name = fd->func_name;
    if (fd->arg_count + fd->var_count > 0) {
        int i;
        b->vardefs = (void *)((uint8_t*)b + vardefs_offset);
        for(i = 0; i < fd->arg_count; i++) {
            JSVarDef *vd = &fd->args[i];
            JSBytecodeVarDef *vd1 = &b->vardefs[i];
            if (strip_var_debug) {
                JS_FreeAtom(ctx, vd->var_name);
                vd1->var_name = JS_ATOM_NULL;
            } else {
                vd1->var_name = vd->var_name;
            }
            vd1->has_scope = (vd->scope_level != 0);
            vd1->scope_next = vd->scope_next;
            vd1->is_const = vd->is_const;
            vd1->is_lexical = vd->is_lexical;
            vd1->is_captured = vd->is_captured;
            vd1->var_kind = vd->var_kind;
            vd1->var_ref_idx = vd->var_ref_idx;
        }
        
        for(i = 0; i < fd->var_count; i++) {
            JSVarDef *vd = &fd->vars[i];
            JSBytecodeVarDef *vd1 = &b->vardefs[i + fd->arg_count];
            if (strip_var_debug) {
                JS_FreeAtom(ctx, vd->var_name);
                vd1->var_name = JS_ATOM_NULL;
            } else {
                vd1->var_name = vd->var_name;
            }
            vd1->has_scope = (vd->scope_level != 0);
            vd1->scope_next = vd->scope_next;
            vd1->is_const = vd->is_const;
            vd1->is_lexical = vd->is_lexical;
            vd1->is_captured = vd->is_captured;
            vd1->var_kind = vd->var_kind;
            vd1->var_ref_idx = vd->var_ref_idx;
        }
        b->var_count = fd->var_count;
        b->arg_count = fd->arg_count;
        b->defined_arg_count = fd->defined_arg_count;
        b->var_ref_count = fd->var_ref_count;
        js_free(ctx, fd->args);
        js_free(ctx, fd->vars);
    }
    b->cpool_count = fd->cpool_count;
    if (b->cpool_count) {
        b->cpool = (void *)((uint8_t*)b + cpool_offset);
        memcpy(b->cpool, fd->cpool, b->cpool_count * sizeof(*b->cpool));
    }
    js_free(ctx, fd->cpool);
    fd->cpool = NULL;

    b->stack_size = stack_size;

    if (fd->strip_debug) {
        JS_FreeAtom(ctx, fd->filename);
        dbuf_free(&fd->pc2line);    // probably useless
    } else {
        /* XXX: source and pc2line info should be packed at the end of the
           JSFunctionBytecode structure, avoiding allocation overhead
         */
        b->has_debug = 1;
        b->debug.filename = fd->filename;

        //DynBuf pc2line;
        //compute_pc2line_info(fd, &pc2line);
        //js_free(ctx, fd->line_number_slots)
        b->debug.pc2line_buf = js_realloc(ctx, fd->pc2line.buf, fd->pc2line.size);
        if (!b->debug.pc2line_buf)
            b->debug.pc2line_buf = fd->pc2line.buf;
        b->debug.pc2line_len = fd->pc2line.size;
        b->debug.source = fd->source;
        b->debug.source_len = fd->source_len;
    }
    if (fd->scopes != fd->def_scope_array)
        js_free(ctx, fd->scopes);

    b->closure_var_count = fd->closure_var_count;
    if (b->closure_var_count) {
        if (strip_var_debug) {
            int i;
            for(i = 0; i < fd->closure_var_count; i++) {
                JSClosureVar *cv = &fd->closure_var[i];
                if (cv->closure_type != JS_CLOSURE_GLOBAL_REF &&
                    cv->closure_type != JS_CLOSURE_GLOBAL_DECL &&
                    cv->closure_type != JS_CLOSURE_GLOBAL &&
                    cv->closure_type != JS_CLOSURE_MODULE_DECL &&
                    cv->closure_type != JS_CLOSURE_MODULE_IMPORT) {
                    JS_FreeAtom(ctx, cv->var_name);
                    cv->var_name = JS_ATOM_NULL;
                }
            }
        }
        b->closure_var = (void *)((uint8_t*)b + closure_var_offset);
        memcpy(b->closure_var, fd->closure_var, b->closure_var_count * sizeof(*b->closure_var));
    }
    js_free(ctx, fd->closure_var);
    fd->closure_var = NULL;

    b->has_prototype = fd->has_prototype;
    b->has_simple_parameter_list = fd->has_simple_parameter_list;
    b->js_mode = fd->js_mode;
    b->is_derived_class_constructor = fd->is_derived_class_constructor;
    b->func_kind = fd->func_kind;
    b->need_home_object = (fd->home_object_var_idx >= 0 ||
                           fd->need_home_object);
    b->new_target_allowed = fd->new_target_allowed;
    b->super_call_allowed = fd->super_call_allowed;
    b->super_allowed = fd->super_allowed;
    b->arguments_allowed = fd->arguments_allowed;
    b->is_direct_or_indirect_eval = (fd->eval_type == JS_EVAL_TYPE_DIRECT ||
                                     fd->eval_type == JS_EVAL_TYPE_INDIRECT);
    b->realm = JS_DupContext(ctx);

    add_gc_object(ctx->rt, &b->header, JS_GC_OBJ_TYPE_FUNCTION_BYTECODE);

#if defined(DUMP_BYTECODE) && (DUMP_BYTECODE & 1)
    if (!fd->strip_debug) {
        js_dump_function_bytecode(ctx, b);
    }
#endif

    if (fd->parent) {
        /* remove from parent list */
        list_del(&fd->link);
    }

    js_free(ctx, fd);
    return JS_MKPTR(JS_TAG_FUNCTION_BYTECODE, b);
 fail:
    js_free_function_def(ctx, fd);
    return JS_EXCEPTION;
}


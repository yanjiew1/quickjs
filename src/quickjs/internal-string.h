/*
 * QuickJS Javascript Engine
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
#ifndef QUICKJS_INTERNAL_STRING_H
#define QUICKJS_INTERNAL_STRING_H

#include "internal-runtime.h"
#include "internal-allocator.h"

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)
#define ATOM_GET_STR_BUF_SIZE 64

typedef struct StringBuffer {
    JSContext *ctx;
    JSString *str;
    int len;
    int size;
    int is_wide_char;
    int error_status;
} StringBuffer;

static inline BOOL qjs_atom_is_tagged_int(JSAtom atom)
{
    return (atom & JS_ATOM_TAG_INT) != 0;
}

static inline BOOL qjs_atom_is_const(JSAtom atom)
{
#if defined(DUMP_LEAKS) && DUMP_LEAKS > 1
    return (int32_t)atom <= 0;
#else
    return (int32_t)atom < JS_ATOM_END;
#endif
}

QJS_INTERNAL void qjs_free_atom_struct(JSRuntime *rt, JSAtomStruct *atom);

static inline void qjs_free_atom(JSContext *ctx, JSAtom atom)
{
    if (!qjs_atom_is_const(atom)) {
        JSAtomStruct *str = ctx->rt->atom_array[atom];

        if (--qjs_get_ref_header(str)->ref_count <= 0)
            qjs_free_atom_struct(ctx->rt, str);
    }
}

static inline JSAtom qjs_atom_from_uint32(uint32_t value)
{
    return JS_ATOM_TAG_INT | value;
}

static inline uint32_t qjs_atom_to_uint32(JSAtom atom)
{
    return atom & ~JS_ATOM_TAG_INT;
}

static inline int qjs_string_get(const JSString *str, int index)
{
    return str->is_wide_char ? str->u.str16[index] : str->u.str8[index];
}

static inline BOOL qjs_is_empty_string(JSValueConst value)
{
    int tag = JS_VALUE_GET_TAG(value);
    return (tag == JS_TAG_STRING || tag == JS_TAG_STRING_ROPE) &&
           JS_VALUE_GET_STRING(value)->len == 0;
}

static inline uint32_t qjs_hash_string(const JSString *str, uint32_t hash)
{
    size_t i;
    if (str->is_wide_char) {
        for (i = 0; i < str->len; i++)
            hash = hash * 263 + str->u.str16[i];
    } else {
        for (i = 0; i < str->len; i++)
            hash = hash * 263 + str->u.str8[i];
    }
    return hash;
}

static inline uint32_t qjs_hash_string_rope(JSValueConst value, uint32_t hash)
{
    if (JS_VALUE_GET_TAG(value) == JS_TAG_STRING)
        return qjs_hash_string(JS_VALUE_GET_STRING(value), hash);
    hash = qjs_hash_string_rope(JS_VALUE_GET_STRING_ROPE(value)->left, hash);
    return qjs_hash_string_rope(JS_VALUE_GET_STRING_ROPE(value)->right, hash);
}

/* Compatibility spellings for the existing RegExp consumer. */
#define qjs_regexp_string_get qjs_string_get
#define qjs_regexp_is_empty_string qjs_is_empty_string

static inline JSAtomKindEnum qjs_atom_get_kind(JSContext *ctx, JSAtom atom)
{
    JSAtomStruct *str;

    if (qjs_atom_is_tagged_int(atom))
        return JS_ATOM_KIND_STRING;
    str = ctx->rt->atom_array[atom];
    switch (str->atom_type) {
    case JS_ATOM_TYPE_STRING:
        return JS_ATOM_KIND_STRING;
    case JS_ATOM_TYPE_GLOBAL_SYMBOL:
        return JS_ATOM_KIND_SYMBOL;
    case JS_ATOM_TYPE_SYMBOL:
        return str->hash == JS_ATOM_HASH_PRIVATE ?
            JS_ATOM_KIND_PRIVATE : JS_ATOM_KIND_SYMBOL;
    default:
        abort();
    }
}

static inline BOOL qjs_atom_is_string(JSContext *ctx, JSAtom atom)
{
    return qjs_atom_get_kind(ctx, atom) == JS_ATOM_KIND_STRING;
}

static inline JSAtom qjs_dup_atom_rt(JSRuntime *rt, JSAtom atom)
{
    if (!qjs_atom_is_const(atom))
        qjs_get_ref_header(rt->atom_array[atom])->ref_count++;
    return atom;
}

static inline JSAtom qjs_dup_atom(JSContext *ctx, JSAtom atom)
{
    return qjs_dup_atom_rt(ctx->rt, atom);
}

static inline void qjs_free_atom_rt(JSRuntime *rt, JSAtom atom)
{
    if (!qjs_atom_is_const(atom)) {
        JSAtomStruct *str = rt->atom_array[atom];

        if (--qjs_get_ref_header(str)->ref_count <= 0)
            qjs_free_atom_struct(rt, str);
    }
}
QJS_INTERNAL JSAtom qjs_new_atom_str(JSContext *ctx, JSString *str);
QJS_INTERNAL JSAtom qjs_new_atom_int64(JSContext *ctx, int64_t value);
static inline JSString *qjs_alloc_string_rt(JSRuntime *rt, int max_len,
                                            int is_wide_char)
{
    JSString *str = qjs_malloc_rt_internal(
        rt, sizeof(JSString) + (max_len << is_wide_char) + 1 - is_wide_char);

    if (unlikely(!str))
        return NULL;
    qjs_get_ref_header(str)->ref_count = 1;
    str->is_wide_char = is_wide_char;
    str->len = max_len;
    str->atom_type = 0;
    str->hash = 0;
    str->hash_next = 0;
#ifdef DUMP_LEAKS
    list_add_tail(&str->link, &rt->string_list);
#endif
    return str;
}

static inline JSString *qjs_alloc_string(JSContext *ctx, int max_len,
                                         int is_wide_char)
{
    JSString *str = qjs_alloc_string_rt(ctx->rt, max_len, is_wide_char);

    if (unlikely(!str)) {
        JS_ThrowOutOfMemory(ctx);
        return NULL;
    }
    return str;
}
QJS_INTERNAL void qjs_free_string_zero_ref(JSRuntime *rt, JSString *str);

/* Same as JS_FreeValueRT(), but optimized for a known string value. */
static inline void qjs_free_string(JSRuntime *rt, JSString *str)
{
    if (--qjs_get_ref_header(str)->ref_count <= 0)
        qjs_free_string_zero_ref(rt, str);
}

QJS_INTERNAL int qjs_atom_string_init_runtime(JSRuntime *rt);
QJS_INTERNAL void qjs_atom_string_free_runtime(JSRuntime *rt);
static inline void qjs_atom_string_free_value_rt(JSRuntime *rt, JSValue value)
{
    uint32_t tag = JS_VALUE_GET_TAG(value);

    if (tag == JS_TAG_STRING || tag == JS_TAG_SYMBOL) {
        JSString *str = JS_VALUE_GET_PTR(value);
        if (str->atom_type) {
            qjs_free_atom_struct(rt, str);
        } else {
#ifdef DUMP_LEAKS
            list_del(&str->link);
#endif
            qjs_free_rt_internal(rt, str);
        }
    } else if (tag == JS_TAG_STRING_ROPE) {
        JSStringRope *rope = JS_VALUE_GET_STRING_ROPE(value);
        JS_FreeValueRT(rt, rope->left);
        JS_FreeValueRT(rt, rope->right);
        qjs_free_rt_internal(rt, rope);
    } else {
        abort();
    }
}
QJS_INTERNAL void qjs_dump_atoms(JSRuntime *rt);
QJS_INTERNAL void qjs_atom_string_compute_memory_usage(
    JSRuntime *rt, JSMemoryUsage *stats);
QJS_INTERNAL JSAtom qjs_new_atom_rt_ascii(JSRuntime *rt, const char *str,
                                          size_t len, int atom_type);
QJS_INTERNAL const char *qjs_atom_get_str(JSContext *ctx, char *buf,
                                          int buf_size, JSAtom atom);
QJS_INTERNAL int qjs_string_memcmp(const JSString *left, int left_pos,
                                   const JSString *right, int right_pos,
                                   int len);

static inline BOOL qjs_string_equal(const JSString *left,
                                    const JSString *right)
{
    if (left->len != right->len)
        return FALSE;
    if (left == right)
        return TRUE;
    return qjs_string_memcmp(left, 0, right, 0, left->len) == 0;
}

static inline int qjs_string_compare(JSContext *ctx, const JSString *left,
                                     const JSString *right)
{
    int result;
    int len = min_int(left->len, right->len);

    (void)ctx;
    result = qjs_string_memcmp(left, 0, right, 0, len);
    if (result == 0) {
        if (left->len < right->len)
            result = -1;
        else if (left->len > right->len)
            result = 1;
    }
    return result;
}
QJS_INTERNAL void qjs_print_atom(JSContext *ctx, JSAtom atom);
QJS_INTERNAL void qjs_dump_value_write(void *opaque, const char *buf,
                                       size_t len);
QJS_INTERNAL JSValue qjs_throw_duplicate_export(JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue qjs_throw_syntax_error_atom(JSContext *ctx, JSAtom atom,
                                                 const char *fmt);
QJS_INTERNAL const char *qjs_atom_get_str_rt(JSRuntime *rt, char *buf,
                                             int buf_size, JSAtom atom);
QJS_INTERNAL JSAtom qjs_atom_concat_str(JSContext *ctx, JSAtom atom,
                                        const char *suffix);
QJS_INTERNAL JSAtom qjs_atom_concat_num(JSContext *ctx, JSAtom atom,
                                        uint32_t number);
QJS_INTERNAL JSValue qjs_atom_is_numeric_index_slow(JSContext *ctx,
                                                    JSAtom atom);

static inline JSValue qjs_atom_is_numeric_index_value(JSContext *ctx,
                                                      JSAtom atom)
{
    JSAtomStruct *str;
    int c;

    if (qjs_atom_is_tagged_int(atom))
        return JS_NewInt32(ctx, qjs_atom_to_uint32(atom));
    assert(atom < ctx->rt->atom_size);
    str = ctx->rt->atom_array[atom];
    if (str->atom_type != JS_ATOM_TYPE_STRING)
        return JS_UNDEFINED;
    switch (atom) {
    case JS_ATOM_minus_zero:
    case JS_ATOM_Infinity:
    case JS_ATOM_minus_Infinity:
    case JS_ATOM_NaN:
        return qjs_atom_is_numeric_index_slow(ctx, atom);
    default:
        break;
    }
    if (str->len == 0)
        return JS_UNDEFINED;
    c = qjs_string_get(str, 0);
    if (!qjs_is_digit(c) && c != '-')
        return JS_UNDEFINED;
    return qjs_atom_is_numeric_index_slow(ctx, atom);
}

static inline int qjs_atom_is_numeric_index(JSContext *ctx, JSAtom atom)
{
    JSValue number = qjs_atom_is_numeric_index_value(ctx, atom);

    if (likely(JS_IsUndefined(number)))
        return FALSE;
    if (JS_IsException(number))
        return -1;
    JS_FreeValue(ctx, number);
    return TRUE;
}
QJS_INTERNAL BOOL qjs_atom_symbol_has_description(JSContext *ctx,
                                                  JSAtom atom);
QJS_INTERNAL int qjs_string_buffer_init2(JSContext *ctx, StringBuffer *buf,
                                         int size, int is_wide);
QJS_INTERNAL int qjs_string_buffer_init(JSContext *ctx, StringBuffer *buf,
                                        int size);
static inline void qjs_string_buffer_free(StringBuffer *buf)
{
    qjs_free_internal(buf->ctx, buf->str);
    buf->str = NULL;
}
QJS_INTERNAL int qjs_string_buffer_putc8(StringBuffer *buf, uint32_t c);
QJS_INTERNAL int qjs_string_buffer_putc_slow(StringBuffer *buf, uint32_t c);
static inline int qjs_string_buffer_putc(StringBuffer *buf, uint32_t c)
{
    if (likely(buf->len < buf->size)) {
        if (buf->is_wide_char) {
            if (c < 0x10000) {
                buf->str->u.str16[buf->len++] = c;
                return 0;
            } else if (likely((buf->len + 1) < buf->size)) {
                buf->str->u.str16[buf->len++] = get_hi_surrogate(c);
                buf->str->u.str16[buf->len++] = get_lo_surrogate(c);
                return 0;
            }
        } else if (c < 0x100) {
            buf->str->u.str8[buf->len++] = c;
            return 0;
        }
    }
    return qjs_string_buffer_putc_slow(buf, c);
}
QJS_INTERNAL JSValue qjs_string_buffer_end(StringBuffer *buf);
QJS_INTERNAL int qjs_string_find_invalid_codepoint(JSString *str);
QJS_INTERNAL JSAtom qjs_get_atom_index(JSRuntime *rt, JSAtomStruct *str);
QJS_INTERNAL JSValue qjs_new_symbol(JSContext *ctx, JSString *str,
                                    int atom_type);
static inline JSValue qjs_new_string8_len(JSContext *ctx, const char *buf,
                                          int len)
{
    JSString *str;

    if (len <= 0)
        return JS_AtomToString(ctx, JS_ATOM_empty_string);
    str = qjs_alloc_string(ctx, len, 0);
    if (!str)
        return JS_EXCEPTION;
    memcpy(str->u.str8, buf, len);
    str->u.str8[len] = '\0';
    return JS_MKPTR(JS_TAG_STRING, str);
}
QJS_INTERNAL JSValue qjs_new_string8(JSContext *ctx, const char *buf);
QJS_INTERNAL JSValue qjs_new_string16_len(JSContext *ctx,
                                          const uint16_t *buf, int len);
QJS_INTERNAL JSValue qjs_new_string_char(JSContext *ctx, uint16_t c);
QJS_INTERNAL JSValue qjs_sub_string(JSContext *ctx, JSString *str,
                                    int start, int end);
QJS_INTERNAL int qjs_string_buffer_putc16(StringBuffer *buf, uint32_t c);
QJS_INTERNAL int qjs_string_buffer_write8(StringBuffer *buf,
                                          const uint8_t *str, int len);
static inline int qjs_string_getc(const JSString *str, int *index)
{
    int pos = *index;
    int c, c1;

    if (str->is_wide_char) {
        c = str->u.str16[pos++];
        if (is_hi_surrogate(c) && pos < str->len) {
            c1 = str->u.str16[pos];
            if (is_lo_surrogate(c1)) {
                c = from_surrogate(c, c1);
                pos++;
            }
        }
    } else {
        c = str->u.str8[pos++];
    }
    *index = pos;
    return c;
}
QJS_INTERNAL int qjs_string_buffer_puts8(StringBuffer *buf, const char *str);
QJS_INTERNAL int qjs_string_buffer_concat(StringBuffer *buf,
                                          const JSString *str,
                                          uint32_t from, uint32_t to);
QJS_INTERNAL int qjs_string_buffer_concat_value(StringBuffer *buf,
                                                JSValueConst value);
QJS_INTERNAL int qjs_string_buffer_concat_value_free(StringBuffer *buf,
                                                     JSValue value);
QJS_INTERNAL int qjs_string_buffer_fill(StringBuffer *buf, int c, int count);
QJS_INTERNAL JSValue qjs_concat_string3(JSContext *ctx, const char *prefix,
                                        JSValue value, const char *suffix);
QJS_INTERNAL int qjs_string_rope_get(JSValueConst value, uint32_t index);
QJS_INTERNAL int qjs_string_rope_compare(JSContext *ctx,
                                         JSValueConst left,
                                         JSValueConst right, BOOL eq_only);
QJS_INTERNAL JSValue qjs_linearize_string_rope(JSContext *ctx,
                                               JSValue rope);
QJS_INTERNAL JSValue qjs_concat_string(JSContext *ctx, JSValue left,
                                       JSValue right);
QJS_INTERNAL BOOL qjs_concat_string_in_place(JSContext *ctx, JSString *left,
                                             JSValueConst right);
QJS_INTERNAL BOOL qjs_atom_is_array_index_slow(JSContext *ctx,
                                               uint32_t *index, JSAtom atom);

static inline BOOL qjs_atom_is_array_index(JSContext *ctx, uint32_t *index,
                                           JSAtom atom)
{
    JSAtomStruct *str;
    int c;

    if (qjs_atom_is_tagged_int(atom)) {
        *index = qjs_atom_to_uint32(atom);
        return TRUE;
    }
    assert(atom < ctx->rt->atom_size);
    str = ctx->rt->atom_array[atom];
    if (str->atom_type != JS_ATOM_TYPE_STRING)
        goto not_index;
    if (str->len == 0 || str->len > 10)
        goto not_index;
    c = qjs_string_get(str, 0);
    if (!qjs_is_digit(c))
        goto not_index;
    return qjs_atom_is_array_index_slow(ctx, index, atom);
not_index:
    *index = 0;
    return FALSE;
}
QJS_INTERNAL JSValue qjs_new_symbol_from_atom(JSContext *ctx, JSAtom atom,
                                              int atom_type);
QJS_INTERNAL JSValue qjs_to_locale_string_free(JSContext *ctx,
                                               JSValue value);
QJS_INTERNAL JSValue qjs_to_string_check_object(JSContext *ctx,
                                                JSValueConst value);

QJS_INTERNAL int qjs_regexp_string_indexof_char(JSString *str, int c,
                                                int from);
QJS_INTERNAL int64_t qjs_regexp_string_advance_index(JSString *str,
                                                     int64_t index,
                                                     BOOL unicode);
QJS_INTERNAL int qjs_string_get_substitution(
    JSContext *ctx, StringBuffer *buf, JSValueConst matched, JSString *str,
    uint32_t position, JSValueConst captures_value,
    JSValueConst named_captures, JSValueConst replacement,
    uint8_t **captures, uint32_t captures_len);

#endif /* QUICKJS_INTERNAL_STRING_H */

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

/* Compatibility spellings for the existing RegExp consumer. */
#define qjs_regexp_string_get qjs_string_get
#define qjs_regexp_is_empty_string qjs_is_empty_string

QJS_INTERNAL BOOL qjs_atom_is_string(JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSAtom qjs_new_atom_str(JSContext *ctx, JSString *str);
QJS_INTERNAL JSString *qjs_alloc_string(JSContext *ctx, int max_len,
                                       int is_wide_char);
QJS_INTERNAL void qjs_free_string(JSRuntime *rt, JSString *str);
QJS_INTERNAL const char *qjs_atom_get_str(JSContext *ctx, char *buf,
                                          int buf_size, JSAtom atom);
QJS_INTERNAL int qjs_string_compare(JSContext *ctx, const JSString *left,
                                    const JSString *right);
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
QJS_INTERNAL int qjs_string_buffer_init(JSContext *ctx, StringBuffer *buf,
                                        int size);
QJS_INTERNAL void qjs_string_buffer_free(StringBuffer *buf);
QJS_INTERNAL int qjs_string_buffer_putc8(StringBuffer *buf, uint32_t c);
QJS_INTERNAL int qjs_string_buffer_putc(StringBuffer *buf, uint32_t c);
QJS_INTERNAL JSValue qjs_string_buffer_end(StringBuffer *buf);
QJS_INTERNAL int qjs_string_find_invalid_codepoint(JSString *str);

QJS_INTERNAL JSValue qjs_regexp_new_string8_len(JSContext *ctx,
                                                const char *buf, int len);
QJS_INTERNAL JSValue qjs_regexp_new_string8(JSContext *ctx, const char *buf);
QJS_INTERNAL JSValue qjs_regexp_sub_string(JSContext *ctx, JSString *str,
                                           int start, int end);
QJS_INTERNAL int qjs_regexp_string_buffer_init2(JSContext *ctx,
                                                StringBuffer *buf, int size,
                                                int is_wide);
QJS_INTERNAL int qjs_regexp_string_buffer_putc16(StringBuffer *buf,
                                                 uint32_t c);
QJS_INTERNAL int qjs_regexp_string_getc(const JSString *str, int *index);
QJS_INTERNAL int qjs_regexp_string_buffer_puts8(StringBuffer *buf,
                                                const char *str);
QJS_INTERNAL int qjs_regexp_string_buffer_concat(StringBuffer *buf,
                                                 const JSString *str,
                                                 int from, int to);
QJS_INTERNAL int qjs_regexp_string_buffer_concat_value(StringBuffer *buf,
                                                       JSValueConst value);
QJS_INTERNAL int qjs_regexp_string_buffer_concat_value_free(
    StringBuffer *buf, JSValue value);
QJS_INTERNAL JSValue qjs_regexp_concat_string3(JSContext *ctx,
                                               const char *prefix,
                                               JSValue str,
                                               const char *suffix);
QJS_INTERNAL int qjs_regexp_string_indexof_char(JSString *str, int c,
                                                int from);
QJS_INTERNAL int64_t qjs_regexp_string_advance_index(JSString *str,
                                                     int64_t index,
                                                     BOOL unicode);
QJS_INTERNAL int qjs_regexp_string_get_substitution(
    JSContext *ctx, StringBuffer *buf, JSValueConst matched, JSString *str,
    uint32_t position, JSValueConst captures_value,
    JSValueConst named_captures, JSValueConst replacement,
    uint8_t **captures, uint32_t captures_len);

#endif /* QUICKJS_INTERNAL_STRING_H */

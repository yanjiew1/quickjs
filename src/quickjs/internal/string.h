/*
 * QuickJS internal string interfaces
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
#ifndef QUICKJS_PRIVATE_STRING_H
#define QUICKJS_PRIVATE_STRING_H

typedef struct StringBuffer {
    JSContext *ctx;
    JSString *str;
    int len;
    int size;
    int is_wide_char;
    int error_status;
} StringBuffer;

/* Internal implementation detail; not part of the public QuickJS API. */
JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char);

static inline void js_free_string(JSRuntime *rt, JSString *str)
{
    if (--js_rc(str)->ref_count <= 0) {
        if (str->atom_type) {
            JS_FreeAtomStruct(rt, str);
        } else {
#ifdef DUMP_LEAKS
            list_del(&str->link);
#endif
            js_free_rt(rt, str);
        }
    }
}

/* Internal implementation detail; not part of the public QuickJS API. */
int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size,
                               int is_wide);

static inline int string_buffer_init(JSContext *ctx, StringBuffer *s, int size)
{
    return string_buffer_init2(ctx, s, size, 0);
}

/* Internal implementation detail; not part of the public QuickJS API. */
int string_buffer_putc_slow(StringBuffer *s, uint32_t c);

static inline int string_buffer_putc(StringBuffer *s, uint32_t c)
{
    if (likely(s->len < s->size)) {
        if (s->is_wide_char) {
            if (c < 0x10000) {
                s->str->u.str16[s->len++] = c;
                return 0;
            } else if (likely((s->len + 1) < s->size)) {
                s->str->u.str16[s->len++] = get_hi_surrogate(c);
                s->str->u.str16[s->len++] = get_lo_surrogate(c);
                return 0;
            }
        } else if (c < 0x100) {
            s->str->u.str8[s->len++] = c;
            return 0;
        }
    }
    return string_buffer_putc_slow(s, c);
}



/* Internal implementation detail; not part of the public QuickJS API. */
void string_buffer_free(StringBuffer *s);

/* Internal implementation detail; not part of the public QuickJS API. */
int string_buffer_putc8(StringBuffer *s, uint32_t c);


/* Internal implementation detail; not part of the public QuickJS API. */
JSValue string_buffer_end(StringBuffer *s);

/* Internal implementation detail; not part of the public QuickJS API. */
int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2);

/* Internal implementation detail; not part of the public QuickJS API. */
int js_string_find_invalid_codepoint(JSString *p);

#endif /* QUICKJS_PRIVATE_STRING_H */

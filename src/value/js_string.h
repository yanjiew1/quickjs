/*
 * QuickJS Javascript Engine: String, StringBuffer, and Rope Definitions
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

#ifndef QUICKJS_VALUE_JS_STRING_H
#define QUICKJS_VALUE_JS_STRING_H

#include "quickjs-internal.h"

/* Allocation and destruction */
JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char);
JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char);
void js_free_string(JSRuntime *rt, JSString *str);

/* Creation and substring */
JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
JSValue js_new_string16_len(JSContext *ctx, const uint16_t *buf, int len);
JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end);
JSValue JS_NewStringLen(JSContext *ctx, const char *buf, size_t buf_len);
JSValue JS_NewAtomString(JSContext *ctx, const char *str);
void JS_FreeCString(JSContext *ctx, const char *ptr);

/* Concatenation */
JSValue JS_ConcatString3(JSContext *ctx, const char *str1, JSValue val2, const char *str3);
JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);
BOOL JS_ConcatStringInPlace(JSContext *ctx, JSString *p1, JSValueConst op2);

/* Comparisons */
int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2, int pos2, int len);
BOOL js_string_eq(JSContext *ctx, const JSString *p1, const JSString *p2);
int js_string_compare(JSContext *ctx, const JSString *p1, const JSString *p2);

/* Ropes */
int string_rope_get(JSValueConst val, uint32_t idx);
int js_string_rope_compare(JSContext *ctx, JSValueConst op1, JSValueConst op2, BOOL eq_only);
JSValue js_linearize_string_rope(JSContext *ctx, JSValue rope);

/* StringBuffer operations */
int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size, int is_wide);
void string_buffer_free(StringBuffer *s);
int string_buffer_putc8(StringBuffer *s, uint32_t c);
int string_buffer_putc16(StringBuffer *s, uint32_t c);
int string_buffer_putc_slow(StringBuffer *s, uint32_t c);
int string_getc(const JSString *p, int *pidx);
int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len);
int string_buffer_concat(StringBuffer *s, const JSString *p, uint32_t from, uint32_t to);
int string_buffer_concat_value(StringBuffer *s, JSValueConst v);
int string_buffer_concat_value_free(StringBuffer *s, JSValue v);
int string_buffer_fill(StringBuffer *s, int c, int count);
JSValue string_buffer_end(StringBuffer *s);

#endif /* QUICKJS_VALUE_JS_STRING_H */

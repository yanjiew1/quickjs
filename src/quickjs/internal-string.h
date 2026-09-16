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

QJS_INTERNAL BOOL __JS_AtomIsTaggedInt(JSAtom v);

QJS_INTERNAL BOOL __JS_AtomIsConst(JSAtom v);

QJS_INTERNAL void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);

QJS_INTERNAL JSAtom __JS_AtomFromUInt32(uint32_t v);

QJS_INTERNAL uint32_t __JS_AtomToUInt32(JSAtom atom);

QJS_INTERNAL int string_get(const JSString *p, int idx);

QJS_INTERNAL BOOL JS_IsEmptyString(JSValueConst v);

QJS_INTERNAL uint32_t hash_string(const JSString *str, uint32_t h);

QJS_INTERNAL uint32_t hash_string_rope(JSValueConst val, uint32_t h);

QJS_INTERNAL JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);

QJS_INTERNAL BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);

QJS_INTERNAL JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);

QJS_INTERNAL JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);
QJS_INTERNAL JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);
QJS_INTERNAL JSString *js_alloc_string_rt(JSRuntime *rt, int max_len, int is_wide_char);

QJS_INTERNAL JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char);

/* Same as JS_FreeValueRT(), but optimized for a known string value. */
QJS_INTERNAL void js_free_string(JSRuntime *rt, JSString *str);

QJS_INTERNAL __maybe_unused void JS_DumpAtoms(JSRuntime *rt);
QJS_INTERNAL const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);
QJS_INTERNAL int js_string_memcmp(const JSString *p1, int pos1, const JSString *p2,
                            int pos2, int len);

QJS_INTERNAL int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2);
QJS_INTERNAL __maybe_unused void print_atom(JSContext *ctx, JSAtom atom);
QJS_INTERNAL void js_dump_value_write(void *opaque, const char *buf, size_t len);
QJS_INTERNAL JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...);
QJS_INTERNAL const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size,
                                   JSAtom atom);
QJS_INTERNAL JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1);
QJS_INTERNAL JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n);

QJS_INTERNAL JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom);

QJS_INTERNAL int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);
QJS_INTERNAL BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v);
QJS_INTERNAL int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size,
                               int is_wide);
QJS_INTERNAL int string_buffer_init(JSContext *ctx, StringBuffer *s, int size);
QJS_INTERNAL void string_buffer_free(StringBuffer *s);
QJS_INTERNAL int string_buffer_putc8(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_putc_slow(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_putc(StringBuffer *s, uint32_t c);
QJS_INTERNAL JSValue string_buffer_end(StringBuffer *s);
QJS_INTERNAL int js_string_find_invalid_codepoint(JSString *p);
QJS_INTERNAL JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);
QJS_INTERNAL JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);
QJS_INTERNAL JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
QJS_INTERNAL JSValue js_new_string8(JSContext *ctx, const char *buf);
QJS_INTERNAL JSValue js_new_string16_len(JSContext *ctx, const uint16_t *buf, int len);
QJS_INTERNAL JSValue js_new_string_char(JSContext *ctx, uint16_t c);
QJS_INTERNAL JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end);
QJS_INTERNAL int string_buffer_putc16(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len);
QJS_INTERNAL int string_getc(const JSString *p, int *pidx);
QJS_INTERNAL int string_buffer_puts8(StringBuffer *s, const char *str);
QJS_INTERNAL int string_buffer_concat(StringBuffer *s, const JSString *p,
                                uint32_t from, uint32_t to);
QJS_INTERNAL int string_buffer_concat_value(StringBuffer *s, JSValueConst v);
QJS_INTERNAL int string_buffer_concat_value_free(StringBuffer *s, JSValue v);
QJS_INTERNAL int string_buffer_fill(StringBuffer *s, int c, int count);
QJS_INTERNAL JSValue JS_ConcatString3(JSContext *ctx, const char *str1,
                                JSValue str2, const char *str3);
QJS_INTERNAL int string_rope_get(JSValueConst val, uint32_t idx);
QJS_INTERNAL int js_string_rope_compare(JSContext *ctx, JSValueConst op1,
                                  JSValueConst op2, BOOL eq_only);
QJS_INTERNAL JSValue js_linearize_string_rope(JSContext *ctx, JSValue rope);
QJS_INTERNAL JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);
QJS_INTERNAL BOOL JS_ConcatStringInPlace(JSContext *ctx, JSString *p1, JSValueConst op2);

QJS_INTERNAL BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom);
QJS_INTERNAL JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr,
                                    int atom_type);
QJS_INTERNAL JSValue JS_ToLocaleStringFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);

QJS_INTERNAL int string_indexof_char(JSString *p, int c, int from);
QJS_INTERNAL int64_t string_advance_index(JSString *p, int64_t index, BOOL unicode);
QJS_INTERNAL int js_string_GetSubstitution(JSContext *ctx,
                                     StringBuffer *b,
                                     JSValueConst matched,
                                     JSString *sp,
                                     uint32_t position,
                                     JSValueConst captures_val,
                                     JSValueConst namedCaptures,
                                     JSValueConst rep,
                                     uint8_t **captures,
                                     uint32_t captures_len);


/* Cross-TU declarations owned by this subsystem. */
QJS_INTERNAL __maybe_unused void JS_DumpString(JSRuntime *rt, const JSString *p);

QJS_INTERNAL int JS_InitAtoms(JSRuntime *rt);

QJS_INTERNAL JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len,
                            int atom_type);

QJS_INTERNAL JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len,
                               int atom_type);

QJS_INTERNAL BOOL atom_is_free(const JSAtomStruct *p);

#endif /* QUICKJS_INTERNAL_STRING_H */

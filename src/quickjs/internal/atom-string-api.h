/*
 * QuickJS Atom String Private Interface
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

#ifndef QJS_INTERNAL_ATOM_STRING_API_H
#define QJS_INTERNAL_ATOM_STRING_API_H

#include "internal/object.h"
#include "internal/vm.h"
#include "internal/runtime-api.h"

QJS_INTERNAL JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);
QJS_INTERNAL const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);
QJS_INTERNAL const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size,
                                   JSAtom atom);
QJS_INTERNAL BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom);
QJS_INTERNAL int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);
QJS_INTERNAL JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom);
QJS_INTERNAL BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);
QJS_INTERNAL BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v);
QJS_INTERNAL JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);
QJS_INTERNAL JSValue JS_ConcatString3(JSContext *ctx, const char *str1,
                                JSValue str2, const char *str3);
QJS_INTERNAL BOOL JS_ConcatStringInPlace(JSContext *ctx, JSString *p1, JSValueConst op2);
QJS_INTERNAL void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);
QJS_INTERNAL int JS_InitAtoms(JSRuntime *rt);
QJS_INTERNAL JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);
QJS_INTERNAL JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);
QJS_INTERNAL int JS_NewClass1(JSRuntime *rt, JSClassID class_id,
                        const JSClassDef *class_def, JSAtom name);
QJS_INTERNAL JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);
QJS_INTERNAL JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr,
                                    int atom_type);
QJS_INTERNAL uint32_t hash_string(const JSString *str, uint32_t h);
QJS_INTERNAL uint32_t hash_string_rope(JSValueConst val, uint32_t h);
QJS_INTERNAL JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n);
QJS_INTERNAL JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1);
QJS_INTERNAL JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);
QJS_INTERNAL JSValue js_linearize_string_rope(JSContext *ctx, JSValue rope);
QJS_INTERNAL JSValue js_new_string16_len(JSContext *ctx, const uint16_t *buf, int len);
QJS_INTERNAL JSValue js_new_string8(JSContext *ctx, const char *buf);
QJS_INTERNAL JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
QJS_INTERNAL JSValue js_new_string_char(JSContext *ctx, uint16_t c);
QJS_INTERNAL int js_string_compare(JSContext *ctx,
                             const JSString *p1, const JSString *p2);
QJS_INTERNAL BOOL js_string_eq(JSContext *ctx,
                         const JSString *p1, const JSString *p2);
QJS_INTERNAL int js_string_rope_compare(JSContext *ctx, JSValueConst op1,
                                  JSValueConst op2, BOOL eq_only);
QJS_INTERNAL JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end);
QJS_INTERNAL int string_buffer_concat(StringBuffer *s, const JSString *p,
                                uint32_t from, uint32_t to);
QJS_INTERNAL int string_buffer_concat_value(StringBuffer *s, JSValueConst v);
QJS_INTERNAL int string_buffer_concat_value_free(StringBuffer *s, JSValue v);
QJS_INTERNAL JSValue string_buffer_end(StringBuffer *s);
QJS_INTERNAL int string_buffer_fill(StringBuffer *s, int c, int count);
QJS_INTERNAL void string_buffer_free(StringBuffer *s);
QJS_INTERNAL int string_buffer_init2(JSContext *ctx, StringBuffer *s, int size,
                               int is_wide);
QJS_INTERNAL int string_buffer_putc16(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_putc8(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_puts8(StringBuffer *s, const char *str);
QJS_INTERNAL int string_buffer_write8(StringBuffer *s, const uint8_t *p, int len);
QJS_INTERNAL int string_getc(const JSString *p, int *pidx);
QJS_INTERNAL int string_rope_get(JSValueConst val, uint32_t idx);

static inline BOOL __JS_AtomIsTaggedInt(JSAtom v)
{
    return (v & JS_ATOM_TAG_INT) != 0;
}

static inline JSAtom __JS_AtomFromUInt32(uint32_t v)
{
    return v | JS_ATOM_TAG_INT;
}

static inline uint32_t __JS_AtomToUInt32(JSAtom atom)
{
    return atom & ~JS_ATOM_TAG_INT;
}

static inline BOOL JS_IsEmptyString(JSValueConst v)
{
    return JS_VALUE_GET_TAG(v) == JS_TAG_STRING && JS_VALUE_GET_STRING(v)->len == 0;
}

static inline int string_buffer_init(JSContext *ctx, StringBuffer *s, int size)
{
    return string_buffer_init2(ctx, s, size, 0);
}

QJS_INTERNAL int string_buffer_putc_slow(StringBuffer *s, uint32_t c);

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

static inline int string_get(const JSString *p, int idx) {
    return p->is_wide_char ? p->u.str16[idx] : p->u.str8[idx];
}

static inline uint32_t atom_get_free(const JSAtomStruct *p)
{
    return (uintptr_t)p >> 1;
}

static inline BOOL atom_is_free(const JSAtomStruct *p)
{
    return (uintptr_t)p & 1;
}

static inline JSAtomStruct *atom_set_free(uint32_t v)
{
    return (JSAtomStruct *)(((uintptr_t)v << 1) | 1);
}

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

#endif /* QJS_INTERNAL_ATOM_STRING_API_H */

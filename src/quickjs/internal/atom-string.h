/*
 * QuickJS Atom and String Internal Interface
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
#ifndef QJS_ATOM_STRING_H
#define QJS_ATOM_STRING_H

#include "base.h"
#include "libunicode.h"

enum {
    __JS_ATOM_NULL = JS_ATOM_NULL,
#define DEF(name, str) JS_ATOM_ ## name,
#include "quickjs-atom.h"
#undef DEF
    JS_ATOM_END,
};
#define JS_ATOM_LAST_KEYWORD JS_ATOM_super
#define JS_STRING_LEN_MAX ((1 << 30) - 1)
#define JS_ATOM_LAST_STRICT_KEYWORD JS_ATOM_yield

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)

/* return the max count from the hash size */
#define JS_ATOM_COUNT_RESIZE(n) ((n) * 2)

static inline BOOL __JS_AtomIsConst(JSAtom v)
{
#if defined(DUMP_LEAKS) && DUMP_LEAKS > 1
        return (int32_t)v <= 0;
#else
        return (int32_t)v < JS_ATOM_END;
#endif
}

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


typedef struct JSString JSString;
typedef struct JSString JSAtomStruct;

#define JS_VALUE_GET_STRING(v) ((JSString *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING_ROPE(v) ((JSStringRope *)JS_VALUE_GET_PTR(v))

enum {
    JS_ATOM_TYPE_STRING = 1,
    JS_ATOM_TYPE_GLOBAL_SYMBOL,
    JS_ATOM_TYPE_SYMBOL,
    JS_ATOM_TYPE_PRIVATE,
};

typedef enum {
    JS_ATOM_KIND_STRING,
    JS_ATOM_KIND_SYMBOL,
    JS_ATOM_KIND_PRIVATE,
} JSAtomKindEnum;

#define JS_ATOM_HASH_MASK  ((1 << 30) - 1)
#define JS_ATOM_HASH_PRIVATE JS_ATOM_HASH_MASK

struct JSString {
    uint32_t len : 31;
    uint8_t is_wide_char : 1; /* 0 = 8 bits, 1 = 16 bits characters */
    /* for JS_ATOM_TYPE_SYMBOL: hash = weakref_count, atom_type = 3,
       for JS_ATOM_TYPE_PRIVATE: hash = JS_ATOM_HASH_PRIVATE, atom_type = 3
       XXX: could change encoding to have one more bit in hash */
    uint32_t hash : 30;
    uint8_t atom_type : 2; /* != 0 if atom, JS_ATOM_TYPE_x */
    uint32_t hash_next; /* atom_index for JS_ATOM_TYPE_SYMBOL */
#ifdef DUMP_LEAKS
    struct list_head link; /* string list */
#endif
    union {
        uint8_t str8[0]; /* 8 bit strings will get an extra null terminator */
        uint16_t str16[0];
    } u;
};

typedef struct JSStringRope {
    uint32_t len;
    uint8_t is_wide_char; /* 0 = 8 bits, 1 = 16 bits characters */
    uint8_t depth; /* max depth of the rope tree */
    /* XXX: could reduce memory usage by using a direct pointer with
       bit 0 to select rope or string */
    JSValue left;
    JSValue right; /* might be the empty string */
} JSStringRope;

static inline int string_get(const JSString *p, int idx) {
    return p->is_wide_char ? p->u.str16[idx] : p->u.str8[idx];
}

typedef struct StringBuffer {
    JSContext *ctx;
    JSString *str;
    int len;
    int size;
    int is_wide_char;
    int error_status;
} StringBuffer;

QJS_INTERNAL int string_buffer_init2(JSContext *ctx, StringBuffer *s,
                                     int size, int is_wide);
QJS_INTERNAL int string_buffer_putc_slow(StringBuffer *s, uint32_t c);

static inline int string_buffer_init(JSContext *ctx, StringBuffer *s, int size)
{
    return string_buffer_init2(ctx, s, size, 0);
}


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


QJS_INTERNAL JSValue js_new_string8(JSContext *ctx, const char *buf);
QJS_INTERNAL JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);
QJS_INTERNAL JSValue JS_ToStringFree(JSContext *ctx, JSValue val);
QJS_INTERNAL JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);
QJS_INTERNAL JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);
QJS_INTERNAL JSValue js_new_string_char(JSContext *ctx, uint16_t c);
QJS_INTERNAL JSValue js_new_string16_len(JSContext *ctx, const uint16_t *buf, int len);
QJS_INTERNAL JSValue js_sub_string(JSContext *ctx, JSString *p, int start, int end);
QJS_INTERNAL JSValue JS_ConcatString(JSContext *ctx, JSValue op1, JSValue op2);
QJS_INTERNAL JSValue JS_ConcatString3(JSContext *ctx, const char *str1,
                                      JSValue str2, const char *str3);
QJS_INTERNAL JSValue JS_ToStringCheckObject(JSContext *ctx, JSValueConst val);
QJS_INTERNAL int string_getc(const JSString *p, int *pidx);
QJS_INTERNAL void string_buffer_free(StringBuffer *s);
QJS_INTERNAL JSValue string_buffer_end(StringBuffer *s);
QJS_INTERNAL int string_buffer_putc16(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_putc8(StringBuffer *s, uint32_t c);
QJS_INTERNAL int string_buffer_concat(StringBuffer *s, const JSString *p,
                                      uint32_t from, uint32_t to);
QJS_INTERNAL int string_buffer_concat_value_free(StringBuffer *s, JSValue v);
QJS_INTERNAL int string_buffer_concat_value(StringBuffer *s, JSValueConst v);
QJS_INTERNAL int string_buffer_fill(StringBuffer *s, int c, int count);
QJS_INTERNAL int string_buffer_puts8(StringBuffer *s, const char *str);

#endif /* QJS_ATOM_STRING_H */

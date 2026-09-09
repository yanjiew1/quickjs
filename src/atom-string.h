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
/* Atom/string representation, tagging and owned string allocation. */
#ifndef QUICKJS_ATOM_STRING_H
#define QUICKJS_ATOM_STRING_H

#include "memory.h"
#include <stdlib.h>
#include <string.h>

#define JS_STRING_LEN_MAX ((1 << 30) - 1)
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


enum {
    __JS_ATOM_NULL = JS_ATOM_NULL,
#define DEF(name, str) JS_ATOM_ ## name,
#include "quickjs-atom.h"
#undef DEF
    JS_ATOM_END,
};
#define JS_ATOM_LAST_KEYWORD JS_ATOM_super
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


static inline int string_get(const JSString *p, int idx) {
    return p->is_wide_char ? p->u.str16[idx] : p->u.str8[idx];
}

static inline JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v)
{
    JSRuntime *rt;
    JSAtomStruct *p;

    rt = ctx->rt;
    if (__JS_AtomIsTaggedInt(v))
        return JS_ATOM_KIND_STRING;
    p = rt->atom_array[v];
    switch(p->atom_type) {
    case JS_ATOM_TYPE_STRING:
        return JS_ATOM_KIND_STRING;
    case JS_ATOM_TYPE_GLOBAL_SYMBOL:
        return JS_ATOM_KIND_SYMBOL;
    case JS_ATOM_TYPE_SYMBOL:
        if (p->hash == JS_ATOM_HASH_PRIVATE)
            return JS_ATOM_KIND_PRIVATE;
        else
            return JS_ATOM_KIND_SYMBOL;
    default:
        abort();
    }
}

static inline BOOL JS_AtomIsString(JSContext *ctx, JSAtom v)
{
    return JS_AtomGetKind(ctx, v) == JS_ATOM_KIND_STRING;
}

/* Release an atom whose reference count has already reached zero. */
JS_INTERNAL void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);

/* same as JS_FreeValueRT() but faster */
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

/* Allocate an owned string with uninitialized contents; throws on OOM. */
JS_INTERNAL JSString *js_alloc_string(JSContext *ctx, int max_len, int is_wide_char);

/* Consume a string and return an owned atom reference, or JS_ATOM_NULL. */
JS_INTERNAL JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);

/* Return an owned Latin-1 string copied from a borrowed span. */
JS_INTERNAL JSValue js_new_string8_len(JSContext *ctx, const char *buf, int len);

static inline JSValue js_new_string8(JSContext *ctx, const char *buf)
{
    return js_new_string8_len(ctx, buf, strlen(buf));
}

#endif /* QUICKJS_ATOM_STRING_H */

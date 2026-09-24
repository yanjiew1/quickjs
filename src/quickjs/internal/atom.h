/*
 * QuickJS atom definitions
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
#ifndef QUICKJS_ATOM_H
#define QUICKJS_ATOM_H

#include "base.h"

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
#define ATOM_GET_STR_BUF_SIZE 64
const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);
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


void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);
JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);
BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);
JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1);
JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n);

JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);

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

typedef enum {
    JS_ATOM_KIND_STRING,
    JS_ATOM_KIND_SYMBOL,
    JS_ATOM_KIND_PRIVATE,
} JSAtomKindEnum;

int JS_InitAtoms(JSRuntime *rt);
JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len, int atom_type);
JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);
JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);
JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len, int atom_type);
JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);
JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);
JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr, int atom_type);
const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size, JSAtom atom);
BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom);
JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom);
int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);
BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v);

#endif

/*
 * QuickJS Atom declarations and helpers
 */
#ifndef QUICKJS_ATOM_H
#define QUICKJS_ATOM_H

#include "quickjs/def.h"
#include "quickjs/string.h"

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

#define JS_ATOM_HASH_MASK    ((1 << 30) - 1)
#define JS_ATOM_HASH_PRIVATE JS_ATOM_HASH_MASK

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)

#define JS_ATOM_COUNT_RESIZE(n) ((n) * 2)

enum {
    __JS_ATOM_NULL = JS_ATOM_NULL,
#define DEF(name, str) JS_ATOM_ ## name,
#include "quickjs-atom.h"
#undef DEF
    JS_ATOM_END,
};
#define JS_ATOM_LAST_KEYWORD JS_ATOM_super
#define JS_ATOM_LAST_STRICT_KEYWORD JS_ATOM_yield

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

static inline int is_num(int c)
{
    return c >= '0' && c <= '9';
}

static inline BOOL is_num_string(uint32_t *pval, const JSString *p)
{
    uint32_t n;
    uint64_t n64;
    int c, i, len;

    len = p->len;
    if (len == 0 || len > 10)
        return FALSE;
    c = string_get(p, 0);
    if (is_num(c)) {
        if (c == '0') {
            if (len != 1)
                return FALSE;
            n = 0;
        } else {
            n = c - '0';
            for(i = 1; i < len; i++) {
                c = string_get(p, i);
                if (!is_num(c))
                    return FALSE;
                n64 = (uint64_t)n * 10 + (c - '0');
                if ((n64 >> 32) != 0)
                    return FALSE;
                n = n64;
            }
        }
        *pval = n;
        return TRUE;
    } else {
        return FALSE;
    }
}

static inline uint32_t hash_string8(const uint8_t *str, size_t len, uint32_t h)
{
    size_t i;
    for(i = 0; i < len; i++)
        h = h * 263 + str[i];
    return h;
}

static inline uint32_t hash_string16(const uint16_t *str, size_t len, uint32_t h)
{
    size_t i;
    for(i = 0; i < len; i++)
        h = h * 263 + str[i];
    return h;
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

void JS_FreeAtomStruct(JSRuntime *rt, JSAtomStruct *p);
JSAtom JS_NewAtomStr(JSContext *ctx, JSString *p);
JSAtomKindEnum JS_AtomGetKind(JSContext *ctx, JSAtom v);
BOOL JS_AtomIsString(JSContext *ctx, JSAtom v);
BOOL JS_AtomIsArrayIndex(JSContext *ctx, uint32_t *pval, JSAtom atom);

#define ATOM_GET_STR_BUF_SIZE 64
const char *JS_AtomGetStrRT(JSRuntime *rt, char *buf, int buf_size, JSAtom atom);
const char *JS_AtomGetStr(JSContext *ctx, char *buf, int buf_size, JSAtom atom);

JSAtom js_atom_concat_num(JSContext *ctx, JSAtom name, uint32_t n);
JSAtom js_atom_concat_str(JSContext *ctx, JSAtom name, const char *str1);
void print_atom(JSContext *ctx, JSAtom atom);
JSAtom js_get_atom_index(JSRuntime *rt, JSAtomStruct *p);

int JS_InitAtoms(JSRuntime *rt);
JSAtom JS_DupAtomRT(JSRuntime *rt, JSAtom v);
JSAtom __JS_NewAtomInit(JSRuntime *rt, const char *str, int len, int atom_type);
JSAtom __JS_FindAtom(JSRuntime *rt, const char *str, size_t len, int atom_type);
void JS_DumpAtoms(JSRuntime *rt);
JSAtom JS_NewAtomInt64(JSContext *ctx, int64_t n);
JSValue JS_NewSymbolFromAtom(JSContext *ctx, JSAtom descr, int atom_type);
JSValue JS_AtomIsNumericIndex1(JSContext *ctx, JSAtom atom);
int JS_AtomIsNumericIndex(JSContext *ctx, JSAtom atom);
BOOL JS_AtomSymbolHasDescription(JSContext *ctx, JSAtom v);
JSAtom js_symbol_to_atom(JSContext *ctx, JSValue val);

#endif /* QUICKJS_ATOM_H */

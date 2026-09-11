/*
 * QuickJS Value, BigInt, and Operator definitions
 */
#ifndef QUICKJS_VALUE_H
#define QUICKJS_VALUE_H

#include "quickjs/def.h"

#if JS_LIMB_BITS == 32

typedef int32_t js_slimb_t;
typedef uint32_t js_limb_t;
typedef int64_t js_sdlimb_t;
typedef uint64_t js_dlimb_t;

#define JS_LIMB_DIGITS 9

#else

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;
typedef int64_t js_slimb_t;
typedef uint64_t js_limb_t;
typedef int128_t js_sdlimb_t;
typedef uint128_t js_dlimb_t;

#define JS_LIMB_DIGITS 19

#endif

struct JSBigInt {
    uint32_t len;
    js_limb_t tab[];
};

typedef struct {
    js_limb_t big_int_buf[sizeof(JSBigInt) / sizeof(js_limb_t)];
    js_limb_t tab[(64 + JS_LIMB_BITS - 1) / JS_LIMB_BITS];
} JSBigIntBuf;

typedef enum {
   /* binary operators */
   JS_OVOP_ADD,
   JS_OVOP_SUB,
   JS_OVOP_MUL,
   JS_OVOP_DIV,
   JS_OVOP_MOD,
   JS_OVOP_POW,
   JS_OVOP_OR,
   JS_OVOP_AND,
   JS_OVOP_XOR,
   JS_OVOP_SHL,
   JS_OVOP_SAR,
   JS_OVOP_SHR,
   JS_OVOP_EQ,
   JS_OVOP_LESS,

   JS_OVOP_BINARY_COUNT,
   /* unary operators */
   JS_OVOP_POS = JS_OVOP_BINARY_COUNT,
   JS_OVOP_NEG,
   JS_OVOP_INC,
   JS_OVOP_DEC,
   JS_OVOP_NOT,

   JS_OVOP_COUNT,
} JSOverloadableOperatorEnum;

typedef struct {
    uint32_t operator_index;
    JSObject *ops[JS_OVOP_BINARY_COUNT];
} JSBinaryOperatorDefEntry;

typedef struct {
    int count;
    JSBinaryOperatorDefEntry *tab;
} JSBinaryOperatorDef;

struct JSOperatorSetData {
    uint32_t operator_counter;
    BOOL is_primitive;
    JSObject *self_ops[JS_OVOP_COUNT];
    JSBinaryOperatorDef left;
    JSBinaryOperatorDef right;
};

static inline BOOL tag_is_string(uint32_t tag)
{
    return tag == JS_TAG_STRING || tag == JS_TAG_STRING_ROPE;
}

JSBigInt *js_bigint_new(JSContext *ctx, int len);
JSBigInt *js_bigint_set_short(JSBigIntBuf *buf, JSValueConst val);
JSValue JS_CompactBigInt(JSContext *ctx, JSBigInt *p);

#define ATOD_INT_ONLY        (1 << 0)
/* accept Oo and Ob prefixes in addition to 0x prefix if radix = 0 */
#define ATOD_ACCEPT_BIN_OCT  (1 << 2)
/* accept O prefix as octal if radix == 0 and properly formed (Annex B) */
#define ATOD_ACCEPT_LEGACY_OCTAL  (1 << 4)
/* accept _ between digits as a digit separator */
#define ATOD_ACCEPT_UNDERSCORES  (1 << 5)
/* allow a suffix to override the type */
#define ATOD_ACCEPT_SUFFIX    (1 << 6)
/* default type */
#define ATOD_TYPE_MASK        (3 << 7)
#define ATOD_TYPE_FLOAT64     (0 << 7)
#define ATOD_TYPE_BIG_INT     (1 << 7)
/* accept -0x1 */
#define ATOD_ACCEPT_PREFIX_AFTER_SIGN (1 << 10)

JSValue js_atof(JSContext *ctx, const char *str, const char **pp,
                int radix, int flags);

#endif /* QUICKJS_VALUE_H */

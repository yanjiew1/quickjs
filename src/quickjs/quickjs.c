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
#include <stdlib.h>
#include "internal/global-environment.h"
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <fenv.h>
#include <math.h>
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#endif

#include "cutils.h"
#include "list.h"
#include "builtins/array-buffer.h"
#include "builtins/array.h"
#include "builtins/async-from-sync-iterator.h"
#include "builtins/atomics.h"
#include "builtins/bigint.h"
#include "builtins/boolean.h"
#include "builtins/collections.h"
#include "builtins/data-view.h"
#include "builtins/date.h"
#include "builtins/error.h"
#include "builtins/finalization-registry.h"
#include "builtins/function.h"
#include "builtins/global.h"
#include "builtins/intrinsics.h"
#include "builtins/iterator.h"
#include "builtins/json.h"
#include "builtins/math.h"
#include "builtins/number.h"
#include "builtins/object.h"
#include "builtins/promise.h"
#include "builtins/proxy.h"
#include "builtins/reflect.h"
#include "builtins/regexp.h"
#include "builtins/string.h"
#include "builtins/symbol.h"
#include "builtins/typed-array.h"
#include "builtins/uint8array-encoding.h"
#include "builtins/weakref.h"
#include "compiler/compiler-internal.h"
#include "compiler/compiler-state.h"
#include "compiler/lexer.h"
#include "compiler/stack-analysis.h"
#include "internal/allocator-types.h"
#include "internal/allocator.h"
#include "internal/atom.h"
#include "internal/base.h"
#include "internal/bigint.h"
#include "internal/bytecode-format.h"
#include "internal/bytecode.h"
#include "internal/class.h"
#include "internal/error.h"
#include "internal/eval.h"
#include "internal/function-list.h"
#include "internal/function.h"
#include "internal/gc.h"
#include "internal/generator.h"
#include "internal/iterator.h"
#include "internal/module.h"
#include "internal/number.h"
#include "internal/object.h"
#include "internal/parse-state.h"
#include "internal/runtime.h"
#include "internal/string.h"
#include "internal/vm.h"
#include "serialization/format.h"
#include "value/compare.h"
#include "value/conversion.h"
#include "value/print.h"
#include "quickjs.h"
#include "libregexp.h"
#include "libunicode.h"
#include "dtoa.h"

#define OPTIMIZE         1
#define SHORT_OPCODES    1
#if defined(__EMSCRIPTEN__)
#define DIRECT_DISPATCH  0
#else
#define DIRECT_DISPATCH  1
#endif

#if defined(__APPLE__)
#define MALLOC_OVERHEAD  0
#else
#define MALLOC_OVERHEAD  8
#endif

#if !defined(_WIN32)
/* define it if printf uses the RNDN rounding mode instead of RNDNA */
#define CONFIG_PRINTF_RNDN
#endif

/* define to include Atomics.* operations which depend on the OS
   threads */
#if !defined(__EMSCRIPTEN__)
#define CONFIG_ATOMICS
#endif

#if !defined(__EMSCRIPTEN__)
/* enable stack limitation */
#define CONFIG_STACK_CHECK
#endif


/* dump object free */
//#define DUMP_FREE
//#define DUMP_CLOSURE
/* dump the bytecode of the compiled functions: combination of bits
   1: dump pass 3 final byte code
   2: dump pass 2 code
   4: dump pass 1 code
   8: dump stdlib functions
  16: dump bytecode in hex
  32: dump line number table
  64: dump compute_stack_size
 */
//#define DUMP_BYTECODE  (1)
/* dump the occurence of the automatic GC */
//#define DUMP_GC
/* dump objects freed by the garbage collector */
//#define DUMP_GC_FREE
/* dump objects leaking when freeing the runtime */
//#define DUMP_LEAKS  1
/* dump memory usage before running the garbage collector */
//#define DUMP_MEM
//#define DUMP_OBJECTS    /* dump objects in JS_FreeContext */
//#define DUMP_ATOMS      /* dump atoms in JS_FreeContext */
//#define DUMP_SHAPES     /* dump shapes in JS_FreeContext */
//#define DUMP_MODULE_RESOLVE
//#define DUMP_MODULE_EXEC
//#define DUMP_PROMISE
//#define DUMP_READ_OBJECT
//#define DUMP_ROPE_REBALANCE
/* add asm labels to each opcode so that it is easier to see the generated code */
//#define OPCODE_ASM_LABEL

/* test the GC by forcing it before each object allocation */
//#define FORCE_GC_AT_MALLOC

#ifdef CONFIG_ATOMICS
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#endif



/* number of typed array types */
#define JS_TYPED_ARRAY_COUNT  (JS_CLASS_FLOAT64_ARRAY - JS_CLASS_UINT8C_ARRAY + 1)

#define typed_array_size_log2(classid)  (typed_array_size_log2[(classid)- JS_CLASS_UINT8C_ARRAY])



/* the variable and scope indexes must fit on 16 bits. The (-1) and
   ARG_SCOPE_END values are reserved. */
#define JS_MAX_LOCAL_VARS 65534
#define JS_STACK_SIZE_MAX 65534
#define JS_STRING_LEN_MAX ((1 << 30) - 1)

/* strings <= this length are not concatenated using ropes. if too
   small, the rope memory overhead becomes high. */
#define JS_STRING_ROPE_SHORT_LEN  512
/* specific threshold for initial rope use */
#define JS_STRING_ROPE_SHORT2_LEN 8192
/* rope depth at which we rebalance */
#define JS_STRING_ROPE_MAX_DEPTH 60

#define __exception __attribute__((warn_unused_result))






#define JS_VALUE_GET_OBJ(v) ((JSObject *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING(v) ((JSString *)JS_VALUE_GET_PTR(v))
#define JS_VALUE_GET_STRING_ROPE(v) ((JSStringRope *)JS_VALUE_GET_PTR(v))





/* JS malloc */

#define JS_MALLOC_ALIGN 8
#define JS_MALLOC_ARENA_SIZE 4096
#define JS_MALLOC_BLOCK_SIZE_COUNT 31
#define JS_MALLOC_MIN_SMALL_SIZE 16
#define JS_MALLOC_MAX_SMALL_SIZE 512
#if defined(__SANITIZE_ADDRESS__)
/* use the host malloc() for all allocations */
#define JS_MALLOC_LARGE_BLOCKS_ONLY 1
#else
#define JS_MALLOC_LARGE_BLOCKS_ONLY 0
#endif

/* allow iteration among the allocated blocks. Currently not used. May
   be used to suppress the memory overhead of JSGCObjectHeader */
//#define JS_MALLOC_USE_ITER

#define FREE_NIL 0xffff

/* 8 byte header */
/* Notes: 
   - the header is necessary at least to recover a pointer to
     JSMallocArena because we don't want to enforce a page
     alignment on the system malloc().
   - could store the block offset instead of (block_idx,
   block_size_idx), but it would require a division to recover the block
   index.
*/








/* end JS Malloc */





#define JS_MODE_STRICT (1 << 0)
#define JS_MODE_ASYNC  (1 << 2) /* async function */
#define JS_MODE_BACKTRACE_BARRIER (1 << 3) /* stop backtrace before this frame */





/* header for GC objects. GC objects are C data structures with a
   reference count that can reference other GC objects. JS Objects are
   a particular type of GC object. */








/* bigint */

#if JS_LIMB_BITS == 32






#define JS_LIMB_DIGITS 9

#else








#define JS_LIMB_DIGITS 19

#endif



/* this bigint structure can hold a 64 bit integer */

    


/* must be large enough to have a negligible runtime cost and small
   enough to call the interrupt callback often. */
#define JS_INTERRUPT_COUNTER_INIT 10000









#define JS_ATOM_HASH_MASK  ((1 << 30) - 1)
#define JS_ATOM_HASH_PRIVATE JS_ATOM_HASH_MASK









#define ARG_SCOPE_INDEX 1
#define ARG_SCOPE_END (-2)





/* for the encoding of the pc2line table */
#define PC2LINE_BASE     (-1)
#define PC2LINE_RANGE    5
#define PC2LINE_OP_FIRST 1
#define PC2LINE_DIFF_PC_MAX ((255 - PC2LINE_OP_FIRST) / PC2LINE_RANGE)

















































#define JS_PROP_INITIAL_SIZE 2
#define JS_PROP_INITIAL_HASH_SIZE 4 /* must be a power of two */












#define JS_ATOM_LAST_KEYWORD JS_ATOM_super
#define JS_ATOM_LAST_STRICT_KEYWORD JS_ATOM_yield








































































#define HINT_STRING  0
#define HINT_NUMBER  1
#define HINT_NONE    2
#define HINT_FORCE_ORDINARY (1 << 4) // don't try Symbol.toPrimitive




static int JS_ToFloat64Free(JSContext *ctx, double *pres, JSValue val);




































































































/* JS malloc */

/* max overhead for size >= 64: 12.5% */


























#ifdef JS_MALLOC_USE_ITER


/* iterate thru allocated blocks. The allocated block list should not
   be modified while iterating. */

#endif

/* end JS malloc */













/* Throw out of memory in case of error */


/* Throw out of memory in case of error */




/* Throw out of memory in case of error */


/* store extra allocated size in *pslack if successful */




/* Throw out of memory exception in case of error */






/* resize the array and update its size if req_size > *psize */





























/* default memory allocation functions with memory limitation */














/* use -1 to disable automatic GC */


#define malloc(s) malloc_is_forbidden(s)
#define free(p) free_is_forbidden(p)
#define realloc(p,s) realloc_is_forbidden(p,s)













/* return 0 if OK, < 0 if exception */




/* return < 0 if exception, 0 if no job pending, 1 if a job was
   executed successfully. The context of the job is stored in '*pctx'
   if pctx != NULL. It may be NULL if the context was already
   destroyed or if no job was pending. The 'pctx' parameter is now
   absolete. */








/* Note: the string contents are uninitialized */




/* same as JS_FreeValueRT() but faster */














/* set the new value and free the old value after (freeing the value
   can reallocate the object data) */








/* XXX: would be more efficient with separate module lists */




/* used by the GC */














/* JSAtom support */

#define JS_ATOM_TAG_INT (1U << 31)
#define JS_ATOM_MAX_INT (JS_ATOM_TAG_INT - 1)
#define JS_ATOM_MAX     ((1U << 30) - 1)

/* return the max count from the hash size */
#define JS_ATOM_COUNT_RESIZE(n) ((n) * 2)











/* return TRUE if the string is a number n with 0 <= n <= 2^32-1 */


/* XXX: could use faster version ? */




























/* string case (internal). Return JS_ATOM_NULL if error. 'str' is
   freed. */


/* only works with zero terminated 8 bit strings */


/* Warning: str must be ASCII only */






/* Warning: 'p' is freed */


/* XXX: optimize */


/* str is UTF-8 encoded */








/* 'p' is freed */


/* description is UTF-8 encoded or NULL */


/* descr must be a non-numeric string atom */


#define ATOM_GET_STR_BUF_SIZE 64

/* Should only be used for debug. */










/* return TRUE if the atom is an array index (i.e. 0 <= index <=
   2^32-2 and return its value */


/* This test must be fast if atom is not a numeric index (e.g. a
   method name). Return JS_UNDEFINED if not a numeric
   index. JS_EXCEPTION can also be returned. */


/* return -1 if exception or TRUE/FALSE */






/* return TRUE if 'v' is a symbol with a string description */


/* free with JS_FreeCString() */


/* return a string atom containing name concatenated with str1 */






/* JSClass support */

#ifdef CONFIG_ATOMICS

#endif

/* a new class ID is allocated if *pclass_id != 0 */






/* create a new object internal class. Return -1 if error, 0 if
   OK. The finalizer can be NULL if none is needed. */
















/* It is valid to call string_buffer_end() and all string_buffer functions even
   if string_buffer_init() or another string_buffer function returns an error.
   If the error_status is set, string_buffer_end() returns JS_EXCEPTION.
 */














/* 0 <= c <= 0xff */


/* 0 <= c <= 0xffff */




/* 0 <= c <= 0x10ffff */








/* appending an ASCII string */












/* create a string from a UTF-8 buffer */






/* return (NULL, 0) if exception. */
/* return pointer into a JSString with a live ref_count */
/* cesu8 determines if non-BMP1 codepoints are encoded as 1 or 2 utf-8 sequences */












/* return < 0, 0 or > 0 */










/* Return the character at position 'idx'. 'val' must be a string or rope */






/* iterate thru a rope and return the strings in order */






/* 'rope' must be a rope. return a string and modify the rope so that
   it won't need to be linearized again. */




/* op1 and op2 must be strings or string ropes */


#define ROPE_N_BUCKETS 44

/* Fibonacii numbers starting from F_2 */




/* Return a new rope which is balanced. Algorithm from "Ropes: an
   Alternative to Strings", Hans-J. Boehm, Russ Atkinson and Michael
   Plass. */


/* op1 and op2 are converted to strings. For convenience, op1 or op2 =
   JS_EXCEPTION are accepted and return JS_EXCEPTION.  */


/* Shape support */







/* same magic hash multiplier as the Linux kernel */


/* truncate the shape hash to 'hash_bits' bits */










/* create a new empty shape with prototype 'proto'. It is not hashed */


/* create a new empty shape with prototype 'proto' */




/* The shape is cloned. The new shape is not inserted in the shape
   hash table */










/* make space to hold at least 'count' properties */


/* remove the deleted properties. */




/* find a hashed empty shape matching the prototype. Return NULL if
   not found */


/* find a hashed shape matching sh + (prop, prop_flags). Return NULL if
   not found */






/* 'props[]' is used to initialized the object properties. The number
   of elements depends on the shape. */




/* WARNING: proto must be an object or JS_NULL */


/* WARNING: the shape is not hashed. It is used for objects where
   factorizing the shape is not relevant (prototypes, constructors) */


#if 0

#endif















/* return NULL without exception if not a function or no bytecode */






/* Modify the name of a method according to the atom and
   'flags'. 'flags' is a bitmask of JS_PROP_HAS_GET and
   JS_PROP_HAS_SET. Also set the home object of the method.
   Return < 0 if exception. */


/* Note: at least 'length' arguments will be readable in 'argv' */


/* Note: at least 'length' arguments will be readable in 'argv' */


























/* indicate that the object may be part of a function prototype cycle */


































/* called with the ref_count of 'v' reaches zero. */




/* garbage collection */



























/* Return false if not an object or if the object has already been
   freed (zombie objects are visible in finalizers when freeing
   cycles). */


/* Compute memory used by various object types */
/* XXX: poor man's approach to handling multiply referenced objects */
















/* WARNING: obj is freed */


/* return the pending exception (cannot be called twice). */












/* use pc_value = -1 to get the position of the function definition */


/* return a string property without executing arbitrary JS code (used
   when dumping the stack trace or in debug print). */


#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)

/* if filename != NULL, an additional level is added with the filename
   and line number information (used for parse error). */


/* Note: it is important that no exception is returned by this function */














/* never use it directly */


/* never use it directly */


/* %s is replaced by 'atom'. The macro is used so that gcc can check
    the format string. */
#define JS_ThrowTypeErrorAtom(ctx, fmt, atom) __JS_ThrowTypeErrorAtom(ctx, atom, fmt, "")
#define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")



































/* Return -1 (exception) or TRUE/FALSE. 'throw_flag' = FALSE indicates
   that it is called from Reflect.setPrototypeOf(). */


/* return -1 (exception) or TRUE/FALSE */


/* Only works for primitive types, otherwise return JS_NULL. */


/* Return an Object, JS_NULL or JS_EXCEPTION in case of exotic object. */




/* return TRUE, FALSE or (-1) in case of exception */


/* return TRUE, FALSE or (-1) in case of exception */


/* return the value associated to the autoinit property or an exception */




/* warning: 'prs' is reallocated after it */






/* Private fields can be added even on non extensible objects or
   Proxies */






/* add a private brand field to 'home_obj' if not already present and
   if obj is != null add a private brand to it */


/* return a boolean telling if the brand of the home object of 'func'
   is present on 'obj' or -1 in case of exception */








/* return < 0 in case if exception, 0 if OK. ptab and its atoms must
   be freed by the user. */




/* Return -1 if exception,
   FALSE if the property does not exist, TRUE if it exists. If TRUE is
   returned, the property descriptor 'desc' is filled. */




/* return -1 if exception (exotic object only) or TRUE/FALSE */


/* return -1 if exception (exotic object only) or TRUE/FALSE */


/* return -1 if exception otherwise TRUE or FALSE */


/* val must be a symbol */


/* return JS_ATOM_NULL in case of exception */






/* Check if an object has a generalized numeric property. Return value:
   -1 for exception,
   TRUE if property exists, stored into *pval,
   FALSE if proprty does not exist.
 */






/* Note: the property value is not initialized. Return NULL if memory
   error. */


/* can be called on JS_CLASS_ARRAY, JS_CLASS_ARGUMENTS or
   JS_CLASS_MAPPED_ARGUMENTS objects. return < 0 if memory alloc
   error. */








/* set the array length and remove the array elements if necessary. */


/* return -1 if exception */


/* Preconditions: 'p' must be of class JS_CLASS_ARRAY, p->fast_array =
   TRUE and p->extensible = TRUE */


/* Allocate a new fast array initialized to JS_UNDEFINED. Its maximum
   size is 2^31-1 elements. For convenience, 'len' is a 64 bit
   integer. */








/* return -1 in case of exception or TRUE or FALSE. Warning: 'val' is
   freed by the function. 'flags' is a bitmask of JS_PROP_THROW and
   JS_PROP_THROW_STRICT. 'this_obj' is the receiver. If obj !=
   this_obj, then obj must be an object (Reflect.set case). */


/* return true if an element can be added to a fast array without further tests */


/* flags can be JS_PROP_THROW or JS_PROP_THROW_STRICT */








/* compute the property flags. For each flag: (JS_PROP_HAS_x forces
   it, otherwise def_flags is used)
   Note: makes assumption about the bit pattern of the flags
*/




/* return FALSE if not OK */


/* ensure that the shape can be safely modified */




/* allowed flags:
   JS_PROP_CONFIGURABLE, JS_PROP_WRITABLE, JS_PROP_ENUMERABLE
   JS_PROP_HAS_GET, JS_PROP_HAS_SET, JS_PROP_HAS_VALUE,
   JS_PROP_HAS_CONFIGURABLE, JS_PROP_HAS_WRITABLE, JS_PROP_HAS_ENUMERABLE,
   JS_PROP_THROW, JS_PROP_NO_EXOTIC.
   If JS_PROP_THROW is set, return an exception instead of FALSE.
   if JS_PROP_NO_EXOTIC is set, do not call the exotic
   define_own_property callback.
   return -1 (exception), FALSE or TRUE.
*/




/* shortcut to add or redefine a new property value */










/* shortcut to add getter & setter */





/* return TRUE if 'obj' has a non empty 'name' string */






#define DEFINE_GLOBAL_LEX_VAR (1 << 7)
#define DEFINE_GLOBAL_FUNC_VAR (1 << 6)



/* flags is 0, DEFINE_GLOBAL_LEX_VAR or DEFINE_GLOBAL_FUNC_VAR */
/* XXX: could support exotic global object. */


/* construct a reference to a global variable */


/* return -1, FALSE or TRUE */


/* return -1, FALSE or TRUE. return FALSE if not configurable or
   invalid object. return -1 in case of exception.
   flags can be 0, JS_PROP_THROW or JS_PROP_THROW_STRICT */














/* must be called after JS_Throw() */




/* return NULL if not an object of class class_id */






















/* bigint support */

#define JS_BIGINT_MAX_SIZE ((1024 * 1024) / JS_LIMB_BITS) /* in limbs */

/* it is currently assumed that JS_SHORT_BIG_INT_BITS = JS_LIMB_BITS */
#if JS_SHORT_BIG_INT_BITS == 32
#define JS_SHORT_BIG_INT_MIN INT32_MIN
#define JS_SHORT_BIG_INT_MAX INT32_MAX
#elif JS_SHORT_BIG_INT_BITS == 64
#define JS_SHORT_BIG_INT_MIN INT64_MIN
#define JS_SHORT_BIG_INT_MAX INT64_MAX
#else
#error unsupported
#endif

#define ADDC(res, carry_out, op1, op2, carry_in)        \
do {                                                    \
    js_limb_t __v, __a, __k, __k1;                      \
    __v = (op1);                                        \
    __a = __v + (op2);                                  \
    __k1 = __a < __v;                                   \
    __k = (carry_in);                                   \
    __a = __a + __k;                                    \
    carry_out = (__a < __k) | __k1;                     \
    res = __a;                                          \
} while (0)

#if JS_LIMB_BITS == 32
/* a != 0 */

#else

#endif

/* handle a = 0 too */






/* compute 0 - op2. carry = 0 or 1. */


/* tabr[] = taba[] * b + l. Return the high carry */




/* tabr[] += taba[] * b, return the high word. */


/* size of the result : op1_size + op2_size. */


/* tabr[] -= taba[] * b. Return the value to substract to the high
   word. */


/* WARNING: d must be >= 2^(JS_LIMB_BITS-1) */


/* return the quotient and the remainder in '*pr'of 'a1*2^JS_LIMB_BITS+a0
   / d' with 0 <= a1 < d. */


#define UDIV1NORM_THRESHOLD 3

/* b must be >= 1 << (JS_LIMB_BITS - 1) */


/* base case division: divides taba[0..na-1] by tabb[0..nb-1]. tabb[nb
   - 1] must be >= 1 << (JS_LIMB_BITS - 1). na - nb must be >= 0. 'taba'
   is modified and contains the remainder (nb limbs). tabq[0..na-nb]
   contains the quotient with tabq[na - nb] <= 1. */


/* 1 <= shift <= JS_LIMB_BITS - 1 */


/* r = (a + high*B^n) >> shift. Return the remainder r (0 <= r < 2^shift). 
   1 <= shift <= LIMB_BITS - 1 */








/* val must be a short big int */














/* Remove redundant high order limbs. Warning: 'a' may be
   reallocated. Can never fail.
*/




/* return 0 or 1 depending on the sign */




/* add the op1 limb */


/* return NULL in case of error. Compute a + b (b_neg = 0) or a - b
   (b_neg = 1) */
/* XXX: optimize */


/* XXX: optimize */




/* return the division or the remainder. 'b' must be != 0. return NULL
   in case of exception (division by zero or memory error) */


/* and, or, xor */










/* return (mant, exp) so that abs(a) ~ mant*2^(exp - (limb_bits -
   1). a must be != 0. */


/* shift left with round to nearest, ties to even. n >= 1 */


/* convert to float64 with round to nearest, ties to even. Return
   +/-infinity if too large. */


/* return (1, NULL) if not an integer, (2, NULL) if NaN or Infinity,
   (0, n) if an integer, (0, NULL) in case of memory error */


/* return -1, 0, 1 or (2) (unordered) */


/* return -1, 0 or 1 */


/* contains 10^i */


/* syntax: [-]digits in base radix. Return NULL if memory error. radix
   = 10, 2, 8 or 16. */


/* 2 <= base <= 36 */


/* special version going backwards */
/* XXX: use dtoa.c */


/* len >= 1. 2 <= radix <= 36 */


#define JS_RADIX_MAX 36







/* if possible transform a BigInt to short big and free it, otherwise
   return a normal bigint */


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

/* return an exception in case of memory error. Return JS_NAN if
   invalid syntax */
/* XXX: directly use js_atod() */




















/* same as JS_ToNumber() but return 0 in case of NaN/Undefined */


/* Note: the integer value is satured to 32 bits */






#define JS_TO_INT64_SAT_INF 1 /* result was +/-Infinity */
#define JS_TO_INT64_SAT_NAN 2 /* result was NaN */





/* same as JS_ToInt64Sat, but return additional flags */





/* Same as JS_ToInt32Free() but with a 64 bit result. Return (<0, 0)
   in case of exception */






/* return (<0, 0) in case of exception */










#define MAX_SAFE_INTEGER (((int64_t)1 << 53) - 1)





/* convert a value to a length between 0 and MAX_SAFE_INTEGER.
   return -1 for exception */


/* Note: can return an exception */




















#define JS_PRINT_MAX_DEPTH 8















/* pretty print the first 'len' characters of 'p' */












/* return 0 if invalid length */






/* similar to js_regexp_toString() but without side effect */


/* similar to js_error_toString() but without side effect */










/* Note: the 'write_func' callback shall not modify the values which
   are being printed */


















/* for debug only: dump an object without side effect */




/* return -1 if exception (proxy case) or TRUE/FALSE */
// TODO: should take flags to make proxy resolution and exceptions optional








/* return NaN if bad bigint literal */




/* JS Numbers are not allowed */




/* XXX: merge with JS_ToInt64Free with a specific flag ? */
/* return the value mod 2^64 */






/* return an exception if the result does not fit in 128 bits */


/* Convert a bigint to a 128 bit signed integer with
   saturation. Return -1 if exception, 0 if OK, 1 if the value was
   clamped to 128 bits. */


















/* op1 must be a bigint or int. */


/* op1 and op2 must be numeric types and at least one must be a
   bigint. No exception is generated. */




































/* XXX: not 100% compatible, but mozilla seems to use a similar
   implementation to ensure that caller in non strict mode does not
   throw (ES5 compatibility) */












#define GLOBAL_VAR_OFFSET 0x40000000
#define ARGUMENT_VAR_OFFSET 0x20000000





/* legacy arguments object: add references to the function arguments */




/* obj -> enum_obj */


/* return -1 if exception, 0 if slow case, 1 if the enumeration is finished */


/* enum_obj -> enum_obj value done */






/* return *pdone = 2 if the iterator object is not parsed */


/* Note: always return JS_UNDEFINED when *pdone = TRUE. */


/* return < 0 in case of exception */


/* obj -> enum_rec (3 slots) */


/* enum_rec [objs] -> enum_rec [objs] value done. There are 'offset'
   objs. If 'done' is true or in case of exception, 'enum_rec' is set
   to undefined. If 'done' is true, 'value' is always set to
   undefined. */














/* Access an Array's internal JSValue array if available */






/* only valid inside C functions */












/* return a new variable reference. Get it from the uninitialized
   variables if it is present. Return NULL in case of memory error. */














#define JS_DEFINE_CLASS_HAS_HERITAGE     (1 << 0)









#define JS_CALL_FLAG_COPY_ARGV   (1 << 1)
#define JS_CALL_FLAG_GENERATOR   (1 << 2)






#define FUNC_RET_AWAIT         0
#define FUNC_RET_YIELD         1
#define FUNC_RET_YIELD_STAR    2
#define FUNC_RET_INITIAL_YIELD 3

#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-label"
#endif

/* argv[] is modified if (flags & JS_CALL_FLAG_COPY_ARGV) = 0. */


#ifdef OPCODE_ASM_LABEL
#pragma GCC diagnostic pop
#endif





/* warning: the refcount of the context is not incremented. Return
   NULL in case of exception (case of revoked proxy only) */




/* argv[] is modified if (flags & JS_CALL_FLAG_COPY_ARGV) = 0. */










/* JSAsyncFunctionState (used by generator and async functions) */










/* Generators */













/* XXX: use enum */
#define GEN_MAGIC_NEXT   0
#define GEN_MAGIC_RETURN 1
#define GEN_MAGIC_THROW  2





/* AsyncFunction */













/* AsyncGenerator */

































/* magic = GEN_MAGIC_x */




/* JS parser */



#define TOK_FIRST_KEYWORD   TOK_NULL
#define TOK_LAST_KEYWORD    TOK_AWAIT

/* unicode code points */
#define CP_NBSP 0x00a0
#define CP_BOM  0xfeff

#define CP_LS   0x2028
#define CP_PS   0x2029

































#if SHORT_OPCODES
/* After the final compilation pass, short opcodes are used. Their
   opcodes overlap with the temporary opcodes which cannot appear in
   the final bytecode. Their description is after the temporary
   opcodes in opcode_info[]. */
#define short_opcode_info(op)           \
    opcode_info[(op) >= OP_TEMP_START ? \
                (op) + (OP_TEMP_END - OP_TEMP_START) : (op)]
#else
#define short_opcode_info(op) opcode_info[op]
#endif







/* return the zero based line and column number in the source. */
/* Note: we no longer support '\r' as line terminator */




/* 'ptr' is the position of the error in the source */






















/* Walk up through arrow parameter contexts to find an ancestor with
   the given func_kind flags (JS_FUNC_ASYNC or JS_FUNC_GENERATOR) */


/* convert a TOK_IDENT to a keyword when needed */


/* if the current token is an identifier or keyword, reparse it
   according to the current function type */


/* 'c' is the first character. Return JS_ATOM_NULL in case of error */





/* 'c' is the first character. Return JS_ATOM_NULL in case of error */
/* XXX: accept unicode identifiers as JSON5 ? */










/* simple_next_token() is used to check for the next token in simple cases.
   It is only used for ':' and '=>', 'let' or 'function' look-ahead.
   (*pp) is only set if TOK_IMPORT is returned for JS_DetectModule()
   Whitespace and comments are skipped correctly.
   Then the next token is analyzed, only for specific words.
   Return values:
   - '\n' if !no_line_terminator
   - TOK_ARROW, TOK_IN, TOK_IMPORT, TOK_OF, TOK_EXPORT, TOK_FUNCTION
   - TOK_IDENT is returned for other identifiers and keywords
   - otherwise the next character or unicode codepoint is returned.
 */






/* return true if 'input' contains the source of a module
   (heuristic). 'input' must be a zero terminated.

   Heuristic: skip comments and expect 'import' keyword not followed
   by '(' or '.' or export keyword.
*/
























/* don't update the last opcode and don't emit line number info */


/* return the label ID offset */


/* return label or -1 if dead code */


/* return the constant pool index. 'val' is not duplicated. */




/* return the variable index or -1 if not found,
   add ARGUMENT_VAR_OFFSET for argument variables */




/* find a variable declaration in a given scope */


/* return true if scope == parent_scope or if scope is a child of
   parent_scope */


/* find a 'var' declaration in the same scope or a child scope */

















/* return the variable index or -1 if error */








/* add an argument definition in the argument scope. Only needed when
   "eval()" may be called in the argument scope. Return 0 if OK. */




/* add a global variable definition */






/* add a private field variable in the current scope */













/* Note: all the fields are already sealed except length */





#define PROP_TYPE_IDENT 0
#define PROP_TYPE_VAR   1
#define PROP_TYPE_GET   2
#define PROP_TYPE_SET   3
#define PROP_TYPE_STAR  4
#define PROP_TYPE_ASYNC 5
#define PROP_TYPE_ASYNC_STAR 6

#define PROP_TYPE_PRIVATE (1 << 4)



/* if the property is an expression, name = JS_ATOM_NULL */








/* return TRUE if a regexp literal is allowed after this token */


#define SKIP_HAS_SEMI       (1 << 0)
#define SKIP_HAS_ELLIPSIS   (1 << 1)
#define SKIP_HAS_ASSIGNMENT (1 << 2)



/* XXX: improve speed with early bailout */
/* XXX: no longer works if regexps are present. Could use previous
   regexp parsing heuristics to handle most cases */








/* allow the 'in' binary operator */
#define PF_IN_ACCEPTED  (1 << 0)
/* allow function calls parsing in js_parse_postfix_expr() */
#define PF_POSTFIX_CALL (1 << 1)
/* allow the exponentiation operator in js_parse_unary() */
#define PF_POW_ALLOWED  (1 << 2)
/* forbid the exponentiation operator in js_parse_unary() */
#define PF_POW_FORBIDDEN (1 << 3)










/* find field in the current scope */


/* initialize the class fields, called by the constructor. Note:
   super() can be called in an arrow function, so <this> and
   <class_fields_init> can be variable references */


/* build a private setter function name from the private getter name */













/* check if scope chain contains a with statement */






/* name has a live reference. 'is_let' is only used with opcode =
   OP_scope_get_var which is never generated by get_lvalue(). */












/* tok = TOK_VAR, TOK_LET or TOK_CONST. Return whether a reference
   must be taken to the variable for proper 'with' or global variable
   evaluation */
/* Note: this function is needed only because variable references are
   not yet optimized in destructuring */




/* Return -1 if error, 0 if no initializer, 1 if an initializer is
   present at the top level. */






/* allowed parse_flags: PF_POSTFIX_CALL */




/* allowed parse_flags: PF_POW_ALLOWED, PF_POW_FORBIDDEN */


/* allowed parse_flags: PF_IN_ACCEPTED */


/* allowed parse_flags: PF_IN_ACCEPTED */




/* allowed parse_flags: PF_IN_ACCEPTED */


/* allowed parse_flags: PF_IN_ACCEPTED */




/* allowed parse_flags: PF_IN_ACCEPTED */










/* execute the finally blocks before return */


#define DECL_MASK_FUNC  (1 << 0) /* allow normal function declaration */
/* ored with DECL_MASK_FUNC if function declarations are allowed with a label */
#define DECL_MASK_FUNC_WITH_LABEL (1 << 1)
#define DECL_MASK_OTHER (1 << 2) /* all other declarations */
#define DECL_MASK_ALL   (DECL_MASK_FUNC | DECL_MASK_FUNC_WITH_LABEL | DECL_MASK_OTHER)







/* allowed parse_flags: PF_IN_ACCEPTED */


/* test if the current token is a label. Use simplistic look-ahead scanner */


/* test if the current token is a let keyword. Use simplistic look-ahead scanner */


/* XXX: handle IteratorClose when exiting the loop before the
   enumeration is done */






/* 'name' is freed. The module is referenced by 'ctx->loaded_modules' */
















/* create a C module */














/* default module filename normalizer */




/* return NULL in case of exception (e.g. module could not be loaded) */
















/* If the return value is JS_RESOLVE_RES_FOUND, return the module
  (*pmodule) and the corresponding local export entry
  (*pme). Otherwise return (NULL, NULL) */















/* Unfortunately, the spec gives a different behavior from GetOwnProperty ! */












/* Load all the required modules for module 'm' */


/* Create the <eval> function associated with the module */


/* must be done before js_link_module() because of cyclic references */



/* Prepare a module to be executed by resolving all the imported
   variables. */


/* Prepare a module to be executed by resolving all the imported
   variables. */


/* return JS_ATOM_NULL if the name cannot be found. Only works with
   not striped bytecode functions. */
















/* Return a promise or an exception in case of memory error. Used by
   os.Worker() */










/* XXX: slow. Could use a linked list instead of ExecModuleList */








#ifdef DUMP_MODULE_EXEC

#endif







/* return < 0 in case of exception. *pvalue contains the exception. */


/* spec: InnerModuleEvaluation. Return (index, JS_UNDEFINED) or (-1,
   exception) */


/* Run the <eval> function of the module and of all its requested
   modules. Return a promise or an exception. */




/* return the module index in m->req_module_entries[] or < 0 if error */


















#ifdef DUMP_BYTECODE









#endif





/* 'fd' must be a parent of 's'. Create in 's' a closure referencing
   another one in 'fd' */














/* test if 'var_name' is in the variable object on the stack. If is it
   the case, handle it and jump to 'label_done' */




/* return the position of the next opcode or -1 if error */


/* search in all scopes */






/* return 0 if OK or -1 if the private field could not be resolved */




/* XXX: should handle the argument scope generically */






/* for direct eval compilation: add references to the variables of the
   calling function */




#define M2(op1, op2)            ((op1) | ((op2) << 8))
#define M3(op1, op2, op3)       ((op1) | ((op2) << 8) | ((op3) << 16))
#define M4(op1, op2, op3, op4)  ((op1) | ((op2) << 8) | ((op3) << 16) | ((op4) << 24))









/* convert global variable accesses to local variables or closure
   variables when necessary */


/* the pc2line table gives a source position for each PC value */


/* XXX: could use a more compact storage */
/* XXX: get_line_col_cached() is slow. For more predictable
   performance, line/cols could be stored every N source
   bytes. Alternatively, get_line_col_cached() could be issued in
   emit_source_pos() so that the deltas are more likely to be
   small. */






/* return the target label, following the OP_goto jumps
   the first opcode at destination is stored in *pop
 */






/* peephole optimizations and resolve goto/labels */


/* compute the maximum stack size needed by the function */



/* 'op' is only used for error indication */








/* create a function object from a function definition. The function
   definition is freed. All the child functions are also created. It
   must be done this way to resolve all the variables. */






/* return TRUE if the keyword is forbidden only in strict mode */




/* create a function to initialize class fields */


/* func_name must be JS_ATOM_NULL for JS_PARSE_FUNC_STATEMENT and
   JS_PARSE_FUNC_EXPR, JS_PARSE_FUNC_ARROW and JS_PARSE_FUNC_VAR */












/* 'input' must be zero terminated i.e. input[input_len] = '\0'. */


/* the indirection is needed to make 'eval' optional */










/*******************************************************************/
/* object list */



/* XXX: reuse it to optimize weak references */








/* the reference count of 'obj' is not modified. Return 0 if OK, -1 if
   memory error */


/* return -1 if not present or the object index */




/*******************************************************************/
/* binary object writer & reader */



#define BC_VERSION 5



#ifdef DUMP_READ_OBJECT

#endif



































/* XXX: be compatible with the structured clone algorithm */












/* create the atom table */








#ifdef DUMP_READ_OBJECT

#else
#define bc_read_trace(...)
#endif















/* XXX: used to read an `int` with a positive value */


















































/*******************************************************************/
/* runtime functions & objects */























/* Note: 'func_obj' is not necessarily a constructor */


/* return 0 if OK, -1 if exception */


#define JS_NEW_CTOR_NO_GLOBAL   (1 << 0) /* don't create a global binding */
#define JS_NEW_CTOR_PROTO_CLASS (1 << 1) /* the prototype class is 'class_id' instead of JS_CLASS_OBJECT */
#define JS_NEW_CTOR_PROTO_EXIST (1 << 2) /* the prototype is already defined */
#define JS_NEW_CTOR_READONLY    (1 << 3) /* read-only constructor field */

/* Return the constructor and. Define it as a global variable unless
   JS_NEW_CTOR_NO_GLOBAL is set. The new class inherit from
   parent_ctor if it is not JS_UNDEFINED. if class_id is != -1,
   class_proto[class_id] is set. */








/* Object class */



















/* magic = 1 if called as Reflect.defineProperty */




/* magic = 1 if called as __defineSetter__ */






















































/* Function class */



/* XXX: add a specific eval mode so that Function("}), ({") is rejected */








/* XXX: should use ValueArray */


/* magic value: 0 = normal apply, 1 = apply for constructor, 2 =
   Reflect.apply */












/* Error class */









/* 2 entries for each native error class */
/* Note: we use an atom to avoid the autoinit definition which does
   not work in get_prop_string() */






/* AggregateError */

/* used by C code. */


/* Array */













/* XXX: optimize */




/* len must be >= 0 */












#define special_every    0
#define special_some     1
#define special_forEach  2
#define special_map      3
#define special_filter   4
#define special_TA       8





#define special_reduce       0
#define special_reduceRight  1

























// Note: a.toReversed() is a.slice().reverse() with the twist that a.slice()
// leaves holes in sparse arrays intact whereas a.toReversed() replaces them
// with undefined, thus in effect creating a dense array.
// Does not use Array[@@species], always returns a base Array.














/* Array sort */









// Note: a.toSorted() is a.slice().sort() with the twist that a.slice()
// leaves holes in sparse arrays intact whereas a.toSorted() replaces them
// with undefined, thus in effect creating a dense array.
// Does not use Array[@@species], always returns a base Array.












/* Iterator Wrap */











/* Iterator */





// note: deliberately doesn't use space-saving bit fields for
// |index|, |count| and |running| because tcc miscompiles them


































/* Iterator Helper */



















/* Number */



#if 0



#endif































/* Boolean */










/* String */



















/* only used in test262 */


#if 0

#endif

















/* return the position of the first invalid character in the string or
   -1 if none */








/* return < 0 if exception or TRUE/FALSE */








/* if captures != NULL, captures_val and matched are ignored. Otherwise,
   captures_len is ignored */


















/* return 0 if before the first char */






#ifdef CONFIG_ALL_UNICODE

/* return (-1, NULL) if exception, otherwise (len, buf) */








/* return < 0, 0 or > 0 */



#else /* CONFIG_ALL_UNICODE */

#endif /* !CONFIG_ALL_UNICODE */

/* also used for String.prototype.valueOf */


/* String Iterator */



/* ES6 Annex B 2.3.2 etc. */














/* Math */

/* precondition: a and b are not NaN */


/* precondition: a and b are not NaN */




















#define SP_LIMB_BITS 56
#define SP_RND_BITS (SP_LIMB_BITS - 53)
/* we add one extra limb to avoid having to test for overflows during the sum */
#define SUM_PRECISE_ACC_LEN 39

#define SUM_PRECISE_COUNTER_INIT 250













/* xorshift* random number generator by Marsaglia */










/* Date */

/* OS dependent. d = argv[0] is in ms from 1970. Return the difference
   between UTC time and local time 'd' in minutes */


#if 0




/* create a new date object */

#endif

/* RegExp */



/* create a string containing the RegExp bytecode */


/* fast regexp creation */


/* set the RegExp fields */




/* return < 0 if exception or TRUE/FALSE */










#define RE_FLAG_COUNT 8













/* this_val must be of JS_CLASS_REGEXP */


/* this_val must be of JS_CLASS_REGEXP */




/* XXX: add group names support */


























/* find in 'p' or its prototypes */






















/* JSON */





    


















/* 'pr' can be NULL */








/* if pr != NULL, then pr->value = holder by construction */































/* Reflect */





















/* Proxy */

























/* return FALSE if not OK */








/* return the index of the property or -1 if not found */








/* `js_resolve_proxy`: resolve the proxy chain
   `*pval` is updated with to ultimate proxy target
   `throw_exception` controls whether exceptions are thown or not
   - return -1 in case of error
   - otherwise return 0
 */


















/* Symbol */



















/* Set/Map/WeakSet/WeakMap */



/* JS_UNDEFINED is considered as a live weakref */
/* XXX: add a specific JSWeakRef value type ? */


/* 'val' can be JS_UNDEFINED */


/* val must be an object, a symbol or undefined (see
   js_weakref_is_target). */


#define MAGIC_SET (1 << 0)
#define MAGIC_WEAK (1 << 1)



/* XXX: could normalize strings to speed up comparison */




/* hash multipliers, same as the Linux kernel (see Knuth vol 3,
   section 6.4, exercise 9) */
#define HASH_MUL32 0x61C88647
#define HASH_MUL64 UINT64_C(0x61C8864680B583EB)







/* XXX: better hash ? */
/* precondition: 1 <= hash_bits <= 32 */










/* warning: the record must be removed from the hash table before */










/* return JS_TRUE or JS_FALSE */




















/* Map Iterator */













/* copy 'this_val' in a new set without side effects */




































/* Generator */




/* Promise */



























































#define PROMISE_MAGIC_all        0
#define PROMISE_MAGIC_allSettled 1
#define PROMISE_MAGIC_any        2



/* magic = 0: Promise.all 1: Promise.allSettled */






















/* AsyncFunction */


/* AsyncIteratorPrototype */



/* AsyncFromSyncIteratorPrototype */





















/* AsyncGeneratorFunction */



/* AsyncGenerator prototype */







/* URI handling */























/* global object */



/* Date */















/* return the year, update days */










/* The spec mandates the use of 'double' and it specifies the order
   of the operations */








/* fmt:
   0: toUTCString: "Tue, 02 Jan 2018 23:04:46 GMT"
   1: toString: "Wed Jan 03 2018 00:05:22 GMT+0100 (CET)"
   2: toISOString: "2018-01-02T23:02:56.927Z"
   3: toLocaleString: "1/2/2018, 11:40:40 PM"
   part: 1=date, 2=time 3=all
   XXX: should use a variant of strftime().
 */


/* OS dependent: return the UTC time in ms since 1970. */






/* Date string parsing */



/* skip spaces, update offset, return next char */


/* skip dashes dots and commas */


/* skip a word, stop on spaces, digits and separators, update offset */


/* parse a numeric field (max_digits = 0 -> no maximum) */














/* parse toISOString format */






/* parse toString, toUTCString and other formats */


























/* eval */



/* BigInt */



















/* Minimum amount of objects to be able to compile code and display
   error messages. */




/* Typed Arrays */













/* create a new ArrayBuffer of length 'len' and copy 'buf' to it */








/* also used for SharedArrayBuffer */










// #sec-get-arraybuffer.prototype.detached












/* get an ArrayBuffer or SharedArrayBuffer */


/* return NULL if exception. WARNING: any JS call can detach the
   buffer and render the returned pointer invalid */




// ES #sec-arraybuffer.prototype.transfer








/* SharedArrayBuffer */







// is the typed array detached or out of bounds relative to its RAB?
// |p| must be a typed array, *not* a DataView


// Be *very* careful if you touch the typed array's memory directly:
// the length is only valid until the next call into JS land because
// JS code can detach or resize the backing array buffer. Functions
// like JS_GetProperty and JS_ToIndex call JS code.
//
// Exclusively reading or writing elements with JS_GetProperty,
// JS_GetPropertyInt64, JS_SetProperty, etc. is safe because they
// perform bounds checks, as does js_get_fast_array_element.














/* Return the buffer associated to the typed array or an exception if
   it is not a typed array or if the buffer is detached. pbyte_offset,
   pbyte_length or pbytes_per_element can be NULL. */
















#if 0

#endif













#define special_indexOf 0
#define special_lastIndexOf 1
#define special_includes -1















/* TypedArray.prototype.sort */























































/* Uint8Array base64/hex (tc39 proposal-arraybuffer-base64) */









#define K_WS 64
#define K_ER 65




 




/* Implements the FromBase64 abstract operation.
   src/src_len: the input string (must be ASCII/latin1)
   dst/max_len: output buffer
   flags: b64_flags or b64_flags_url (selects valid characters)
   last_chunk: B64_LAST_LOOSE, B64_LAST_STRICT, or B64_LAST_STOP_BEFORE_PARTIAL
   *p_read: set to number of input characters consumed
   *p_err: set to 1 on error, 0 on success
   Returns: number of bytes written to dst */


/* Hex helpers */




/* Decode hex string to bytes.
   Returns bytes written. Sets *p_read to chars consumed, *p_err on error. */




/* Validate that this_val is a Uint8Array (type check only, no detach check).
   Returns the JSObject pointer or NULL on error (throws). */


/* Get the data pointer and length of a Uint8Array, checking for detached
   buffers. Must be called after options are read (per spec ordering).
   Returns 0 on success, -1 on error (throws). */


/* Validate options is undefined or an object (GetOptionsObject).
   Returns 0 on success, -1 on error (throws). */


/* Parse the 'alphabet' option from an options object.
   Returns B64_ALPHABET_BASE64 or B64_ALPHABET_BASE64URL, or -1 on error. */


/* Parse the 'lastChunkHandling' option. Returns mode or -1 on error. */


/* Uint8Array.prototype.toBase64([options]) */


/* Uint8Array.prototype.toHex() */


/* Uint8Array.fromBase64(string[, options]) */


/* Uint8Array.fromHex(string) */


/* Return a { read, written } result object */


/* Uint8Array.prototype.setFromBase64(string[, options]) */


/* Uint8Array.prototype.setFromHex(string) */














/* 'obj' must be an allocated typed array object */

















// is the DataView out of bounds relative to its parent arraybuffer?
















/* Atomics */
#ifdef CONFIG_ATOMICS
















#if defined(__aarch64__)

#elif defined(__x86_64) || defined(__i386__)

#else

#endif

// no-op: Atomics.pause() is not allowed to block or yield to another
// thread, only to hint the CPU that it should back off for a bit;
// the amount of work we do here is a good enough substitute












#endif /* CONFIG_ATOMICS */



/* WeakRef */







































/*
 * QuickJS Runtime and Context definitions
 */
#ifndef QUICKJS_RUNTIME_H
#define QUICKJS_RUNTIME_H

#include "quickjs/def.h"
#include "quickjs/shape.h"
#include "quickjs/object.h"
#include "quickjs/function.h"
#include "quickjs/module.h"

struct JSClass {
    uint32_t class_id; /* 0 means free entry */
    JSAtom class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
    JSClassCall *call;
    const JSClassExoticMethods *exotic;
};

struct JSRuntime {
    JSMallocContext malloc_ctx;
    const char *rt_info;

    int atom_hash_size; /* power of two */
    int atom_count;
    int atom_size;
    int atom_count_resize; /* resize hash table at this count */
    uint32_t *atom_hash;
    JSAtomStruct **atom_array;
    int atom_free_index; /* 0 = none */

    int class_count;    /* size of class_array */
    JSClass *class_array;

    struct list_head context_list; /* list of JSContext.link */
    struct list_head gc_obj_list;
    struct list_head gc_zero_ref_count_list;
    struct list_head tmp_obj_list; /* used during GC */
    JSGCPhaseEnum gc_phase : 8;
    size_t malloc_gc_threshold;
    struct list_head weakref_list; /* list of JSWeakRefHeader.link */
#ifdef DUMP_LEAKS
    struct list_head string_list; /* list of JSString.link */
#endif
    /* stack limitation */
    uintptr_t stack_size; /* in bytes, 0 if no limit */
    uintptr_t stack_top;
    uintptr_t stack_limit; /* lower stack limit */

    JSValue current_exception;
    BOOL current_exception_is_uncatchable : 8;
    BOOL in_out_of_memory : 8;

    struct JSStackFrame *current_stack_frame;

    JSInterruptHandler *interrupt_handler;
    void *interrupt_opaque;

    JSHostPromiseRejectionTracker *host_promise_rejection_tracker;
    void *host_promise_rejection_tracker_opaque;

    struct list_head job_list; /* list of JSJobEntry.link */

    JSModuleNormalizeFunc *module_normalize_func;
    BOOL module_loader_has_attr;
    union {
        JSModuleLoaderFunc *module_loader_func;
        JSModuleLoaderFunc2 *module_loader_func2;
    } u;
    JSModuleCheckSupportedImportAttributes *module_check_attrs;
    void *module_loader_opaque;
    int64_t module_async_evaluation_next_timestamp;

    BOOL can_block : 8; /* TRUE if Atomics.wait can block */
    JSSharedArrayBufferFunctions sab_funcs;
    uint8_t strip_flags;
    
    /* Shape hash table */
    int shape_hash_bits;
    int shape_hash_size;
    int shape_hash_count; /* number of hashed shapes */
    JSShape **shape_hash;
    void *user_opaque;
};

struct JSContext {
    JSGCObjectHeader header; /* must come first */
    JSRuntime *rt;
    struct list_head link;

    uint16_t binary_object_count;
    int binary_object_size;
    
    JSShape *array_shape;
    JSShape *arguments_shape;
    JSShape *mapped_arguments_shape;
    JSShape *regexp_shape;
    JSShape *regexp_result_shape;

    JSValue *class_proto;
    JSValue function_proto;
    JSValue function_ctor;
    JSValue array_ctor;
    JSValue regexp_ctor;
    JSValue promise_ctor;
    JSValue native_error_proto[JS_NATIVE_ERROR_COUNT];
    JSValue iterator_ctor;
    JSValue async_iterator_proto;
    JSValue array_proto_values;
    JSValue throw_type_error;
    JSValue eval_obj;

    JSValue global_obj;
    JSValue global_var_obj;

    uint64_t random_state;

    int interrupt_counter;

    struct list_head loaded_modules;

    JSValue (*compile_regexp)(JSContext *ctx, JSValueConst pattern,
                              JSValueConst flags);
    JSValue (*eval_internal)(JSContext *ctx, JSValueConst this_obj,
                             const char *input, size_t input_len,
                             const char *filename, int flags, int scope_idx);
    void *user_opaque;
};

struct JSJobEntry {
    struct list_head link;
    JSContext *realm;
    JSJobFunc *job_func;
    int argc;
    JSValue argv[0];
};

#if !defined(CONFIG_STACK_CHECK)
static inline uintptr_t js_get_stack_pointer(void)
{
    return 0;
}
static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    return FALSE;
}
#else
static inline uintptr_t js_get_stack_pointer(void)
{
    return (uintptr_t)__builtin_frame_address(0);
}
static inline BOOL js_check_stack_overflow(JSRuntime *rt, size_t alloca_size)
{
    uintptr_t sp;
    sp = js_get_stack_pointer() - alloca_size;
    return unlikely(sp < rt->stack_limit);
}
#endif

static inline BOOL is_strict_mode(JSContext *ctx)
{
    JSStackFrame *sf = ctx->rt->current_stack_frame;
    return (sf && (sf->js_mode & JS_MODE_STRICT));
}

static inline void js_dbuf_init(JSContext *ctx, DynBuf *s)
{
    dbuf_init2(s, ctx->rt, (DynBufReallocFunc *)js_realloc_rt);
}

static inline int js_realloc_array(JSContext *ctx, void **parray,
                                  int elem_size, int *psize, int req_size)
{
    int new_size;
    size_t slack;
    void *new_array;
    new_size = max_int(req_size, *psize * 3 / 2);
    new_array = js_realloc2(ctx, *parray, new_size * elem_size, &slack);
    if (!new_array)
        return -1;
    new_size += slack / elem_size;
    *psize = new_size;
    *parray = new_array;
    return 0;
}

static inline int js_resize_array(JSContext *ctx, void **parray, int elem_size,
                                  int *psize, int req_size)
{
    if (unlikely(req_size > *psize))
        return js_realloc_array(ctx, parray, elem_size, psize, req_size);
    else
        return 0;
}

void add_gc_object(JSRuntime *rt, JSGCObjectHeader *h, JSGCObjectTypeEnum type);
JSValue JS_ThrowStackOverflow(JSContext *ctx);

#define JS_BACKTRACE_FLAG_SKIP_FIRST_LEVEL (1 << 0)
void build_backtrace(JSContext *ctx, JSValueConst error_obj,
                     const char *filename, int line_num,
                     int col_num, int flags);
JSValue JS_ThrowError2(JSContext *ctx, JSErrorEnum error_num,
                       const char *fmt, va_list ap, BOOL add_backtrace);

static inline JSValue JS_CallFree(JSContext *ctx, JSValue func_obj, JSValueConst this_obj,
                                  int argc, JSValueConst *argv)
{
    JSValue res = JS_Call(ctx, func_obj, this_obj, argc, argv);
    JS_FreeValue(ctx, func_obj);
    return res;
}

static inline void remove_gc_object(JSGCObjectHeader *h)
{
    list_del(&h->link);
}

static inline JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowSyntaxErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowSyntaxError(ctx, fmt,
                             JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}
#define JS_ThrowSyntaxErrorAtom(ctx, fmt, atom) __JS_ThrowSyntaxErrorAtom(ctx, atom, fmt, "")

static inline JSValue __attribute__((format(printf, 3, 4))) __JS_ThrowTypeErrorAtom(JSContext *ctx, JSAtom atom, const char *fmt, ...)
{
    char buf[ATOM_GET_STR_BUF_SIZE];
    return JS_ThrowTypeError(ctx, fmt,
                             JS_AtomGetStr(ctx, buf, sizeof(buf), atom));
}
#define JS_ThrowTypeErrorAtom(ctx, fmt, atom) __JS_ThrowTypeErrorAtom(ctx, atom, fmt, "")

void js_dump_value_write(void *opaque, const char *buf, size_t len);
int __attribute__((format(printf, 3, 4))) JS_ThrowTypeErrorOrFalse(JSContext *ctx, int flags, const char *fmt, ...);

typedef struct JSClassShortDef {
    JSAtom class_name;
    JSClassFinalizer *finalizer;
    JSClassGCMark *gc_mark;
} JSClassShortDef;

int init_class_range(JSRuntime *rt, JSClassShortDef const *tab, int start, int count);
int JS_EnqueueJob2(JSContext *ctx, JSJobFunc *job_func, int argc, JSValueConst *argv, BOOL no_exception);
JSValueConst JS_GetActiveFunction(JSContext *ctx);
JSContext *JS_GetFunctionRealm(JSContext *ctx, JSValueConst func_obj);
JSValue JS_NewCFunction3(JSContext *ctx, JSCFunction *func, const char *name, int length, JSCFunctionEnum cproto, int magic, JSValueConst proto_val, int n_fields);
JSValue JS_NewObjectProtoClassAlloc(JSContext *ctx, JSValueConst proto_val, JSClassID class_id, int n_alloc_props);
JSValue JS_NewSymbol(JSContext *ctx, JSString *p, int atom_type);
JSValue JS_ThrowError(JSContext *ctx, JSErrorEnum error_num, const char *fmt, va_list ap);
void JS_ThrowInterrupted(JSContext *ctx);
JSValue JS_ThrowTypeErrorInvalidClass(JSContext *ctx, int class_id);
JSValue JS_ThrowTypeErrorNotAConstructor(JSContext *ctx, JSValueConst func_obj);
JSValue JS_ThrowTypeErrorNotAnObject(JSContext *ctx);
void set_cycle_flag(JSContext *ctx, JSValueConst obj);
void free_zero_refcount(JSRuntime *rt);
int js_poll_interrupts(JSContext *ctx);
JSValue js_create_from_ctor(JSContext *ctx, JSValueConst ctor, int class_id);

#endif /* QUICKJS_RUNTIME_H */

/* C host coverage for two realms, GC retention, and raw String intrinsics.
   Compile/link with the CONFIG_INTL engine and its ICU dependencies. */
#include "quickjs.h"
#include <stdio.h>
#include <string.h>

static int evaluate(JSContext *ctx, const char *source, JSValue *result)
{
    *result = JS_Eval(ctx, source, strlen(source), "intl-native-host-test",
                      JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(*result)) {
        JSValue exception = JS_GetException(ctx);
        const char *message = JS_ToCString(ctx, exception);
        fprintf(stderr, "Intl host test: %s\n", message ? message : "exception");
        JS_FreeCString(ctx, message);
        JS_FreeValue(ctx, exception);
        return -1;
    }
    return 0;
}

static int run(JSContext *ctx, const char *source)
{
    JSValue result;
    if (evaluate(ctx, source, &result) < 0)
        return -1;
    JS_FreeValue(ctx, result);
    return 0;
}

static int transfer(JSContext *from, JSContext *to,
                     const char *source, const char *name)
{
    JSValue value, global;
    int result;
    if (evaluate(from, source, &value) < 0)
        return -1;
    global = JS_GetGlobalObject(to);
    result = JS_DefinePropertyValueStr(to, global, name, value, JS_PROP_C_W_E);
    JS_FreeValue(to, global);
    return result < 0 ? -1 : 0;
}

int main(void)
{
    JSRuntime *rt = NULL;
    JSContext *a = NULL, *b = NULL, *raw = NULL, *raw_target_context = NULL;
    int result = 1;
    rt = JS_NewRuntime();
    if (!rt)
        goto done;
    a = JS_NewContext(rt);
    b = JS_NewContext(rt);
    if (!a || !b)
        goto done;
    if (transfer(a, b, "new Intl.Collator('en').compare", "foreignCompare") < 0 ||
        transfer(a, b, "TypeError", "foreignTypeError") < 0 ||
        transfer(a, b, "Function.prototype", "foreignFunctionPrototype") < 0 ||
        transfer(a, b, "Object.prototype", "foreignObjectPrototype") < 0 ||
        transfer(a, b,
            "new Intl.Segmenter('en').segment('A\\ud83d\\ude00e\\u0301')"
            "[Symbol.iterator]()", "foreignIterator") < 0)
        goto done;
    JS_RunGC(rt);
    if (run(b,
        "if (Object.getPrototypeOf(foreignCompare) !== foreignFunctionPrototype)"
        " throw Error('compare function prototype realm');"
        "try { foreignCompare(Symbol(), 'x'); throw Error('missing error'); }"
        "catch (e) { if (!(e instanceof foreignTypeError)) throw e; }"
        "if (foreignCompare('e\\u0301', '\\u00e9') !== 0)"
        " throw Error('canonical equivalence');"
        "let first = foreignIterator.next();"
        "if (first.value.segment !== 'A' ||"
        " Object.getPrototypeOf(first.value) !== foreignObjectPrototype)"
        " throw Error('segment data realm');") < 0)
        goto done;
    /* Host drops the originating context. Retained built-in realms and slots
       must keep cross-realm callbacks/segmentation valid through collection. */
    JS_FreeContext(a);
    a = NULL;
    JS_RunGC(rt);
    if (run(b,
        "if (foreignIterator.next().value.index !== 1)"
        " throw Error('iterator lifetime');"
        "if (foreignCompare('a', 'b') >= 0) throw Error('compare lifetime');"
        "delete globalThis.foreignCompare; delete globalThis.foreignIterator;"
        "delete globalThis.foreignTypeError;"
        "delete globalThis.foreignFunctionPrototype;"
        "delete globalThis.foreignObjectPrototype;") < 0)
        goto done;
    JS_RunGC(rt);

    raw = JS_NewContextRaw(rt);
    if (!raw || JS_AddIntrinsicBaseObjects(raw) < 0 ||
        JS_AddIntrinsicEval(raw) < 0 ||
        JS_AddIntrinsicStringNormalize(raw) < 0)
        goto done;
    if (run(raw,
        "if (typeof Intl !== 'undefined') throw Error('raw global Intl exposed');"
        "if ('2'.localeCompare('10', 'en', {numeric:true}) >= 0)"
        " throw Error('raw localeCompare service');"
        "if (typeof Intl !== 'undefined') throw Error('lazy service exposed Intl');") < 0)
        goto done;
    raw_target_context = JS_NewContextRaw(rt);
    if (!raw_target_context || JS_AddIntrinsicBaseObjects(raw_target_context) < 0 ||
        JS_AddIntrinsicEval(raw_target_context) < 0)
        goto done;
    if (transfer(raw_target_context, b,
        "(() => { function Target() {} Target.prototype = 0; return Target; })()",
        "rawTarget") < 0 ||
        transfer(raw_target_context, b, "Object.prototype", "rawObjectPrototype") < 0 ||
        run(b,
        "let rawInstance = Reflect.construct(Intl.Collator, ['en'], rawTarget);"
        "if (Object.getPrototypeOf(Object.getPrototypeOf(rawInstance)) !=="
        " rawObjectPrototype) throw Error('raw constructor fallback realm');"
        "if (rawInstance.compare('a', 'b') >= 0) throw Error('raw brand');"
        "let rawSegmenter = Reflect.construct(Intl.Segmenter, ['en'], rawTarget);"
        "globalThis.rawSegmentsPrototype ="
        " Object.getPrototypeOf(rawSegmenter.segment('ab'));"
        "globalThis.rawIteratorPrototype = Object.getPrototypeOf("
        " rawSegmenter.segment('ab')[Symbol.iterator]());"
        "delete globalThis.rawTarget; delete globalThis.rawObjectPrototype;") < 0 ||
        run(raw_target_context,
        "if (typeof Intl !== 'undefined') throw Error('raw fallback exposed Intl');") < 0)
        goto done;
    if (JS_AddIntrinsicIntl(raw_target_context) < 0 ||
        transfer(raw_target_context, b,
            "Object.getPrototypeOf(new Intl.Segmenter('en').segment('ab'))",
            "publishedSegmentsPrototype") < 0 ||
        transfer(raw_target_context, b,
            "Object.getPrototypeOf(new Intl.Segmenter('en').segment('ab')"
            "[Symbol.iterator]())", "publishedIteratorPrototype") < 0 ||
        run(b,
            "if (rawSegmentsPrototype !== publishedSegmentsPrototype ||"
            " rawIteratorPrototype !== publishedIteratorPrototype)"
            " throw Error('private prototype identity after publication');"
            "delete globalThis.rawSegmentsPrototype;"
            "delete globalThis.rawIteratorPrototype;"
            "delete globalThis.publishedSegmentsPrototype;"
            "delete globalThis.publishedIteratorPrototype;") < 0)
        goto done;
    result = 0;
 done:
    if (raw_target_context)
        JS_FreeContext(raw_target_context);
    if (raw)
        JS_FreeContext(raw);
    if (b)
        JS_FreeContext(b);
    if (a)
        JS_FreeContext(a);
    if (rt) {
        JS_RunGC(rt);
        JS_FreeRuntime(rt);
    }
    return result;
}

/*
 * QuickJS C library
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
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
#include "internal.h"

typedef JSModuleDef *(JSInitModuleFunc)(JSContext *ctx,
                                        const char *module_name);


#if defined(_WIN32)
static JSModuleDef *js_module_loader_so(JSContext *ctx,
                                        const char *module_name)
{
    JS_ThrowReferenceError(ctx, "shared library modules are not supported yet");
    return NULL;
}
#else
static JSModuleDef *js_module_loader_so(JSContext *ctx,
                                        const char *module_name)
{
    JSModuleDef *m;
    void *hd;
    JSInitModuleFunc *init;
    char *filename;

    if (!strchr(module_name, '/')) {
        /* must add a '/' so that the DLL is not searched in the
           system library paths */
        filename = js_malloc(ctx, strlen(module_name) + 2 + 1);
        if (!filename)
            return NULL;
        strcpy(filename, "./");
        strcpy(filename + 2, module_name);
    } else {
        filename = (char *)module_name;
    }

    /* C module */
    hd = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    if (filename != module_name)
        js_free(ctx, filename);
    if (!hd) {
        JS_ThrowReferenceError(ctx, "could not load module filename '%s' as shared library",
                               module_name);
        goto fail;
    }

    init = dlsym(hd, "js_init_module");
    if (!init) {
        JS_ThrowReferenceError(ctx, "could not load module filename '%s': js_init_module not found",
                               module_name);
        goto fail;
    }

    m = init(ctx, module_name);
    if (!m) {
        JS_ThrowReferenceError(ctx, "could not load module filename '%s': initialization error",
                               module_name);
    fail:
        if (hd)
            dlclose(hd);
        return NULL;
    }
    return m;
}
#endif /* !_WIN32 */

int js_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
                              JS_BOOL use_realpath, JS_BOOL is_main)
{
    JSModuleDef *m;
    char buf[PATH_MAX + 16];
    JSValue meta_obj;
    JSAtom module_name_atom;
    const char *module_name;

    assert(JS_VALUE_GET_TAG(func_val) == JS_TAG_MODULE);
    m = JS_VALUE_GET_PTR(func_val);

    module_name_atom = JS_GetModuleName(ctx, m);
    module_name = JS_AtomToCString(ctx, module_name_atom);
    JS_FreeAtom(ctx, module_name_atom);
    if (!module_name)
        return -1;
    if (!strchr(module_name, ':')) {
        strcpy(buf, "file://");
#if !defined(_WIN32)
        /* realpath() cannot be used with modules compiled with qjsc
           because the corresponding module source code is not
           necessarily present */
        if (use_realpath) {
            char *res = realpath(module_name, buf + strlen(buf));
            if (!res) {
                JS_ThrowTypeError(ctx, "realpath failure");
                JS_FreeCString(ctx, module_name);
                return -1;
            }
        } else
#endif
        {
            pstrcat(buf, sizeof(buf), module_name);
        }
    } else {
        pstrcpy(buf, sizeof(buf), module_name);
    }
    JS_FreeCString(ctx, module_name);

    meta_obj = JS_GetImportMeta(ctx, m);
    if (JS_IsException(meta_obj))
        return -1;
    JS_DefinePropertyValueStr(ctx, meta_obj, "url",
                              JS_NewString(ctx, buf),
                              JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, meta_obj, "main",
                              JS_NewBool(ctx, is_main),
                              JS_PROP_C_W_E);
    JS_FreeValue(ctx, meta_obj);
    return 0;
}

static int json_module_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue val;
    val = JS_GetModulePrivateValue(ctx, m);
    JS_SetModuleExport(ctx, m, "default", val);
    return 0;
}

static JSModuleDef *create_json_module(JSContext *ctx, const char *module_name, JSValue val)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, json_module_init);
    if (!m) {
        JS_FreeValue(ctx, val);
        return NULL;
    }
    /* only export the "default" symbol which will contain the JSON object */
    JS_AddModuleExport(ctx, m, "default");
    JS_SetModulePrivateValue(ctx, m, val);
    return m;
}

/* in order to conform with the specification, only the keys should be
   tested and not the associated values. */
int js_module_check_attributes(JSContext *ctx, void *opaque,
                               JSValueConst attributes)
{
    JSPropertyEnum *tab;
    uint32_t i, len;
    int ret;
    const char *cstr;
    size_t cstr_len;

    if (JS_GetOwnPropertyNames(ctx, &tab, &len, attributes, JS_GPN_ENUM_ONLY | JS_GPN_STRING_MASK))
        return -1;
    ret = 0;
    for(i = 0; i < len; i++) {
        cstr = JS_AtomToCStringLen(ctx, &cstr_len, tab[i].atom);
        if (!cstr) {
            ret = -1;
            break;
        }
        if (!(cstr_len == 4 && !memcmp(cstr, "type", cstr_len))) {
            JS_ThrowTypeError(ctx, "import attribute '%s' is not supported", cstr);
            ret = -1;
        }
        JS_FreeCString(ctx, cstr);
        if (ret)
            break;
    }
    JS_FreePropertyEnum(ctx, tab, len);
    return ret;
}

/* return > 0 if the attributes indicate a JSON module */
int js_module_test_json(JSContext *ctx, JSValueConst attributes)
{
    JSValue str;
    const char *cstr;
    size_t len;
    BOOL res;

    if (JS_IsUndefined(attributes))
        return FALSE;
    str = JS_GetPropertyStr(ctx, attributes, "type");
    if (!JS_IsString(str))
        return FALSE;
    cstr = JS_ToCStringLen(ctx, &len, str);
    JS_FreeValue(ctx, str);
    if (!cstr)
        return FALSE;
    /* XXX: raise an error if unknown type ? */
    if (len == 4 && !memcmp(cstr, "json", len)) {
        res = 1;
    } else if (len == 5 && !memcmp(cstr, "json5", len)) {
        res = 2;
    } else {
        res = 0;
    }
    JS_FreeCString(ctx, cstr);
    return res;
}

JSModuleDef *js_module_loader(JSContext *ctx,
                              const char *module_name, void *opaque,
                              JSValueConst attributes)
{
    JSModuleDef *m;
    int res;

    if (has_suffix(module_name, ".so")) {
        m = js_module_loader_so(ctx, module_name);
    } else {
        size_t buf_len;
        uint8_t *buf;

        buf = js_load_file(ctx, &buf_len, module_name);
        if (!buf) {
            JS_ThrowReferenceError(ctx, "could not load module filename '%s'",
                                   module_name);
            return NULL;
        }
        res = js_module_test_json(ctx, attributes);
        if (has_suffix(module_name, ".json") || res > 0) {
            /* compile as JSON or JSON5 depending on "type" */
            JSValue val;
            int flags;
            if (res == 2)
                flags = JS_PARSE_JSON_EXT;
            else
                flags = 0;
            val = JS_ParseJSON2(ctx, (char *)buf, buf_len, module_name, flags);
            js_free(ctx, buf);
            if (JS_IsException(val))
                return NULL;
            m = create_json_module(ctx, module_name, val);
            if (!m)
                return NULL;
        } else {
            JSValue func_val;
            /* compile the module */
            func_val = JS_Eval(ctx, (char *)buf, buf_len, module_name,
                               JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
            js_free(ctx, buf);
            if (JS_IsException(func_val))
                return NULL;
            /* XXX: could propagate the exception */
            js_module_set_import_meta(ctx, func_val, TRUE, FALSE);
            /* the module is already referenced, so we must free it */
            m = JS_VALUE_GET_PTR(func_val);
            JS_FreeValue(ctx, func_val);
        }
    }
    return m;
}
void js_std_eval_binary(JSContext *ctx, const uint8_t *buf, size_t buf_len,
                        int load_only)
{
    JSValue obj, val;
    obj = JS_ReadObject(ctx, buf, buf_len, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(obj))
        goto exception;
    if (load_only) {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
            js_module_set_import_meta(ctx, obj, FALSE, FALSE);
        }
        JS_FreeValue(ctx, obj);
    } else {
        if (JS_VALUE_GET_TAG(obj) == JS_TAG_MODULE) {
            if (JS_ResolveModule(ctx, obj) < 0) {
                JS_FreeValue(ctx, obj);
                goto exception;
            }
            js_module_set_import_meta(ctx, obj, FALSE, TRUE);
            val = JS_EvalFunction(ctx, obj);
            val = js_std_await(ctx, val);
        } else {
            val = JS_EvalFunction(ctx, obj);
        }
        if (JS_IsException(val)) {
        exception:
            js_std_dump_error(ctx);
            exit(1);
        }
        JS_FreeValue(ctx, val);
    }
}

void js_std_eval_binary_json_module(JSContext *ctx,
                                    const uint8_t *buf, size_t buf_len,
                                    const char *module_name)
{
    JSValue obj;
    JSModuleDef *m;

    obj = JS_ReadObject(ctx, buf, buf_len, 0);
    if (JS_IsException(obj))
        goto exception;
    m = create_json_module(ctx, module_name, obj);
    if (!m) {
    exception:
        js_std_dump_error(ctx);
        exit(1);
    }
}

/*
 * QuickJS Module definitions
 */
#ifndef QUICKJS_MODULE_H
#define QUICKJS_MODULE_H

#include "quickjs/def.h"

struct JSReqModuleEntry {
    JSAtom module_name;
    JSModuleDef *module;
    JSValue attributes;
};

typedef enum JSExportTypeEnum {
    JS_EXPORT_TYPE_LOCAL,
    JS_EXPORT_TYPE_INDIRECT,
} JSExportTypeEnum;

struct JSExportEntry {
    union {
        struct {
            int var_idx;
            JSVarRef *var_ref;
        } local;
        int req_module_idx;
    } u;
    JSExportTypeEnum export_type;
    JSAtom local_name;
    JSAtom export_name;
};

struct JSStarExportEntry {
    int req_module_idx;
};

struct JSImportEntry {
    int var_idx;
    BOOL is_star;
    JSAtom import_name;
    int req_module_idx;
};

typedef enum {
    JS_MODULE_STATUS_UNLINKED,
    JS_MODULE_STATUS_LINKING,
    JS_MODULE_STATUS_LINKED,
    JS_MODULE_STATUS_EVALUATING,
    JS_MODULE_STATUS_EVALUATING_ASYNC,
    JS_MODULE_STATUS_EVALUATED,
} JSModuleStatus;

struct JSModuleDef {
    JSGCObjectHeader header;
    JSAtom module_name;
    struct list_head link;

    JSReqModuleEntry *req_module_entries;
    int req_module_entries_count;
    int req_module_entries_size;

    JSExportEntry *export_entries;
    int export_entries_count;
    int export_entries_size;

    JSStarExportEntry *star_export_entries;
    int star_export_entries_count;
    int star_export_entries_size;

    JSImportEntry *import_entries;
    int import_entries_count;
    int import_entries_size;

    JSValue module_ns;
    JSValue func_obj;
    JSModuleInitFunc *init_func;
    BOOL has_tla : 8;
    BOOL resolved : 8;
    BOOL func_created : 8;
    JSModuleStatus status : 8;
    int dfs_index, dfs_ancestor_index;
    JSModuleDef *stack_prev;
    JSModuleDef **async_parent_modules;
    int async_parent_modules_count;
    int async_parent_modules_size;
    int pending_async_dependencies;
    BOOL async_evaluation;
    int64_t async_evaluation_timestamp;
    JSModuleDef *cycle_root;
    JSValue promise;
    JSValue resolving_funcs[2];

    BOOL eval_has_exception : 8;
    JSValue eval_exception;
    JSValue meta_obj;
    JSValue private_value;
};

#endif /* QUICKJS_MODULE_H */

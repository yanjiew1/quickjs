/*
 * QuickJS Shape definitions and inlines
 */
#ifndef QUICKJS_SHAPE_H
#define QUICKJS_SHAPE_H

#include "quickjs/def.h"

struct JSShapeProperty {
    uint32_t hash_next : 26; /* 0 if last in list */
    uint32_t flags : 6;      /* JS_PROP_XXX */
    JSAtom atom;             /* JS_ATOM_NULL = free property entry */
};

struct JSShape {
    JSGCObjectHeader header;
    uint8_t is_hashed;
    uint32_t hash;
    uint32_t prop_hash_mask; /* >= 2 */
    int prop_size;           /* allocated properties */
    int prop_count;          /* include deleted properties */
    int deleted_prop_count;
    JSShape *shape_hash_next;
    JSObject *proto;
    uint32_t hash_table[];   /* prop_hash_mask + 1 elements */
    /* followed by JSShapeProperty prop[prop_size]; */
};

static inline size_t get_shape_size(size_t hash_size, size_t prop_size)
{
    return sizeof(JSShape) + hash_size * sizeof(uint32_t) +
        prop_size * sizeof(JSShapeProperty);
}

static inline JSShapeProperty *get_shape_prop(JSShape *sh)
{
    return (JSShapeProperty *)((uint32_t *)(sh + 1) + sh->prop_hash_mask + 1);
}

int add_shape_property(JSContext *ctx, JSShape **psh, JSObject *p, JSAtom atom, int prop_flags);
JSShape *js_dup_shape(JSShape *sh);
JSShape *js_new_shape2(JSContext *ctx, JSObject *proto, int hash_size, int prop_size);
JSValue JS_NewObjectFromShape(JSContext *ctx, JSShape *sh, JSClassID class_id, JSProperty *props);

#endif /* QUICKJS_SHAPE_H */

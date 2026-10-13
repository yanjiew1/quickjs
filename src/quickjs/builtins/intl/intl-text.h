/* Private engine text boundary. No ICU types or provider lifecycle. */
#ifndef QUICKJS_INTL_TEXT_H
#define QUICKJS_INTL_TEXT_H
#include "../../internal/base.h"
#ifdef CONFIG_INTL
/* Outputs use engine allocation, preserve every UTF16 code unit, and include
 * an extra NUL only for backend interoperability. Length excludes that NUL.
 * ToString and rope flattening happen exactly once, as in the old helpers.
 * The caller frees a successful buffer with js_free(ctx, buffer).
 */
uint16_t *js_intl_alloc_utf16(JSContext *ctx, int32_t required);
int js_intl_to_utf16(JSContext *ctx, JSValueConst value,
                     uint16_t **result, int32_t *length);
JSValue js_intl_from_utf16(JSContext *ctx, const uint16_t *value,
                          int32_t length);
#endif
#endif

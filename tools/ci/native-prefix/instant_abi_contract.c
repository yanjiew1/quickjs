/* Compile-only public-header ABI witness; no private types or ICU headers.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include "quickjs.h"

JSValue (*const instant_constructor_contract)(JSContext *, uint64_t,
                                             uint64_t) = JS_NewTemporalInstant;
int (*const instant_getter_contract)(JSContext *, JSValueConst, uint64_t *,
                                     uint64_t *) =
    JS_GetTemporalInstantEpochNanoseconds;

void instant_public_abi_contract_use(void)
{
    (void)instant_constructor_contract;
    (void)instant_getter_contract;
}

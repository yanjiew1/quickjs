/* Public-header C/C++ ABI checks; no conditional API guard.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 */
#include "quickjs.h"
JSValue (*const utf16_new_contract)(JSContext *,const uint16_t *,size_t)=JS_NewStringUTF16;
const uint16_t *(*const utf16_len_contract)(JSContext *,size_t *,JSValueConst)=JS_ToCStringLenUTF16;
const uint16_t *(*const utf16_contract)(JSContext *,JSValueConst)=JS_ToCStringUTF16;
void (*const cstring_rt_contract)(JSRuntime *,const char *)=JS_FreeCStringRT;
void (*const utf16_free_contract)(JSContext *,const uint16_t *)=JS_FreeCStringUTF16;
void (*const utf16_free_rt_contract)(JSRuntime *,const uint16_t *)=JS_FreeCStringRT_UTF16;

void utf16_public_abi_contract_use(void)
{
    (void)utf16_new_contract;
    (void)utf16_len_contract;
    (void)utf16_contract;
    (void)cstring_rt_contract;
    (void)utf16_free_contract;
    (void)utf16_free_rt_contract;
}

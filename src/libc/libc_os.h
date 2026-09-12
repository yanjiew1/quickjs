/*
 * QuickJS C library: OS Subsystem Header
 *
 * Copyright (c) 2017-2021 Fabrice Bellard
 * Copyright (c) 2017-2021 Charlie Gordon
 */
#ifndef QUICKJS_LIBC_OS_H
#define QUICKJS_LIBC_OS_H

#include "libc_internal.h"

JSModuleDef *js_init_module_os(JSContext *ctx, const char *module_name);

#endif /* QUICKJS_LIBC_OS_H */

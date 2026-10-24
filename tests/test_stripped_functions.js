/* Source-stripped functions must not read their observable name property.
 * Copyright (c) 2026 Yan-Jie Wang
 * SPDX-License-Identifier: MIT
 * Run with qjs --strip-source.
 */
import { assert } from "./assert.js";

for (const fn of [
    function named() {},
    function* named() {},
    async function named() {},
    async function* named() {},
]) {
    Object.defineProperty(fn, "name", {
        get() { throw Error("source-stripped name read"); },
    });
    assert(Function.prototype.toString.call(fn),
           "function () {\n    [native code]\n}");
}

/*
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

import * as namespace from "./fixture_module_review_cycle_keys.js";

const ownKeys = Reflect.ownKeys(namespace);
const ownNames = Object.getOwnPropertyNames(namespace);
const proxyOwnKeys = Reflect.ownKeys(new Proxy(namespace, {}));
let keysError, entriesError, forInError;
try {
    Object.keys(namespace);
} catch (e) {
    keysError = e;
}
try {
    Object.entries(namespace);
} catch (e) {
    entriesError = e;
}
try {
    for (const key in namespace) {}
} catch (e) {
    forInError = e;
}
export const snapshot = { ownKeys, ownNames, proxyOwnKeys,
                          keysError, entriesError, forInError };

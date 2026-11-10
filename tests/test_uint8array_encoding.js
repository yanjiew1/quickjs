/*
 * Uint8Array encoding option regressions
 * Copyright (c) 2026 Yan-Jie Wang
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
import { assert, assertThrows } from "./assert.js";

const bytes = new Uint8Array([97]);

for (const alphabet of ["base64\0x", "base64url\0x"]) {
    assertThrows(TypeError, () => Uint8Array.fromBase64("YQ==", { alphabet }));
    assertThrows(TypeError, () => bytes.toBase64({ alphabet }));

    const destination = new Uint8Array([7, 8]);
    assertThrows(TypeError, () => destination.setFromBase64("YQ==", { alphabet }));
    assert(destination.join(","), "7,8");
}

for (const lastChunkHandling of ["loose\0x", "strict\0x",
                                "stop-before-partial\0x"]) {
    assertThrows(TypeError, () => Uint8Array.fromBase64("YQ==", { lastChunkHandling }));
    const destination = new Uint8Array([7, 8]);
    assertThrows(TypeError, () => destination.setFromBase64("YQ==", { lastChunkHandling }));
    assert(destination.join(","), "7,8");
}

let laterOptionRead = false;
const invalid = {
    alphabet: "base64\0x",
    get lastChunkHandling() {
        laterOptionRead = true;
        throw Error("lastChunkHandling read after invalid alphabet");
    },
    get omitPadding() {
        laterOptionRead = true;
        throw Error("omitPadding read after invalid alphabet");
    },
};
assertThrows(TypeError, () => Uint8Array.fromBase64("YQ==", invalid));
assertThrows(TypeError, () => bytes.toBase64(invalid));
assert(laterOptionRead, false);

assert(Uint8Array.fromBase64("YQ==", { alphabet: "base64" }).toHex(), "61");
assert(bytes.toBase64({ alphabet: "base64url" }), "YQ==");

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

import * as std from "std";
import * as os from "os";
import { url, main } from "./fixture_qjs_import_meta.js";

function assert(actual, expected)
{
    if (!Object.is(actual, expected))
        throw Error("qjs import.meta assertion failed: " + actual +
                    " !== " + expected);
}

function fileURL(filename)
{
    /* The libc metadata helper preserves raw file names on Windows. */
    if (os.platform !== "win32") {
        const [canonical, error] = os.realpath(filename);
        assert(error, 0);
        filename = canonical;
    }
    return "file://" + filename;
}

assert(globalThis.std, std);
assert(globalThis.os, os);
assert(import.meta.main, true);
assert(import.meta.url, fileURL(scriptArgs[0]));
assert(main, false);
assert(url, fileURL("fixture_qjs_import_meta.js"));
print("qjs-cli-file-meta-ok");

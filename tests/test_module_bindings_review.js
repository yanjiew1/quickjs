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

import { assert } from "./assert.js";
import * as valid from "./fixture_module_review_reserved_valid.js";

async function test_reserved_binding_matrix()
{
    const invalid = [
        "named_eval", "named_arguments",
        "named_escaped_eval", "named_escaped_arguments",
        "namespace_eval", "namespace_arguments",
        "namespace_escaped_eval", "namespace_escaped_arguments",
        "default_eval", "default_arguments",
        "named_implements", "named_interface",
        "named_package", "named_private",
        "named_protected", "named_public",
        "named_escaped_yield", "named_escaped_static"
    ];
    for (const name of invalid) {
        let error;
        try {
            await import("./fixture_module_review_invalid_" + name + ".js");
        } catch (e) {
            error = e;
        }
        assert(error instanceof SyntaxError, true, name);
    }
    for (const name of ["evaluation", "argumentValue", "awaited", "yielded",
                        "implemented", "interfaced", "letValue", "packaged",
                        "privateValue", "protectedValue", "publicValue",
                        "staticValue", "enumeration", "klass", "quotedEvaluation",
                        "quotedAwaited", "eval", "arguments", "await", "yield",
                        "reexportEval", "reexportArguments", "reexportAwait",
                        "reexportYield", "reexportEnum", "reexportClass"])
        assert(valid[name], 42, name);
    assert(valid.null.value, 42);
    assert(valid.implements, valid.null);
}

await test_reserved_binding_matrix();

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

/* Used by the C API host that supports kind, mode and other attributes. */
import first from "attribute-target" with { kind: "first", mode: "test" };
import second from "attribute-target" with { mode: "test", kind: "second" };
import equal from "attribute-target" with { mode: "test", kind: "first" };
/* The mode-first request matches mode before finding kind absent here. */
import alternate from "attribute-target" with { other: "first", mode: "test" };
import empty from "attribute-empty";
import emptyAgain from "attribute-empty" with {};
import "attribute-target" with { mode: "test", kind: "first" };
export { default as indirectFirst } from "attribute-target"
    with { kind: "first", mode: "test" };
import * as sharedFirst from "attribute-shared" with { kind: "first" };
import * as sharedSecond from "attribute-shared" with { kind: "second" };

if (first !== "first" || second !== "second" || equal !== first ||
    alternate !== "alternate" ||
    empty !== "empty" || emptyAgain !== empty || sharedFirst !== sharedSecond)
    throw new Error("static module attribute identity");
export const ok = true;

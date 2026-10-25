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

import payload from "./payload.so" with { type: "json" };
import payloadAgain from "./payload.so" with { type: "json" };
import otherType from "./payload.so" with { type: "json5" };
import extended from "./extended.so" with { type: "json5" };
import plain from "./plain.json";
import plainAgain from "./plain.json" with {};
import namedStd from "std" with { type: "json" };
if (payload.value !== 42 || payload.kind !== "plain JSON .so")
    throw Error("qjsc did not serialize the JSON .so payload");
if (payload !== payloadAgain)
    throw Error("equal attributed imports did not retain identity");
if (otherType.value !== 42 || otherType === payload)
    throw Error("distinct serialized requests lost their attribute identity");
if (extended.value !== 51 || plain.value !== 61 || plain !== plainAgain)
    throw Error("JSON5 or absent/empty attribute preloads were not retained");
if (namedStd.kind !== "JSON before a system module name")
    throw Error("qjsc selected the std C module before JSON attributes");
print("qjsc-json-attribute-payload-ok");

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

import { \u0065val as evaluation, arguments as argumentValue,
         \u0061wait as awaited, yi\u0065ld as yielded,
         implements as implemented, interface as interfaced, let as letValue,
         package as packaged, private as privateValue, protected as protectedValue,
         public as publicValue, static as staticValue, enum as enumeration,
         class as klass } from "./fixture_module_review_reserved_source.js";
import { "eval" as quotedEvaluation, "await" as quotedAwaited }
    from "./fixture_module_review_reserved_source.js";
export { evaluation, argumentValue, awaited, yielded, implemented, interfaced,
         letValue, packaged, privateValue, protectedValue, publicValue,
         staticValue, enumeration, klass, quotedEvaluation, quotedAwaited };
export { evaluation as eval, argumentValue as arguments, awaited as await,
         yielded as yield };
export { eval as reexportEval, arguments as reexportArguments,
         await as reexportAwait, yield as reexportYield,
         enum as reexportEnum, class as reexportClass }
    from "./fixture_module_review_reserved_source.js";
export * as null from "./fixture_module_review_reserved_source.js";
export * as implements from "./fixture_module_review_reserved_source.js";

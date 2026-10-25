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

/* ECMA-262 UnicodeMatchProperty Tables 65/66/67 exact spelling and aliases. */
function reject(pattern, flags) {
    try {
        new RegExp(pattern, flags);
    } catch (error) {
        if (error instanceof SyntaxError)
            return;
        throw error;
    }
    throw new Error("Accepted invalid Unicode property escape: " + pattern);
}
for (const name of ["IDS_Unary_Operator", "IDSU", "Modifier_Combining_Mark", "MCM",
                    "ID_Compat_Math_Start", "ID_Compat_Math_Continue", "InCB",
                    "Alphabetic ", "alphabetic", "ALPHABETIC", "Other_Math"])
    for (const flags of ["u", "v"])
        reject("\\p{" + name + "}", flags);
for (const name of ["ASCII", "Any", "Assigned", "Alphabetic", "Alpha", "AHex",
                    "Lower", "Upper", "space", "CWCF", "IDSB", "IDST"])
    for (const flags of ["u", "v"])
        new RegExp("\\p{" + name + "}", flags);
for (const pattern of ["\\p{script=Latin}", "\\p{Script=latin}",
                       "\\p{GeneralCategory=Lu}", "\\p{gc=uppercase_letter}",
                       "\\p{Block=Basic_Latin}", "\\p{Age=1.1}"])
    reject(pattern, "u");

/* Unicode 18 boundary-property names, shared by the generator and reader.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE.
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
#ifdef UNICODE_GRAPHEME_BREAK
DEF(Other)
DEF(CR)
DEF(LF)
DEF(Control)
DEF(Extend)
DEF(ZWJ)
DEF(Regional_Indicator)
DEF(Prepend)
DEF(SpacingMark)
DEF(L)
DEF(V)
DEF(T)
DEF(LV)
DEF(LVT)
#endif

#ifdef UNICODE_WORD_BREAK
DEF(Other)
DEF(CR)
DEF(LF)
DEF(Newline)
DEF(Extend)
DEF(ZWJ)
DEF(Regional_Indicator)
DEF(Format)
DEF(Katakana)
DEF(Hebrew_Letter)
DEF(ALetter)
DEF(Single_Quote)
DEF(Double_Quote)
DEF(MidNumLet)
DEF(MidLetter)
DEF(MidNum)
DEF(Numeric)
DEF(ExtendNumLet)
DEF(WSegSpace)
#endif

#ifdef UNICODE_SENTENCE_BREAK
DEF(Other)
DEF(CR)
DEF(LF)
DEF(Extend)
DEF(Sep)
DEF(Format)
DEF(Sp)
DEF(Lower)
DEF(Upper)
DEF(OLetter)
DEF(Numeric)
DEF(ATerm)
DEF(STerm)
DEF(Close)
DEF(SContinue)
#endif

#ifdef UNICODE_INDIC_CONJUNCT_BREAK
DEF(None)
DEF(Consonant)
DEF(Extend)
DEF(Linker)
#endif

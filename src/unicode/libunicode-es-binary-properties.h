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

/* ECMA-262 UnicodeMatchProperty, Table 66. Names are exact grammar aliases;
   property enum tokens are only expanded by the selected ICU implementation.
   ASCII, Any and Assigned are the three additional synthetic properties. */
#ifndef QUICKJS_UNICODE_ES_BINARY_PROPERTIES_H
#define QUICKJS_UNICODE_ES_BINARY_PROPERTIES_H
#define UNICODE_ES_BINARY_PROPERTIES(X) \
 X("ASCII_Hex_Digit,AHex", UCHAR_ASCII_HEX_DIGIT) \
 X("Alphabetic,Alpha", UCHAR_ALPHABETIC) \
 X("Bidi_Control,Bidi_C", UCHAR_BIDI_CONTROL) \
 X("Bidi_Mirrored,Bidi_M", UCHAR_BIDI_MIRRORED) \
 X("Case_Ignorable,CI", UCHAR_CASE_IGNORABLE) \
 X("Cased", UCHAR_CASED) \
 X("Changes_When_Casefolded,CWCF", UCHAR_CHANGES_WHEN_CASEFOLDED) \
 X("Changes_When_Casemapped,CWCM", UCHAR_CHANGES_WHEN_CASEMAPPED) \
 X("Changes_When_Lowercased,CWL", UCHAR_CHANGES_WHEN_LOWERCASED) \
 X("Changes_When_NFKC_Casefolded,CWKCF", UCHAR_CHANGES_WHEN_NFKC_CASEFOLDED) \
 X("Changes_When_Titlecased,CWT", UCHAR_CHANGES_WHEN_TITLECASED) \
 X("Changes_When_Uppercased,CWU", UCHAR_CHANGES_WHEN_UPPERCASED) \
 X("Dash", UCHAR_DASH) \
 X("Default_Ignorable_Code_Point,DI", UCHAR_DEFAULT_IGNORABLE_CODE_POINT) \
 X("Deprecated,Dep", UCHAR_DEPRECATED) \
 X("Diacritic,Dia", UCHAR_DIACRITIC) \
 X("Emoji", UCHAR_EMOJI) \
 X("Emoji_Component,EComp", UCHAR_EMOJI_COMPONENT) \
 X("Emoji_Modifier,EMod", UCHAR_EMOJI_MODIFIER) \
 X("Emoji_Modifier_Base,EBase", UCHAR_EMOJI_MODIFIER_BASE) \
 X("Emoji_Presentation,EPres", UCHAR_EMOJI_PRESENTATION) \
 X("Extended_Pictographic,ExtPict", UCHAR_EXTENDED_PICTOGRAPHIC) \
 X("Extender,Ext", UCHAR_EXTENDER) \
 X("Grapheme_Base,Gr_Base", UCHAR_GRAPHEME_BASE) \
 X("Grapheme_Extend,Gr_Ext", UCHAR_GRAPHEME_EXTEND) \
 X("Hex_Digit,Hex", UCHAR_HEX_DIGIT) \
 X("ID_Continue,IDC", UCHAR_ID_CONTINUE) \
 X("ID_Start,IDS", UCHAR_ID_START) \
 X("IDS_Binary_Operator,IDSB", UCHAR_IDS_BINARY_OPERATOR) \
 X("IDS_Trinary_Operator,IDST", UCHAR_IDS_TRINARY_OPERATOR) \
 X("Ideographic,Ideo", UCHAR_IDEOGRAPHIC) \
 X("Join_Control,Join_C", UCHAR_JOIN_CONTROL) \
 X("Logical_Order_Exception,LOE", UCHAR_LOGICAL_ORDER_EXCEPTION) \
 X("Lowercase,Lower", UCHAR_LOWERCASE) \
 X("Math", UCHAR_MATH) \
 X("Noncharacter_Code_Point,NChar", UCHAR_NONCHARACTER_CODE_POINT) \
 X("Pattern_Syntax,Pat_Syn", UCHAR_PATTERN_SYNTAX) \
 X("Pattern_White_Space,Pat_WS", UCHAR_PATTERN_WHITE_SPACE) \
 X("Quotation_Mark,QMark", UCHAR_QUOTATION_MARK) \
 X("Radical", UCHAR_RADICAL) \
 X("Regional_Indicator,RI", UCHAR_REGIONAL_INDICATOR) \
 X("Sentence_Terminal,STerm", UCHAR_S_TERM) \
 X("Soft_Dotted,SD", UCHAR_SOFT_DOTTED) \
 X("Terminal_Punctuation,Term", UCHAR_TERMINAL_PUNCTUATION) \
 X("Unified_Ideograph,UIdeo", UCHAR_UNIFIED_IDEOGRAPH) \
 X("Uppercase,Upper", UCHAR_UPPERCASE) \
 X("Variation_Selector,VS", UCHAR_VARIATION_SELECTOR) \
 X("White_Space,space", UCHAR_WHITE_SPACE) \
 X("XID_Continue,XIDC", UCHAR_XID_CONTINUE) \
 X("XID_Start,XIDS", UCHAR_XID_START)
#endif

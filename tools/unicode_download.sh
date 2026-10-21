#!/bin/sh
# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

set -e

version="18.0.0"
url="https://www.unicode.org/Public"

files="Blocks.txt CaseFolding.txt DerivedNormalizationProps.txt PropList.txt \
SpecialCasing.txt CompositionExclusions.txt ScriptExtensions.txt \
UnicodeData.txt DerivedCoreProperties.txt NormalizationTest.txt Scripts.txt \
PropertyValueAliases.txt"

mkdir -p unicode

for f in $files; do
    g="${url}/${version}/ucd/${f}"
    wget --https-only --timeout=30 --tries=3 "$g" -O "unicode/$f"
done

wget --https-only --timeout=30 --tries=3 "${url}/${version}/ucd/emoji/emoji-data.txt" -O unicode/emoji-data.txt

wget --https-only --timeout=30 --tries=3 "${url}/${version}/emoji/emoji-sequences.txt" -O unicode/emoji-sequences.txt
wget --https-only --timeout=30 --tries=3 "${url}/${version}/emoji/emoji-zwj-sequences.txt" -O unicode/emoji-zwj-sequences.txt

# Keep boundary properties and their conformance oracles at the same release
# as normalization/casing. Preserve auxiliary paths for generator dependencies.
mkdir -p unicode/auxiliary
for f in GraphemeBreakProperty.txt WordBreakProperty.txt SentenceBreakProperty.txt \
GraphemeBreakTest.txt WordBreakTest.txt SentenceBreakTest.txt; do
    wget --https-only --timeout=30 --tries=3 "${url}/${version}/ucd/auxiliary/$f" -O "unicode/auxiliary/$f"
done

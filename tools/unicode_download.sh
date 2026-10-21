#!/bin/sh
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

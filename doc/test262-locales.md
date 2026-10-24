# Locale assumptions in Test262

The minimal Intl profile offers `en`; native also offers `en-US`. ECMA-402
allows this difference. A regional request can resolve by prefix to `en`,
while `supportedLocalesOf()` preserves the requested regional tag.

Four tests at Test262 `d53ebc8b1931d9f1541e43d59650776e4b766248` require an
exact resolved `en-US` identity:

- `test/intl402/DisplayNames/prototype/resolvedOptions/return-object.js`
- `test/intl402/ListFormat/prototype/resolvedOptions/type.js`
- `test/intl402/RelativeTimeFormat/prototype/resolvedOptions/type.js`
- `test/intl402/Segmenter/prototype/resolvedOptions/type-without-lbs.js`

The first declares `locale: [en-US]`; the remaining three omit locale metadata
but assert the same fixed locale. The runner does not filter locale metadata.
The minimal overlay excludes these data-dependent cases; native runs all four
unmodified. This is an availability distinction, not an accepted descriptor
failure. No Test262 source patch or expected-error entry is needed.

`tests/test_intl_profile.js` checks the matched locale, ordinary fresh result
objects and locale property descriptors across all constructors in both
profiles. It also covers regional requests and exact `en-US` support in native.

Basis: ECMA-402 living draft commit
`3691634ebc1794f2af74cb26e59824b21090c3ad`, reviewed 2026-10-24:
[service locale inventories](https://tc39.es/ecma402/#sec-internal-slots),
[prefix matching](https://tc39.es/ecma402/#sec-lookupmatchinglocalebyprefix) and
[locale resolution](https://tc39.es/ecma402/#sec-resolvelocale).
Test262 documents the [locale metadata contract](https://github.com/tc39/test262/blob/d53ebc8b1931d9f1541e43d59650776e4b766248/INTERPRETING.md#locale).

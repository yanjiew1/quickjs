# Locale assumptions in Test262

The minimal Intl profile offers `en`; native also offers `en-US`. ECMA-402
allows this difference. A regional request can resolve by prefix to `en`,
while `supportedLocalesOf()` preserves the requested regional tag.

Four tests at Test262 `d53ebc8b1931d9f1541e43d59650776e4b766248` check ordinary
result objects and property descriptors but assume an exact resolved `en-US`:

- `test/intl402/DisplayNames/prototype/resolvedOptions/return-object.js`
- `test/intl402/ListFormat/prototype/resolvedOptions/type.js`
- `test/intl402/RelativeTimeFormat/prototype/resolvedOptions/type.js`
- `test/intl402/Segmenter/prototype/resolvedOptions/type-without-lbs.js`

`tests/test262.patch` uses the available generic English locale for these
checks: request `EN`, expect `en`. Every object, descriptor, key-order and
option assertion remains. Uppercase input retains canonicalization coverage.
`tests/test_intl_profile.js` separately checks regional matching in both profiles,
including exact `en-US` support in native. No test is excluded or accepted as a
known error by this adaptation. English is this project's profile policy, not
a universally required locale in ECMA-402.

Basis: ECMA-402 living draft commit
`3691634ebc1794f2af74cb26e59824b21090c3ad`, reviewed 2026-10-24:
[service locale inventories](https://tc39.es/ecma402/#sec-internal-slots),
[prefix matching](https://tc39.es/ecma402/#sec-lookupmatchinglocalebyprefix) and
[locale resolution](https://tc39.es/ecma402/#sec-resolvelocale).

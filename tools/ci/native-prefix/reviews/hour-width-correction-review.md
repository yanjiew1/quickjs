# Contextual hour-width correction review

Reviewed on 2026-10-15. This approves a bounded semantic correction, not
complete native Intl or Temporal conformance.

## Implementation and standards

The best-fit matcher selected CLDR `HH:mm:ss` but changed its hour width to
numeric. Preserve contextual numeric hour widths, consistently with minutes
and seconds. Basic matching and explicit two-digit adjustment remain unchanged.

ECMA-402 living draft `730ca2b88c305862db7be2f83c510dcca33ab48c`
(2026-10-12), reviewed 2026-10-15:

- https://tc39.es/ecma402/#sec-bestfitformatmatcher
- https://tc39.es/ecma402/#sec-formatdatetimepattern
- https://unicode.org/reports/tr35/tr35-dates.html#Matching_Skeletons

CLDR 48.2, `11299982335beb974c1c63c45265184e759c0f41`, English
`common/main/en.xml`: `Hms` uses `HH:mm:ss`; `hms` uses `h:mm:ss a`.

## Captures

Historical qualified source: `cb52ec6a404d0b9b902aa7365ace7f5f94f9f5fe`.
Clang ASan capture:
https://github.com/yanjiew1/quickjs/actions/runs/38580266736

Corrected source: `68cae1ac427c280d8f1d041add011e4627b72aa5`.
GCC normal capture:
https://github.com/yanjiew1/quickjs/actions/runs/38604239020

Corrected complete validated summary SHA-256:
`88fecb16b7d9cdf6aea21346d33ed50b287f6e4204348a0f988b1e22ee16d319`.
Independent revalidation command SHA-256:
`ca7301a6991b040bd318a25655a769efb942ae21d6341a34f442d09950afa80b`.

The complete capture retains 118 original sources, 210 script variants and
all complete relative-time observations. Exactly eight complete semantic
fingerprints change; the other 110 and all stable configurations match.
Event counts are unchanged. Actual source, engine and configuration identities
remain in captures; only the existing authenticated identity normalization is
used for cross-row semantic comparisons.

## Independently reviewed changes

- Four Temporal `toLocaleString/hourcycle.js` sources: `Instant`, `PlainTime`,
  `PlainDateTime`, `ZonedDateTime`. Both variants add the missing leading zero
  for `hour12: false` and `h23`. Resolved hour width becomes `2-digit` for
  `h23` and `h24`; the already correct `24:00:00` stays unchanged. Related
  invariant and premise values follow these changes. Two failed assertions
  per trace pass; mismatch counts change from two to zero.
- Temporal `PlainTime` and `PlainDateTime` `resolved-time-zone.js`: only the
  hour padding changes from `0:30:45` to `00:30:45`, including the date prefix.
  Each failed comparison passes; mismatch counts change from one to zero.
  Other timezone cases remain identical.
- DateTimeFormat `resolvedOptions/hourCycle.js` and `hourCycle-default.js`:
  only English resolved hour width changes from `numeric` to `2-digit`.
  Hourcycle results and existing German locale failures remain unchanged.
  Mismatch counts remain five and one respectively.

The independent read-only review checked all 16 affected raw trace hashes,
event counts, complete changed-event index sets, and all 20 assertion exit
transitions. Every exit changes only from failed/throw/continued/error to
passed/normal/not continued/undefined. All 16,092 serialized string
representations agree with their values. Strict and nonstrict delta signatures
match. No additional semantic change was found.

## Qualification boundary

Derive the six repaired failure removals and eight fingerprint replacements
in memory through one authenticated correction transition. Preserve the
historical policy, packet, per-file reviews and mandatory proof inventories.
Retain all original witnesses and the required 18/2/114 selections.

Require positive raw PASS observations for both variants of each repaired
source, exact remaining failure identities/signatures, the unchanged executed
and skipped source sets, and no extra exclusions. Current raw failures and
new failures become 103; executed sources remain 52,734, excluded 27 and
skipped 856. No failure waiver or conformance acceptance is granted.

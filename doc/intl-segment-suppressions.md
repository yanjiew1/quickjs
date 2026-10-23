# Literal sentence suppression preparation

The source collector preserves raw CLDR suppression entries. The inheritance
owner selects the nearest declared **whole list** along the `segmentations`
parent graph. It does not union entries or use exceptional main-data parents.
An explicit empty list stops inheritance. A missing declaration and a declared
empty list can both execute as empty policy, but retain different origins.

`segment_suppressions.py` keeps the original ordered entries and declaration
origin beside a canonical literal graph. It sorts keys only for graph
construction. Spaces, punctuation, case and Unicode spelling remain unchanged;
multi-part abbreviations are not split or interpreted as regular expressions.
Empty keys, duplicate keys and malformed scalar text are errors. An empty list
has no graph, rather than an accepting epsilon path.

## Shared mechanics, separate source policy

`scalar_trie.py` owns the scalar-key graph mechanics used by both suppression
lists and dictionaries. It retains only the current sorted-key prefix frontier,
finalizes children before parents and interns equivalent suffix states.
Terminal payload identity participates in sharing. Keys are Unicode scalar
strings; payloads are uint32 values. A `None` source payload means membership
and maps to zero. Postorder IDs
ensure every edge points to a smaller state. This is a host preparation shape,
not a new runtime wire format.

The dictionary frontend retains its stricter source rules: no separator/control
keys, and exact membership versus weighted-cost modes. Sharing graph mechanics
does not make dictionary input accept suppression phrases. The dictionary wire
format and payload identity remain unchanged.

## Resource bounds

Common default limits are 1,000,000 entries, 16,777,216 total key scalars,
4,096 scalars per key, 1,000,000 retained states, 2,000,000 retained plus active
frontier edges, 65,536 alphabet entries and 65,535 payload values. The frontier
contains at most one node per active key position. Count/key/scalar preflights
precede character scans and graph construction; limits cannot be loosened by a
caller. Both frontend policy scans and shared mechanics enforce relevant caps.

Suppression preparation tightens these to 65,536 entries, 1,048,576 total
scalars, 256 scalars per key, 262,144 states, 524,288 edges and one membership
payload. The reviewed CLDR profile has seven declared lists, with a longest key
of 11 scalars; bounds leave explicit room for future source updates without
silently truncating them.

## Subsequent runtime policy

Prepared literal graphs alone do not satisfy native suppression execution.
Runtime integration must match complete raw entries, including internal cuts
in multi-part abbreviations, and remove only approved soft sentence decisions.
It must preserve endpoints, paragraph breaks and the original terminator anchor
through the admitted Close/Sp/ignored tail. It must not skip arbitrary ignored
characters inside literal entries. Native locale admission remains separate.

Sources reviewed 2026-10-23: CLDR 49 beta4 commit
`7cdeb9e7fac52c59f22fc7248b05f989ee5689c2`, `common/segments/*.xml`, and
[UAX #29 revision 49: sentence boundaries](https://www.unicode.org/reports/tr29/tr29-49.html#Sentence_Boundaries).

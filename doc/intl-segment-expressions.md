# Prepared segmentation expressions

`tools/intl/segment_expression.py` parses the bounded expression profile used
by the pinned CLDR segmentation sources. It produces immutable typed trees,
not runtime regular expressions. Source collection and inheritance remain in
`segment_sources.py` and `segment_inheritance.py`.

## Types and historical bindings

Character sets cover Unicode code points, including lone surrogate code
points that can occur in JavaScript UTF-16 strings. Dictionary keys separately
require Unicode scalar values. Dot means any code point, including LF and NUL;
dot inside brackets is a literal period.

Properties, literals, unions, differences and complements are set-valued.
Grouping and alternation of one-character languages preserve that type.
Concatenation of multiple atoms, repetition, optional expressions and absolute
text-start assertions are sequence-valued. Empty source denotes epsilon;
`[]` denotes an empty character set.

References store prior binding arena slots rather than expanding text. Parse
each variable with its captured historical environment; rules later use the
final visible environment. A redefined variable may change from a set to a
sequence without changing any earlier binding. Sequence-valued references
cannot appear inside character sets. Unknown names fail: this layer supplies
no implicit Japanese `$FE` definition or other source correction.

`segment_compat.py` separately defines the native Japanese word-profile
correction. The pinned Japanese tailoring still uses `$FE` after CLDR-18917
removed that declaration from root in favor of ignore remaps. The explicit
correction restores the historical `[$Format $Extend $ZWJ]` union before the
first Japanese wrapper. It requires the exact CLDR commit, root/Japanese
source hashes and declaration sequence; changed inputs require another review.
Its input is the unmodified collector output; explicit declaration checks also
guard the three inherited dependencies used by the inserted union.

This produces derived records and an immutable correction receipt. It never
rewrites authenticated XML or changes strict raw compilation. The receipt marks
the inserted declaration as synthetic despite the derived group's retained
source identity. No other unresolved variable receives a fallback definition.
The historical definition can be inspected in
[root before the ignore-remap migration](https://github.com/unicode-org/cldr/blob/56a2de16366cd5bfe18628e941ea84849cfd4153/common/segments/root.xml).

## Grammar and later preparation

The profile admits properties, literal escapes, nested sets, set union and
subtraction, concatenation, alternation, `*`, `?` and absolute-start `^`.
Set union and subtraction have equal precedence and associate left to right.
Subtraction requires full-set operands on both sides; bracket wrappers retain
the difference between a character and a singleton set even through aliases.
Ambiguous literal-range syntax is rejected. Escaped metacharacters remain literals.
Only Unicode Pattern_White_Space is insignificant; NBSP remains a literal.

Source size, token count, node count, recursive nesting and constructed tree
height are bounded before publication. Flat adjacent unions use n-ary nodes;
difference boundaries retain their left-to-right structure. Errors retain
original expression offsets. Property names remain symbolic until a separate
pinned C-property resolver checks their meanings.

Complement nodes and reference provenance deliberately survive parsing. The
later UAX #29 ignore lowering needs to distinguish positive properties from
negations even when they have equivalent evaluated ranges. Boundary operators
and the two recognized ignore remaps belong to `segment_rules.py`; they are not
generic capture or replacement syntax in the expression parser. That owner
prepares the historical arena and verifies its snapshots, then parses rules
against the final visible bindings. It retains inherited priority and original
source identity, with aggregate node, binding and rule budgets.

Only a top-level unescaped `×`, `÷` or `→` separates a rule. Empty ordinary
sides are epsilon. The word rule 4 and sentence rule 5 ignore forms require
their exact named capture, excluded/ignored variable lists, closure and `${X}`
replacement. They become dedicated ignore records at their original rule
positions; preparation does not strip input or reorder those phases. Unknown
remaps fail rather than falling through to host regex substitution.

`segment_sets.py` evaluates character-set operands into normalized immutable
half-open intervals over `[0, 0x110000)`. It uses integer interval algebra and
an injected property owner, without host Unicode data or code-point enumeration.
Evaluation and sequence-operand traversal follow shared arena slots iteratively;
long alias chains do not consume the Python call stack. The source AST remains
available beside every result for later polarity-aware ignore lowering.

Each evaluator bounds node visits, interval work, individual result size,
distinct queries and retained range pairs. Aliases and bracket nodes share
tuple owners instead of copying their ranges. The default profile allows
131,072 nodes, 16,384 intervals per result, 8,000,000 work visits, 256 distinct
properties and 262,144 retained interval pairs. Exceeding a budget fails without
publishing an incomplete set; operational/property errors never imply an empty
set or fallback to a host Unicode implementation.

`segment_property.py` connects that evaluator to the explicit build-machine
`intl-segment-property` executable. It never discovers a target binary on PATH.
Successful replies must have exactly the Unicode 18 version and range schema;
duplicate keys, non-integer endpoints and unnormalized ranges are rejected.
Exact query strings share a generation-scoped immutable cache.

The bridge drains stdout/stderr concurrently with fixed byte caps, a per-query
deadline and a cumulative subprocess time budget. Failed children are killed
and reaped before returning, and partially received replies are never cached.
Defaults are 512 KiB stdout, 8 KiB stderr, ten seconds per query and 120 seconds
per generation, with 256 queries and 262,144 retained interval pairs. These
limits bound host preparation; they do not change runtime segmentation rules.
Callers may tighten these profile caps; raising them requires a reviewed
profile change. The executable argument must contain an explicit path.

Automata, binary encoding, runtime matching and native locale capability
advertisement are subsequent acceptance stages.

References reviewed 2026-10-23:

- [CLDR source pin](https://github.com/unicode-org/cldr/tree/7cdeb9e7fac52c59f22fc7248b05f989ee5689c2),
  including `common/segments/root.xml` and `common/segments/ja.xml`.
- [UTS #35 revision 79: Unicode sets](https://www.unicode.org/reports/tr35/tr35-79/tr35.html#Unicode_Sets).
- [UAX #29 revision 49: replacing ignore rules](https://www.unicode.org/reports/tr29/tr29-49.html#Replacing_Ignore_Rules).

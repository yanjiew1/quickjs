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
original expression offsets. Property names remain
symbolic until a separate pinned C-property resolver checks their meanings.

Complement nodes and reference provenance deliberately survive parsing. The
later UAX #29 ignore lowering needs to distinguish positive properties from
negations even when they have equivalent evaluated ranges. Boundary operators
and the two recognized ignore remaps belong to separate rule preparation;
they are not generic capture or replacement syntax in this expression parser.
Automata, binary encoding, runtime matching and native locale capability
advertisement are subsequent acceptance stages.

References reviewed 2026-10-23:

- [CLDR source pin](https://github.com/unicode-org/cldr/tree/7cdeb9e7fac52c59f22fc7248b05f989ee5689c2),
  including `common/segments/root.xml` and `common/segments/ja.xml`.
- [UTS #35 revision 79: Unicode sets](https://www.unicode.org/reports/tr35/tr35-79/tr35.html#Unicode_Sets).
- [UAX #29 revision 49: replacing ignore rules](https://www.unicode.org/reports/tr29/tr29-49.html#Replacing_Ignore_Rules).

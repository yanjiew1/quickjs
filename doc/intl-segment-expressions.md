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

## Ignore-phase language preparation

`segment_lower.py` translates prepared expressions into immutable language DAGs
while retaining each original rule and character-atom source. At an ignore
record, it inserts a no-break rule `complement(excluded) × ignored`. Earlier
rules remain raw. Later positive character atoms acquire a following ignored
closure; complemented atoms do not. Grouped one-character alternations and
positive set differences remain single predicates. Unreviewed mixed/nested
negation shapes fail rather than receive guessed Boolean-to-language semantics.

The returned `LoweredProgram` retains its matching `PreparedVariables` owner:
a shared character source can be a declaration span rather than a substring
of its using rule. Keep that binding context with subsequent automata records.
Historical sequence aliases share immutable IR nodes. Raw and ignore-aware
uses have separate memo entries; generated closures bypass lowering so they
cannot grow recursively. Node, edge, height and work budgets bound preparation.
The source phase remains visible; no input text is stripped or normalized.
The bounded short-input interpreter in `tests/intl_segment_reference.py` is
only a semantic test oracle, not a production traversal algorithm.

The future automata compiler must give repeated Thompson occurrences fresh
continuations, or use a continuation-correct construction. Shared language-DAG
identity does not justify sharing mutable NFA fragments in different contexts.

## Bounded NFA construction and proof execution

`segment_nfa.py` compiles each language into immutable host instructions with
fresh Thompson fragments for each occurrence. Predicates share normalized range
tuples; continuations do not share mutable patches. Construction is iterative,
with limits of 8,192 states (and at most two edges per state), 65,536 work
visits, 256 predicates and 262,144 retained interval pairs per language.
Each predicate has at most 16,384 intervals. Budget exhaustion is an error,
not permission to truncate or approximate a language.

Reversal reverses concatenation recursively, preserving alternation, repetition
and the assertion's **original text-start** meaning. The bounded host reference
executor computes accepted code-point endpoints and closes nullable epsilon
cycles with a visited set. It is checked against the independent short language
oracle, including shared continuations and anchored parity beyond short context
limits. It is neither a UTF-16 decoder nor a production per-candidate matcher.
Deterministic automata, checked binary encoding and native input preparation
remain separate stages; this host instruction shape declares no runtime format.

`segment_alphabet.py` partitions code points by the realized membership
signatures of the program's overlapping predicates. It sweeps grouped interval
endpoints rather than Unicode characters or all possible bit combinations.
Equal signatures share a class ID, numbered by first code-point occurrence;
the resulting contiguous class map covers the full code-point domain. The
result retains its normalized ordered predicates, so later NFA-local predicate
IDs can map to the matching membership-bit owner rather than a separate list.
Predicate, pair, endpoint, class, output-range and work caps bound preparation,
including a conservative allowance before sorting endpoints. This retains
overlaps instead of choosing one winning Unicode property per character.

`segment_dfa.py` builds independent complete side machines over that shared
alphabet. Unanchored machines inject the NFA entry after every code point,
recognizing suffix existence on each consumed prefix. Reversed right machines
run from original EOF backward to recognize right-prefix existence.

Production normalization admits only the pinned leading-left concatenation
anchor form, retaining the original language and deriving an anchor-free body.
That body seeds once at original BOS and never restarts. Other production
assertion placements fail for review. Generic NFA proof execution still retains
the original assertion semantics.

Determinization uses sorted epsilon-closed subsets, breadth-first state order
and ascending class IDs. State, complete-transition-cell, retained-subset-member
and construction-work caps precede interning/publication. Defaults per machine
are 4,096 states, 1,048,576 transition cells, 262,144 retained members and
16,000,000 work visits. Aggregate rule-profile budgets, serialization and native
UTF-16 execution remain subsequent acceptance stages; host acceptance helpers
are bounded proof tools, not the production input owner.

`segment_program.py` assembles complete rule programs with one shared alphabet
and mode-aware side-machine owners. Identical completed bodies can share a
table, while direction and BOS/restart policy remain in identity. Rule order,
actions and source records stay separate. Semantic table keys exclude subset
proof IDs and construction statistics; retained lowered context preserves
original rule/declaration provenance.

Aggregate caps apply across all rule sides, in addition to component limits:
1,024 rules, 131,072 retained temporary NFA states, 65,536 retained DFA states,
8,388,608 transition cells, 4,194,304 subset members and 128,000,000 work visits.
At most 262,144 unique predicate pairs are retained; completed temporary NFAs
are rebound to canonical tuple owners before storing the next side.
Remaining work is delegated to each compiler before it runs. Completed duplicate
machines still charge construction/key work even when their retained storage
shares an owner. NFA/alphabet/DFA host records expose construction work for that
accounting; statistics are not part of runtime table identity or wire data.

## Refinement-aware native profiles

`segment_profile.py` owns the pinned native policy beside pure base languages.
An explicit word-rule ID whitelist permits later lexical replacement; all
unreviewed, separator, raw ZWJ/space/ignore and RI decisions remain protected.
Core lexical rows require their root origin; Japanese's two extra lexical rows
require their Japanese origin. Only root sentence rule 11 is a suppressible soft
break. Rule action changes require review.

Sentence context predicates for SATerm, Close, Sp, ParaSep and the actual ignored
union join the same alphabet, including any predicates unused by a base machine.
Membership bits therefore give homogeneous per-class flags, not guesses from one
sample character. The selected profile requires these context categories to be
disjoint. Original winning-rule indexes remain available for refinement.

Context preparation has a separate fixed set-evaluator budget, shared by all
five queries: 131,072 nodes, 16,384 intervals per result, 8,000,000 work steps,
256 property queries and 262,144 retained interval pairs. The program limits
above cover base-machine construction, including auxiliary-predicate storage.
Context-to-predicate lookups occur once per context; flag assignment then uses
at most five bit tests for each of the alphabet's at most 16,384 classes.

Profile identity includes granularity, rule roles, class context and unresolved
requirements, in addition to base machine tables. Word requires dictionaries,
word status and selected grapheme protection; sentence requires suppressions and
grapheme protection. These prepared flags do not advertise executable native
coverage. The runtime must satisfy every requirement and retain endpoint
and hard-boundary protections before exposing native locales.

References reviewed 2026-10-23:

- [CLDR 49 release source](https://github.com/unicode-org/cldr/tree/96e54b273d2da4ec6fbad350ba19ff23fa24a588),
  including `common/segments/root.xml` and `common/segments/ja.xml`.
- [UTS #35 revision 79: Unicode sets](https://www.unicode.org/reports/tr35/tr35-79/tr35.html#Unicode_Sets).
- [UAX #29 revision 49: replacing ignore rules](https://www.unicode.org/reports/tr29/tr29-49.html#Replacing_Ignore_Rules).

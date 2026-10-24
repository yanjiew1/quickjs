# Collation compiler input format (QJCI)

The host-side writer in [`collation_ir.py`](../tools/intl/collation_ir.py)
and reader in [`collation-ir.c`](../tools/intl/collation-ir.c) share this
counted input format. It carries import-expanded source instructions, not
executable runtime tailoring. The independent root source format is
[QJCR](intl-collation-root-format.md); the compiled runtime archive is
[QJCL](intl-collation-compiled-format.md).

The private `QJCI` version 2 layout is independent of the runtime `QJIN` format.
All integers are unsigned little-endian 32-bit values. Record sizes include their
headers, labels are counted UTF-8 without C0/DEL characters, and there is no
alignment padding:

| Record | Fields |
| --- | --- |
| File header | `QJCI` magic, version, Unicode version (`18 << 16`), total bytes, program count, 32-byte source-manifest fingerprint |
| Program | record bytes, locale bytes, collation bytes, alternate bytes, instruction count; then the three labels and instructions |
| Instruction | record bytes, kind, auxiliary, rule-text line, rule-text column, source-label bytes; then the source label and three scalar vectors |
| Scalar vector | element count, followed by that many Unicode scalar values |

Instruction kinds are reset (1), relation (2), starred relation (3), setting (4),
suppression (5) and optimization hint (6). Reset vectors are text/special-name/
empty, with auxiliary `before` strength; relation vectors are prefix/text/
extension, with auxiliary relation strength. Range instructions carry inclusive
endpoint pairs in the first vector; only starred relations have a strength.
Setting vectors contain name/NUL-terminated values/empty, with zero auxiliary.
Imports must already be expanded. The mandatory source fingerprint is the same
manifest identity stored by QJCR; `--audit-pair root.qjcr input.qjci` requires exact
equality before normalization. It identifies the actual supplied source inventory,
not a checksum/authentication signature for arbitrary modified artifacts. Source positions refer to original rule text,
not invented XML line coordinates. Artifacts and normalization allocations are
bounded. Enumerated work (starred relations and suppression ranges) is limited
to 1,000,000 scalars per program and 4,000,000 per artifact, counting repeated
ranges. Optimize records are validated hints and do not enumerate their scalars.
A format change must update the writer, reader, fixtures and this description.

`collation-ir.c` owns reusable structural preflight and borrowed program visits.
Opening validates the complete artifact, strictly sorted unique identity tuples,
UTF-8 labels, scalar vectors, record grammar and work budgets without allocation
or callbacks. Only then may a visitor process begin/instruction/end events.
Descriptor pointers last for their callbacks; byte slices borrow the unchanged
input lifetime. Callback errors propagate without later callbacks, and end is a
success finalizer rather than error cleanup. Execution owns unconditional cleanup
of an active builder. The CLI remains a separate pinned-Unicode audit adapter,
retaining its nonallocating prefix-check pass before allocating normalization.


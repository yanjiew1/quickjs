# Native calendar publication data

The immutable runtime table is `src/calendar/provider.c`. Its pinned source
receipts, coverage limits and table hashes are recorded in
`src/calendar/provider-provenance.json`. Builds consume that compiled table;
they do not require Python, source PDFs, JavaScript, XML or JSON.

## Archived acquisition and audit pipeline

The `host` scripts and `inputs` receipts preserve the original offline audit
pipeline. They are **not a self-contained regeneration command**. The full
provider preparation scripts refer to private captured-input directories;
`widget_review.py` also requires the original external `prepare_hko.py` parser.
Those captures and that parser are not bundled. Absolute paths in the sealed
receipts identify the original acquisition and have not been relabeled as
portable inputs. The Persian table likewise includes historical provenance,
not a bundled regeneration pipeline.

The PMO, KASI and boundary preparers accept externally supplied inputs and
remain useful for reviewing such captures. Do not register complete provider
regeneration as a repository test without first supplying the missing pinned
inputs and parsers. Historical acceptance booleans describe their original
review checkpoint, not the current runtime activation.

PMO publication hosting does not establish authorship or a standalone license
grant for the acquired numeric source. See `NOTICE-PMO.txt`, the detailed
provenance and preserved ICU notices before reusing or extending the data.

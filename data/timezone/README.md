<!--
Copyright (c) 2017-2021 Fabrice Bellard
Copyright (c) 2017-2021 Charlie Gordon

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-->

# Native timezone data

`timezone-data.bin` is the single QJTZ pack generated from IANA tzdata **2026f**
and CLDR common release **49**. Its manifest records exact source revisions,
generation settings, identifier/primary relationships and payload hashes.
See [the generator guide](../../tools/timezone/README.md) for acquisition,
offline regeneration and the binary format.

The pack retains the supplied pre-1970 histories, including `backzone`, and
POSIX recurrence rules for dates beyond explicit transitions. It applies no
leap-second corrections or generation-time date-range restriction. IANA's
historical records can be approximate; storing them does not establish
historical accuracy.

Canonical UTC remains in this source pack for complete source enumeration.
The common library owns UTC behavior; backends supply additional identifiers
and reuse core behavior for aliases whose primary identifier is UTC.

The snapshot has no active rename-waiting overrides. Review replacement-name
policy when updating the dataset; geographic splits are not renames.
Generation uses fixed pins and does not change with the current date.

The IANA data is public domain; see [LICENSE-IANA](LICENSE-IANA).
CLDR metadata and derived mappings use the
[Unicode license](sources/cldr/LICENSE). Raw CLDR XML is retained as generator
input and is not parsed by the target runtime.

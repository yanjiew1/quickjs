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

# ICU4X bridge

This private Rust static library adapts pinned ICU4X public APIs to the engine's
plain C provider contracts. It is optional; ordinary native/disabled profiles
do not need Rust. `Cargo.lock` records exact crate checksums. Sources are fetched
by Cargo rather than copied into the repository.

The calendar feature uses ICU4X 2.4.0, whose minimum Rust version is 1.88. Use
`cargo test --manifest-path src/icu4x/Cargo.toml --locked --features calendars`
for the bridge's direct unit tests. Set `CARGO_TARGET_DIR` to a build directory;
do not put generated objects in the source tree. `--offline` works after the
locked dependencies have been fetched.

## C boundary

- Calendar IDs mirror `QJSCalendarId`; ISO arithmetic remains in C core.
- Arguments and results are fixed-layout C records. No Rust reference, string,
  enum representation or object handle crosses the boundary.
- Calls publish outputs only after success, including caught Rust panics.
  Borrowed Japanese era names have process lifetime; failed initialization is
  retryable. Concurrent initialization cannot publish a partial record.
- Metadata years are deliberately limited to ±900000, safely covering all
  Temporal dates without bypassing ICU4X's fundamental date bounds. Conversion
  validates roundtrips to detect a foreign date clamp.
- Month addition delegates to ICU4X's public date arithmetic, including its
  own distant Chinese/Dangi model. It does not assume the native provider's
  astronomical mean phase or iterate over days in C.

ICU4X's internal infallible allocations can abort on OOM. Catching a panic is
not a promise that every Rust allocation failure becomes a recoverable engine
exception. Engine-owned buffers still follow the ordinary allocator contract.

The Meson build collects the resolved crates' full license notices and the
selected Rust standard-library copyright/license notices alongside the
companion archive. Retain those when redistributing linked binaries.

The C provider uses the shared required publication records for Chinese
1900–2100 and Dangi 1900–2050, because ICU4X's approximation can disagree with
an official row (Chinese 2057). Other dates retain ICU4X arithmetic. Every
required year's month count agrees with the pinned ICU4X month sequence;
repository tests check this, official seams and month-addition roundtrips.
Large lunar month additions traverse calendar years in the upstream API;
ordinary and large-delta behavior have separate regression coverage.

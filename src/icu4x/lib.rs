/* Private ICU4X provider bridge.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license, see project LICENSE.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
//! Private C ABI only. Foreign objects never cross the engine boundary.

#[cfg(feature = "calendars")]
mod calendar;
#[cfg(feature = "collations")]
mod collation;
#[cfg(feature = "compact_formats")]
mod compact;
#[cfg(feature = "currency_names")]
mod currency;
#[cfg(feature = "currency_formats")]
mod currency_format;
#[cfg(feature = "currency_numbers")]
mod currency_number;
#[cfg(feature = "decimals")]
mod decimal;
#[cfg(feature = "lists")]
mod list;
#[cfg(feature = "locales")]
mod locale;
#[cfg(feature = "locales")]
mod locale_data;
#[cfg(feature = "names")]
mod names;
#[cfg(feature = "plurals")]
mod plural;
#[cfg(any(
    feature = "currency_formats",
    feature = "compact_formats",
    feature = "relatives"
))]
mod plural_pattern;
#[cfg(feature = "relatives")]
mod relative;
#[cfg(feature = "segments")]
mod segment;
#[cfg(feature = "timezones")]
mod timezone;

#[cfg(feature = "casing")]
mod casing;
#[cfg(any(feature = "lists", feature = "casing", feature = "names"))]
mod text;

const RANGE: i32 = -1;
#[cfg(any(
    feature = "calendars",
    feature = "timezones",
    feature = "locales",
    feature = "names",
    feature = "decimals",
    feature = "currency_numbers",
    feature = "collations",
    feature = "relatives"
))]
const UNSUPPORTED: i32 = -3;
#[cfg(any(feature = "calendars", feature = "casing"))]
const MEMORY: i32 = -4;
const BACKEND: i32 = -5;

// Publish a complete POD result only after success. A library panic is caught
// here; it must never unwind through C. Rust internal OOM can still abort where
// an upstream operation does not expose a fallible allocation API.
unsafe fn write_result<T>(
    output: *mut T,
    operation: impl FnOnce() -> Result<T, i32> + std::panic::UnwindSafe,
) -> i32 {
    if output.is_null() {
        return RANGE;
    }
    match std::panic::catch_unwind(operation) {
        Ok(Ok(value)) => {
            // SAFETY: each exported operation requires a valid writable output.
            unsafe { output.write(value) };
            0
        }
        Ok(Err(error)) => error,
        Err(_) => BACKEND,
    }
}

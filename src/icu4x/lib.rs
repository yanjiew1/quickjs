/* Private ICU4X calendar bridge.
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

const RANGE: i32 = -1;
const UNSUPPORTED: i32 = -3;
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

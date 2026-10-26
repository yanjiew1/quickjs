/* Private ICU4X counted UTF-16 output.
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
#[cfg(any(feature = "lists", feature = "casing"))]
use crate::RANGE;
#[cfg(any(feature = "casing", feature = "names"))]
use std::ffi::c_void;
use std::fmt;

#[cfg(any(feature = "lists", feature = "casing"))]
#[repr(C)]
#[derive(Clone, Copy)]
pub struct Utf16 {
    pub(crate) data: *const u16,
    pub(crate) length: usize,
}

#[cfg(any(feature = "lists", feature = "casing"))]
impl Utf16 {
    pub(crate) unsafe fn units<'a>(self) -> Result<&'a [u16], i32> {
        if self.length > isize::MAX as usize / 2 || (self.length != 0 && self.data.is_null()) {
            return Err(RANGE);
        }
        if self.length == 0 {
            return Ok(&[]);
        }
        // SAFETY: C owns the complete readable aligned span for this call.
        Ok(unsafe { std::slice::from_raw_parts(self.data, self.length) })
    }
}

// Literal/case output is scalar text; encode in bounded stack chunks rather
// than allocate an intermediate UTF-16 string. Element copies bypass this.
#[cfg(feature = "lists")]
pub(crate) fn encode_utf16(text: &str, mut emit: impl FnMut(&[u16]) -> fmt::Result) -> fmt::Result {
    let mut buffer = [0u16; 128];
    let mut count = 0;
    for unit in text.encode_utf16() {
        buffer[count] = unit;
        count += 1;
        if count == buffer.len() {
            emit(&buffer)?;
            count = 0;
        }
    }
    if count != 0 {
        emit(&buffer[..count])?;
    }
    Ok(())
}

#[cfg(any(feature = "casing", feature = "names"))]
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TextSink {
    pub opaque: *mut c_void,
    pub write: unsafe extern "C" fn(*mut c_void, *const u16, usize) -> i32,
}

#[cfg(any(feature = "casing", feature = "names"))]
pub(crate) struct TextOutput {
    sink: TextSink,
    pub error: i32,
    buffer: [u16; 128],
    used: usize,
}
#[cfg(any(feature = "casing", feature = "names"))]
impl TextOutput {
    pub fn new(sink: TextSink) -> Self {
        Self {
            sink,
            error: 0,
            buffer: [0; 128],
            used: 0,
        }
    }
    pub fn flush(&mut self) -> fmt::Result {
        if self.error != 0 {
            return Err(fmt::Error);
        }
        if self.used == 0 {
            return Ok(());
        }
        let status =
            unsafe { (self.sink.write)(self.sink.opaque, self.buffer.as_ptr(), self.used) };
        if status != 0 {
            self.error = status;
            return Err(fmt::Error);
        }
        self.used = 0;
        Ok(())
    }
    pub fn units(&mut self, mut units: &[u16]) -> fmt::Result {
        if self.error != 0 {
            return Err(fmt::Error);
        }
        while !units.is_empty() {
            let count = units.len().min(self.buffer.len() - self.used);
            self.buffer[self.used..self.used + count].copy_from_slice(&units[..count]);
            self.used += count;
            units = &units[count..];
            if self.used == self.buffer.len() {
                self.flush()?;
            }
        }
        Ok(())
    }
}
#[cfg(any(feature = "casing", feature = "names"))]
impl fmt::Write for TextOutput {
    fn write_str(&mut self, text: &str) -> fmt::Result {
        if self.error != 0 {
            return Err(fmt::Error);
        }
        // CaseMapper often writes one scalar at a time. Retain this buffer
        // across calls so a foreign callback does not become a per-scalar cost.
        for unit in text.encode_utf16() {
            self.buffer[self.used] = unit;
            self.used += 1;
            if self.used == self.buffer.len() {
                self.flush()?;
            }
        }
        Ok(())
    }
    fn write_char(&mut self, scalar: char) -> fmt::Result {
        self.units(scalar.encode_utf16(&mut [0; 2]))
    }
}

/* Private ICU4X segmentation adapter.
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
use crate::{BACKEND, RANGE, write_result};
use icu_locale_core::LanguageIdentifier;
use icu_segmenter::{
    GraphemeClusterSegmenter, SentenceSegmenter, WordSegmenter,
    options::{SentenceBreakOptions, WordBreakOptions},
};
use std::{
    ffi::c_void,
    panic::{AssertUnwindSafe, catch_unwind},
};

enum SegmentKind {
    Grapheme,
    Word(WordSegmenter),
    Sentence(SentenceSegmenter),
}
pub struct SegmentHandle(SegmentKind);
#[repr(C)]
#[derive(Clone, Copy)]
pub struct BoundarySink {
    opaque: *mut c_void,
    emit: unsafe extern "C" fn(*mut c_void, usize, i32) -> i32,
}

unsafe fn language(data: *const u8, length: usize) -> Result<LanguageIdentifier, i32> {
    if data.is_null() || length > isize::MAX as usize {
        return Err(RANGE);
    }
    LanguageIdentifier::try_from_utf8(unsafe { std::slice::from_raw_parts(data, length) })
        .map_err(|_| RANGE)
}
fn new(locale: &LanguageIdentifier, kind: u32) -> Result<SegmentHandle, i32> {
    Ok(SegmentHandle(match kind {
        0 => SegmentKind::Grapheme,
        1 => {
            let mut options = WordBreakOptions::default();
            options.content_locale = Some(locale);
            SegmentKind::Word(WordSegmenter::try_new_auto(options).map_err(|_| BACKEND)?)
        }
        2 => {
            let mut options = SentenceBreakOptions::default();
            options.content_locale = Some(locale);
            SegmentKind::Sentence(SentenceSegmenter::try_new(options).map_err(|_| BACKEND)?)
        }
        _ => return Err(RANGE),
    }))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_segment_check(
    data: *const u8,
    length: usize,
    version: *const u8,
    version_length: usize,
) -> i32 {
    match catch_unwind(|| {
        if version.is_null() || version_length > isize::MAX as usize {
            return Err(RANGE);
        }
        let version = unsafe { std::slice::from_raw_parts(version, version_length) };
        if version != icu_segmenter_data::cldr_tag!().as_bytes() {
            return Err(BACKEND);
        }
        let locale = unsafe { language(data, length)? };
        // A global rule marker is not an available-locale list. The C owner
        // supplies the matching CLDR registry and verifies each real engine.
        let _ = GraphemeClusterSegmenter::new();
        new(&locale, 1)?;
        new(&locale, 2)?;
        Ok(())
    }) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_segment_open(
    locale: *const u8,
    length: usize,
    kind: u32,
    out: *mut *mut SegmentHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            Ok(Box::into_raw(Box::new(new(
                &language(locale, length)?,
                kind,
            )?)))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_segment_close(handle: *mut SegmentHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}

unsafe fn boundaries(handle: &SegmentHandle, text: &[u16], sink: BoundarySink) -> Result<(), i32> {
    let emit = |end, word| {
        // ICU's first boundary is zero and describes no segment. Status belongs
        // to the segment preceding every later boundary, including the last.
        if end == 0 {
            return Ok(());
        }
        let status = unsafe { (sink.emit)(sink.opaque, end, word) };
        if status == 0 { Ok(()) } else { Err(status) }
    };
    match &handle.0 {
        SegmentKind::Grapheme => {
            for end in GraphemeClusterSegmenter::new().segment_utf16(text) {
                emit(end, 0)?;
            }
        }
        SegmentKind::Word(segmenter) => {
            for (end, word) in segmenter
                .as_borrowed()
                .segment_utf16(text)
                .iter_with_word_type()
            {
                emit(end, i32::from(word.is_word_like()))?;
            }
        }
        SegmentKind::Sentence(segmenter) => {
            for end in segmenter.as_borrowed().segment_utf16(text) {
                emit(end, 0)?;
            }
        }
    }
    Ok(())
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_segment_boundaries(
    handle: *const SegmentHandle,
    text: *const u16,
    length: usize,
    sink: BoundarySink,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        if handle.is_null() || length > isize::MAX as usize / 2 || (length != 0 && text.is_null()) {
            return Err(RANGE);
        }
        let text = if length == 0 {
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(text, length) }
        };
        unsafe { boundaries(&*handle, text, sink) }
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Output {
        rows: Vec<(usize, i32)>,
        calls: usize,
        fail_at: usize,
    }
    unsafe extern "C" fn emit(opaque: *mut c_void, end: usize, word: i32) -> i32 {
        let output = unsafe { &mut *(opaque as *mut Output) };
        output.calls += 1;
        if output.fail_at == output.calls {
            return -4;
        }
        output.rows.push((end, word));
        0
    }
    fn run(locale: &str, kind: u32, text: &[u16], output: &mut Output) -> i32 {
        let locale = LanguageIdentifier::try_from_str(locale).unwrap();
        let handle = new(&locale, kind).unwrap();
        unsafe {
            qjs_icu4x_segment_boundaries(
                &handle,
                text.as_ptr(),
                text.len(),
                BoundarySink {
                    opaque: output as *mut _ as *mut c_void,
                    emit,
                },
            )
        }
    }

    #[test]
    fn locale_tailoring_and_preceding_word_status() {
        let text: Vec<_> = "hello; world".encode_utf16().collect();
        let mut english = Output::default();
        let mut greek = Output::default();
        assert_eq!(run("en", 2, &text, &mut english), 0);
        assert_eq!(run("el", 2, &text, &mut greek), 0);
        assert_eq!(english.rows, [(12, 0)]);
        assert_eq!(greek.rows, [(7, 0), (12, 0)]);
        let text: Vec<_> = "abc 123".encode_utf16().collect();
        let mut words = Output::default();
        assert_eq!(run("en", 1, &text, &mut words), 0);
        assert_eq!(words.rows, [(3, 1), (4, 0), (7, 1)]);
    }

    #[test]
    fn callback_failure_stops_each_granularity() {
        // UTF-16 indices count both halves of a supplementary scalar; lone
        // surrogates and NUL remain original input, not replacement UTF-8.
        let text = [65, 0x301, 32, 0xd800, 0, 0xd83d, 0xde00];
        for kind in 0..3 {
            let mut expected = Output::default();
            assert_eq!(run("en", kind, &text, &mut expected), 0);
            assert_eq!(expected.rows.last().unwrap().0, text.len());
            for fail_at in 1..=expected.calls {
                let mut output = Output {
                    fail_at,
                    ..Default::default()
                };
                assert_eq!(run("en", kind, &text, &mut output), -4);
                assert_eq!(output.calls, fail_at);
                assert_eq!(output.rows, &expected.rows[..fail_at - 1]);
            }
        }
    }

    #[test]
    fn invalid_requests_preserve_outputs_and_reject_mixed_data() {
        let sentinel = std::ptr::dangling_mut::<SegmentHandle>();
        let mut output = sentinel;
        unsafe {
            assert_eq!(
                qjs_icu4x_segment_open(b"en".as_ptr(), 2, 3, &mut output),
                RANGE
            );
            assert_eq!(output, sentinel);
            assert_eq!(
                qjs_icu4x_segment_check(b"en".as_ptr(), 2, b"48.0.0".as_ptr(), 6),
                BACKEND
            );
            qjs_icu4x_segment_close(std::ptr::null_mut());
        }
    }
}

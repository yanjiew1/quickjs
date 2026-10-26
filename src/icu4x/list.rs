/* Private ICU4X list formatting bridge.
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
use crate::text::{Utf16, encode_utf16};
use crate::{BACKEND, RANGE, write_result};
use icu_list::{
    ListFormatter,
    options::{ListFormatterOptions, ListLength},
    parts,
};
use icu_locale_core::Locale;
use std::{
    ffi::c_void,
    fmt,
    panic::{AssertUnwindSafe, catch_unwind},
};
use writeable::{Part, PartsWrite, Writeable};

// Enumerate the real list markers, not another component's language list.
// These public generated macros provide iteration without enabling datagen.
mod inventory {
    use icu_list_data::*;
    pub mod icu {
        pub use icu_list as list;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_list_and_v1!(Provider, ITER);
    impl_list_or_v1!(Provider, ITER);
    impl_list_unit_v1!(Provider, ITER);
}

fn available() -> Result<&'static std::collections::BTreeSet<String>, i32> {
    use icu_list::provider::{ListAndV1, ListOrV1, ListUnitV1};
    use icu_provider::IterableDataProvider;
    use std::{collections::BTreeSet, sync::OnceLock};
    static TAGS: OnceLock<Result<BTreeSet<String>, i32>> = OnceLock::new();

    TAGS.get_or_init(|| {
        let and = <inventory::Provider as IterableDataProvider<ListAndV1>>::iter_ids(
            &inventory::Provider,
        )
        .map_err(|_| BACKEND)?;
        let or =
            <inventory::Provider as IterableDataProvider<ListOrV1>>::iter_ids(&inventory::Provider)
                .map_err(|_| BACKEND)?;
        let unit = <inventory::Provider as IterableDataProvider<ListUnitV1>>::iter_ids(
            &inventory::Provider,
        )
        .map_err(|_| BACKEND)?;
        // Baked data omits redundant rows: a width may inherit from root or
        // a parent. Admit actual locales from this module's three inventories,
        // then verify all nine formatter choices through normal data fallback.
        let mut candidates: BTreeSet<_> = and
            .iter()
            .chain(or.iter())
            .chain(unit.iter())
            .map(|id| id.locale.to_string())
            .collect();
        crate::locale_candidates::likely_regions(&mut candidates)?;
        let mut result = BTreeSet::new();
        for tag in candidates {
            if tag == "und" {
                continue;
            }
            let locale = Locale::try_from_str(&tag).map_err(|_| BACKEND)?;
            for kind in 0..3 {
                for width in 0..3 {
                    let options = ListFormatterOptions::default().with_length(match width {
                        0 => ListLength::Wide,
                        1 => ListLength::Short,
                        _ => ListLength::Narrow,
                    });
                    let prefs = locale.clone().into();
                    let formatter = match kind {
                        0 => ListFormatter::try_new_and(prefs, options),
                        1 => ListFormatter::try_new_or(prefs, options),
                        _ => ListFormatter::try_new_unit(prefs, options),
                    };
                    formatter.map_err(|_| BACKEND)?;
                }
            }
            // ECMA AvailableLocales also requires the scriptless counterpart.
            if locale.id.script.is_some() && locale.id.region.is_some() {
                let mut counterpart = locale.id.clone();
                counterpart.script = None;
                result.insert(counterpart.to_string());
            }
            result.insert(tag);
        }
        if result.is_empty() {
            return Err(BACKEND);
        }
        Ok(result)
    })
    .as_ref()
    .map_err(|status| *status)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_list_locales(
    opaque: *mut c_void,
    emit: unsafe extern "C" fn(*mut c_void, *const u8, usize) -> i32,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        for tag in available()? {
            // Borrowed only for this callback; C copies its owned inventory.
            let status = unsafe { emit(opaque, tag.as_ptr(), tag.len()) };
            if status != 0 {
                return Err(status);
            }
        }
        Ok(())
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}

#[derive(Clone, Copy)]
struct Item<'a>(&'a [u16]);
impl Writeable for Item<'_> {
    fn write_to<W: fmt::Write + ?Sized>(&self, output: &mut W) -> fmt::Result {
        // Contextual pattern inspection needs scalar text. Only inspection is
        // lossy: the actual element is copied unchanged by the parts sink.
        for scalar in char::decode_utf16(self.0.iter().copied()) {
            output.write_char(scalar.unwrap_or(char::REPLACEMENT_CHARACTER))?;
        }
        Ok(())
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Sink {
    opaque: *mut c_void,
    write: unsafe extern "C" fn(*mut c_void, *const u16, usize) -> i32,
    part: unsafe extern "C" fn(*mut c_void, usize, usize, u32) -> i32,
}

struct Output<'a> {
    sink: Sink,
    items: &'a [Utf16],
    element: usize,
    length: usize,
    error: i32,
}
impl Output<'_> {
    fn checked(&mut self, status: i32) -> fmt::Result {
        if status == 0 {
            Ok(())
        } else {
            self.error = status;
            Err(fmt::Error)
        }
    }
    fn units(&mut self, units: &[u16]) -> fmt::Result {
        let Some(length) = self.length.checked_add(units.len()) else {
            self.error = RANGE;
            return Err(fmt::Error);
        };
        // SAFETY: callbacks and opaque belong to the synchronous C call; the
        // borrowed span stays valid through the callback and is never retained.
        let status = unsafe { (self.sink.write)(self.sink.opaque, units.as_ptr(), units.len()) };
        self.checked(status)?;
        self.length = length;
        Ok(())
    }
}
impl fmt::Write for Output<'_> {
    fn write_str(&mut self, text: &str) -> fmt::Result {
        encode_utf16(text, |units| self.units(units))
    }
}
impl PartsWrite for Output<'_> {
    type SubPartsWrite = Self;
    fn with_part(
        &mut self,
        part: Part,
        mut operation: impl FnMut(&mut Self) -> fmt::Result,
    ) -> fmt::Result {
        let start = self.length;
        let kind;
        if part == parts::ELEMENT {
            let Some(item) = self.items.get(self.element) else {
                return Err(fmt::Error);
            };
            // SAFETY: every item was validated before formatting. Keep lone
            // surrogates, NULs and empty elements exactly as supplied by C.
            self.units(unsafe { item.units() }.map_err(|_| fmt::Error)?)?;
            self.element += 1;
            kind = 1;
        } else if part == parts::LITERAL {
            operation(self)?;
            if start == self.length {
                return Ok(());
            }
            kind = 0;
        } else {
            return Err(fmt::Error);
        }
        let status = unsafe { (self.sink.part)(self.sink.opaque, start, self.length, kind) };
        self.checked(status)
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_list_open(
    locale: *const u8,
    length: usize,
    kind: u32,
    width: u32,
    output: *mut *mut ListFormatter,
) -> i32 {
    unsafe {
        write_result(output, || {
            if length == 0 || length > isize::MAX as usize || locale.is_null() {
                return Err(RANGE);
            }
            let bytes = std::slice::from_raw_parts(locale, length);
            let locale = Locale::try_from_utf8(bytes).map_err(|_| RANGE)?;
            let width = match width {
                0 => ListLength::Wide,
                1 => ListLength::Short,
                2 => ListLength::Narrow,
                _ => return Err(RANGE),
            };
            let options = ListFormatterOptions::default().with_length(width);
            let formatter = match kind {
                0 => ListFormatter::try_new_and(locale.into(), options),
                1 => ListFormatter::try_new_or(locale.into(), options),
                2 => ListFormatter::try_new_unit(locale.into(), options),
                _ => return Err(RANGE),
            }
            .map_err(|_| BACKEND)?;
            Ok(Box::into_raw(Box::new(formatter)))
        })
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_list_close(formatter: *mut ListFormatter) {
    if !formatter.is_null() {
        // SAFETY: one close consumes the live owned handle returned by open.
        unsafe {
            drop(Box::from_raw(formatter));
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_list_format(
    formatter: *const ListFormatter,
    items: *const Utf16,
    count: usize,
    sink: Sink,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        if formatter.is_null()
            || count > isize::MAX as usize / std::mem::size_of::<Utf16>()
            || (count != 0 && items.is_null())
        {
            return Err(RANGE);
        }
        let items = if count == 0 {
            &[]
        } else {
            unsafe { std::slice::from_raw_parts(items, count) }
        };
        for item in items {
            unsafe {
                item.units()?;
            }
        }
        let values = items
            .iter()
            .map(|item| Item(unsafe { item.units().unwrap() }));
        let mut output = Output {
            sink,
            items,
            element: 0,
            length: 0,
            error: 0,
        };
        let result = unsafe { &*formatter }
            .format(values)
            .write_to_parts(&mut output);
        if result.is_err() {
            return Err(if output.error != 0 {
                output.error
            } else {
                BACKEND
            });
        }
        if output.element != count {
            return Err(BACKEND);
        }
        Ok(())
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(error)) => error,
        Err(_) => BACKEND,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[derive(Default)]
    struct Collected {
        text: Vec<u16>,
        parts: Vec<(usize, usize, u32)>,
    }
    unsafe extern "C" fn write(opaque: *mut c_void, text: *const u16, length: usize) -> i32 {
        let output = unsafe { &mut *opaque.cast::<Collected>() };
        output
            .text
            .extend_from_slice(unsafe { std::slice::from_raw_parts(text, length) });
        0
    }
    unsafe extern "C" fn part(opaque: *mut c_void, start: usize, end: usize, kind: u32) -> i32 {
        unsafe { &mut *opaque.cast::<Collected>() }
            .parts
            .push((start, end, kind));
        0
    }
    fn format(locale: &str, inputs: &[&[u16]]) -> Collected {
        let mut formatter = std::ptr::null_mut();
        assert_eq!(
            unsafe { qjs_icu4x_list_open(locale.as_ptr(), locale.len(), 0, 0, &mut formatter) },
            0
        );
        let items: Vec<_> = inputs
            .iter()
            .map(|text| Utf16 {
                data: text.as_ptr(),
                length: text.len(),
            })
            .collect();
        let mut output = Collected::default();
        let sink = Sink {
            opaque: (&mut output as *mut Collected).cast(),
            write,
            part,
        };
        assert_eq!(
            unsafe { qjs_icu4x_list_format(formatter, items.as_ptr(), items.len(), sink) },
            0
        );
        unsafe {
            qjs_icu4x_list_close(formatter);
        }
        output
    }
    #[test]
    fn actual_locale_inventory() {
        let tags = available().unwrap();
        assert!(tags.contains("en"));
        assert!(tags.contains("es"));
        assert!(tags.contains("he"));
        assert!(!tags.contains("und"));
        for tag in tags {
            for kind in 0..3 {
                for width in 0..3 {
                    let mut formatter = std::ptr::null_mut();
                    assert_eq!(
                        unsafe {
                            qjs_icu4x_list_open(
                                tag.as_ptr(),
                                tag.len(),
                                kind,
                                width,
                                &mut formatter,
                            )
                        },
                        0
                    );
                    unsafe {
                        qjs_icu4x_list_close(formatter);
                    }
                }
            }
        }
    }

    #[test]
    fn original_utf16_and_empty_parts() {
        let a = [0xd800, 0, b'a' as u16];
        let b = [0xdc00];
        let output = format("en", &[&a, &[], &b]);
        let elements: Vec<_> = output.parts.iter().filter(|p| p.2 == 1).collect();
        assert_eq!(elements.len(), 3);
        assert_eq!(&output.text[elements[0].0..elements[0].1], &a);
        assert_eq!(elements[1].0, elements[1].1);
        assert_eq!(&output.text[elements[2].0..elements[2].1], &b);
        assert!(format("en", &[]).parts.is_empty());
        assert_eq!(format("en", &[&[]]).parts, [(0, 0, 1)]);
    }
    #[test]
    fn callback_failure_stops_output() {
        #[derive(Default)]
        struct Failed {
            writes: usize,
            parts: usize,
            emits: usize,
            fail_part: bool,
        }
        unsafe extern "C" fn write(opaque: *mut c_void, _: *const u16, _: usize) -> i32 {
            let state = unsafe { &mut *opaque.cast::<Failed>() };
            state.writes += 1;
            if state.fail_part { 0 } else { -4 }
        }
        unsafe extern "C" fn part(opaque: *mut c_void, _: usize, _: usize, _: u32) -> i32 {
            unsafe { &mut *opaque.cast::<Failed>() }.parts += 1;
            -4
        }
        unsafe extern "C" fn emit(opaque: *mut c_void, _: *const u8, _: usize) -> i32 {
            unsafe { &mut *opaque.cast::<Failed>() }.emits += 1;
            -4
        }
        let mut formatter = std::ptr::null_mut();
        assert_eq!(
            unsafe { qjs_icu4x_list_open(b"en".as_ptr(), 2, 0, 0, &mut formatter) },
            0
        );
        let units = [b'A' as u16];
        let item = Utf16 {
            data: units.as_ptr(),
            length: units.len(),
        };
        let items = [item, item];
        for fail_part in [false, true] {
            let mut state = Failed {
                fail_part,
                ..Default::default()
            };
            let sink = Sink {
                opaque: (&mut state as *mut Failed).cast(),
                write,
                part,
            };
            assert_eq!(
                unsafe { qjs_icu4x_list_format(formatter, items.as_ptr(), items.len(), sink) },
                -4
            );
            assert_eq!(state.writes, 1);
            assert_eq!(state.parts, usize::from(fail_part));
        }
        let mut state = Failed::default();
        assert_eq!(
            unsafe { qjs_icu4x_list_locales((&mut state as *mut Failed).cast(), emit) },
            -4
        );
        assert_eq!(state.emits, 1);
        unsafe {
            qjs_icu4x_list_close(formatter);
        }
    }

    #[test]
    fn contextual_patterns() {
        let a: Vec<_> = "España".encode_utf16().collect();
        let b: Vec<_> = "Italia".encode_utf16().collect();
        assert_eq!(
            String::from_utf16(&format("es", &[&a, &b]).text).unwrap(),
            "España e Italia"
        );
        let a: Vec<_> = "א".encode_utf16().collect();
        let b: Vec<_> = "Latin".encode_utf16().collect();
        assert_eq!(
            String::from_utf16(&format("he", &[&a, &b]).text).unwrap(),
            "א ו\u{2011}Latin"
        );
        let surrogate = [0xd800];
        let output = format("he", &[&a, &surrogate]);
        let mut expected: Vec<_> = "א ו\u{2011}".encode_utf16().collect();
        expected.push(0xd800);
        assert_eq!(output.text, expected);
    }
}

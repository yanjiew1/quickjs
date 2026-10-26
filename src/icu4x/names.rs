/* Private ICU4X display-name adapter.
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
use crate::text::{TextOutput, TextSink};
use crate::{BACKEND, RANGE, UNSUPPORTED, write_result};
use icu_locale::names::{
    DisplayNamesPreferences, LanguageIdentifierDisplayName, LanguageIdentifierDisplayNameOptions,
    RegionDisplayName, ScriptDisplayName,
};
use icu_locale_core::{
    LanguageIdentifier, Locale,
    subtags::{Language, Region, Script},
};
use icu_provider::DataErrorKind;
use std::{
    fmt,
    panic::{AssertUnwindSafe, catch_unwind},
};
use writeable::{TryWriteable, Writeable};

// One public generated provider owns both name queries and their real locale
// inventory. No duplicate ordinary-name supplement or generated-source parser.
mod inventory {
    use icu_locale_data::*;
    pub mod icu {
        pub use icu_locale as locale;
    }
    pub struct Provider;
    make_provider!(Provider);
    impl_locale_names_language_medium_heavy_v1!(Provider, ITER);
    impl_locale_names_language_medium_light_v1!(Provider, ITER);
    impl_locale_names_language_medium_tiny_v1!(Provider, ITER);
    impl_locale_names_language_short_heavy_v1!(Provider, ITER);
    impl_locale_names_language_short_light_v1!(Provider, ITER);
    impl_locale_names_script_medium_heavy_v1!(Provider, ITER);
    impl_locale_names_script_medium_light_v1!(Provider, ITER);
    impl_locale_names_script_medium_tiny_v1!(Provider, ITER);
    impl_locale_names_script_short_heavy_v1!(Provider, ITER);
    impl_locale_names_region_medium_light_v1!(Provider, ITER);
    impl_locale_names_region_medium_tiny_v1!(Provider, ITER);
    impl_locale_names_region_short_light_v1!(Provider, ITER);
    impl_locale_names_region_short_tiny_v1!(Provider, ITER);
    impl_locale_names_variant_medium_heavy_v1!(Provider, ITER);
    impl_locale_names_essentials_v1!(Provider, ITER);
}

fn available() -> Result<&'static std::collections::BTreeSet<String>, i32> {
    use icu_provider::{DataMarker, IterableDataProvider};
    use std::{collections::BTreeSet, sync::OnceLock};
    static LOCALES: OnceLock<Result<BTreeSet<String>, i32>> = OnceLock::new();
    LOCALES
        .get_or_init(|| {
            fn add<M: DataMarker>(result: &mut BTreeSet<String>) -> Result<(), i32>
            where
                inventory::Provider: IterableDataProvider<M>,
            {
                // Drop each marker's temporary IDs before loading the next one;
                // payload attributes multiply records without adding locale tags.
                for id in
                    <inventory::Provider as IterableDataProvider<M>>::iter_ids(&inventory::Provider)
                        .map_err(|_| BACKEND)?
                {
                    let text = id.locale.to_string();
                    if text == "und" {
                        continue;
                    }
                    let locale = Locale::try_from_str(&text).map_err(|_| BACKEND)?;
                    if locale.id.script.is_some() && locale.id.region.is_some() {
                        let mut counterpart = locale.id.clone();
                        counterpart.script = None;
                        result.insert(counterpart.to_string());
                    }
                    result.insert(locale.id.to_string());
                }
                Ok(())
            }
            let mut result = BTreeSet::new();
            add::<icu_locale::provider::names::LocaleNamesLanguageMediumHeavyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesLanguageMediumLightV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesLanguageMediumTinyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesLanguageShortHeavyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesLanguageShortLightV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesScriptMediumHeavyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesScriptMediumLightV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesScriptMediumTinyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesScriptShortHeavyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesRegionMediumLightV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesRegionMediumTinyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesRegionShortLightV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesRegionShortTinyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesVariantMediumHeavyV1>(&mut result)?;
            add::<icu_locale::provider::names::LocaleNamesEssentialsV1>(&mut result)?;
            Ok(result)
        })
        .as_ref()
        .map_err(|status| *status)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_locales(
    opaque: *mut std::ffi::c_void,
    emit: unsafe extern "C" fn(*mut std::ffi::c_void, *const u8, usize) -> i32,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        for locale in available()? {
            let status = unsafe { emit(opaque, locale.as_ptr(), locale.len()) };
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

pub struct NameHandle {
    prefs: DisplayNamesPreferences,
    short: bool,
    dialect: bool,
    language: Language,
    capitalization: u32,
}
unsafe fn input<'a>(data: *const u8, length: usize) -> Result<&'a [u8], i32> {
    if data.is_null() || length > isize::MAX as usize {
        return Err(RANGE);
    }
    Ok(unsafe { std::slice::from_raw_parts(data, length) })
}
fn data_error(error: icu_provider::DataError) -> i32 {
    if error.kind == DataErrorKind::IdentifierNotFound {
        UNSUPPORTED
    } else {
        BACKEND
    }
}
struct Discard;
impl fmt::Write for Discard {
    fn write_str(&mut self, _: &str) -> fmt::Result {
        Ok(())
    }
}
fn write<T: Writeable>(name: T, sink: TextSink) -> Result<(), i32> {
    let mut output = TextOutput::new(sink);
    if name
        .write_to(&mut output)
        .and_then(|_| output.flush())
        .is_err()
    {
        return Err(if output.error != 0 {
            output.error
        } else {
            BACKEND
        });
    }
    Ok(())
}

fn capitalized(handle: &NameHandle, text: &str, sink: TextSink) -> Result<(), i32> {
    use icu_casemap::{
        CaseMapper,
        options::{LeadingAdjustment, TitlecaseOptions, TrailingCase},
    };
    use icu_properties::{CodePointSetData, props::Lowercase};

    // Match the standalone context's lowercase-initial policy using the
    // selected ICU4X Unicode data, not the Rust standard library's version.
    if !text
        .chars()
        .next()
        .is_some_and(|c| CodePointSetData::new::<Lowercase>().contains(c))
    {
        return write(text, sink);
    }
    let mut language = LanguageIdentifier::UNKNOWN;
    language.language = handle.language;
    let mut options = TitlecaseOptions::default();
    options.leading_adjustment = Some(LeadingAdjustment::None);
    options.trailing_case = Some(TrailingCase::Unchanged);
    write(
        CaseMapper::new().titlecase_segment_with_only_case_data(text, &language, options),
        sink,
    )
}

fn write_name<T: Writeable>(
    handle: &NameHandle,
    kind: u32,
    name: T,
    sink: TextSink,
) -> Result<(), i32> {
    if handle.capitalization & (1 << kind) != 0 {
        capitalized(handle, &name.write_to_string(), sink)
    } else {
        write(name, sink)
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_open(
    locale: *const u8,
    length: usize,
    style: u32,
    dialect: u32,
    out: *mut *mut NameHandle,
) -> i32 {
    unsafe { qjs_icu4x_names_open_context(locale, length, style, dialect, 0, out) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_open_context(
    locale: *const u8,
    length: usize,
    style: u32,
    dialect: u32,
    capitalization: u32,
    out: *mut *mut NameHandle,
) -> i32 {
    unsafe {
        write_result(out, || {
            if style > 2 || dialect > 1 || capitalization & !23 != 0 {
                return Err(RANGE);
            }
            let locale = Locale::try_from_utf8(input(locale, length)?).map_err(|_| RANGE)?;
            let language = locale.id.language;
            Ok(Box::into_raw(Box::new(NameHandle {
                prefs: locale.into(),
                short: style != 0,
                dialect: dialect != 0,
                language,
                capitalization,
            })))
        })
    }
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_close(handle: *mut NameHandle) {
    if !handle.is_null() {
        unsafe {
            drop(Box::from_raw(handle));
        }
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_format(
    handle: *const NameHandle,
    kind: u32,
    code: *const u8,
    length: usize,
    sink: TextSink,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        if handle.is_null() {
            return Err(RANGE);
        }
        let handle = unsafe { &*handle };
        let code = unsafe { input(code, length)? };
        match kind {
            0 => {
                use icu_locale::names::LanguageDisplay;
                let language = LanguageIdentifier::try_from_utf8(code).map_err(|_| RANGE)?;
                let mut options = LanguageIdentifierDisplayNameOptions::default();
                options.language_display = Some(if handle.dialect {
                    LanguageDisplay::Dialect
                } else {
                    LanguageDisplay::Standard
                });
                let name = if handle.short {
                    LanguageIdentifierDisplayName::try_new_short_heavy_unstable(
                        &inventory::Provider,
                        handle.prefs,
                        language,
                        options,
                    )
                } else {
                    LanguageIdentifierDisplayName::try_new_heavy_unstable(
                        &inventory::Provider,
                        handle.prefs,
                        language,
                        options,
                    )
                }
                .map_err(data_error)?;
                let name = name.as_borrowed();
                // Absence can be reported by composition after a known prefix.
                // Detect it before writing or enforcing a C output budget;
                // comparing rendered text to its code cannot prove absence.
                if name
                    .try_write_to(&mut Discard)
                    .map_err(|_| BACKEND)?
                    .is_err()
                {
                    return Err(UNSUPPORTED);
                }
                if handle.capitalization & 1 != 0 {
                    let text = name.try_write_to_string().map_err(|_| BACKEND)?;
                    return capitalized(handle, &text, sink);
                }
                let mut output = TextOutput::new(sink);
                let result = name.try_write_to(&mut output);
                if result.is_err() || output.flush().is_err() {
                    return Err(if output.error != 0 {
                        output.error
                    } else {
                        BACKEND
                    });
                }
                result.map_err(|_| BACKEND)?.map_err(|_| BACKEND)
            }
            1 => {
                let region = Region::try_from_utf8(code).map_err(|_| RANGE)?;
                let name = if handle.short {
                    RegionDisplayName::try_new_short_light_unstable(
                        &inventory::Provider,
                        handle.prefs,
                        region,
                    )
                } else {
                    RegionDisplayName::try_new_light_unstable(
                        &inventory::Provider,
                        handle.prefs,
                        region,
                    )
                }
                .map_err(data_error)?;
                write_name(handle, kind, name, sink)
            }
            2 => {
                let script = Script::try_from_utf8(code).map_err(|_| RANGE)?;
                let name = if handle.short {
                    ScriptDisplayName::try_new_short_heavy_unstable(
                        &inventory::Provider,
                        handle.prefs,
                        script,
                    )
                } else {
                    ScriptDisplayName::try_new_heavy_unstable(
                        &inventory::Provider,
                        handle.prefs,
                        script,
                    )
                }
                .map_err(data_error)?;
                write_name(handle, kind, name, sink)
            }
            #[cfg(feature = "currency_names")]
            3 => {
                let name = crate::currency::plain_name(handle.prefs.locale_preferences, code)?;
                write_name(handle, kind, &**name.get(), sink)
            }
            _ => Err(RANGE),
        }
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_text(
    handle: *const NameHandle,
    kind: u32,
    text: *const u8,
    length: usize,
    sink: TextSink,
) -> i32 {
    match catch_unwind(AssertUnwindSafe(|| {
        if handle.is_null() || kind > 5 {
            return Err(RANGE);
        }
        let handle = unsafe { &*handle };
        let text = std::str::from_utf8(unsafe { input(text, length)? }).map_err(|_| RANGE)?;
        write_name(handle, kind, text, sink)
    })) {
        Ok(Ok(())) => 0,
        Ok(Err(status)) => status,
        Err(_) => BACKEND,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_names_check_version(text: *const u8, length: usize) -> i32 {
    if text.is_null() || length > isize::MAX as usize {
        return RANGE;
    }
    if unsafe { std::slice::from_raw_parts(text, length) }
        == icu_locale_data::cldr_tag!().as_bytes()
    {
        0
    } else {
        BACKEND
    }
}

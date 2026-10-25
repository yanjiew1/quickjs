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
use crate::{BACKEND, MEMORY, RANGE, UNSUPPORTED, write_result};
use icu_calendar::{
    AnyCalendar, AnyCalendarKind, Date, Iso, Ref,
    options::{DateAddOptions, DateFromFieldsOptions, Overflow},
    types::{DateDuration, DateFields, RataDie},
};
use icu_provider::DataProvider;
use std::{
    ffi::c_char,
    sync::{Mutex, OnceLock},
};

const EPOCH_RD: i64 = 719_163;
// Keep every calendar's metadata safely inside ICU4X's ISO fundamental range.
// This is wider than all observable Temporal calendar years, including Hijri.
const METADATA_YEAR_LIMIT: i32 = 900_000;

// Private IDs mirror QJSCalendarId. C integration tests exercise every ID's
// known conversion, so an enum change cannot silently select another calendar.
fn calendar(id: u32) -> Result<AnyCalendar, i32> {
    let kind = match id {
        1 => AnyCalendarKind::Buddhist,
        2 => AnyCalendarKind::Chinese,
        3 => AnyCalendarKind::Coptic,
        4 => AnyCalendarKind::Dangi,
        5 => AnyCalendarKind::EthiopianAmeteAlem,
        6 => AnyCalendarKind::Ethiopian,
        7 => AnyCalendarKind::Gregorian,
        8 => AnyCalendarKind::Hebrew,
        9 => AnyCalendarKind::Indian,
        10 => AnyCalendarKind::HijriTabularTypeIIFriday,
        11 => AnyCalendarKind::HijriTabularTypeIIThursday,
        12 => AnyCalendarKind::HijriUmmAlQura,
        13 => AnyCalendarKind::Japanese,
        14 => AnyCalendarKind::Persian,
        15 => AnyCalendarKind::Roc,
        _ => return Err(UNSUPPORTED),
    };
    Ok(AnyCalendar::new(kind))
}

fn date(calendar: &AnyCalendar, year: i32, month: u8) -> Result<Date<Ref<'_, AnyCalendar>>, i32> {
    let mut fields = DateFields::default();
    fields.extended_year = Some(year);
    fields.ordinal_month = Some(month);
    fields.day = Some(1);
    let mut options = DateFromFieldsOptions::default();
    options.overflow = Some(Overflow::Reject);
    // Unlike the convenience constructors, this supports the full Temporal
    // domain. Public Date still enforces its separate fundamental ISO bounds.
    Date::try_from_fields(fields, options, Ref(calendar)).map_err(|_| RANGE)
}

#[repr(C)]
#[derive(Default)]
pub struct YearLayout {
    first_day: i64,
    end_day: i64,
    lengths: [u8; 13],
    months: i32,
    leap_month: i32,
}

fn year_layout(id: u32, year: i32) -> Result<YearLayout, i32> {
    let calendar = calendar(id)?;
    if !(-METADATA_YEAR_LIMIT..=METADATA_YEAR_LIMIT).contains(&year) {
        return Err(RANGE);
    }
    let first = date(&calendar, year, 1)?;
    let mut result = YearLayout {
        first_day: first.to_rata_die().to_i64_date() - EPOCH_RD,
        months: i32::from(first.months_in_year()),
        ..Default::default()
    };
    if !(12..=13).contains(&result.months) || first.year().extended_year() != year {
        return Err(BACKEND);
    }
    let mut end = result.first_day;
    for index in 0..result.months as usize {
        let value = date(&calendar, year, (index + 1) as u8)?;
        let month = value.month();
        let length = value.days_in_month();
        if value.to_rata_die().to_i64_date() - EPOCH_RD != end
            || month.ordinal as usize != index + 1
            || !(5..=31).contains(&length)
        {
            return Err(BACKEND);
        }
        result.lengths[index] = length;
        end += i64::from(length);
        if month.to_input().is_leap() {
            if result.leap_month != 0 {
                return Err(BACKEND);
            }
            result.leap_month = i32::from(month.number());
        }
    }
    if end - result.first_day != i64::from(first.days_in_year()) {
        return Err(BACKEND);
    }
    result.end_day = end;
    Ok(result)
}

/// # Safety
/// `output` must be a writable C QJSCalendarYearLayout, if non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_calendar_year(
    id: u32,
    year: i32,
    output: *mut YearLayout,
) -> i32 {
    unsafe { write_result(output, || year_layout(id, year)) }
}

#[repr(C)]
#[derive(Default)]
pub struct CalendarDate {
    year: i32,
    month: i32,
    day: i32,
    day_of_year: i32,
    days_in_month: i32,
    days_in_year: i32,
    months_in_year: i32,
    in_leap_year: i32,
    month_code: [u8; 5],
}

fn from_day(id: u32, day: i64) -> Result<CalendarDate, i32> {
    let calendar = calendar(id)?;
    // This broad guard brackets ICU4X2.4's entire fundamental date domain and
    // keeps hostile i64 inputs out of RataDie's internal arithmetic invariant.
    // The exact foreign bounds are checked by the roundtrip below, not guessed.
    if !(-400_000_000..=400_000_000).contains(&day) {
        return Err(RANGE);
    }
    let rd = RataDie::new(day + EPOCH_RD);
    let value = Date::from_rata_die(rd, Ref(&calendar));
    if value.to_rata_die() != rd {
        return Err(RANGE); // The public ICU4X constructor clamps out-of-range RD.
    }
    let month = value.month();
    let code = month.to_input().code();
    let mut result = CalendarDate {
        year: value.year().extended_year(),
        month: i32::from(month.ordinal),
        day: i32::from(value.day_of_month().0),
        day_of_year: i32::from(value.day_of_year().0),
        days_in_month: i32::from(value.days_in_month()),
        days_in_year: i32::from(value.days_in_year()),
        months_in_year: i32::from(value.months_in_year()),
        in_leap_year: i32::from(value.is_in_leap_year()),
        ..Default::default()
    };
    let bytes = code.0.as_bytes();
    if !(3..=4).contains(&bytes.len()) {
        return Err(BACKEND);
    }
    result.month_code[..bytes.len()].copy_from_slice(bytes);
    Ok(result)
}

/// # Safety
/// `output` must be a writable C QJSICU4XCalendarDate, if non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_calendar_date(
    id: u32,
    day: i64,
    output: *mut CalendarDate,
) -> i32 {
    unsafe { write_result(output, || from_day(id, day)) }
}

struct StoredEra {
    first_day: i32,
    year_offset: i32,
    name: [u8; 9],
}

#[repr(C)]
pub struct Era {
    first_day: i32,
    year_offset: i32,
    name: *const c_char,
}

static ERAS: OnceLock<Vec<StoredEra>> = OnceLock::new();
static ERA_INITIALIZER: Mutex<()> = Mutex::new(());

fn load_eras() -> Result<Vec<StoredEra>, i32> {
    use icu_calendar::provider::{Baked, CalendarJapaneseModernV1};
    let response =
        <Baked as DataProvider<CalendarJapaneseModernV1>>::load(&Baked, Default::default())
            .map_err(|_| BACKEND)?;
    let eras = response.payload.get();
    let mut result = Vec::new();
    result
        .try_reserve_exact(eras.eras().count())
        .map_err(|_| MEMORY)?;
    // Modern Japanese arithmetic begins at Meiji6, while its era-year offset
    // still uses the emperor's original accession year.
    let cutoff = Date::try_new_iso(1873, 1, 1)
        .map_err(|_| BACKEND)?
        .to_rata_die()
        .to_i64_date()
        - EPOCH_RD;
    for (start, code, _) in eras.eras() {
        let mut fields = DateFields::default();
        fields.extended_year = Some(start.year);
        fields.ordinal_month = Some(start.month);
        fields.day = Some(start.day);
        let date = Date::try_from_fields(fields, Default::default(), Iso).map_err(|_| BACKEND)?;
        let mut name = [0; 9];
        let bytes = code.as_bytes();
        if bytes.is_empty() || bytes.len() >= name.len() {
            return Err(BACKEND);
        }
        name[..bytes.len()].copy_from_slice(bytes);
        let epoch = date.to_rata_die().to_i64_date() - EPOCH_RD;
        let first_day = i32::try_from(epoch.max(cutoff)).map_err(|_| BACKEND)?;
        result.push(StoredEra {
            first_day,
            year_offset: start.year - 1,
            name,
        });
    }
    Ok(result)
}

fn eras() -> Result<&'static [StoredEra], i32> {
    if let Some(records) = ERAS.get() {
        return Ok(records);
    }
    // Failed or panicking initialization publishes nothing and can retry.
    // Recovering a poisoned lock is safe because the result has one owner and
    // is installed atomically only after the whole immutable vector is valid.
    let _guard = ERA_INITIALIZER
        .lock()
        .unwrap_or_else(|error| error.into_inner());
    if let Some(records) = ERAS.get() {
        return Ok(records);
    }
    let records = load_eras()?;
    Ok(ERAS.get_or_init(|| records))
}

/// # Safety
/// `output` must be a writable size_t, if non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_japanese_era_count(output: *mut usize) -> i32 {
    unsafe { write_result(output, || Ok(eras()?.len())) }
}

/// # Safety
/// `output` must be a writable C QJSCalendarEra, if non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_japanese_era_at(index: usize, output: *mut Era) -> i32 {
    unsafe {
        write_result(output, || {
            let record = eras()?.get(index).ok_or(UNSUPPORTED)?;
            Ok(Era {
                first_day: record.first_day,
                year_offset: record.year_offset,
                name: record.name.as_ptr().cast(),
            })
        })
    }
}

#[repr(C)]
#[derive(Debug, PartialEq, Eq)]
pub struct YearMonth {
    year: i32,
    month: i32,
}

fn add_months(id: u32, year: i32, month: i32, delta: i64) -> Result<YearMonth, i32> {
    if !(-METADATA_YEAR_LIMIT..=METADATA_YEAR_LIMIT).contains(&year)
        || !(-26_000_013..=26_000_013).contains(&delta)
    {
        return Err(RANGE);
    }
    let calendar = calendar(id)?;
    let input = date(&calendar, year, u8::try_from(month).map_err(|_| RANGE)?)?;
    let mut options = DateAddOptions::default();
    options.overflow = Some(Overflow::Reject);
    // Delegate the actual month sequence to ICU4X, including its distant
    // Chinese/Dangi model, rather than assuming another provider's moon phase.
    let value = input
        .try_added_with_options(DateDuration::for_months(delta as i32), options)
        .map_err(|_| RANGE)?;
    let year = value.year().extended_year();
    if !(-METADATA_YEAR_LIMIT..=METADATA_YEAR_LIMIT).contains(&year) {
        return Err(RANGE);
    }
    Ok(YearMonth {
        year,
        month: i32::from(value.month().ordinal),
    })
}

/// # Safety
/// `output` must be a writable C QJSICU4XCalendarYearMonth, if non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn qjs_icu4x_calendar_add_months(
    id: u32,
    year: i32,
    month: i32,
    delta: i64,
    output: *mut YearMonth,
) -> i32 {
    unsafe { write_result(output, || add_months(id, year, month, delta)) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn full_temporal_roundtrips_and_year_layouts() {
        for id in 1..=15 {
            for day in [-100_000_000, -719_163, 0, 100_000_000] {
                let date = from_day(id, day).unwrap();
                let layout = year_layout(id, date.year).unwrap();
                let index = (date.month - 1) as usize;
                let rebuilt = layout.first_day
                    + layout.lengths[..index]
                        .iter()
                        .map(|v| i64::from(*v))
                        .sum::<i64>()
                    + i64::from(date.day)
                    - 1;
                assert_eq!(rebuilt, day, "calendar {id}");
            }
        }
    }

    #[test]
    fn metadata_bounds_do_not_clamp() {
        for id in 1..=15 {
            for year in [-METADATA_YEAR_LIMIT, -1, 0, 2024, METADATA_YEAR_LIMIT] {
                let layout = year_layout(id, year).unwrap();
                assert_eq!(from_day(id, layout.first_day).unwrap().year, year);
                assert_eq!(from_day(id, layout.end_day - 1).unwrap().year, year);
            }
            assert_eq!(year_layout(id, i32::MIN).err(), Some(RANGE));
            assert_eq!(year_layout(id, i32::MAX).err(), Some(RANGE));
        }
    }

    #[test]
    fn public_month_arithmetic_roundtrips() {
        for id in 1..=15 {
            for year in [-200_000, 0, 2024, 275_760] {
                for delta in [-12_001, -1, 0, 1, 12_001] {
                    let target = add_months(id, year, 1, delta).unwrap();
                    assert_eq!(
                        add_months(id, target.year, target.month, -delta).unwrap(),
                        YearMonth { year, month: 1 },
                        "calendar {id}, year {year}, delta {delta}"
                    );
                }
            }
        }
        assert_eq!(add_months(2, 2024, 1, i64::MAX).err(), Some(RANGE));
        assert_eq!(add_months(2, 2024, 0, 0).err(), Some(RANGE));
        assert_eq!(add_months(4, 2024, 99, 0).err(), Some(RANGE));
    }

    #[test]
    fn invalid_calls_preserve_outputs() {
        let mut result = 42_i64;
        assert_eq!(unsafe { write_result(&mut result, || Err(RANGE)) }, RANGE);
        assert_eq!(result, 42);
        assert_eq!(
            unsafe { write_result(&mut result, || -> Result<i64, i32> { panic!("test") }) },
            BACKEND
        );
        assert_eq!(result, 42);
        assert_eq!(from_day(7, i64::MAX).err(), Some(RANGE));
        assert_eq!(year_layout(0, 2024).err(), Some(UNSUPPORTED));
    }

    #[test]
    fn era_records_have_stable_names() {
        let records = eras().unwrap();
        assert!(records.len() >= 5);
        assert_eq!(&records[0].name[..5], b"meiji");
        assert_eq!(records[0].year_offset, 1867);
        assert_eq!(records[0].first_day, -35_428);
        assert!(records.iter().any(|era| &era.name[..5] == b"reiwa"));
        assert!(
            records
                .windows(2)
                .all(|pair| pair[0].first_day < pair[1].first_day)
        );
    }
}

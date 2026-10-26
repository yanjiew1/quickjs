/* Private ICU4X packed plural-metadata selection.
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
use crate::{BACKEND, RANGE};
use icu_plurals::provider::{PluralRulesData, PluralsCardinalV1, rules::runtime::ast::Rule};
use icu_plurals::{PluralCategory, PluralRules};
use icu_provider::{DataError, DataPayload, DataProvider, DataRequest, DataResponse};

// This provider does not classify an amount. It makes the public packed-data
// getter select a category already computed by the exact C evaluator. The
// artificial operands below only choose explicit 0/1 or ordinary metadata.
struct CategorySelector(PluralCategory);
impl DataProvider<PluralsCardinalV1> for CategorySelector {
    fn load(&self, _: DataRequest) -> Result<DataResponse<PluralsCardinalV1>, DataError> {
        let mut rules = PluralRulesData::default();
        let always = Some(Rule(zerovec::VarZeroVec::new()));
        match self.0 {
            PluralCategory::Zero => rules.zero = always,
            PluralCategory::One => rules.one = always,
            PluralCategory::Two => rules.two = always,
            PluralCategory::Few => rules.few = always,
            PluralCategory::Many => rules.many = always,
            PluralCategory::Other => {}
        }
        Ok(DataResponse {
            metadata: Default::default(),
            payload: DataPayload::from_owned(rules),
        })
    }
}
pub(crate) fn selector(category: u32) -> Result<PluralRules, i32> {
    let category = match category {
        0 => PluralCategory::Zero,
        1 => PluralCategory::One,
        2 => PluralCategory::Two,
        3 => PluralCategory::Few,
        4 => PluralCategory::Many,
        5 => PluralCategory::Other,
        _ => return Err(RANGE),
    };
    PluralRules::try_new_cardinal_unstable(&CategorySelector(category), Default::default())
        .map_err(|_| BACKEND)
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Text {
    pub(crate) data: *const u8,
    pub(crate) length: usize,
}
impl From<&str> for Text {
    fn from(value: &str) -> Self {
        Self {
            data: value.as_ptr(),
            length: value.len(),
        }
    }
}

// Keep decoded literal spans distinct from the numeric placeholder. This is
// shared by compact, relative and unit metadata; no caller reparses braces.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SinglePattern {
    pub(crate) prefix: Text,
    pub(crate) suffix: Text,
    pub(crate) has_number: u32,
}
pub(crate) fn single_pattern(
    pattern: &icu_pattern::SinglePlaceholderPattern,
) -> Result<SinglePattern, i32> {
    use icu_pattern::PatternItem;
    let mut result = SinglePattern {
        prefix: "".into(),
        suffix: "".into(),
        has_number: 0,
    };
    for item in pattern.iter() {
        match item {
            PatternItem::Literal(value) => {
                let target = if result.has_number == 0 {
                    &mut result.prefix
                } else {
                    &mut result.suffix
                };
                if target.length != 0 {
                    return Err(BACKEND);
                }
                *target = value.into();
            }
            PatternItem::Placeholder(_) => {
                if result.has_number != 0 {
                    return Err(BACKEND);
                }
                result.has_number = 1;
            }
        }
    }
    Ok(result)
}

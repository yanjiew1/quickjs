/* Private ICU4X locale identity adapter.
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
use crate::BACKEND;
use icu_locale::LocaleExpander;
use icu_locale_core::Locale;
use std::collections::BTreeSet;

// Public baked rows omit default-region forms. A caller must subsequently
// verify every new candidate through its own actual public constructors.
pub(crate) fn likely_regions(candidates: &mut BTreeSet<String>) -> Result<(), i32> {
    for tag in candidates.clone() {
        let mut locale = Locale::try_from_str(&tag).map_err(|_| BACKEND)?;
        if tag != "und" && locale.id.region.is_none() {
            LocaleExpander::new_extended().maximize(&mut locale.id);
            locale.id.script = None;
            if locale.id.region.is_some() {
                candidates.insert(locale.id.to_string());
            }
        }
    }
    Ok(())
}

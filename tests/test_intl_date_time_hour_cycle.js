/* ECMA-402 living draft, commit 7ae78cfdf8255468ffc8ebda33dafaea952808dd
 * Commit date: 2026-10-06; reviewed: 2026-10-09.
 * Stable anchors: sec-intl.datetimeformat-internal-slots, sec-createdatetimeformat.
 * Numeric semantics and exact parts preserve locale literal bytes.
 */
(function () {
  if (typeof Intl === 'undefined' || typeof Intl.DateTimeFormat !== 'function') return;
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }
  const cycles = ['h11', 'h12', 'h23', 'h24'];
  const locales = ['en', 'fr', 'it', 'ja', 'zh', 'ko', 'ar', 'hi'];
  function formatter(locale, additional) {
    return new Intl.DateTimeFormat(locale, Object.assign({
      hour: '2-digit', timeZone: 'UTC', calendar: 'gregory', numberingSystem: 'latn'
    }, additional));
  }
  function checkHour(dtf, hour, label) {
    const time = Date.UTC(2024, 0, 1, hour);
    const ro = dtf.resolvedOptions();
    const parts = dtf.formatToParts(time);
    check(ro.hour12 === (ro.hourCycle === 'h11' || ro.hourCycle === 'h12'),
          label + ': resolved hour12 matches cycle family');
    const hours = parts.filter(p => p.type === 'hour');
    check(hours.length === 1, label + ': one hour part');
    let expected = hour;
    if (ro.hourCycle === 'h11') expected %= 12;
    if (ro.hourCycle === 'h12') expected = hour % 12 || 12;
    if (ro.hourCycle === 'h24') expected = hour || 24;
    let digits = String(expected);
    if (ro.hour === '2-digit') digits = digits.padStart(2, '0');
    check(hours[0].value === digits, label + ': correct cycle at hour ' + hour);
    check(parts.map(p => p.value).join('') === dtf.format(time),
          label + ': format preserves every part, including literal bytes');
    if (ro.hourCycle === 'h11' || ro.hourCycle === 'h12') {
      check(parts.some(p => p.type === 'dayPeriod' && p.value.length > 0),
            label + ': 12-hour pattern includes a day period');
    }
  }
  for (const locale of locales) {
    const normal = formatter(locale);
    const twelve = formatter(locale, {hour12: true});
    const twentyFour = formatter(locale, {hour12: false});
    check(cycles.includes(normal.resolvedOptions().hourCycle), locale + ': valid default');
    check(['h11', 'h12'].includes(twelve.resolvedOptions().hourCycle), locale + ': valid 12-hour family');
    check(['h23', 'h24'].includes(twentyFour.resolvedOptions().hourCycle), locale + ': valid 24-hour family');
    for (const dtf of [normal, twelve, twentyFour]) {
      for (const hour of [0, 2, 12, 23]) checkHour(dtf, hour, locale);
    }
    for (const hc of cycles) {
      const explicit = formatter(locale, {hourCycle: hc});
      check(explicit.resolvedOptions().hourCycle === hc, locale + ': explicit ' + hc);
      const extended = formatter(locale + '-u-hc-' + hc);
      check(extended.resolvedOptions().hourCycle === hc, locale + ': extension ' + hc);
      for (const hour of [0, 12, 23]) checkHour(explicit, hour, locale + '/' + hc);
      for (const flag of [true, false]) {
        const baseline = flag ? twelve : twentyFour;
        const expected = baseline.resolvedOptions().hourCycle;
        check(formatter(locale, {hourCycle: hc, hour12: flag}).resolvedOptions().hourCycle === expected,
              locale + ': hour12 overrides explicit hourCycle');
        const overridden = formatter(locale + '-u-hc-' + hc, {hour12: flag}).resolvedOptions();
        check(overridden.hourCycle === expected,
              locale + ': hour12 overrides extension hourCycle');
        check(new Intl.Locale(overridden.locale).hourCycle === undefined,
              locale + ': ignored hc extension is omitted from resolved locale');
      }
    }
    let threw = false;
    try { formatter(locale, {hourCycle: 'bad', hour12: true}); }
    catch (e) { threw = e instanceof RangeError; }
    check(threw, locale + ': invalid hourCycle is still validated');
  }
})();

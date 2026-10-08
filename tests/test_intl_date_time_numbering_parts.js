/* ECMA-402 living draft, commit 7ae78cfdf8255468ffc8ebda33dafaea952808dd
 * Commit date: 2026-10-06; reviewed: 2026-10-09.
 * Stable anchors: sec-formatdatetimepattern, sec-formatdatetimetoparts.
 * Numeric semantics and exact parts preserve locale literal bytes.
 */
(function () {
  if (typeof Intl === 'undefined' || typeof Intl.DateTimeFormat !== 'function') return;
  function check(condition, message) {
    if (!condition) throw new Error(message);
  }
  const systems = [
    ['latn', '0123456789'], ['arab', '٠١٢٣٤٥٦٧٨٩'],
    ['deva', '०१२३४५६७८९'], ['hanidec', '〇一二三四五六七八九']
  ];
  const time = Date.UTC(2024, 0, 1, 2, 35, 6, 789);
  function encode(value, width, digits) {
    let s = String(value);
    if (width === '2-digit') s = s.padStart(2, '0');
    return Array.from(s, c => digits[Number(c)]).join('');
  }
  for (const [system, digits] of systems) {
    for (const options of [
      {hour: 'numeric', minute: 'numeric', second: 'numeric'},
      {hour: 'numeric', minute: 'numeric', second: 'numeric', fractionalSecondDigits: 3},
      {hour: '2-digit', minute: '2-digit', second: '2-digit'},
      {second: 'numeric'}
    ]) {
      const dtf = new Intl.DateTimeFormat('en-US-u-nu-' + system, Object.assign({
        timeZone: 'UTC', calendar: 'gregory', hourCycle: 'h12'
      }, options));
      const ro = dtf.resolvedOptions();
      check(ro.numberingSystem === system, system + ': numbering system honored');
      const parts = dtf.formatToParts(time);
      check(parts.map(p => p.value).join('') === dtf.format(time),
            system + ': exact parts reconstruction includes unchanged literals');
      for (const [type, value] of [['hour', 2], ['minute', 35], ['second', 6]]) {
        if (options[type] === undefined) continue;
        const selected = parts.filter(p => p.type === type);
        check(selected.length === 1, system + ': one ' + type + ' part');
        check(selected[0].value === encode(value, ro[type], digits),
              system + ': correct ' + type + ' digits and resolved padding');
        check(['numeric', '2-digit'].includes(ro[type]), system + ': numeric resolved width');
      }
      if (options.fractionalSecondDigits !== undefined) {
        const fraction = parts.filter(p => p.type === 'fractionalSecond');
        check(ro.fractionalSecondDigits === 3, system + ': resolved fractional width');
        check(fraction.length === 1 && fraction[0].value === encode(789, 'numeric', digits),
              system + ': fractionalSecond uses requested numbering system');
      }
      if (options.hour !== undefined) {
        check(parts.some(p => p.type === 'dayPeriod' && p.value.length > 0),
              system + ': 12-hour day period retained');
      }
    }
  }
})();

/* Optional root-owned diagnostic only. Reviewer did not execute this file. */
const twelve = new Intl.DateTimeFormat('ja', {hour: 'numeric', hour12: true});
const two = new Intl.DateTimeFormat('en-US-u-nu-hanidec', {
  hour: '2-digit', minute: '2-digit', second: '2-digit', timeZone: 'UTC'
});
const time = Date.UTC(2024, 0, 1, 2, 35, 6, 789);
const formatted = two.format(time);
print(JSON.stringify({
  japaneseHour12: twelve.resolvedOptions(),
  hanidec: {
    resolvedOptions: two.resolvedOptions(), formatted,
    codePoints: Array.from(formatted, c => 'U+' + c.codePointAt(0).toString(16).toUpperCase().padStart(4, '0')),
    parts: two.formatToParts(time),
    officialAsciiSpaceIncludes: formatted.includes('〇二:三五:〇六 AM')
  }
}));

/* Actual en/en-US CLDR48.2 names; inspect the timezone part alone. */
function check(zone, iso, style, expected) {
    for (const locale of ['en', 'en-US']) {
        const formatter = new Intl.DateTimeFormat(locale, {
            timeZone: zone, hour: 'numeric', timeZoneName: style
        });
        const parts = formatter.formatToParts(Date.parse(iso));
        const actual = parts.filter(p => p.type === 'timeZoneName');
        if (actual.length !== 1 || actual[0].value !== expected)
            throw Error(`${locale} ${zone} ${iso} ${style}: ${JSON.stringify(actual)} expected ${expected}`);
    }
}
check('Pacific/Chatham', '1970-01-01T00:00:00Z', 'long', 'Chatham Standard Time');
check('Pacific/Chatham', '2040-01-01T00:00:00Z', 'long', 'Chatham Daylight Time');
check('Pacific/Chatham', '2040-07-01T00:00:00Z', 'long', 'Chatham Standard Time');
check('Pacific/Chatham', '1970-01-01T00:00:00Z', 'longGeneric', 'Chatham Time');
check('Pacific/Chatham', '2040-01-01T00:00:00Z', 'longGeneric', 'Chatham Time');
check('America/Phoenix', '2040-01-01T00:00:00Z', 'long', 'Mountain Standard Time (Phoenix)');
check('America/Phoenix', '2040-07-01T00:00:00Z', 'short', 'MST (Phoenix)');
check('America/Phoenix', '2040-01-01T00:00:00Z', 'shortGeneric', 'MT (Phoenix)');
check('America/Denver', '2040-07-01T00:00:00Z', 'long', 'Mountain Daylight Time');
check('Europe/Dublin', '2040-01-01T00:00:00Z', 'long', 'Greenwich Mean Time (Ireland)');
check('Europe/Dublin', '2040-07-01T00:00:00Z', 'long', 'Irish Standard Time');
check('Europe/Dublin', '1971-10-31T01:59:59.999Z', 'long', 'Irish Standard Time');
check('Europe/Dublin', '1971-10-31T02:00:00.000Z', 'long', 'Greenwich Mean Time (Ireland)');
check('Europe/Lisbon', '1992-09-27T00:59:59.999Z', 'long', 'Western European Summer Time (Lisbon)');
check('Europe/Lisbon', '1992-09-27T01:00:00.000Z', 'long', 'Central European Standard Time (Lisbon)');

/* Component matching retains each timezone style through width adjustment,
 * both hour families, and the full-pattern range fallback. */
for (const locale of ['en', 'en-US']) {
    for (const hourCycle of ['h11', 'h12', 'h23', 'h24']) {
        for (const formatMatcher of [undefined, 'best fit']) {
            for (const timeZoneName of ['short', 'long', 'shortOffset', 'longOffset', 'shortGeneric', 'longGeneric']) {
                for (const withDate of [false, true]) {
                    const options = {timeZone:'Pacific/Chatham', hour:'numeric', hourCycle, timeZoneName};
                    if (formatMatcher !== undefined) options.formatMatcher = formatMatcher;
                    if (withDate) Object.assign(options, {year:'numeric', month:'numeric', day:'numeric'});
                    const f = new Intl.DateTimeFormat(locale, options);
                    const resolved = f.resolvedOptions();
                    if (resolved.timeZoneName !== timeZoneName || resolved.hourCycle !== hourCycle)
                        throw Error(`component resolved fields lost: ${JSON.stringify(options)} ${JSON.stringify(resolved)}`);
                    const start = Date.parse('1970-01-01T00:00:00Z');
                    const end = start + 3600000;
                    const parts = f.formatToParts(start);
                    const zones = parts.filter(p => p.type === 'timeZoneName');
                    if (zones.length !== 1 || !zones[0].value || parts.map(p => p.value).join('') !== f.format(start))
                        throw Error(`component timezone part lost: ${JSON.stringify(options)} ${JSON.stringify(parts)}`);
                    const expected = {long:'Chatham Standard Time', shortOffset:'GMT+12:45', longOffset:'GMT+12:45', longGeneric:'Chatham Time'}[timeZoneName];
                    if (expected !== undefined && zones[0].value !== expected)
                        throw Error(`component timezone value: ${JSON.stringify(options)} ${zones[0].value} expected ${expected}`);
                    const range = f.formatRangeToParts(start, end);
                    const rangeZones = range.filter(p => p.type === 'timeZoneName');
                    if (!rangeZones.length || rangeZones.some(p => p.value !== zones[0].value) ||
                        range.map(p => p.value).join('') !== f.formatRange(start, end))
                        throw Error(`range timezone part lost: ${JSON.stringify(options)} ${JSON.stringify(range)}`);
                }
            }
        }
    }
}

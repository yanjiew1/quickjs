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

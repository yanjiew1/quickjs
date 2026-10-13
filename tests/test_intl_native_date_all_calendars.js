/* Native frontend closure over accepted CLDR48.2 Date banks and the sole
 * acquired primary calendar provider. Copyright 2026 Yan-Jie Wang, MIT.
 * Expected lunar names and style strings are independently pinned in
 * the date-bank C fixtures; this test exercises JS callbacks and banks.
 * The default matrix covers every calendar, style and required subset;
 * intlDateExhaustive retains all locale/matcher/width combinations.
 */
(function () {
    function check(value, label) { if (!value) throw Error(label || "assertion"); }
    function same(a, b, label) { check(a === b, (label || "") + ": " + a + " !== " + b); }
    function text(parts) { let value = ""; for (const p of parts) value += p.value; return value; }
    function field(parts, type) { for (const p of parts) if (p.type === type) return p.value; }
    const calendars = ["buddhist", "chinese", "coptic", "dangi", "ethioaa", "ethiopic",
        "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla", "islamic-umalqura",
        "iso8601", "japanese", "persian", "roc"];
    const exhaustive = globalThis.intlDateExhaustive === true;
    const locales = ["en", "en-US"];
    const matchers = ["basic", "best fit"];
    const focused = ["gregory", "chinese", "hebrew", "japanese"];
    function index(calendar) { return calendars.indexOf(calendar); }
    function selected(calendar, locale, matcher, ordinal = 0) {
        return exhaustive || (locale === locales[(index(calendar) + ordinal) % 2] &&
            matcher === matchers[(index(calendar) + Math.floor(ordinal / 2)) % 2]);
    }
    function widths(calendar, ordinal = 0) {
        return exhaustive || focused.includes(calendar) ? [1, 2, 3] :
            [1 + (index(calendar) + ordinal) % 3];
    }
    const epoch = 1707523200000; // independently acquired 2024-02-10 UTC lunar M01 day1
    for (const locale of ["en", "en-US"]) for (const calendar of calendars) {
        if (!exhaustive && locale !== locales[index(calendar) % 2]) continue;
        for (const dateStyle of ["full", "long", "medium", "short"]) {
            const f = new Intl.DateTimeFormat(locale, {calendar, dateStyle, timeZone: "UTC"});
            same(f.resolvedOptions().calendar, calendar, locale + " " + calendar);
            same(f.resolvedOptions().locale, locale);
            const parts = f.formatToParts(epoch);
            same(text(parts), f.format(epoch)); check(parts.length);
            for (const p of parts) { same(Object.keys(p).join(","), "type,value"); check(p.value.length); }
            if ((calendar === "chinese" || calendar === "dangi") &&
                (dateStyle === "full" || dateStyle === "long")) {
                same(field(parts, "relatedYear"), "2024"); same(field(parts, "yearName"), "jia-chen");
            }
            const range = f.formatRangeToParts(epoch, epoch + 86400000);
            same(text(range), f.formatRange(epoch, epoch + 86400000));
            check(range.some(p => p.source === "startRange")); check(range.some(p => p.source === "endRange"));
            for (const p of range) same(Object.keys(p).join(","), "type,value,source");
            for (const p of f.formatRangeToParts(epoch, epoch)) same(p.source, "shared");
        }
    }
    function expected(calendar, dateStyle, at, value) {
        same(new Intl.DateTimeFormat("en", {calendar, dateStyle, timeZone: "UTC"}).format(at), value, calendar);
    }
    expected("chinese", "long", epoch, "First Month 1, 2024(jia-chen)");
    expected("dangi", "long", epoch, "First Month 1, 2024(jia-chen)");
    expected("buddhist", "long", 1556668800000, "May 1, 2562 BE");
    expected("roc", "long", 1556668800000, "May 1, 108 Minguo");
    expected("japanese", "short", 1556668800000, "5/1/1 R");
    expected("iso8601", "short", epoch, "2024-02-10");
    same(new Intl.DateTimeFormat("en", {calendar: "islamicc", timeZone: "UTC"}).resolvedOptions().calendar, "islamic-civil");
    same(new Intl.DateTimeFormat("en", {calendar: "ethiopic-amete-alem", timeZone: "UTC"}).resolvedOptions().calendar, "ethioaa");
    same(Intl.DateTimeFormat.supportedLocalesOf(["en", "en-US", "fr"]).join(","), "en,en-US");
    /* Fraction records must be available to both matchers, including the
     * three required time/date-time/dayPeriod subsets in every calendar.
     * Numeric related-year or year-name parts are both valid year records.
     */
    const fractionSubsets = [
        {hour: "numeric", minute: "numeric", second: "numeric"},
        {dayPeriod: "short", hour: "numeric", minute: "numeric", second: "numeric"},
        {weekday: "short", year: "numeric", month: "numeric", day: "numeric",
         hour: "numeric", minute: "numeric", second: "numeric"}
    ];
    const fractionEpoch = Date.UTC(2000, 5, 15, 0, 30, 45, 987);
    for (const calendar of calendars) for (const formatMatcher of ["basic", "best fit"]) {
        for (const components of fractionSubsets) {
            const options = {...components, calendar, formatMatcher, timeZone: "UTC",
                hourCycle: components.dayPeriod ? "h12" : "h23"};
            const noFraction = new Intl.DateTimeFormat("en", options);
            same(noFraction.resolvedOptions().fractionalSecondDigits, undefined);
            same(field(noFraction.formatToParts(fractionEpoch), "fractionalSecond"), undefined);
            same(text(noFraction.formatToParts(fractionEpoch)), noFraction.format(fractionEpoch));
            for (const digits of widths(calendar, fractionSubsets.indexOf(components))) {
                const f = new Intl.DateTimeFormat("en", {...options, fractionalSecondDigits: digits});
                const label = calendar + "/" + formatMatcher + "/" + digits;
                const ro = f.resolvedOptions(), parts = f.formatToParts(fractionEpoch);
                same(ro.calendar, calendar, label);
                same(ro.fractionalSecondDigits, digits, label);
                for (const component of Object.keys(components))
                    check(ro[component] !== undefined, label + " missing " + component);
                check(field(parts, "second") !== undefined, label + " missing second part");
                if (components.dayPeriod)
                    check(field(parts, "dayPeriod") !== undefined, label + " missing dayPeriod part");
                same(field(parts, "fractionalSecond"), "987".slice(0, digits), label);
                same(text(parts), f.format(fractionEpoch), label);
                same(field(f.formatToParts(fractionEpoch - 980), "fractionalSecond"),
                     "007".slice(0, digits), label + " leading zero");
                same(field(f.formatToParts(-1), "fractionalSecond"),
                     "999".slice(0, digits), label + " negative epoch");
            }
        }
        /* Fraction-only is accepted without Number-slot date defaults. */
        for (const digits of widths(calendar)) {
            const f = new Intl.DateTimeFormat("en", {
                calendar, formatMatcher, fractionalSecondDigits: digits, timeZone: "UTC"
            });
            same(f.resolvedOptions().fractionalSecondDigits, digits);
            same(field(f.formatToParts(fractionEpoch), "fractionalSecond"), "987".slice(0, digits));
            same(text(f.formatToParts(fractionEpoch)), f.format(fractionEpoch));
        }
        /* Style records bypass fraction enrichment and matcher adjustment. */
        for (const styles of [{dateStyle: "medium"}, {timeStyle: "medium"},
                              {dateStyle: "medium", timeStyle: "medium"}]) {
            const f = new Intl.DateTimeFormat("en", {calendar, formatMatcher, timeZone: "UTC", ...styles});
            const control = new Intl.DateTimeFormat("en", {calendar, timeZone: "UTC", ...styles});
            same(f.resolvedOptions().fractionalSecondDigits, undefined);
            same(field(f.formatToParts(fractionEpoch), "fractionalSecond"), undefined);
            same(f.format(fractionEpoch), control.format(fractionEpoch));
            same(text(f.formatToParts(fractionEpoch)), f.format(fractionEpoch));
        }
    }
    /* Every required subset has its own era-free record. Check the selected
     * fields and genuine parts; clearing only resolvedOptions().era is not
     * sufficient. Related-year and year-name parts share the year component.
     */
    const requiredSubsets = [
        {weekday: "short", year: "numeric", month: "numeric", day: "numeric",
         hour: "numeric", minute: "numeric", second: "numeric", fractionalSecondDigits: 1},
        {weekday: "short", year: "numeric", month: "numeric", day: "numeric",
         hour: "numeric", minute: "numeric", second: "numeric"},
        {weekday: "short", year: "numeric", month: "numeric", day: "numeric"},
        {year: "numeric", month: "numeric", day: "numeric"},
        {year: "numeric", month: "numeric"},
        {month: "numeric", day: "numeric"},
        {month: "numeric"},
        {hour: "numeric", minute: "numeric", second: "numeric", fractionalSecondDigits: 1},
        {hour: "numeric", minute: "numeric", second: "numeric"},
        {hour: "numeric", minute: "numeric"},
        {dayPeriod: "short", hour: "numeric", minute: "numeric", second: "numeric",
         fractionalSecondDigits: 1},
        {dayPeriod: "short", hour: "numeric", minute: "numeric", second: "numeric"},
        {dayPeriod: "short", hour: "numeric", minute: "numeric"},
        {dayPeriod: "short", hour: "numeric"}
    ];
    const components = ["weekday", "era", "year", "month", "day", "dayPeriod",
        "hour", "minute", "second", "fractionalSecondDigits", "timeZoneName"];
    for (const locale of ["en", "en-US"]) for (const calendar of calendars)
    for (const formatMatcher of ["basic", "best fit"]) {
        for (const subset of requiredSubsets)
        for (const digits of subset.fractionalSecondDigits ? [1, 2, 3] : [undefined]) {
            // Cover every calendar/subset, with complete focused fractional
            // interactions. The optional exhaustive mode keeps the full matrix.
            const ordinal = requiredSubsets.indexOf(subset);
            const fullFraction = focused.includes(calendar) && subset.fractionalSecondDigits;
            if (!fullFraction && !selected(calendar, locale, formatMatcher, ordinal)) continue;
            if (!exhaustive && !fullFraction && digits !== undefined &&
                digits !== 1 + (index(calendar) + ordinal) % 3) continue;
            const request = {...subset};
            if (digits !== undefined) request.fractionalSecondDigits = digits;
            const f = new Intl.DateTimeFormat(locale, {...request, calendar, formatMatcher,
                timeZone: "UTC", hourCycle: subset.dayPeriod ? "h12" : "h23"});
            const label = locale + "/" + calendar + "/" + formatMatcher + "/" +
                Object.keys(request).join(",") + "/" + digits;
            const ro = f.resolvedOptions(), parts = f.formatToParts(fractionEpoch);
            same(ro.calendar, calendar, label);
            for (const component of components)
                same(ro[component] !== undefined, request[component] !== undefined,
                     label + " selected " + component);
            same(field(parts, "era"), undefined, label + " emitted era");
            for (const component of Object.keys(request)) {
                const partType = component === "fractionalSecondDigits" ? "fractionalSecond" : component;
                if (component === "year")
                    check(parts.some(p => p.type === "year" || p.type === "relatedYear" ||
                        p.type === "yearName"), label + " missing year part");
                else check(field(parts, partType) !== undefined, label + " missing " + partType);
            }
            if (digits !== undefined)
                same(field(parts, "fractionalSecond"), "987".slice(0, digits), label);
            same(text(parts), f.format(fractionEpoch), label);
            same(f.format(fractionEpoch).trim(), f.format(fractionEpoch), label + " boundary separator");
        }
    }
    /* Original era-bearing records and date styles remain selectable. */
    const eraCalendars = ["buddhist", "coptic", "ethioaa", "ethiopic", "indian",
        "islamic-civil", "islamic-tbla", "islamic-umalqura", "japanese", "persian", "roc"];
    for (const locale of ["en", "en-US"]) for (const calendar of eraCalendars)
    for (const formatMatcher of ["basic", "best fit"]) {
        for (const era of ["narrow", "short", "long"]) {
            if (calendar !== "japanese" && !selected(calendar, locale, formatMatcher,
                    ["narrow", "short", "long"].indexOf(era))) continue;
            const f = new Intl.DateTimeFormat(locale, {calendar, formatMatcher, era,
                year: "numeric", month: "numeric", day: "numeric", timeZone: "UTC"});
            check(f.resolvedOptions().era !== undefined, calendar + " requested era");
            check(field(f.formatToParts(fractionEpoch), "era") !== undefined, calendar + " era part");
            same(text(f.formatToParts(fractionEpoch)), f.format(fractionEpoch));
        }
        for (const dateStyle of ["full", "long", "medium", "short"]) {
            if (calendar !== "japanese" && !selected(calendar, locale, formatMatcher,
                    ["full", "long", "medium", "short"].indexOf(dateStyle))) continue;
            const options = {calendar, dateStyle, timeZone: "UTC"};
            const f = new Intl.DateTimeFormat(locale, {...options, formatMatcher});
            const control = new Intl.DateTimeFormat(locale, options);
            check(field(f.formatToParts(fractionEpoch), "era") !== undefined, calendar + " style era");
            same(f.format(fractionEpoch), control.format(fractionEpoch));
            same(text(f.formatToParts(fractionEpoch)), f.format(fractionEpoch));
        }
    }
    /* Numeric requests must retain CLDR English Hebrew month names. An
     * ordinal 6 would incorrectly turn Adar I (M05L) into ordinary M06.
     * Test the adjacent leap months and ordinary Adar, including standalone
     * M/L patterns and both format matchers. No Temporal dependency is needed
     * for the Intl-only build; when enabled, check the shared calendar too.
     */
    const hebrewMonths = [
        ["2049-03-04", "Adar I", "M05L", 30],
        ["2049-03-05", "Adar II", "M06", 1],
        ["2025-03-14", "Adar", "M06", 14]
    ];
    for (const locale of ["en", "en-US"])
    for (const formatMatcher of ["basic", "best fit"])
    for (const month of ["numeric", "2-digit"])
    for (const components of [{}, {day: "numeric"}, {year: "numeric", day: "numeric"}]) {
        const f = new Intl.DateTimeFormat(locale, {calendar: "hebrew", timeZone: "UTC",
            formatMatcher, month, ...components});
        const ro = f.resolvedOptions();
        check(ro.month === "short" || ro.month === "long", "Hebrew named resolved month");
        for (const [iso, name, code, day] of hebrewMonths) {
            const epoch = Date.parse(iso + "T00:00:00Z");
            const parts = f.formatToParts(epoch);
            same(field(parts, "month"), name, iso + " Hebrew named month");
            same(text(parts), f.format(epoch), "Hebrew parts join");
            if (components.year && components.day) {
                const year = field(parts, "year");
                same(parts.map(p => p.type).join(","),
                     "day,literal,month,literal,year", "Hebrew matched date order");
                same(parts[1].value, " ", "Hebrew first date literal");
                same(parts[3].value, " ", "Hebrew second date literal");
                same(f.format(epoch), String(day) + " " + name + " " + year,
                     "Hebrew named bank record");
            }
            if (components.day) same(field(parts, "day"), String(day));
            if (typeof Temporal !== "undefined") {
                const date = Temporal.PlainDate.from(iso + "[u-ca=hebrew]");
                same(date.monthCode, code, iso + " Hebrew month code");
                same(date.day, day, iso + " Hebrew day");
                same(field(f.formatToParts(date), "month"), name, "Temporal Hebrew named month");
            }
        }
    }
    for (const locale of ["en", "en-US"])
    for (const dateStyle of ["full", "long", "medium", "short"]) {
        const f = new Intl.DateTimeFormat(locale, {calendar: "hebrew", timeZone: "UTC", dateStyle});
        for (const [iso, name] of hebrewMonths) {
            const epoch = Date.parse(iso + "T00:00:00Z");
            same(field(f.formatToParts(epoch), "month"), name, dateStyle + " Hebrew month name");
            same(text(f.formatToParts(epoch)), f.format(epoch), "Hebrew style parts join");
        }
    }
})();

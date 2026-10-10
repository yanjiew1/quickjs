/* Native frontend closure over accepted CLDR49 Date banks and the sole
 * acquired primary calendar provider. Copyright 2026 Yan-Jie Wang, MIT.
 * Expected lunar names and style strings are independently pinned in
 * test-date-calendar-data.c; this fixture exercises JS callbacks and banks.
 */
(function () {
    function check(value, label) { if (!value) throw Error(label || "assertion"); }
    function same(a, b, label) { check(a === b, (label || "") + ": " + a + " !== " + b); }
    function text(parts) { let value = ""; for (const p of parts) value += p.value; return value; }
    function field(parts, type) { for (const p of parts) if (p.type === type) return p.value; }
    const calendars = ["buddhist", "chinese", "coptic", "dangi", "ethioaa", "ethiopic",
        "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla", "islamic-umalqura",
        "iso8601", "japanese", "persian", "roc"];
    const epoch = 1707523200000; // independently acquired 2024-02-10 UTC lunar M01 day1
    for (const locale of ["en", "en-US"]) for (const calendar of calendars) {
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
})();

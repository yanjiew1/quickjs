"use strict";
/* Calendar enumeration/negotiation source unit; execution awaits root gates. */
function same(actual, expected) {
    if (!Object.is(actual, expected))
        throw Error(`expected ${expected}, got ${actual}`);
}
function equalArray(actual, expected) {
    same(actual.length, expected.length);
    actual.forEach((value, index) => same(value, expected[index]));
}
const calendars = [
    "buddhist", "chinese", "coptic", "dangi", "ethioaa", "ethiopic",
    "gregory", "hebrew", "indian", "islamic-civil", "islamic-tbla",
    "islamic-umalqura", "iso8601", "japanese", "persian", "roc"
];
const publicCalendars = Intl.supportedValuesOf("calendar");
equalArray(publicCalendars, calendars);
same(publicCalendars !== Intl.supportedValuesOf("calendar"), true);
publicCalendars.splice(0);
equalArray(Intl.supportedValuesOf("calendar"), calendars);

for (const calendar of calendars) {
    const optionLocale = new Intl.Locale("en", { calendar });
    const extensionLocale = new Intl.Locale(`en-u-ca-${calendar}`);
    same(optionLocale.calendar, calendar);
    same(extensionLocale.calendar, calendar);
    equalArray(optionLocale.getCalendars(), [calendar]);
    same(new Intl.DateTimeFormat("en", { calendar }).resolvedOptions().calendar,
         calendar);
    const extension = new Intl.DateTimeFormat(`en-u-ca-${calendar}`)
        .resolvedOptions();
    same(extension.calendar, calendar);
    same(extension.locale, `en-u-ca-${calendar}`);
}
for (const [alias, canonical] of [
    ["ethiopic-amete-alem", "ethioaa"], ["islamicc", "islamic-civil"]
]) {
    for (const spelling of [alias, alias.toUpperCase()]) {
        same(new Intl.Locale("en", { calendar: spelling }).calendar, canonical);
        same(new Intl.Locale(`en-u-ca-${spelling}`).calendar, canonical);
        same(Intl.getCanonicalLocales(`en-u-ca-${spelling}`)[0],
             `en-u-ca-${canonical}`);
        same(new Intl.DateTimeFormat("en", { calendar: spelling })
             .resolvedOptions().calendar, canonical);
        const equalOption = new Intl.DateTimeFormat(`en-u-ca-${canonical}`,
                                                    { calendar: spelling });
        same(equalOption.resolvedOptions().locale, `en-u-ca-${canonical}`);
    }
}
for (const locale of ["en", "fa-IR", "ar-SA", "th-TH", "am-ET"]) {
    const preferences = new Intl.Locale(locale).getCalendars();
    same(preferences.length > 0, true);
    for (const calendar of preferences)
        same(calendars.includes(calendar), true);
    const defaultCalendar = new Intl.DateTimeFormat(locale)
        .resolvedOptions().calendar;
    same(calendars.includes(defaultCalendar), true);
    for (const unsupported of ["foobar", "islamic", "islamic-rgsa"]) {
        /* Locale records well-formed strings without negotiating support. */
        const loc = new Intl.Locale(locale, { calendar: unsupported });
        same(loc.calendar, unsupported);
        equalArray(loc.getCalendars(), [unsupported]);
        const option = new Intl.DateTimeFormat(locale, { calendar: unsupported })
            .resolvedOptions();
        const extension = new Intl.DateTimeFormat(`${locale}-u-ca-${unsupported}`)
            .resolvedOptions();
        same(option.calendar, defaultCalendar);
        same(extension.calendar, defaultCalendar);
        same(extension.locale.includes(`-ca-${unsupported}`), false);
    }
}
same(new Intl.DateTimeFormat("en-u-ca-buddhist", { calendar: "gregory" })
     .resolvedOptions().locale, "en");
same(new Intl.DateTimeFormat("en-u-ca-buddhist", { calendar: "foobar" })
     .resolvedOptions().calendar, "buddhist");

const access = [];
const calendarValue = {
    toString() { access.push("calendar.toString"); return "islamicc"; }
};
new Intl.DateTimeFormat("en", {
    get calendar() { access.push("calendar"); return calendarValue; },
    get numberingSystem() { access.push("numberingSystem"); return undefined; },
    get hour12() { access.push("hour12"); return undefined; },
    get hourCycle() { access.push("hourCycle"); return undefined; }
});
equalArray(access, ["calendar", "calendar.toString", "numberingSystem",
                    "hour12", "hourCycle"]);

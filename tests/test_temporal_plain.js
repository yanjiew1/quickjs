/* Native Plain object regression; no JS implementation fallback. */
function assert(value, message) {
    if (!value) throw Error(message || "assertion failed");
}
function same(actual, expected) {
    assert(Object.is(actual, expected), String(actual) + " != " + expected);
}
function throws(type, callback) {
    let caught;
    try { callback(); } catch (error) { caught = error; }
    assert(caught instanceof type);
}
let date = new Temporal.PlainDate(2020, 2, 29);
same(date.add({ years: 1 }).toString(), "2021-02-28");
throws(RangeError, () => date.add({ years: 1 }, { overflow: "reject" }));
same(date.add({ hours: 48 }).toString(), "2020-03-02");
same(date.with({ month: 1 }).toString(), "2020-01-29");
same(new Temporal.PlainDate(2021, 1, 31).until("2021-02-28", {
    largestUnit: "month"
}).toString(), "P28D");
same(new Temporal.PlainDate(2020, 2, 29).until("2021-02-28", {
    largestUnit: "year"
}).toString(), "P11M30D");
same(date.toPlainYearMonth().toString(), "2020-02");
same(date.toPlainMonthDay().toString(), "02-29");
same(date.toPlainDateTime("23:59").add({ minutes: 2 }).toString(),
     "2020-03-01T00:01:00");
same(new Temporal.PlainDateTime(2021, 1, 1, 12).round({
    smallestUnit: "day", roundingMode: "halfEven"
}).toString(), "2021-01-01T00:00:00");
same(new Temporal.PlainDate(2020, 1, 1).until("2021-01-20", {
    largestUnit: "year", smallestUnit: "month", roundingMode: "halfExpand"
}).toString(), "P1Y1M");
same(new Temporal.PlainYearMonth(2020, 2).toPlainDate({ day: 29 }).day, 29);
throws(RangeError, () => new Temporal.PlainYearMonth(2020, 2).add({ days: 1 }));
same(Temporal.PlainMonthDay.from("2023-02-28").toString(), "02-28");
same(new Temporal.PlainMonthDay(2, 29).toPlainDate({ year: 2023 }).day, 28);
let operations = [];
let fields = new Proxy({ year: 2024, month: 1, day: 2 }, {
    get(target, key) { operations.push(String(key)); return target[key]; }
});
Temporal.PlainDateTime.from(fields, {
    get overflow() { operations.push("overflow"); return "constrain"; }
});
same(operations.join(","),
     "calendar,day,hour,microsecond,millisecond,minute,month," +
     "monthCode,nanosecond,second,year,overflow");
operations = [];
fields = new Proxy({ year: 1e300, month: 1, day: 1 }, {
    get(target, key) { operations.push(String(key)); return target[key]; }
});
throws(RangeError, () => Temporal.PlainDate.from(fields, {
    get overflow() { operations.push("overflow"); return "constrain"; }
}));
same(operations.join(","), "calendar,day,month,monthCode,year,overflow");
let readPrototype = false;
// A proxy allows observing the otherwise nonconfigurable prototype slot.
let NewTarget = new Proxy(function () {}, {
    get(target, key, receiver) {
        if (key === "prototype") readPrototype = true;
        return Reflect.get(target, key, receiver);
    }
});
throws(RangeError, () => Reflect.construct(Temporal.PlainDate,
    [2024, 2, 31], NewTarget));
assert(!readPrototype);
throws(TypeError, () => Temporal.PlainDate.from({
    toString() { return "2024-01-01"; }
}));
throws(TypeError, () => Temporal.PlainDate.prototype.with.call({}, {}));
let minDate = new Temporal.PlainDate(-271821, 4, 19);
same(minDate.day, 19);
throws(RangeError, () => minDate.toPlainDateTime());
same(new Temporal.PlainYearMonth(-271821, 4).month, 4);
same(new Temporal.PlainDate(2021, 1, 1).weekOfYear, 53);
same(new Temporal.PlainDate(2021, 1, 1).yearOfWeek, 2020);
for (let ctor of [Temporal.PlainDate, Temporal.PlainDateTime,
                  Temporal.PlainYearMonth, Temporal.PlainMonthDay]) {
    same(ctor.prototype.toString.length, 0);
    same(ctor.prototype.toLocaleString.length, 0);
    same(ctor.prototype.with.length, 1);
    assert(Object.getOwnPropertyDescriptor(ctor, "prototype").writable === false);
    class Sub extends ctor {}
    let args = ctor === Temporal.PlainMonthDay ? [1, 1] :
               ctor === Temporal.PlainYearMonth ? [2024, 1] : [2024, 1, 1];
    let sub = new Sub(...args);
    assert(sub instanceof Sub);
    assert(!(ctor.from(sub) instanceof Sub));
    same(sub.toString(), sub.toJSON());
}

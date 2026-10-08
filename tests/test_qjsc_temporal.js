/* qjsc-generated raw contexts must initialize the enabled Temporal feature. */
if (typeof Temporal !== "object" || typeof Temporal.ZonedDateTime !== "function")
    throw Error("qjsc omitted Temporal intrinsic initialization");
if (Temporal.Instant.from("1970-01-01T00:00:00Z").epochNanoseconds !== 0n)
    throw Error("qjsc Temporal Instant initialization is incomplete");
if (Temporal.PlainDate.from("2024-02-29").daysInMonth !== 29)
    throw Error("qjsc Temporal calendar initialization is incomplete");

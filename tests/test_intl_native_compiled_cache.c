/* Compiled native blob validation, retry and independent host threads.
 * Copyright (c) 2026 Yan-Jie Wang.
 * Link the Make-owned counted provider and Number objects before libquickjs.a.
 * Each named mode runs in a separate process with a cold compiled cache. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <stdatomic.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/intl/provider-native-duration.h"
#include "../src/intl/native-data-validation-internal.h"
#include "../src/intl/data/number-extra-validation.h"
#include "../src/intl/data/locale-metadata.h"

#ifdef _WIN32
typedef volatile LONG TestAtomic;
static unsigned int load(TestAtomic *value)
{
    return (unsigned int)InterlockedCompareExchange(value, 0, 0);
}
static void store(TestAtomic *value, unsigned int next)
{
    InterlockedExchange(value, (LONG)next);
}
static unsigned int take(TestAtomic *value)
{
    return (unsigned int)InterlockedExchange(value, 0);
}
static void increment(TestAtomic *value)
{
    InterlockedIncrement(value);
}
#else
typedef _Atomic unsigned int TestAtomic;
static unsigned int load(TestAtomic *value)
{
    return atomic_load_explicit(value, memory_order_relaxed);
}
static void store(TestAtomic *value, unsigned int next)
{
    atomic_store_explicit(value, next, memory_order_relaxed);
}
static unsigned int take(TestAtomic *value)
{
    return atomic_exchange_explicit(value, 0, memory_order_relaxed);
}
static void increment(TestAtomic *value)
{
    atomic_fetch_add_explicit(value, 1, memory_order_relaxed);
}
#endif
static TestAtomic reader_calls = 0;
static TestAtomic injected_failure = 0;
static TestAtomic number_scans = 0;

QJSIntlDataStatus qjs_intl_counted_number_validate(const QJSIntlDataView *);
QJSIntlDataStatus qjs_intl_counted_number_validate(const QJSIntlDataView *view)
{
    increment(&number_scans);
    return qjs_intl_number_extra_data_validate(view);
}

QJSIntlDataStatus qjs_intl_counted_data_open(const void *data,
    size_t length, QJSIntlDataView *out);
QJSIntlDataStatus qjs_intl_counted_data_open(const void *data,
    size_t length, QJSIntlDataView *out)
{
    unsigned int failure;
    assert(data == qjs_intl_locale_metadata_blob);
    assert(length == qjs_intl_locale_metadata_blob_size);
    increment(&reader_calls);
    failure = take(&injected_failure);
    if (failure) {
        if (out) memset(out, 0, sizeof(*out));
        return (QJSIntlDataStatus)failure;
    }
    return qjs_intl_data_open(data, length, out);
}

typedef struct Heap {
    size_t calls, failure, live;
    size_t *sizes, size_capacity;
} Heap;
static void *allocate(void *opaque, size_t size)
{
    Heap *heap = opaque;
    void *pointer;
    heap->calls++;
    if (heap->sizes) {
        assert(heap->calls <= heap->size_capacity);
        heap->sizes[heap->calls - 1] = size;
    }
    if (heap->calls == heap->failure) return NULL;
    pointer = malloc(size);
    if (pointer) heap->live++;
    return pointer;
}
static void release(void *opaque, void *pointer)
{
    Heap *heap = opaque;
    if (pointer) {
        assert(heap->live);
        heap->live--;
        free(pointer);
    }
}
static void *resize(void *opaque, void *pointer, size_t size)
{
    Heap *heap = opaque;
    if (!pointer) return allocate(opaque, size);
    if (!size) {
        release(opaque, pointer);
        return NULL;
    }
    heap->calls++;
    if (heap->sizes) {
        assert(heap->calls <= heap->size_capacity);
        heap->sizes[heap->calls - 1] = size;
    }
    if (heap->calls == heap->failure) return NULL;
    return realloc(pointer, size);
}
static QJSIntlBytes bytes(const char *value)
{
    return (QJSIntlBytes){value, strlen(value)};
}
static void equal(QJSIntlBytes value, const char *expected)
{
    assert(value.length == strlen(expected));
    assert(!memcmp(value.data, expected, value.length));
}
static QJSIntlProvider *create(Heap *heap, const char *locale,
    const char *zone, QJSIntlStatus expected)
{
    QJSIntlProviderConfig config = {0};
    QJSIntlProvider *provider = (QJSIntlProvider *)(uintptr_t)1;
    config.backend = QJS_INTL_BACKEND_NATIVE;
    config.allocator = (QJSIntlAllocator){
        heap, allocate, resize, release
    };
    config.default_locale = bytes(locale);
    config.default_time_zone = bytes(zone);
    assert(qjs_intl_provider_new(&config, &provider) == expected);
    if (expected == QJS_INTL_OK) {
        const QJSIntlDataView *view;
        assert(provider);
        view = qjs_intl_native_provider_view(provider);
        assert(view->data == qjs_intl_locale_metadata_blob);
        assert(view->length == qjs_intl_locale_metadata_blob_size);
        assert(qjs_intl_native_provider_allocator(provider)->opaque ==
               heap);
        equal(qjs_intl_provider_default_locale(provider), locale);
        equal(qjs_intl_provider_default_time_zone(provider), zone);
    } else {
        assert(!provider && !heap->live);
    }
    return provider;
}
static void destroy(Heap *heap, QJSIntlProvider *provider)
{
    qjs_intl_provider_free(provider);
    assert(!heap->live);
}
static void warm(Heap *heap)
{
    QJSIntlProvider *provider;
    unsigned int calls = load(&reader_calls);
    provider = create(heap, "en-US", "UTC", QJS_INTL_OK);
    destroy(heap, provider);
    assert(load(&reader_calls) == calls);
}

static void retry(void)
{
    Heap heap = {0};
    QJSIntlProvider *provider;
    heap.failure = 1;
    create(&heap, "en-US", "UTC", QJS_INTL_NO_MEMORY);
    assert(load(&reader_calls) == 0);
    heap.failure = 0;
    store(&injected_failure, QJS_INTL_DATA_INVALID);
    create(&heap, "en-US", "UTC", QJS_INTL_DATA_ERROR);
    assert(load(&reader_calls) == 1);
    store(&injected_failure, QJS_INTL_DATA_UNSUPPORTED_VERSION);
    create(&heap, "en-US", "UTC", QJS_INTL_DATA_ERROR);
    assert(load(&reader_calls) == 2);
    provider = create(&heap, "en-US", "UTC", QJS_INTL_OK);
    assert(load(&reader_calls) == 3);
    destroy(&heap, provider);
    heap.calls = 0;
    heap.failure = 1;
    create(&heap, "en-US", "UTC", QJS_INTL_NO_MEMORY);
    heap.calls = 0;
    heap.failure = 2;
    create(&heap, "en-US", "UTC", QJS_INTL_NO_MEMORY);
    heap.failure = 0;
    assert(load(&reader_calls) == 3);
    warm(&heap);
}
static void provider_failure(void)
{
    Heap heap = {0};
    /* The first real reader succeeds, then default admission fails.
     * No provider has succeeded yet; reader success remains cached. */
    create(&heap, "fr", "UTC", QJS_INTL_UNSUPPORTED);
    assert(load(&reader_calls) == 1);
    warm(&heap);
    create(&heap, "en-US", "Missing/Zone", QJS_INTL_UNSUPPORTED);
    assert(load(&reader_calls) == 1);
    warm(&heap);
}

typedef struct Worker {
    Heap heap;
    unsigned int index;
} Worker;
static void run_worker(Worker *worker)
{
    const char *locale = worker->index % 2 ? "en-US" : "en";
    const char *zone = worker->index % 2 ? "America/New_York" : "UTC";
    const char *next_locale = worker->index % 2 ? "en" : "en-US";
    QJSIntlProvider *provider;
    provider = create(&worker->heap, locale, zone, QJS_INTL_OK);
    assert(qjs_intl_native_provider_set_default_locale(provider,
        bytes(next_locale)) == QJS_INTL_OK);
    equal(qjs_intl_provider_default_locale(provider), next_locale);
    equal(qjs_intl_provider_default_time_zone(provider), zone);
    destroy(&worker->heap, provider);
}
#ifdef _WIN32
static DWORD WINAPI worker_entry(LPVOID opaque)
{
    run_worker(opaque);
    return 0;
}
#else
static void *worker_entry(void *opaque)
{
    run_worker(opaque);
    return NULL;
}
#endif
static void concurrent(void)
{
    enum { THREAD_COUNT = 4 };
    Worker workers[THREAD_COUNT] = {0};
    Heap heap = {0};
    unsigned int i, calls;
#ifdef _WIN32
    HANDLE threads[THREAD_COUNT];
#else
    pthread_t threads[THREAD_COUNT];
#endif
    for (i = 0; i < THREAD_COUNT; i++) {
        workers[i].index = i;
#ifdef _WIN32
        threads[i] = CreateThread(NULL, 0, worker_entry, &workers[i],
                                   0, NULL);
        assert(threads[i]);
#else
        assert(!pthread_create(&threads[i], NULL, worker_entry,
                               &workers[i]));
#endif
    }
    for (i = 0; i < THREAD_COUNT; i++) {
#ifdef _WIN32
        assert(WaitForSingleObject(threads[i], INFINITE) ==
               WAIT_OBJECT_0);
        assert(CloseHandle(threads[i]));
#else
        assert(!pthread_join(threads[i], NULL));
#endif
        assert(!workers[i].heap.live);
    }
    /* Cold scans may overlap or finish before another thread starts.
     * Joins precede observation and subsequent warm construction. */
    calls = load(&reader_calls);
    assert(calls >= 1 && calls <= THREAD_COUNT);
    for (i = 0; i < THREAD_COUNT; i++) warm(&heap);
    assert(load(&reader_calls) == calls);
}

static void mutable(void)
{
    Heap heap = {0};
    QJSIntlProvider *provider;
    QJSIntlDataView view;
    unsigned char *copy, saved;
    size_t length = qjs_intl_locale_metadata_blob_size;
    provider = create(&heap, "en-US", "UTC", QJS_INTL_OK);
    destroy(&heap, provider);
    assert(load(&reader_calls) == 1);
    view = (QJSIntlDataView){qjs_intl_locale_metadata_blob, length};
    assert(qjs_intl_data_open(qjs_intl_locale_metadata_blob, length - 1,
        &view) == QJS_INTL_DATA_INVALID);
    assert(!view.data && !view.length);
    copy = malloc(length);
    assert(copy);
    memcpy(copy, qjs_intl_locale_metadata_blob, length);
    assert(qjs_intl_data_open(copy, length, &view) == QJS_INTL_DATA_OK);
    assert(view.data == copy && view.length == length);
    /* Abandon the old borrowed view before changing the same buffer. */
    memset(&view, 0, sizeof(view));
    saved = copy[0];
    copy[0] ^= 1;
    view = (QJSIntlDataView){qjs_intl_locale_metadata_blob, length};
    assert(qjs_intl_data_open(copy, length, &view) ==
           QJS_INTL_DATA_INVALID);
    assert(!view.data && !view.length);
    copy[0] = saved;
    assert(qjs_intl_data_open(copy, length, &view) == QJS_INTL_DATA_OK);
    assert(view.data == copy && view.length == length);
    memset(&view, 0, sizeof(view));
    free(copy);
    assert(load(&reader_calls) == 1);
    warm(&heap);
}

static uint32_t index_of(const QJSIntlDataView *view, uint32_t id,
    const char *name)
{
    QJSIntlDataSection section;
    QJSIntlDataSlice value;
    uint32_t i;
    assert(qjs_intl_data_section(view, id, &section) == QJS_INTL_DATA_OK);
    for (i = 0; i < section.record_count; i++) {
        assert(qjs_intl_data_record_string(view, &section, i, 0, &value) ==
               QJS_INTL_DATA_OK);
        if (value.length == strlen(name) &&
            !memcmp(value.data, name, value.length)) return i;
    }
    assert(0);
    return 0;
}
static QJSIntlNumberOptions number_options(void)
{
    QJSIntlNumberOptions options = {0};
    options.grouping = QJS_INTL_NUMBER_GROUP_AUTO;
    options.maximum_output_length = options.digits.maximum_output_length = 4096;
    options.digits.minimum_integer_digits = 1;
    options.digits.maximum_fraction_digits = 3;
    options.digits.minimum_significant_digits = 1;
    options.digits.maximum_significant_digits = 21;
    options.digits.rounding_increment = 1;
    options.digits.rounding_mode = QJS_INTL_ROUND_HALF_EXPAND;
    return options;
}
static QJSIntlDurationOptions duration_options(void)
{
    QJSIntlDurationOptions options = {0};
    size_t i;
    options.style = QJS_INTL_DURATION_DIGITAL;
    options.fractional_digits = 9;
    options.maximum_output_length = 4096;
    for (i = 0; i < QJS_INTL_DURATION_UNIT_COUNT; i++) {
        options.units[i].style = i < 4 ? QJS_INTL_DURATION_UNIT_SHORT :
            i == 4 ? QJS_INTL_DURATION_NUMERIC :
            i < 7 ? QJS_INTL_DURATION_TWO_DIGIT : QJS_INTL_DURATION_FRACTIONAL;
        options.units[i].always = i >= 4 && i <= 6;
    }
    return options;
}
typedef struct SnapshotCase {
    QJSIntlProvider *provider;
    const QJSIntlDataView *view;
    uint32_t locale, system;
    QJSIntlNumberOptions number;
    QJSIntlDurationOptions duration;
    int is_duration;
} SnapshotCase;
typedef struct Trial {
    QJSIntlStatus status;
    size_t calls;
    unsigned int scans;
} Trial;
static SnapshotCase snapshot_case(QJSIntlProvider *provider, int is_duration)
{
    SnapshotCase subject = {0};
    subject.provider = provider;
    subject.view = qjs_intl_native_provider_view(provider);
    subject.locale = index_of(subject.view, QJS_INTL_DATA_LOCALE, "en");
    subject.system = index_of(subject.view, QJS_INTL_DATA_NUMBERING, "latn");
    subject.number = number_options();
    subject.duration = duration_options();
    subject.is_duration = is_duration;
    return subject;
}
/* Same allocator and selections for both routes. Record constructor requests
 * only; test transcript storage uses the host allocator outside the probe.
 * Every trial closes its snapshot and restores the provider's live baseline. */
static Trial snapshot_trial(const SnapshotCase *subject, int certified,
    size_t failure, size_t *sizes, size_t capacity)
{
    const QJSIntlAllocator *allocator =
        qjs_intl_native_provider_allocator(subject->provider);
    Heap *heap = allocator->opaque;
    size_t live = heap->live;
    unsigned int scans = load(&number_scans);
    Trial trial;
    heap->calls = 0;
    heap->failure = failure;
    heap->sizes = sizes;
    heap->size_capacity = capacity;
    if (subject->is_duration) {
        QJSIntlNativeDuration *duration = (QJSIntlNativeDuration *)(uintptr_t)1;
        if (certified)
            trial.status = qjs_intl_native_provider_duration_open(
                subject->provider, bytes("en-US"), bytes("latn"),
                &subject->duration, &duration);
        else
            trial.status = qjs_intl_native_duration_open_data(allocator,
                subject->view, subject->locale, subject->system,
                &subject->duration, &duration);
        assert((trial.status == QJS_INTL_OK) == (duration != NULL));
        heap->failure = 0;
        qjs_intl_native_duration_close(duration);
    } else {
        QJSIntlNativeNumber *number = (QJSIntlNativeNumber *)(uintptr_t)1;
        if (certified)
            trial.status = qjs_intl_native_provider_number_open(
                subject->provider, bytes("en-US"), bytes("latn"),
                &subject->number, &number);
        else
            trial.status = qjs_intl_native_number_open_data(allocator,
                subject->view, subject->locale, subject->system,
                &subject->number, &number);
        assert((trial.status == QJS_INTL_OK) == (number != NULL));
        heap->failure = 0;
        qjs_intl_native_number_close(number);
    }
    trial.calls = heap->calls;
    trial.scans = load(&number_scans) - scans;
    heap->sizes = NULL;
    heap->size_capacity = 0;
    assert(heap->live == live);
    if (certified) assert(!trial.scans);
    else if (trial.status == QJS_INTL_OK)
        assert(trial.scans == (subject->is_duration ? 14u : 1u));
    return trial;
}
static void equal_formatted(const QJSIntlFormatted *left,
    const QJSIntlFormatted *right)
{
    size_t i;
    assert(left->length && left->part_count);
    assert(left->length == right->length && left->part_count == right->part_count);
    assert(!memcmp(left->text, right->text, left->length * sizeof(*left->text)));
    for (i = 0; i < left->part_count; i++) {
        const QJSIntlPart *a = &left->parts[i], *b = &right->parts[i];
        assert(a->start == b->start && a->end == b->end);
        assert(a->type == b->type && a->source == b->source);
        assert(a->unit.length == b->unit.length);
        assert(!a->unit.length || !memcmp(a->unit.data, b->unit.data, a->unit.length));
    }
}
static void snapshot_output(const SnapshotCase *subject)
{
    const QJSIntlAllocator *allocator =
        qjs_intl_native_provider_allocator(subject->provider);
    Heap *heap = allocator->opaque;
    size_t live = heap->live;
    QJSIntlFormatted output[2] = {{0}};
    unsigned int scans = load(&number_scans);
    if (subject->is_duration) {
        QJSIntlNativeDuration *duration[2] = {NULL, NULL};
        QJSIntlDurationRecord record = {0};
        size_t i;
        assert(qjs_intl_native_provider_duration_open(subject->provider,
            bytes("en-US"), bytes("latn"), &subject->duration, &duration[0]) ==
            QJS_INTL_OK);
        assert(load(&number_scans) == scans);
        assert(qjs_intl_native_duration_open_data(allocator, subject->view,
            subject->locale, subject->system, &subject->duration, &duration[1]) ==
            QJS_INTL_OK);
        assert(load(&number_scans) == scans + 14);
        for (i = 0; i < QJS_INTL_DURATION_UNIT_COUNT; i++)
            record.magnitudes[i] = bytes("0");
        record.sign = -1;
        record.magnitudes[0] = bytes("2");
        record.magnitudes[4] = bytes("1");
        record.magnitudes[5] = bytes("2");
        record.magnitudes[6] = bytes("3");
        record.magnitudes[9] = bytes("1");
        for (i = 0; i < 2; i++)
            assert(qjs_intl_native_duration_format(duration[i], &record,
                &output[i]) == QJS_INTL_OK);
        equal_formatted(&output[0], &output[1]);
        for (i = 0; i < 2; i++) {
            qjs_intl_native_duration_result_clear(allocator, &output[i]);
            qjs_intl_native_duration_close(duration[i]);
        }
    } else {
        QJSIntlNativeNumber *number[2] = {NULL, NULL};
        QJSIntlMathematicalValue value = {QJS_INTL_FINITE, {"-12345.5", 8}};
        size_t i;
        assert(qjs_intl_native_provider_number_open(subject->provider,
            bytes("en-US"), bytes("latn"), &subject->number, &number[0]) ==
            QJS_INTL_OK);
        assert(load(&number_scans) == scans);
        assert(qjs_intl_native_number_open_data(allocator, subject->view,
            subject->locale, subject->system, &subject->number, &number[1]) ==
            QJS_INTL_OK);
        assert(load(&number_scans) == scans + 1);
        for (i = 0; i < 2; i++)
            assert(qjs_intl_native_number_format(number[i], &value, &output[i]) ==
                QJS_INTL_OK);
        equal_formatted(&output[0], &output[1]);
        for (i = 0; i < 2; i++) {
            qjs_intl_native_number_result_clear(number[i], &output[i]);
            qjs_intl_native_number_close(number[i]);
        }
    }
    assert(heap->live == live);
}
/* Number compares every constructor allocation boundary. Duration compares
 * bounded cold/middle/final boundaries; its existing integration sweeps still
 * exercise every allocation and retry. Repeating the entire generic Duration
 * sweep here would itself multiply whole-table scans by fourteen. */
static void snapshot_allocations(const SnapshotCase *subject)
{
    Trial baseline = snapshot_trial(subject, 1, 0, NULL, 0), left, right;
    size_t *original, *expected, *actual;
    size_t boundaries[4], count, i, failure;
    assert(baseline.status == QJS_INTL_OK && baseline.calls);
    assert(baseline.calls < SIZE_MAX / sizeof(*expected));
    original = malloc(baseline.calls * sizeof(*original));
    expected = malloc(baseline.calls * sizeof(*expected));
    actual = malloc(baseline.calls * sizeof(*actual));
    assert(original && expected && actual);
    left = snapshot_trial(subject, 1, 0, original, baseline.calls);
    right = snapshot_trial(subject, 0, 0, actual, baseline.calls);
    assert(left.status == QJS_INTL_OK && right.status == left.status);
    assert(left.calls == baseline.calls && right.calls == left.calls);
    assert(!memcmp(original, actual, left.calls * sizeof(*original)));
    boundaries[0] = 1;
    boundaries[1] = baseline.calls / 2 + baseline.calls % 2;
    boundaries[2] = baseline.calls;
    boundaries[3] = baseline.calls + 1;
    count = subject->is_duration ? 4 : baseline.calls + 1;
    for (i = 0; i < count; i++) {
        failure = subject->is_duration ? boundaries[i] : i + 1;
        left = snapshot_trial(subject, 1, failure, expected, baseline.calls);
        right = snapshot_trial(subject, 0, failure, actual, baseline.calls);
        assert(left.status == (failure <= baseline.calls ?
            QJS_INTL_NO_MEMORY : QJS_INTL_OK));
        assert(right.status == left.status && right.calls == left.calls);
        assert(!memcmp(expected, actual, left.calls * sizeof(*expected)));
        assert(!memcmp(original, expected, left.calls * sizeof(*original)));
        if (!subject->is_duration) assert(right.scans == 1);
        /* Both fault routes must recover with the original complete request
         * sequence. No failed snapshot changes the certificate or retry. */
        left = snapshot_trial(subject, 1, 0, expected, baseline.calls);
        right = snapshot_trial(subject, 0, 0, actual, baseline.calls);
        assert(left.status == QJS_INTL_OK && right.status == left.status);
        assert(left.calls == baseline.calls && right.calls == left.calls);
        assert(!memcmp(expected, actual, left.calls * sizeof(*expected)));
        assert(!memcmp(original, expected, left.calls * sizeof(*original)));
    }
    free(actual);
    free(expected);
    free(original);
    snapshot_output(subject);
}
static void number(void)
{
    Heap heap = {0}, other_heap = {0};
    QJSIntlProvider *provider = create(&heap, "en-US", "UTC", QJS_INTL_OK);
    QJSIntlProvider *other = create(&other_heap, "en", "UTC", QJS_INTL_OK);
    SnapshotCase subject = snapshot_case(provider, 0);
    QJSIntlDataView copy = *subject.view;
    unsigned int digits = 99, scans = load(&number_scans);
    assert(qjs_intl_native_provider_validate_number(provider, subject.view) ==
           QJS_INTL_DATA_OK);
    assert(qjs_intl_native_provider_validate_number(provider, &copy) ==
           QJS_INTL_DATA_INVALID_ARGUMENT);
    assert(qjs_intl_native_provider_validate_number(other, subject.view) ==
           QJS_INTL_DATA_INVALID_ARGUMENT);
    assert(qjs_intl_native_provider_validate_number(NULL, subject.view) ==
           QJS_INTL_DATA_INVALID_ARGUMENT);
    assert(qjs_intl_native_provider_validate_number(provider, NULL) ==
           QJS_INTL_DATA_INVALID_ARGUMENT);
    destroy(&other_heap, other);
    assert(qjs_intl_native_provider_number_currency_digits(provider,
        bytes("USD"), &digits) == QJS_INTL_OK && digits == 2);
    assert(load(&number_scans) == scans);
    digits = 99;
    assert(qjs_intl_native_number_currency_digits(subject.view,
        bytes("USD"), &digits) == QJS_INTL_OK && digits == 2);
    assert(load(&number_scans) == scans + 1);
    snapshot_allocations(&subject);
    subject.number.style = QJS_INTL_NUMBER_UNIT;
    subject.number.unit = bytes("meter-per-second");
    snapshot_allocations(&subject);
    subject.number = number_options();
    subject.number.style = QJS_INTL_NUMBER_CURRENCY;
    subject.number.currency = bytes("USD");
    subject.number.currency_display = QJS_INTL_CURRENCY_NAME;
    snapshot_allocations(&subject);
    subject.number = number_options();
    subject.number.notation = QJS_INTL_NUMBER_COMPACT;
    subject.number.compact_display = QJS_INTL_COMPACT_LONG;
    snapshot_allocations(&subject);
    assert(load(&reader_calls) == 1);
    destroy(&heap, provider);
}
static void duration(void)
{
    Heap heap = {0};
    QJSIntlProvider *provider = create(&heap, "en-US", "UTC", QJS_INTL_OK);
    SnapshotCase subject = snapshot_case(provider, 1);
    snapshot_allocations(&subject);
    assert(load(&reader_calls) == 1);
    destroy(&heap, provider);
}

typedef struct Rejection {
    const QJSIntlDataView *view;
    unsigned int calls;
} Rejection;
static QJSIntlDataStatus reject_number(void *opaque,
    const QJSIntlDataView *view)
{
    Rejection *rejection = opaque;
    assert(view == rejection->view);
    rejection->calls++;
    return QJS_INTL_DATA_INVALID;
}
static void number_mutable(void)
{
    Heap heap = {0};
    QJSIntlProvider *provider = create(&heap, "en-US", "UTC", QJS_INTL_OK);
    const QJSIntlAllocator *allocator =
        qjs_intl_native_provider_allocator(provider);
    SnapshotCase subject = snapshot_case(provider, 0);
    QJSIntlDataView view, owner_copy = *subject.view;
    QJSIntlDataSection section;
    QJSIntlDataSlice row;
    QJSIntlNativeNumber *number = NULL;
    QJSIntlNativeDuration *duration = NULL;
    Rejection rejection = {&view, 0};
    Trial trial;
    unsigned char *copy = malloc(qjs_intl_locale_metadata_blob_size);
    unsigned char *field, saved[4];
    unsigned int scans, digits;
    size_t live = heap.live;
    assert(copy);
    memcpy(copy, qjs_intl_locale_metadata_blob, qjs_intl_locale_metadata_blob_size);
    assert(qjs_intl_data_open(copy, qjs_intl_locale_metadata_blob_size, &view) ==
           QJS_INTL_DATA_OK);
    subject.view = &view;
    /* A copied owner view has identical immutable pointer/length but is not
     * the provider's exact object. Passing it through the private route fails. */
    scans = load(&number_scans);
    heap.calls = 0;
    number = (QJSIntlNativeNumber *)(uintptr_t)1;
    assert(qjs_intl_native_number_open_data_with_validator(allocator,
        &owner_copy, subject.locale, subject.system, &subject.number,
        qjs_intl_native_provider_validate_number, provider, &number) ==
        QJS_INTL_DATA_ERROR && !number);
    assert(!heap.calls && heap.live == live && load(&number_scans) == scans);
    /* Private dispatch has exactly the existing options -> table validation
     * -> indices order, including NULL output and malformed currency. */
    subject.number.maximum_output_length = 0;
    assert(qjs_intl_native_number_open_data_with_validator(allocator, &view,
        UINT32_MAX, UINT32_MAX, &subject.number, reject_number, &rejection,
        &number) == QJS_INTL_INVALID_ARGUMENT && !number);
    assert(!rejection.calls && !heap.calls);
    subject.number = number_options();
    assert(qjs_intl_native_number_open_data_with_validator(allocator, &view,
        UINT32_MAX, UINT32_MAX, &subject.number, reject_number, &rejection,
        NULL) == QJS_INTL_INVALID_ARGUMENT);
    assert(!rejection.calls);
    assert(qjs_intl_native_number_open_data_with_validator(allocator, &view,
        UINT32_MAX, UINT32_MAX, &subject.number, reject_number, &rejection,
        &number) == QJS_INTL_DATA_ERROR && !number);
    assert(rejection.calls == 1 && !heap.calls);
    digits = 99;
    assert(qjs_intl_native_number_currency_digits_with_validator(&view,
        bytes("usd"), reject_number, &rejection, &digits) ==
        QJS_INTL_INVALID_ARGUMENT && !digits);
    assert(rejection.calls == 1);
    digits = 99;
    assert(qjs_intl_native_number_currency_digits_with_validator(&view,
        bytes("USD"), reject_number, &rejection, &digits) ==
        QJS_INTL_DATA_ERROR && !digits);
    assert(rejection.calls == 2 && load(&number_scans) == scans);
    /* Mutate the same admitted utility buffer at a base Number invariant;
     * Number and currency utilities must each revalidate it on every call. */
    assert(qjs_intl_data_section(&view, QJS_INTL_DATA_NUMBER_SYMBOL, &section) ==
           QJS_INTL_DATA_OK && section.record_count);
    assert(qjs_intl_data_record(&section, 0, &row) == QJS_INTL_DATA_OK);
    field = copy + (row.data - view.data) + 8;
    saved[0] = *field;
    *field = 0; /* minimumGroupingDigits is required to be 1..9 */
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_DATA_ERROR && trial.scans == 1 && !trial.calls);
    subject.number.maximum_output_length = 0;
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_INVALID_ARGUMENT && !trial.scans && !trial.calls);
    subject.number = number_options();
    scans = load(&number_scans);
    digits = 99;
    assert(qjs_intl_native_number_currency_digits(&view, bytes("USD"), &digits) ==
           QJS_INTL_DATA_ERROR && !digits);
    assert(load(&number_scans) == scans + 1);
    *field = saved[0];
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_OK && trial.scans == 1);
    scans = load(&number_scans);
    assert(qjs_intl_native_number_currency_digits(&view, bytes("USD"), &digits) ==
           QJS_INTL_OK && digits == 2);
    assert(load(&number_scans) == scans + 1);
    /* Corrupt an extra table unrelated to decimal selection. This catches
     * accidental selected-row-only validation or arbitrary pointer caching. */
    assert(qjs_intl_data_section(&view, QJS_INTL_DATA_NUMBER_COMPACT, &section) ==
           QJS_INTL_DATA_OK && section.record_count);
    assert(qjs_intl_data_record(&section, 0, &row) == QJS_INTL_DATA_OK);
    field = copy + (row.data - view.data) + 16;
    memcpy(saved, field, sizeof(saved));
    memset(field, 0xff, sizeof(saved)); /* exponent exceeds magnitude */
    subject.locale = UINT32_MAX;
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_DATA_ERROR && trial.scans == 1 && !trial.calls);
    memcpy(field, saved, sizeof(saved));
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_INVALID_ARGUMENT && trial.scans == 1 && !trial.calls);
    subject.locale = index_of(&view, QJS_INTL_DATA_LOCALE, "en");
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_OK && trial.scans == 1);
    /* Generic Duration keeps the same full Number checks and clears partial
     * List/clock/Number ownership on rejection. The next call recovers. */
    memset(field, 0xff, sizeof(saved));
    subject.is_duration = 1;
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_DATA_ERROR && trial.scans == 1);
    memcpy(field, saved, sizeof(saved));
    trial = snapshot_trial(&subject, 0, 0, NULL, 0);
    assert(trial.status == QJS_INTL_OK && trial.scans == 14);
    /* The private Duration callback is borrowed only during synchronous
     * construction; rejection closes the partial owner and leaves NULL. */
    heap.calls = 0;
    duration = (QJSIntlNativeDuration *)(uintptr_t)1;
    assert(qjs_intl_native_duration_open_data_with_validator(allocator, &view,
        subject.locale, subject.system, &subject.duration, reject_number,
        &rejection, &duration) == QJS_INTL_DATA_ERROR && !duration);
    assert(rejection.calls == 3 && heap.live == live);
    /* Provider admission ordering and invalid options remain independent of
     * the certificate. These failures allocate no Number snapshot. */
    scans = load(&number_scans);
    heap.calls = 0;
    subject.number.maximum_output_length = 0;
    assert(qjs_intl_native_provider_number_open(provider, bytes("en-US"),
        bytes("latn"), &subject.number, &number) == QJS_INTL_INVALID_ARGUMENT &&
        !number);
    assert(qjs_intl_native_provider_number_open(provider, bytes("fr"),
        bytes("latn"), &subject.number, &number) == QJS_INTL_UNSUPPORTED && !number);
    assert(qjs_intl_native_provider_number_open(provider, bytes("en-US"),
        bytes("missing"), &subject.number, &number) == QJS_INTL_UNSUPPORTED && !number);
    assert(!heap.calls && load(&number_scans) == scans && heap.live == live);
    subject.view = qjs_intl_native_provider_view(provider);
    trial = snapshot_trial(&subject, 1, 0, NULL, 0);
    assert(trial.status == QJS_INTL_OK && !trial.scans);
    memset(&view, 0, sizeof(view));
    free(copy);
    assert(load(&reader_calls) == 1);
    destroy(&heap, provider);
}

/* A success proof is independent of allocator and provider lifetime.
 * Failed probes remain retryable; warm list construction still handles OOM. */
static void available(void)
{
    static const QJSIntlService services[] = {
        QJS_INTL_NUMBER_FORMAT, QJS_INTL_PLURAL_RULES, QJS_INTL_RELATIVE_TIME_FORMAT,
        QJS_INTL_DISPLAY_NAMES, QJS_INTL_DURATION_FORMAT, QJS_INTL_SEGMENTER,
        QJS_INTL_COLLATOR, QJS_INTL_COLLATOR_SEARCH
    };
    size_t i;
    for (i = 0; i < sizeof(services) / sizeof(services[0]); i++) {
        Heap cold = {0}, warm_heap = {0};
        QJSIntlProvider *provider = create(&cold, "en-US", "UTC", QJS_INTL_OK);
        QJSIntlTagList tags = {0};
        size_t before, cold_calls;
        cold.failure = cold.calls + 1;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_NO_MEMORY);
        assert(!tags.count && !tags.items);
        cold.failure = 0;
        before = cold.calls;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_OK);
        cold_calls = cold.calls - before;
        assert(cold_calls > 3 && tags.count == 2);
        equal(tags.items[0], "en"); equal(tags.items[1], "en-US");
        qjs_intl_tag_list_clear(provider, &tags);
        destroy(&cold, provider);
        provider = create(&warm_heap, "en-US", "UTC", QJS_INTL_OK);
        warm_heap.failure = warm_heap.calls + 1;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_NO_MEMORY);
        assert(!tags.count && !tags.items);
        warm_heap.failure = 0;
        before = warm_heap.calls;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_OK);
        assert(warm_heap.calls - before == 3 && tags.count == 2);
        printf("native service proof %u cold allocations=%zu warm allocations=3\n",
               (unsigned int)services[i], cold_calls);
        qjs_intl_tag_list_clear(provider, &tags);
        destroy(&warm_heap, provider);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Run via test-intl-native-compiled-cache.\n");
        return 2;
    }
    if (!strcmp(argv[1], "retry")) retry();
    else if (!strcmp(argv[1], "provider-failure")) provider_failure();
    else if (!strcmp(argv[1], "concurrent")) concurrent();
    else if (!strcmp(argv[1], "mutable")) mutable();
    else if (!strcmp(argv[1], "number")) number();
    else if (!strcmp(argv[1], "duration")) duration();
    else if (!strcmp(argv[1], "available")) available();
    else if (!strcmp(argv[1], "number-mutable")) number_mutable();
    else return 2;
    return 0;
}

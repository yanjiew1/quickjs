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
#include "../src/intl/provider-native-plural.h"
#include "../src/intl/provider-native-relative.h"
#include "../src/intl/data/plural-data-validation.h"
#include "../src/intl/data/relative-data-validation.h"
#include "../src/intl/data/date-data-validation.h"
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
static TestAtomic plural_scans = 0, relative_scans = 0, date_scans = 0;

QJSIntlDataStatus qjs_intl_counted_number_validate(const QJSIntlDataView *);
QJSIntlDataStatus qjs_intl_counted_number_validate(const QJSIntlDataView *view)
{
    increment(&number_scans);
    return qjs_intl_number_extra_data_validate(view);
}

QJSIntlDataStatus qjs_intl_counted_plural_validate(const QJSIntlDataView *);
QJSIntlDataStatus qjs_intl_counted_plural_validate(const QJSIntlDataView *view)
{
    increment(&plural_scans);
    return qjs_intl_data_validate_plural_extension(view);
}
QJSIntlDataStatus qjs_intl_counted_relative_validate(const QJSIntlDataView *);
QJSIntlDataStatus qjs_intl_counted_relative_validate(const QJSIntlDataView *view)
{
    increment(&relative_scans);
    return qjs_intl_data_validate_relative_extension(view);
}
QJSIntlDataStatus qjs_intl_counted_date_validate(const QJSIntlDataView *);
QJSIntlDataStatus qjs_intl_counted_date_validate(const QJSIntlDataView *view)
{
    increment(&date_scans);
    return qjs_intl_date_data_validate(view);
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
/* Both routes share the post-validation allocation/cleanup implementation.
 * Compare their complete successful traces and output, then keep every Number
 * fault/retry on that shared code. Whole validation is allocation-free; ordinary
 * first/middle/final/sentinel probes cover the route without repeating the same
 * full-table scan at every identical allocation boundary. Duration retains its
 * bounded comparisons and existing independent integration sweeps. */
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
        int compare_ordinary;
        failure = subject->is_duration ? boundaries[i] : i + 1;
        compare_ordinary = subject->is_duration || failure == boundaries[0] ||
            failure == boundaries[1] || failure == boundaries[2] || failure == boundaries[3];
        left = snapshot_trial(subject, 1, failure, expected, baseline.calls);
        assert(left.status == (failure <= baseline.calls ?
            QJS_INTL_NO_MEMORY : QJS_INTL_OK));
        assert(!memcmp(original, expected, left.calls * sizeof(*original)));
        if (compare_ordinary) {
            right = snapshot_trial(subject, 0, failure, actual, baseline.calls);
            assert(right.status == left.status && right.calls == left.calls);
            assert(!memcmp(expected, actual, left.calls * sizeof(*expected)));
            if (!subject->is_duration) assert(right.scans == 1);
        }
        /* Every fault still requires the original complete successful retry. */
        left = snapshot_trial(subject, 1, 0, expected, baseline.calls);
        assert(left.status == QJS_INTL_OK && left.calls == baseline.calls);
        assert(!memcmp(original, expected, left.calls * sizeof(*original)));
        if (compare_ordinary) {
            right = snapshot_trial(subject, 0, 0, actual, baseline.calls);
            assert(right.status == left.status && right.calls == left.calls);
            assert(!memcmp(expected, actual, left.calls * sizeof(*expected)));
        }
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
    assert(qjs_intl_native_provider_validate_compiled_data(provider, subject.view) ==
           QJS_INTL_DATA_OK);
    assert(qjs_intl_native_provider_validate_compiled_data(provider, &copy) ==
           QJS_INTL_DATA_INVALID_ARGUMENT);
    assert(qjs_intl_native_provider_validate_compiled_data(other, subject.view) ==
           QJS_INTL_DATA_INVALID_ARGUMENT);
    assert(qjs_intl_native_provider_validate_compiled_data(NULL, subject.view) ==
           QJS_INTL_DATA_INVALID_ARGUMENT);
    assert(qjs_intl_native_provider_validate_compiled_data(provider, NULL) ==
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
        qjs_intl_native_provider_validate_compiled_data, provider, &number) ==
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
    assert(qjs_intl_native_provider_number_open(provider, bytes("zz"),
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

/* Admission must agree with actual default-number formatter construction. */
static void number_locale_list(QJSIntlProvider *provider, const QJSIntlTagList *tags)
{
    QJSIntlNumberOptions options = number_options();
    QJSIntlDataSection locales;
    const QJSIntlDataView *view = qjs_intl_native_provider_view(provider);
    size_t i;
    assert(qjs_intl_data_section(view, QJS_INTL_DATA_LOCALE, &locales) == QJS_INTL_DATA_OK);
    for (i = 0; i < tags->count; i++) {
        QJSIntlNativeNumber *number = NULL;
        QJSIntlBytes preferred;
        QJSIntlDataSlice borrowed;
        uint32_t row;
        assert(qjs_intl_native_provider_locale_row(provider, tags->items[i], &row) == QJS_INTL_OK);
        assert(qjs_intl_native_provider_default_numbering(provider, row, &preferred) == QJS_INTL_OK);
        assert(qjs_intl_data_record_string(view, &locales, row, 0, &borrowed) == QJS_INTL_DATA_OK);
        assert(tags->items[i].data != (const char *)borrowed.data);
        assert(qjs_intl_native_provider_number_open(provider, tags->items[i], preferred,
            &options, &number) == QJS_INTL_OK && number);
        qjs_intl_native_number_close(number);
    }
    {
        QJSIntlTagList other = {0};
        assert(qjs_intl_locale_key_values(provider, QJS_INTL_RELATIVE_TIME_FORMAT,
            bytes("de"), bytes("nu"), &other) == QJS_INTL_OK);
        assert(other.count && other.items && other.items[0].length == 4 &&
               !memcmp(other.items[0].data, "latn", 4));
        qjs_intl_tag_list_clear(provider, &other);
    }
}

static void list_locale_list(QJSIntlProvider *provider, const QJSIntlTagList *tags)
{
    size_t i;
    unsigned int type, style;
    for (i = 0; i < tags->count; i++) {
        for (type = 0; type < 3; type++) {
            for (style = 0; style < 3; style++) {
                QJSIntlNativeList *list = NULL;
                assert(qjs_intl_native_provider_list_open(provider, tags->items[i],
                    (QJSIntlListType)type, (QJSIntlListStyle)style, &list) == QJS_INTL_OK);
                assert(list);
                qjs_intl_native_list_close(list);
            }
        }
    }
}

static void plural_locale_list(QJSIntlProvider *provider, const QJSIntlTagList *tags)
{
    QJSIntlPluralOptions options = {0};
    size_t i;
    unsigned int type;
    options.digits = number_options().digits;
    for (i = 0; i < tags->count; i++) {
        for (type = 0; type < 2; type++) {
            QJSIntlNativePlural *plural = NULL;
            options.type = (QJSIntlPluralType)type;
            assert(qjs_intl_native_provider_plural_open(provider, tags->items[i],
                &options, &plural) == QJS_INTL_OK && plural);
            qjs_intl_native_plural_close(plural);
        }
    }
}

static QJSIntlStatus admission_number_format(void *opaque,
    const QJSIntlDecimalResult *value, QJSIntlFormatted *out)
{
    return qjs_intl_native_number_format_rounded(opaque, value, out);
}
static void admission_number_clear(void *opaque, QJSIntlFormatted *out)
{
    qjs_intl_native_number_result_clear(opaque, out);
}
static void relative_locale_list(QJSIntlProvider *provider, const QJSIntlTagList *tags)
{
    QJSIntlNumberOptions number_options_value = number_options();
    QJSIntlRelativeOptions options = {0};
    size_t i;
    unsigned int style, numeric;
    options.maximum_output_length = 4096;
    for (i = 0; i < tags->count; i++) {
        QJSIntlNativeNumber *number = NULL;
        QJSIntlBytes preferred;
        QJSIntlRelativeNumberBridge bridge;
        uint32_t row;
        assert(qjs_intl_native_provider_locale_row(provider, tags->items[i], &row) == QJS_INTL_OK);
        assert(qjs_intl_native_provider_default_numbering(provider, row, &preferred) == QJS_INTL_OK);
        assert(qjs_intl_native_provider_number_open(provider, tags->items[i], preferred,
            &number_options_value, &number) == QJS_INTL_OK && number);
        bridge = (QJSIntlRelativeNumberBridge){number, admission_number_format, admission_number_clear};
        for (style = 0; style < 3; style++) {
            for (numeric = 0; numeric < 2; numeric++) {
                QJSIntlNativeRelative *relative = NULL;
                options.style = (QJSIntlRelativeStyle)style;
                options.numeric = (QJSIntlRelativeNumeric)numeric;
                assert(qjs_intl_native_provider_relative_open(provider, tags->items[i],
                    &options, &bridge, &relative) == QJS_INTL_OK && relative);
                qjs_intl_native_relative_close(relative);
            }
        }
        qjs_intl_native_number_close(number);
    }
}

/* Warm failures include partial result lists, not just the array allocation. */
static void available_list_faults(QJSIntlProvider *provider, Heap *heap,
    QJSIntlService service, size_t count, size_t requests)
{
    size_t failure, live = heap->live;
    for (failure = 1; failure <= requests + 1; failure++) {
        QJSIntlTagList tags = {0};
        QJSIntlStatus status;
        heap->failure = heap->calls + failure;
        status = qjs_intl_locale_available(provider, service, &tags);
        heap->failure = 0;
        if (failure <= requests) {
            assert(status == QJS_INTL_NO_MEMORY && !tags.items && !tags.count);
        } else {
            assert(status == QJS_INTL_OK && tags.count == count);
        }
        qjs_intl_tag_list_clear(provider, &tags);
        assert(heap->live == live);
        assert(qjs_intl_locale_available(provider, service, &tags) == QJS_INTL_OK);
        assert(tags.count == count);
        qjs_intl_tag_list_clear(provider, &tags);
        assert(heap->live == live);
    }
}

/* A success proof is independent of allocator and provider lifetime.
 * Failed probes remain retryable; warm list construction still handles OOM. */
static void available(void)
{
    static const QJSIntlService services[] = {
        QJS_INTL_NUMBER_FORMAT, QJS_INTL_LIST_FORMAT, QJS_INTL_PLURAL_RULES,
        QJS_INTL_RELATIVE_TIME_FORMAT,
        QJS_INTL_DISPLAY_NAMES, QJS_INTL_DURATION_FORMAT, QJS_INTL_SEGMENTER,
        QJS_INTL_COLLATOR, QJS_INTL_COLLATOR_SEARCH
    };
    size_t i;
    for (i = 0; i < sizeof(services) / sizeof(services[0]); i++) {
        Heap cold = {0}, warm_heap = {0};
        QJSIntlProvider *provider = create(&cold, "en-US", "UTC", QJS_INTL_OK);
        QJSIntlTagList tags = {0};
        size_t before, cold_calls, admitted_count, warm_calls, j;
        int english = 0, american = 0;
        cold.failure = cold.calls + 1;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_NO_MEMORY);
        assert(!tags.count && !tags.items);
        cold.failure = 0;
        before = cold.calls;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_OK);
        cold_calls = cold.calls - before;
        admitted_count = tags.count;
        assert(admitted_count >= 2 && cold_calls > admitted_count + 1);
        for (j = 0; j < tags.count; j++) {
            if (tags.items[j].length == 2 && !memcmp(tags.items[j].data, "en", 2)) english = 1;
            if (tags.items[j].length == 5 && !memcmp(tags.items[j].data, "en-US", 5)) american = 1;
        }
        assert(english && american);
        if (services[i] == QJS_INTL_NUMBER_FORMAT) number_locale_list(provider, &tags);
        if (services[i] == QJS_INTL_LIST_FORMAT) list_locale_list(provider, &tags);
        if (services[i] == QJS_INTL_PLURAL_RULES) plural_locale_list(provider, &tags);
        if (services[i] == QJS_INTL_RELATIVE_TIME_FORMAT) relative_locale_list(provider, &tags);
        qjs_intl_tag_list_clear(provider, &tags);
        destroy(&cold, provider);
        provider = create(&warm_heap, "en-US", "UTC", QJS_INTL_OK);
        warm_heap.failure = warm_heap.calls + 1;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_NO_MEMORY);
        assert(!tags.count && !tags.items);
        warm_heap.failure = 0;
        before = warm_heap.calls;
        assert(qjs_intl_locale_available(provider, services[i], &tags) == QJS_INTL_OK);
        warm_calls = warm_heap.calls - before;
        assert(warm_calls == admitted_count + 1 && tags.count == admitted_count);
        printf("native service proof %u cold allocations=%zu warm allocations=%zu\n",
               (unsigned int)services[i], cold_calls, admitted_count + 1);
        if (services[i] == QJS_INTL_NUMBER_FORMAT) number_locale_list(provider, &tags);
        if (services[i] == QJS_INTL_LIST_FORMAT) list_locale_list(provider, &tags);
        if (services[i] == QJS_INTL_PLURAL_RULES) plural_locale_list(provider, &tags);
        if (services[i] == QJS_INTL_RELATIVE_TIME_FORMAT) relative_locale_list(provider, &tags);
        qjs_intl_tag_list_clear(provider, &tags);
        available_list_faults(provider, &warm_heap, services[i], admitted_count, warm_calls);
        destroy(&warm_heap, provider);
    }
}

static QJSIntlStatus table_number(void *opaque, const QJSIntlDecimalResult *value,
    QJSIntlFormatted *out)
{
    return qjs_intl_native_number_format_rounded(opaque, value, out);
}
static void table_number_clear(void *opaque, QJSIntlFormatted *out)
{
    qjs_intl_native_number_result_clear(opaque, out);
}
static void table_certificates(void)
{
    Heap heap = {0};
    QJSIntlProvider *provider = create(&heap, "en-US", "UTC", QJS_INTL_OK);
    const QJSIntlAllocator *allocator = qjs_intl_native_provider_allocator(provider);
    const QJSIntlDataView *view = qjs_intl_native_provider_view(provider);
    QJSIntlDataView copied = *view, mutable_view;
    QJSIntlNumberOptions number_options_value = number_options();
    QJSIntlPluralOptions plural_options = {0};
    QJSIntlRelativeOptions relative_options = {0};
    QJSIntlNativePlural *plain_plural = NULL, *certified_plural = NULL;
    QJSIntlNativeRelative *plain_relative = NULL, *certified_relative = NULL;
    QJSIntlNativeNumber *number = NULL;
    QJSIntlRelativeNumberBridge bridge;
    QJSIntlMathematicalValue value = {QJS_INTL_FINITE, {"-1", 2}};
    QJSIntlPluralCategory first, second;
    QJSIntlFormatted plain_result = {0}, certified_result = {0};
    QJSIntlDataSection bindings, rules, names;
    QJSIntlDataSlice row;
    uint32_t locale = index_of(view, QJS_INTL_DATA_LOCALE, "en");
    uint32_t i, key, rule, mask, certified_mask;
    unsigned char *bytes_copy, saved;
    size_t live = heap.live;
    plural_options.digits = number_options_value.digits;
    assert(qjs_intl_native_plural_open_data(allocator, view, locale, &plural_options,
        &plain_plural) == QJS_INTL_OK && load(&plural_scans) == 1);
    assert(qjs_intl_native_provider_plural_open(provider, bytes("en"), &plural_options,
        &certified_plural) == QJS_INTL_OK && load(&plural_scans) == 1);
    assert(qjs_intl_native_plural_select(plain_plural, &value, &first) == QJS_INTL_OK);
    assert(qjs_intl_native_plural_select(certified_plural, &value, &second) == QJS_INTL_OK);
    assert(first == second);
    qjs_intl_native_plural_close(plain_plural); plain_plural = NULL;
    qjs_intl_native_plural_close(certified_plural); certified_plural = NULL;
    assert(qjs_intl_native_plural_open_data_with_validator(allocator, &copied, locale,
        &plural_options, qjs_intl_native_provider_validate_compiled_data, provider,
        &certified_plural) == QJS_INTL_DATA_ERROR && !certified_plural);
    assert(heap.live == live && load(&plural_scans) == 1);
    assert(qjs_intl_native_provider_number_open(provider, bytes("en"), bytes("latn"),
        &number_options_value, &number) == QJS_INTL_OK);
    bridge = (QJSIntlRelativeNumberBridge){number, table_number, table_number_clear};
    relative_options.maximum_output_length = 4096;
    assert(qjs_intl_native_relative_open_data(allocator, view, locale, &relative_options,
        &bridge, &plain_relative) == QJS_INTL_OK && load(&relative_scans) == 1);
    assert(qjs_intl_native_provider_relative_open(provider, bytes("en"), &relative_options,
        &bridge, &certified_relative) == QJS_INTL_OK && load(&relative_scans) == 1);
    assert(qjs_intl_native_relative_format(plain_relative, &value, QJS_INTL_RELATIVE_DAY,
        &plain_result) == QJS_INTL_OK);
    assert(qjs_intl_native_relative_format(certified_relative, &value, QJS_INTL_RELATIVE_DAY,
        &certified_result) == QJS_INTL_OK);
    equal_formatted(&plain_result, &certified_result);
    qjs_intl_native_relative_result_clear(allocator, &plain_result);
    qjs_intl_native_relative_result_clear(allocator, &certified_result);
    qjs_intl_native_relative_close(plain_relative); plain_relative = NULL;
    qjs_intl_native_relative_close(certified_relative); certified_relative = NULL;
    assert(qjs_intl_native_relative_open_data_with_validator(allocator, &copied, locale,
        &relative_options, &bridge, qjs_intl_native_provider_validate_compiled_data,
        provider, &certified_relative) == QJS_INTL_DATA_ERROR && !certified_relative);
    assert(load(&relative_scans) == 1);
    assert(qjs_intl_native_date_data_calendar_mask(view, locale, &mask) == QJS_INTL_OK &&
        load(&date_scans) == 1);
    assert(qjs_intl_native_provider_date_calendar_mask(provider, &certified_mask) == QJS_INTL_OK &&
        mask == certified_mask && load(&date_scans) == 1);
    assert(qjs_intl_native_date_data_calendar_mask_with_validator(&copied, locale,
        qjs_intl_native_provider_validate_compiled_data, provider, &certified_mask) ==
        QJS_INTL_DATA_ERROR && !certified_mask && load(&date_scans) == 1);
    bytes_copy = malloc(view->length); assert(bytes_copy);
    memcpy(bytes_copy, view->data, view->length);
    assert(qjs_intl_data_open(bytes_copy, view->length, &mutable_view) == QJS_INTL_DATA_OK);
    assert(qjs_intl_data_section(&mutable_view, QJS_INTL_DATA_PLURAL_LOCALE, &bindings) == QJS_INTL_DATA_OK);
    assert(qjs_intl_data_section(&mutable_view, QJS_INTL_DATA_PLURAL_RULE, &rules) == QJS_INTL_DATA_OK);
    for (i = 0; i < bindings.record_count; i++) {
        assert(qjs_intl_data_record_u32(&bindings, i, 0, &key) == QJS_INTL_DATA_OK);
        assert(qjs_intl_data_record(&bindings, i, &row) == QJS_INTL_DATA_OK);
        if (key == locale && row.data[4] == QJS_INTL_PLURAL_ORDINAL) break;
    }
    assert(i < bindings.record_count);
    assert(qjs_intl_data_record_u32(&bindings, i, 8, &rule) == QJS_INTL_DATA_OK);
    assert(qjs_intl_data_record(&rules, rule, &row) == QJS_INTL_DATA_OK);
    saved = row.data[0]; bytes_copy[row.data - mutable_view.data] = 255;
    /* Corrupt an unselected ordinal rule: cardinal/relative utilities must
       still validate their entire caller-owned plural dependency. */
    assert(qjs_intl_native_plural_open_data(allocator, &mutable_view, locale, &plural_options,
        &plain_plural) == QJS_INTL_DATA_ERROR && !plain_plural && load(&plural_scans) == 2);
    assert(qjs_intl_native_relative_open_data(allocator, &mutable_view, locale, &relative_options,
        &bridge, &plain_relative) == QJS_INTL_DATA_ERROR && !plain_relative && load(&relative_scans) == 2);
    bytes_copy[row.data - mutable_view.data] = saved;
    qjs_intl_native_number_close(number); number = NULL;
    bridge.opaque = NULL;
    assert(qjs_intl_data_section(&mutable_view, 101, &names) == QJS_INTL_DATA_OK);
    assert(qjs_intl_data_record(&names, 0, &row) == QJS_INTL_DATA_OK);
    saved = row.data[15]; bytes_copy[row.data - mutable_view.data + 15] = 1;
    assert(qjs_intl_native_date_data_calendar_mask(&mutable_view, locale, &certified_mask) ==
        QJS_INTL_DATA_ERROR && !certified_mask && load(&date_scans) == 2);
    bytes_copy[row.data - mutable_view.data + 15] = saved;
    free(bytes_copy);
    assert(heap.live == live && load(&reader_calls) == 1);
    destroy(&heap, provider);
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
    else if (!strcmp(argv[1], "tables")) table_certificates();
    else if (!strcmp(argv[1], "available")) available();
    else if (!strcmp(argv[1], "number-mutable")) number_mutable();
    else return 2;
    return 0;
}

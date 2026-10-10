/* Exact bounded stdio reader errors. No filesystem/host tzdata dependency. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct {
    size_t length;
    int open_error, read_error, extra_error, close_error, extra_byte, fail_malloc;
    int opens, closes, allocations, frees, phase;
} io;
static FILE *mock_open(const char *path, const char *mode)
{
    io.opens++;
    assert(strstr(path, "/UTC") && !strcmp(mode, "rb"));
    if (io.open_error) { errno = io.open_error; return NULL; }
    return (FILE *)&io;
}
static size_t mock_read(void *data, size_t size, size_t count, FILE *file)
{
    size_t length = io.length < count ? io.length : count;
    assert(file == (FILE *)&io && size == 1);
    io.phase = 1; memset(data, 0x35, length);
    if (io.read_error) errno = io.read_error;
    return length;
}
static int mock_get(FILE *file)
{
    assert(file == (FILE *)&io); io.phase = 2;
    if (io.extra_error) errno = io.extra_error;
    return io.extra_byte ? 0x35 : EOF;
}
static int mock_error(FILE *file)
{
    assert(file == (FILE *)&io);
    return io.phase == 1 ? io.read_error != 0 : io.extra_error != 0;
}
static int mock_close(FILE *file)
{
    assert(file == (FILE *)&io); io.closes++;
    if (io.close_error) { errno = io.close_error; return EOF; }
    return 0;
}
static void *mock_malloc(size_t size)
{
    void *p;
    if (io.fail_malloc) return NULL;
    p = malloc(size); if (p) io.allocations++; return p;
}
static void mock_free(void *p)
{ if (p) io.frees++; free(p); }

/* Include the exact production reader with only stdio/heap boundaries
   replaced after their declarations. TZif and embedded code remain linked
   production modules. No alternative reader implementation is tested. */
#define fopen mock_open
#define fread mock_read
#define fgetc mock_get
#define ferror mock_error
#define fclose mock_close
#define malloc mock_malloc
#define free mock_free
#include "../src/timezone/timezone.c"
#undef fopen
#undef fread
#undef fgetc
#undef ferror
#undef fclose
#undef malloc
#undef free

static void reset(void)
{ memset(&io, 0, sizeof io); io.length = 110; }
static void failure(int expected, int closes)
{
    const unsigned char *bytes = (void *)1; size_t size = 42;
    assert(system_read(NULL, "UTC", &bytes, &size) == expected);
    assert(bytes == (void *)1 && size == 42);
    assert(io.opens == 1 && io.closes == closes && io.allocations == io.frees);
}
int main(void)
{
    const unsigned char *bytes; size_t size;
    reset(); errno = ENOMEM; /* stale errno must not turn valid EOF into OOM */
    assert(!system_read(NULL, "UTC", &bytes, &size));
    assert(size == 110 && bytes[0] == 0x35 && io.closes == 1);
    system_release(NULL, bytes, size); assert(io.allocations == io.frees);
    reset(); io.length = QJS_TZ_MAX_FILE_SIZE;
    assert(!system_read(NULL, "UTC", &bytes, &size));
    assert(size == QJS_TZ_MAX_FILE_SIZE); system_release(NULL, bytes, size);
    reset(); io.open_error = ENOENT; failure(QJS_TZ_ABSENT, 0);
    reset(); io.open_error = ENOMEM; failure(QJS_TZ_MEMORY, 0);
    reset(); io.fail_malloc = 1; failure(QJS_TZ_MEMORY, 1);
    reset(); io.read_error = EIO; failure(QJS_TZ_INVALID, 1);
    reset(); io.read_error = ENOMEM; failure(QJS_TZ_MEMORY, 1);
    reset(); io.extra_error = EIO; failure(QJS_TZ_INVALID, 1);
    reset(); io.extra_error = ENOMEM; failure(QJS_TZ_MEMORY, 1);
    reset(); io.length = QJS_TZ_MAX_FILE_SIZE; io.extra_byte = 1;
    failure(QJS_TZ_INVALID, 1);
    reset(); io.close_error = EIO; failure(QJS_TZ_INVALID, 1);
    reset(); io.close_error = ENOMEM; failure(QJS_TZ_MEMORY, 1);
    return 0;
}

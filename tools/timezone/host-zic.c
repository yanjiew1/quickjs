/* Build the pinned standalone zic without modifying third_party/tz. */

/* Host data generation needs no message catalog or libintl dependency. */
#define HAVE_GETTEXT 0

#if defined(__MINGW32__)
/* Upstream's documented native Windows portability settings.  The
   HAVE_DIRECT_H branch supplies uid_t/gid_t and maps mkdir to _mkdir.
   MinGW declares mempcpy, but upstream's default detection misses it.
   Upstream copies zone aliases when hard and symbolic links are absent. */
#define HAVE_DIRECT_H 1
#define HAVE_FCHMOD 0
#define HAVE_LINK 0
#define HAVE_SYMLINK 0
#define HAVE_MEMPCPY 1
#define HAVE_PWD_H 0
#define HAVE_GETRANDOM 0
#endif

/* This translation unit belongs only to HOST_ZIC, never libquickjs. */
#include "../../third_party/tz/zic.c"

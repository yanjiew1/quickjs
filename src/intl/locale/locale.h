/* Locale identity service contract; no backend or engine representation.
 * Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
#ifndef QJS_INTL_LOCALE_H
#define QJS_INTL_LOCALE_H
#include "../common/types.h"

typedef struct QJSIntlLocaleService QJSIntlLocaleService;

/* Open the compiled backend and borrow its immutable data. Copy allocator
   callbacks; their opaque state must outlive the service and its outputs.
   On failure, *out is NULL. Closing NULL is safe. No callback runs user code.
   A service has immutable state and no retained operation scratch. */
QJSIntlStatus qjs_intl_locale_open(const QJSIntlAllocator *allocator,
                                   QJSIntlLocaleService **out);
void qjs_intl_locale_close(QJSIntlLocaleService *service);
/* Release an owned identity result with the service's original allocator.
   Passing NULL output is safe; a non-NULL output requires its live service. */
void qjs_intl_locale_result_free(const QJSIntlLocaleService *service, char *result);
/* Match a canonical request against the compiled profile's default-locale
   candidates, excluding root. Return a borrowed extension-free identifier or
   UNSUPPORTED. This is default policy input, not a substitute for each service's
   own capability inventory. All services must admit the selected default. */
QJSIntlStatus qjs_intl_locale_profile_match(const QJSIntlLocaleService *service,
                                          QJSIntlBytes request, QJSIntlBytes *out);
/* Syntax failure is OK with *well_formed=0; resource failures remain errors. */
QJSIntlStatus qjs_intl_locale_validate(const QJSIntlLocaleService *service,
                                      QJSIntlBytes input, int *well_formed);

/* Length-bearing Unicode locale input. On success, return a separately owned
   NUL-terminated string, released using the supplied allocator's free callback.
   On failure, *out is NULL. Output storage must not overlap input or service.
   INVALID_ARGUMENT identifies malformed locale syntax; NO_MEMORY is never a
   locale fallback. Identity does not imply formatter locale availability. */
QJSIntlStatus qjs_intl_locale_canonicalize(const QJSIntlLocaleService *service,
                                           QJSIntlBytes input, char **out);
QJSIntlStatus qjs_intl_locale_maximize(const QJSIntlLocaleService *service,
                                       QJSIntlBytes input, char **out);
QJSIntlStatus qjs_intl_locale_minimize(const QJSIntlLocaleService *service,
                                       QJSIntlBytes input, char **out);
QJSIntlStatus
qjs_intl_locale_canonicalize_uvalue(const QJSIntlLocaleService *service,
                                    QJSIntlBytes key, QJSIntlBytes value,
                                    char **out);
#endif

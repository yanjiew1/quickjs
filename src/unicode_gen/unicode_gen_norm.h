#ifndef UNICODE_GEN_NORM_H
#define UNICODE_GEN_NORM_H

#include "unicode_gen_common.h"

void build_cc_table(FILE *f);
void build_decompose_table(FILE *f);
#ifdef USE_TEST
void check_decompose_table(void);
void check_compose_table(void);
void check_cc_table(void);
void normalization_test(const char *filename);
#endif

#endif /* UNICODE_GEN_NORM_H */

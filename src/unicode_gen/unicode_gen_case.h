#ifndef UNICODE_GEN_CASE_H
#define UNICODE_GEN_CASE_H

#include "unicode_gen_common.h"

BOOL is_complicated_case(const CCInfo *ci);
void find_run_type(TableEntry *te, CCInfo *tab, int code);
void build_conv_table(CCInfo *tab);
void dump_case_conv_table(FILE *f);
void dump_case_folding_special_cases(CCInfo *tab);
#ifdef USE_TEST
void check_case_conv(void);
#endif

#endif /* UNICODE_GEN_CASE_H */

#ifndef UNICODE_GEN_PROP_H
#define UNICODE_GEN_PROP_H

#include "unicode_gen_common.h"

void compute_internal_props(void);
void build_flags_tables(FILE *f);
void build_general_category_table(FILE *f);
void build_script_table(FILE *f);
void build_script_ext_table(FILE *f);
void build_prop_list_table(FILE *f);
void build_sequence_prop_list_table(FILE *f);
#ifdef USE_TEST
void check_flags(void);
#endif

#endif /* UNICODE_GEN_PROP_H */

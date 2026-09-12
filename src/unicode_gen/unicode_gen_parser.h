#ifndef UNICODE_GEN_PARSER_H
#define UNICODE_GEN_PARSER_H

#include "unicode_gen_common.h"

void parse_unicode_data(const char *filename);
void parse_special_casing(CCInfo *tab, const char *filename);
void parse_case_folding(CCInfo *tab, const char *filename);
void parse_composition_exclusions(const char *filename);
void parse_derived_core_properties(const char *filename);
void parse_derived_norm_properties(const char *filename);
void parse_prop_list(const char *filename);
void parse_sequence_prop_list(const char *filename);
void parse_scripts(const char *filename);
void parse_script_extensions(const char *filename);

#endif /* UNICODE_GEN_PARSER_H */

/* ECMAScript property-escape grammar metadata. This is a strict allow-list,
   not Unicode character data. ICU's loose property-name matching is never
   exposed to JavaScript. See ECMA-262 Unicode property escapes tables. */
#ifndef QUICKJS_UNICODE_ES_ALIASES_H
#define QUICKJS_UNICODE_ES_ALIASES_H
#include "libunicode-es-binary-properties.h"

typedef struct {
    const char *names; /* exact aliases, separated by commas */
    int32_t value;
} UnicodeESAlias;

static const UnicodeESAlias unicode_es_binary_aliases[] = {
#define UNICODE_ES_ALIAS_ENTRY(names, value) { names, value },
    UNICODE_ES_BINARY_PROPERTIES(UNICODE_ES_ALIAS_ENTRY)
#undef UNICODE_ES_ALIAS_ENTRY
};

static const UnicodeESAlias unicode_es_gc_aliases[] = {
    { "Unassigned,Cn", U_GC_CN_MASK },
    { "Uppercase_Letter,Lu", U_GC_LU_MASK },
    { "Lowercase_Letter,Ll", U_GC_LL_MASK },
    { "Titlecase_Letter,Lt", U_GC_LT_MASK },
    { "Modifier_Letter,Lm", U_GC_LM_MASK },
    { "Other_Letter,Lo", U_GC_LO_MASK },
    { "Nonspacing_Mark,Mn", U_GC_MN_MASK },
    { "Spacing_Mark,Mc", U_GC_MC_MASK },
    { "Enclosing_Mark,Me", U_GC_ME_MASK },
    { "Decimal_Number,Nd,digit", U_GC_ND_MASK },
    { "Letter_Number,Nl", U_GC_NL_MASK },
    { "Other_Number,No", U_GC_NO_MASK },
    { "Math_Symbol,Sm", U_GC_SM_MASK },
    { "Currency_Symbol,Sc", U_GC_SC_MASK },
    { "Modifier_Symbol,Sk", U_GC_SK_MASK },
    { "Other_Symbol,So", U_GC_SO_MASK },
    { "Connector_Punctuation,Pc", U_GC_PC_MASK },
    { "Dash_Punctuation,Pd", U_GC_PD_MASK },
    { "Open_Punctuation,Ps", U_GC_PS_MASK },
    { "Close_Punctuation,Pe", U_GC_PE_MASK },
    { "Initial_Punctuation,Pi", U_GC_PI_MASK },
    { "Final_Punctuation,Pf", U_GC_PF_MASK },
    { "Other_Punctuation,Po", U_GC_PO_MASK },
    { "Space_Separator,Zs", U_GC_ZS_MASK },
    { "Line_Separator,Zl", U_GC_ZL_MASK },
    { "Paragraph_Separator,Zp", U_GC_ZP_MASK },
    { "Control,Cc,cntrl", U_GC_CC_MASK },
    { "Format,Cf", U_GC_CF_MASK },
    { "Surrogate,Cs", U_GC_CS_MASK },
    { "Private_Use,Co", U_GC_CO_MASK },
    { "Cased_Letter,LC", U_GC_LC_MASK },
    { "Letter,L", U_GC_L_MASK },
    { "Mark,M,Combining_Mark", U_GC_M_MASK },
    { "Number,N", U_GC_N_MASK },
    { "Symbol,S", U_GC_S_MASK },
    { "Punctuation,P,punct", U_GC_P_MASK },
    { "Separator,Z", U_GC_Z_MASK },
    { "Other,C", U_GC_C_MASK },
};

static const UnicodeESAlias unicode_es_string_aliases[] = {
    { "Basic_Emoji", UCHAR_BASIC_EMOJI },
    { "Emoji_Keycap_Sequence", UCHAR_EMOJI_KEYCAP_SEQUENCE },
    { "RGI_Emoji_Modifier_Sequence", UCHAR_RGI_EMOJI_MODIFIER_SEQUENCE },
    { "RGI_Emoji_Flag_Sequence", UCHAR_RGI_EMOJI_FLAG_SEQUENCE },
    { "RGI_Emoji_Tag_Sequence", UCHAR_RGI_EMOJI_TAG_SEQUENCE },
    { "RGI_Emoji_ZWJ_Sequence", UCHAR_RGI_EMOJI_ZWJ_SEQUENCE },
    { "RGI_Emoji", UCHAR_RGI_EMOJI },
};
#endif

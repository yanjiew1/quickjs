#ifndef QUICKJS_PARSER_H
#define QUICKJS_PARSER_H

#include "quickjs/def.h"
#include "quickjs/string.h"
#include "quickjs/atom.h"
#include "quickjs/shape.h"
#include "quickjs/object.h"
#include "quickjs/function.h"
#include "quickjs/module.h"
#include "quickjs/runtime.h"
#include "quickjs/opcode.h"
#include "cutils.h"

/* JS parser token definitions */
enum {
    TOK_NUMBER = -128,
    TOK_STRING,
    TOK_TEMPLATE,
    TOK_IDENT,
    TOK_REGEXP,
    /* warning: order matters (see js_parse_assign_expr) */
    TOK_MUL_ASSIGN,
    TOK_DIV_ASSIGN,
    TOK_MOD_ASSIGN,
    TOK_PLUS_ASSIGN,
    TOK_MINUS_ASSIGN,
    TOK_SHL_ASSIGN,
    TOK_SAR_ASSIGN,
    TOK_SHR_ASSIGN,
    TOK_AND_ASSIGN,
    TOK_XOR_ASSIGN,
    TOK_OR_ASSIGN,
    TOK_POW_ASSIGN,
    TOK_LAND_ASSIGN,
    TOK_LOR_ASSIGN,
    TOK_DOUBLE_QUESTION_MARK_ASSIGN,
    TOK_DEC,
    TOK_INC,
    TOK_SHL,
    TOK_SAR,
    TOK_SHR,
    TOK_LT,
    TOK_LTE,
    TOK_GT,
    TOK_GTE,
    TOK_EQ,
    TOK_STRICT_EQ,
    TOK_NEQ,
    TOK_STRICT_NEQ,
    TOK_LAND,
    TOK_LOR,
    TOK_POW,
    TOK_ARROW,
    TOK_ELLIPSIS,
    TOK_DOUBLE_QUESTION_MARK,
    TOK_QUESTION_MARK_DOT,
    TOK_ERROR,
    TOK_PRIVATE_NAME,
    TOK_EOF,
    /* keywords: WARNING: same order as atoms */
    TOK_NULL, /* must be first */
    TOK_FALSE,
    TOK_TRUE,
    TOK_IF,
    TOK_ELSE,
    TOK_RETURN,
    TOK_VAR,
    TOK_THIS,
    TOK_DELETE,
    TOK_VOID,
    TOK_TYPEOF,
    TOK_NEW,
    TOK_IN,
    TOK_INSTANCEOF,
    TOK_DO,
    TOK_WHILE,
    TOK_FOR,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_SWITCH,
    TOK_CASE,
    TOK_DEFAULT,
    TOK_THROW,
    TOK_TRY,
    TOK_CATCH,
    TOK_FINALLY,
    TOK_FUNCTION,
    TOK_DEBUGGER,
    TOK_WITH,
    /* FutureReservedWord */
    TOK_CLASS,
    TOK_CONST,
    TOK_ENUM,
    TOK_EXPORT,
    TOK_EXTENDS,
    TOK_IMPORT,
    TOK_SUPER,
    /* FutureReservedWords when parsing strict mode code */
    TOK_IMPLEMENTS,
    TOK_INTERFACE,
    TOK_LET,
    TOK_PACKAGE,
    TOK_PRIVATE,
    TOK_PROTECTED,
    TOK_PUBLIC,
    TOK_STATIC,
    TOK_YIELD,
    TOK_AWAIT, /* must be last */
    TOK_OF,     /* only used for js_parse_skip_parens_token() */
};

#define TOK_FIRST_KEYWORD   TOK_NULL
#define TOK_LAST_KEYWORD    TOK_AWAIT

/* unicode code points */
#define CP_NBSP 0x00a0
#define CP_BOM  0xfeff
#define CP_LS   0x2028
#define CP_PS   0x2029

typedef struct BlockEnv BlockEnv;
typedef struct JSGlobalVar JSGlobalVar;
typedef struct RelocEntry RelocEntry;
typedef struct JumpSlot JumpSlot;
typedef struct LabelSlot LabelSlot;
typedef struct LineNumberSlot LineNumberSlot;
typedef struct GetLineColCache GetLineColCache;
typedef enum JSParseFunctionEnum JSParseFunctionEnum;
typedef enum JSParseExportEnum JSParseExportEnum;
typedef struct JSVarScope JSVarScope;
typedef struct JSVarDef JSVarDef;
typedef struct JSFunctionDef JSFunctionDef;
typedef struct JSToken JSToken;
typedef struct JSParseState JSParseState;

struct BlockEnv {
    struct BlockEnv *prev;
    JSAtom label_name; /* JS_ATOM_NULL if none */
    int label_break; /* -1 if none */
    int label_cont; /* -1 if none */
    int drop_count; /* number of stack elements to drop */
    int label_finally; /* -1 if none */
    int scope_level;
    uint8_t has_iterator : 1;
    uint8_t is_regular_stmt : 1; /* i.e. not a loop statement */
};

struct JSGlobalVar {
    int cpool_idx; /* if >= 0, index in the constant pool for hoisted
                      function defintion*/
    uint8_t force_init : 1; /* force initialization to undefined */
    uint8_t is_lexical : 1; /* global let/const definition */
    uint8_t is_const   : 1; /* const definition */
    int scope_level;    /* scope of definition */
    JSAtom var_name;  /* variable name */
};

struct RelocEntry {
    struct RelocEntry *next;
    uint32_t addr; /* address to patch */
    int size;   /* address size: 1, 2 or 4 bytes */
};

struct JumpSlot {
    int op;
    int size;
    int pos;
    int label;
};

struct LabelSlot {
    int ref_count;
    int pos;    /* phase 1 address, -1 means not resolved yet */
    int pos2;   /* phase 2 address, -1 means not resolved yet */
    int addr;   /* phase 3 address, -1 means not resolved yet */
    RelocEntry *first_reloc;
};

struct LineNumberSlot {
    uint32_t pc;
    uint32_t source_pos;
};

struct GetLineColCache {
    /* last source position */
    const uint8_t *ptr;
    int line_num;
    int col_num;
    const uint8_t *buf_start;
};

enum JSParseFunctionEnum {
    JS_PARSE_FUNC_STATEMENT,
    JS_PARSE_FUNC_VAR,
    JS_PARSE_FUNC_EXPR,
    JS_PARSE_FUNC_ARROW,
    JS_PARSE_FUNC_GETTER,
    JS_PARSE_FUNC_SETTER,
    JS_PARSE_FUNC_METHOD,
    JS_PARSE_FUNC_CLASS_STATIC_INIT,
    JS_PARSE_FUNC_CLASS_CONSTRUCTOR,
    JS_PARSE_FUNC_DERIVED_CLASS_CONSTRUCTOR,
};

enum JSParseExportEnum {
    JS_PARSE_EXPORT_NONE,
    JS_PARSE_EXPORT_NAMED,
    JS_PARSE_EXPORT_DEFAULT,
};

struct JSVarScope {
    int parent;  /* index into fd->scopes of the enclosing scope */
    int first;   /* index into fd->vars of the last variable in this scope */
};

struct JSVarDef {
    JSAtom var_name;
    int scope_level;
    int scope_next;
    uint8_t is_const : 1;
    uint8_t is_lexical : 1;
    uint8_t is_captured : 1;
    uint8_t is_static_private : 1;
    uint8_t var_kind : 4;
    uint16_t var_ref_idx;
    int func_pool_idx;
};

struct JSFunctionDef {
    JSContext *ctx;
    struct JSFunctionDef *parent;
    int parent_cpool_idx;
    int parent_scope_level;
    struct list_head child_list;
    struct list_head link;

    BOOL is_eval;
    int eval_type;
    BOOL is_global_var;
    BOOL is_func_expr;
    BOOL has_home_object;
    BOOL has_prototype;
    BOOL has_simple_parameter_list;
    BOOL has_parameter_expressions;
    BOOL has_use_strict;
    BOOL has_eval_call;
    BOOL has_arguments_binding;
    BOOL has_this_binding;
    BOOL new_target_allowed;
    BOOL super_call_allowed;
    BOOL super_allowed;
    BOOL arguments_allowed;
    BOOL is_derived_class_constructor;
    BOOL in_function_body;
    JSFunctionKindEnum func_kind : 8;
    JSParseFunctionEnum func_type : 8;
    uint8_t js_mode;
    JSAtom func_name;

    JSVarDef *vars;
    int var_size;
    int var_count;
    JSVarDef *args;
    int arg_size;
    int arg_count;
    int defined_arg_count;
    int var_ref_count;
    int var_object_idx;
    int arg_var_object_idx;
    int arguments_var_idx;
    int arguments_arg_idx;
    int func_var_idx;
    int eval_ret_idx;
    int this_var_idx;
    int new_target_var_idx;
    int this_active_func_var_idx;
    int home_object_var_idx;
    BOOL need_home_object;

    int scope_level;
    int scope_first;
    int scope_size;
    int scope_count;
    JSVarScope *scopes;
    JSVarScope def_scope_array[4];
    int body_scope;

    int global_var_count;
    int global_var_size;
    JSGlobalVar *global_vars;

    DynBuf byte_code;
    int last_opcode_pos;
    const uint8_t *last_opcode_source_ptr;
    BOOL use_short_opcodes;

    LabelSlot *label_slots;
    int label_size;
    int label_count;
    BlockEnv *top_break;

    JSValue *cpool;
    int cpool_count;
    int cpool_size;

    int closure_var_count;
    int closure_var_size;
    JSClosureVar *closure_var;

    JumpSlot *jump_slots;
    int jump_size;
    int jump_count;

    LineNumberSlot *line_number_slots;
    int line_number_size;
    int line_number_count;
    int line_number_last;
    int line_number_last_pc;

    BOOL strip_debug : 1;
    BOOL strip_source : 1;
    JSAtom filename;
    uint32_t source_pos;
    GetLineColCache *get_line_col_cache;
    DynBuf pc2line;

    char *source;
    int source_len;

    JSModuleDef *module;
    BOOL has_await;
};

struct JSToken {
    int val;
    const uint8_t *ptr;
    union {
        struct {
            JSValue str;
            int sep;
        } str;
        struct {
            JSValue val;
        } num;
        struct {
            JSAtom atom;
            BOOL has_escape;
            BOOL is_reserved;
        } ident;
        struct {
            JSValue body;
            JSValue flags;
        } regexp;
    } u;
};

struct JSParseState {
    JSContext *ctx;
    const char *filename;
    JSToken token;
    BOOL got_lf; /* true if got line feed before the current token */
    const uint8_t *last_ptr;
    const uint8_t *buf_start;
    const uint8_t *buf_ptr;
    const uint8_t *buf_end;

    /* current function code */
    JSFunctionDef *cur_func;
    BOOL is_module; /* parsing a module */
    BOOL allow_html_comments;
    BOOL ext_json; /* JSON parsing: true if accepting JSON superset */
    GetLineColCache get_line_col_cache;
};

/* Token and Lexer functions */
__exception int next_token(JSParseState *s);
void free_token(JSParseState *s, JSToken *token);
int peek_token(JSParseState *s, BOOL no_line_terminator);
static inline BOOL token_is_pseudo_keyword(JSParseState *s, JSAtom atom) {
    return s->token.val == TOK_IDENT && s->token.u.ident.atom == atom && !s->token.u.ident.has_escape;
}
static inline BOOL token_is_ident(int tok) {
    return tok == TOK_IDENT || (tok >= TOK_FIRST_KEYWORD && tok <= TOK_LAST_KEYWORD);
}
__exception int js_parse_string(JSParseState *s, int sep, BOOL do_throw, const uint8_t *p, JSToken *token, const uint8_t **pp);
__exception int js_parse_template_part(JSParseState *s, const uint8_t *p);
__exception int js_parse_regexp(JSParseState *s);
void reparse_ident_token(JSParseState *s);
int js_parse_expect(JSParseState *s, int tok);
int js_parse_expect_semi(JSParseState *s);
int js_parse_error_reserved_identifier(JSParseState *s);
__attribute__((format(printf, 2, 3))) int js_parse_error(JSParseState *s, const char *fmt, ...);
__attribute__((format(printf, 3, 4))) int js_parse_error_pos(JSParseState *s, const uint8_t *ptr, const char *fmt, ...);
int js_parse_error_v(JSParseState *s, const uint8_t *ptr, const char *fmt, va_list ap);
static inline int get_prev_opcode(JSFunctionDef *fd) {
    if (fd->last_opcode_pos < 0 || dbuf_error(&fd->byte_code))
        return OP_invalid;
    else
        return fd->byte_code.buf[fd->last_opcode_pos];
}
BOOL js_is_live_code(JSParseState *s);
int get_line_col(int *pcol_num, const uint8_t *buf, size_t len);
int get_line_col_cached(GetLineColCache *s, int *pcol_num, const uint8_t *ptr);
void skip_shebang(const uint8_t **pp, const uint8_t *buf_end);
__exception int json_next_token(JSParseState *s);

#endif /* QUICKJS_PARSER_H */

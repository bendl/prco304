//
// Created by BDL on 19/02/2018.
//

#include "parser.h"
#include "dbug.h"
#include "types.h"
#include "adt/ast.h"
#include "adt/list.h"

// Parser shouldnt need to know about target architecture
#include "arch/target.h"
#include "module.h"

// http://wiki.tcl.tk/36951
#include "tok_prec.inc.c"

#ifdef _WIN32
        #include <Shlwapi.h>
        #define PLAT_MAX_PATH MAX_PATH
#else
        #define PLAT_MAX_PATH 256
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static int g_uid = 0;
#define NEW_GUID() \
        (++g_uid)

static char line_buf[80];
static int line_buf_pos = 0;

// Parser stack
#define PARSER_MAX_STACK 5
struct text_parser *g_parser_stack[PARSER_MAX_STACK];
int g_cur_parser_index = -1;
#define g_cur_parser() g_parser_stack[g_cur_parser_index]

#define LEXER_GET_STR() \
        g_cur_parser()->lexer_str

#define LEXER_GET_CHAR() \
        g_cur_parser()->lexer_char

#define LEXER_GET_TOK() \
        g_cur_parser()->lexer_current_token

#define LEXER_GET_NUM() \
        g_cur_parser()->lexer_current_number


struct list_item *g_locals;     //< Linked list of current local decls
struct list_item *g_params;     //< Linked list of current prototype params
struct list_item *g_resv_words; //< Linked list of reserved words

// TODO: Use or remove
#define eat() \
        g_cur_parser = lexer_next();

// TODO: Use or remove
#define lexer_tok_to_string(t) TOK_STR[t]

static void
parser_free(_in_ struct text_parser *parser)
{
        assert(parser);
        assert(parser->file_input);
        assert(parser->lpstr_input_dir);
        assert(parser->lpstr_input_fp);

        fclose(parser->file_input);
        free(parser->lpstr_input_dir);

        dbprintf(D_INFO, "fp: %s\r\n", parser->lpstr_input_fp);
        free(parser->lpstr_input_fp);
        free(parser);
}

int
parser_pushp(_inout_ struct text_parser *parser)
{
        g_cur_parser_index++;
        if (g_cur_parser_index > PARSER_MAX_STACK - 1) {
                // Set parser index back to last one, to prevent out of bounds
                // access on function return
                g_cur_parser_index = PARSER_MAX_STACK - 1;
                return PARSER_ERROR_STACK;
        }

        g_parser_stack[g_cur_parser_index] = parser;
        return 0;
}

void
parser_popp()
{
        // Free parser resources
        parser_free(g_cur_parser());
        g_parser_stack[g_cur_parser_index--] = NULL;

        // Trigger lexer on the previous parser
        if (g_cur_parser_index >= 0) {
                LEXER_GET_CHAR() = ' ';
                LEXER_GET_TOK() = ' ';
                lexer_eat();
        }
}

/// Determine relative or full path
static int
file_exists(const char *fname)
{
#ifdef __WIN32
        // TODO: Create cross-platform implementation
        return PathFileExists(fname);
#else
        FILE * file;
        file = fopen(fname, "r");
        if (file) {
                fclose(file);
                return 1;
        } else {
                return 0;
        }
#endif
}

// TODO: Make work for linux
struct text_parser *
parser_fopen(_in_ const char *fname,
             _out_ int *size,
             _in_ struct text_parser *parser)
{
        struct text_parser *new_parser;

        dbprintf(D_INFO, "parser_fopen: %s\r\n", fname);

        new_parser = zalloc(new_parser);
        new_parser->lpstr_input_fp = malloc(PLAT_MAX_PATH);
        new_parser->lpstr_input_dir = malloc(PLAT_MAX_PATH);

        #ifdef _WIN32
        // Hack to replace linux directory separators / with \
        // TODO: Use cross-platform solution for handling directory filepaths
        {
                char *c = fname;
                while (*c != 0) {
                        if (*c == '/')
                                *c = '\\';
                        c++;
                }
        }

        if (PathIsRelative(fname)) {
                dbprintf(D_INFO, "Path is relative\r\n");
                // Get current dir from current parser
                // If current parser is null, use executable pwd
                // I.e. pwd + fname
                if (parser == NULL) {
                        GetCurrentDirectory(PLAT_MAX_PATH, new_parser->lpstr_input_dir);
                        dbprintf(D_INFO, "dir: %s\r\n", new_parser->lpstr_input_dir);
                        dbprintf(D_INFO, "fname: %s\r\n", fname);
                        PathCombine(new_parser->lpstr_input_fp,
                                    new_parser->lpstr_input_dir, fname);
                        strcpy(new_parser->lpstr_input_dir, new_parser->lpstr_input_fp);
                        PathRemoveFileSpec(new_parser->lpstr_input_dir);
                        dbprintf(D_INFO, "fp: %s\r\n", new_parser->lpstr_input_fp);
                        dbprintf(D_INFO, "Parser null\r\n");
                } else {
                        // Copy old parser dir to new parser dir
                        strcpy(new_parser->lpstr_input_dir,
                               parser->lpstr_input_dir);
                        PathCombine(new_parser->lpstr_input_fp,
                                    new_parser->lpstr_input_dir,
                                    fname);
                }

                dbprintf(D_INFO, "Origin path: %s\r\n", fname);
                dbprintf(D_INFO, "New path: %s\r\n", new_parser->lpstr_input_fp);
        } else {
                // Extract dir from fname
                // TODO: Pass size to this function
                int fname_size = strlen(fname) + 1;
                strcpy(new_parser->lpstr_input_fp, fname);
                strcpy(new_parser->lpstr_input_dir, fname);
                PathRemoveFileSpec(new_parser->lpstr_input_dir);
        }
        #else
                strcpy(new_parser->lpstr_input_fp, fname);
        #endif

        if (!file_exists(new_parser->lpstr_input_fp)) {
                // File doesn't exist relative to current file
                // Check to see if id exists in the std include path
                char *std_include = new_parser->lpstr_input_dir;
                int ret;

                // TODO: Add linux impl for getenv
                /*
                dbprintf(D_INFO, "File doesnt exist!\r\n");

                ret = GetEnvironmentVariable(COMPILER_INCLUDE_ENV_VAR,
                                             new_parser->lpstr_input_dir, PLAT_MAX_PATH);
                if (ret == 0) {
                        dbprintf(
                                D_WARN,
                                "WARNING: " COMPILER_INCLUDE_ENV_VAR
                                        " environment variable is not set. Cannot search std include.\r\n");

                        // Else use previous parser's directory
                        if (parser == NULL) {
                                std_include = new_parser->lpstr_input_dir;
                        } else {
                                std_include = parser->lpstr_input_dir;
                        }
                }
                */

                #ifdef __WIN32
                // Create new path from fname and std_include path
                // TODO: Search each directory in PATH for fname?
                new_parser->lpstr_input_dir = std_include;
                PathCombine(
                        new_parser->lpstr_input_fp,
                        new_parser->lpstr_input_dir,
                        fname);
                #endif
        }

        dbprintf(D_INFO, "New path exists: %s\r\n", new_parser->lpstr_input_fp);

l_fname_exists:
        new_parser->file_input = fopen(new_parser->lpstr_input_fp, "r");
        if (!new_parser->file_input) {
                dbprintf(D_ERR,
                        "ERROR: parser_fopen: Cannot find file: %s!\r\n",
                        new_parser->lpstr_input_fp);
                return NULL;
        }

        if (size != NULL) {
                fseek(new_parser->file_input, 0L, SEEK_END);
                *size = ftell(new_parser->file_input);
                fseek(new_parser->file_input, 0L, SEEK_SET);
        }

        return new_parser;
}

static void
parser_add_resv(char *pstr_name, int dt_size, token_type tok)
{
        struct resv_word *w;
        struct list_item *ll;

        ll = zalloc(ll);
        w  = zalloc(w);
        w->lpstr_name = pstr_name;
        w->dt_size = dt_size;
        w->tok = tok;
        ll->value = w;

        g_resv_words = append_ll_item_head(g_resv_words, w);
}

static char
lexer_fgetc()
{
        g_cur_parser()->lexer_char = fgetc(g_cur_parser()->file_input);

        if (g_cur_parser()->lexer_char == '\r' ||
                g_cur_parser()->lexer_char == '\n') {
                dbprintf(D_INFO, "Parsed line: %s\r\n", line_buf);
                line_buf[0] = 0;
                line_buf_pos = 0;
        } else {
                line_buf[line_buf_pos++] = (char)g_cur_parser()->lexer_char;
                line_buf[line_buf_pos + 1] = 0;
        }
        return (char)g_cur_parser()->lexer_char;
}

int
parse_top_level(void)
{
        int parse_result = 0;

        while (1) {
                switch (LEXER_GET_TOK()) {
                case TOK_FNC:
                case TOK_CC_CDECL:
                case TOK_CC_STDCALL:
                case TOK_CC_FASTCALL:
                        parse_result = !parse_fnc();
                        break;

                case TOK_EOF:
                        goto exit_top_level;

                default:
                        dbprintf(D_ERR, "Illegal top level expr!\r\n");
                        parse_result = 1;
                        break;
                } // end switch

                if (parse_result != 0) {
                        goto exit_top_level;
                }
        }

exit_top_level:
        assert(g_cur_parser());

        // Set current parsers result
        g_cur_parser()->parse_result = parse_result;
        return parse_result;
}

void
lexer_init()
{
        LEXER_GET_CHAR() = ' ';
        LEXER_GET_STR()  = NULL;
        g_cur_parser()->lexer_current_token = 0;
        // Eat the space to get next token
        lexer_eat();
}

int
parser_run(_in_ struct text_parser *parser)
{
        // Initialize parser
        int parse_result = 0;

        // Declare reserved words
        // Todo, datasizes should be target independant
        parser_add_resv("int", dtINT, TOK_VARIABLE);
        parser_add_resv("var", dtINT, TOK_VARIABLE);
        parser_add_resv("fnc", 0, TOK_FNC);
        parser_add_resv("ext", 0, TOK_EXT);
        parser_add_resv("if", 0, TOK_IF);
        parser_add_resv("for", 0, TOK_FOR);
        parser_add_resv("while", 0, TOK_WHILE);
        parser_add_resv("else", 0, TOK_ELSE);
        parser_add_resv("ret", 0, TOK_RET);
        parser_add_resv("call", 0, TOK_CALL);

        // Calling convention words
        parser_add_resv("__stdcall", 0, TOK_CC_STDCALL);
        parser_add_resv("__cdecl", 0, TOK_CC_CDECL);
        parser_add_resv("__fastcall", 0, TOK_CC_FASTCALL);

        // PORT words
        parser_add_resv("PORT1", 0, TOK_PORT_PORT1);
        parser_add_resv("UART1", 0, TOK_PORT_UART1);

        parser_add_resv("LE", 0, TOK_BOOL_LE);
        parser_add_resv("GE", 0, TOK_BOOL_GE);


        // Push new parser to stack
        parse_result = parser_pushp(parser);
        if (parse_result != R_OK) {
                dbprintf(D_ERR,
                        "ERROR: Parser stack limit (%d) reached!\r\n",
                        PARSER_MAX_STACK);
                goto parser_run_cleanup;
        }

        // Initialize lexer
        lexer_init();

        // lexer loop
        parse_result = parse_top_level();

parser_run_cleanup:
        parser_popp();
        return parse_result;
}

int
lexer_check_single(int c, token_type* tok)
{
        switch(c) {
        case '+':
                lexer_fgetc();
                *tok =  TOK_PLUS;
                return 1;
        case '-':
                lexer_fgetc();
                *tok = TOK_SUB;
                return 1;
        case '(':
                lexer_fgetc();
                *tok = TOK_LBRACE;
                return 1;
        case ')':
                lexer_fgetc();
                *tok = TOK_RBRACE;
                return 1;
        case ',':
                lexer_fgetc();
                *tok = TOK_COMMA;
                return 1;
        case '{':
                lexer_fgetc();
                *tok = TOK_LCBRACE;
                return 1;
        case '}':
                lexer_fgetc();
                *tok = TOK_RCBRACE;
                return 1;
        case '=':
                lexer_fgetc();
                *tok = TOK_ASSIGNMENT;
                return 1;

        case '<':
                lexer_fgetc();
                *tok = TOK_BOOL_L;
                return 1;
        case '>':
                lexer_fgetc();
                *tok = TOK_BOOL_G;
                return 1;

        case '"':
                lexer_fgetc();
                *tok = TOK_DQUOTE;
                return 1;

        case '@':
                lexer_fgetc();
                *tok = TOK_DEREF;
                return 1;

        case EOF:
                *tok = TOK_EOF;
                return 1;
        }

        return 0;
}

int
lexer_digit(char *buf)
{
        int buf_i = 0;
        do {
                buf[buf_i++] = (char)LEXER_GET_CHAR();
                lexer_fgetc();
        } while (isdigit(LEXER_GET_CHAR()));

        // Null terminate the string number buffer
        buf[buf_i] = 0;

        return atoi(buf);
}

void
lexer_string(char *buf)
{
        int buf_i = 0;
        do {
                buf[buf_i++] = (char)LEXER_GET_CHAR();
                lexer_fgetc();
        } while (isalnum(LEXER_GET_CHAR()) || LEXER_GET_CHAR() == '_');

        // Null terminate the string buffer
        buf[buf_i] = 0;
}

struct resv_word *
string_is_resv(char *buf)
{
        struct list_item *resv_it;
        struct resv_word *w;

        resv_it = g_resv_words;
        list_for_each(resv_it) {
                w = (struct resv_word *) resv_it->value;
                dbprintf(D_PARSE, "Comparing '%s' to resv '%s'\r\n",
                        buf, w->lpstr_name);

                if (strcmp(buf, w->lpstr_name) == 0) {
                        dbprintf(D_PARSE, "Resv word found! %s\r\n",
                                w->lpstr_name);
                        return w;
                }
        }

        return NULL;
}

token_type
lexer_next(void)
{
        char buf[32];
        token_type single_tok;
        struct resv_word *resv;

        // If we don't have a current parser,
        // or file input, theres a bug, so just crash
        assert(g_cur_parser());
        assert(g_cur_parser()->file_input);

        dbprintf(D_INFO, "Lexing: %s\r\n", g_cur_parser()->lpstr_input_fp);

        // Skip whitespace
        while (isspace(LEXER_GET_CHAR())) lexer_fgetc();

        // Switch Single character operators
        if(lexer_check_single(LEXER_GET_CHAR(), &single_tok)) {
                return single_tok;
        }

        // It could be a string, so loop over until non alpha character reach
        if (isalpha(LEXER_GET_CHAR()) || LEXER_GET_CHAR() == '_')
        {
                lexer_string(buf);

                // Compare buf with reserved words
                resv = string_is_resv(buf);
                if(resv) return resv->tok;

                // It's not a resv word so id must be an ident
                LEXER_GET_STR()
                        = calloc(strlen(buf + 1), sizeof(*LEXER_GET_STR()));

                // Copy the ident string to the new ident allocation
                strcpy(LEXER_GET_STR(), buf);

                // Return we've found an ident
                return TOK_ID;
        }
                // Not an string, so try number
        else if (isdigit(LEXER_GET_CHAR()))
        {
                LEXER_GET_NUM() = lexer_digit(buf);
                return TOK_NUM;
        }
                // If its an semi-colon, ignore id
        else if (LEXER_GET_CHAR() == ';')
        {
                lexer_fgetc();
                return lexer_next();
        }
        else
        {
                dbprintf(D_ERR, "Unknown character: %d %c\r\n",
                        LEXER_GET_CHAR(),
                        LEXER_GET_CHAR());
                return TOK_ERROR;
        }
}

void
lexer_eat(void)
{
        LEXER_GET_TOK() = lexer_next();
}

enum token_type
lexer_token(void)
{
        return (enum token_type) LEXER_GET_TOK();
}

enum token_type
lexer_match_next(enum token_type t)
{
        if (lexer_token() != t) {
                dbprintf(D_ERR, "Unexpected token! %d %c\r\n",
                        LEXER_GET_TOK());
                return TOK_ERROR;
        }

        lexer_eat();
        return lexer_token();
}

enum token_type
lexer_match_opt(enum token_type t)
{
        if (lexer_match(t)) {
                lexer_eat();
        }

        return (enum token_type) lexer_token();
}

int
lexer_match(enum token_type t)
{
        return (lexer_token() == t);
}

int
lexer_match_req(enum token_type t)
{
        if (!lexer_match(t)) {
                dbprintf(D_ERR, "Required text not found! %d %c\r\n",
                        lexer_token(), lexer_token());
                return TOK_ERROR;
        }

        return R_OK;
}


struct ast_proto *
proto_already_exists(enum token_type t, char *name)
{
        struct ast_proto *p_it = get_g_module()->prototypes;
        list_for_each(p_it) {
                if (strcmp(p_it->name, name) == 0) {
                        if (t == TOK_EXT) {
                                return p_it;
                        } else {
                                dbprintf(D_ERR, "Duplication of prototype: %s\r\n",
                                         name);
                                return NULL;
                        }
                }
        }

}

struct ast_proto *
parse_proto(enum token_type t)
{
        struct list_item *args;
        struct list_item *arg;
        struct ast_lvar *lvarg;
        struct ast_lvar *parg;
        struct ast_proto *proto;
        struct ast_proto *proto_it;

        char *fn_name;
        int argc = 0;
        int pargc = 0;

        args = zalloc(args);

        // fnc ident (
        lexer_match_opt(TOK_CC_STDCALL);
        lexer_match_opt(TOK_CC_CDECL);
        lexer_match_opt(TOK_CC_FASTCALL);

        lexer_match_opt(TOK_FNC);
        lexer_match_req(TOK_ID);
        fn_name = LEXER_GET_STR();
        lexer_match_next(TOK_ID);
        lexer_match_next(TOK_LBRACE);

        // args, arg1
        lexer_match_opt(TOK_VARIABLE);
        while (lexer_match(TOK_ID) || lexer_match(TOK_VARIABLE)) {
                lexer_match_opt(TOK_VARIABLE);
                lvarg = ast_lvar_create(ast_var_create(LEXER_GET_STR(), dtINT));
                append_ll_item(args, lvarg);
                argc++;

                lexer_eat();
                lexer_match_opt(TOK_COMMA);
        }

        // TODO: Is it really the parsers job to do
        // implementation specific stack management?
        pargc = argc + 1;
        arg = args;
        list_for_each(arg) {
                if (!arg->value) break;
                parg = (struct ast_lvar *) arg->value;
                parg->bp_offset = pargc * 1;
                pargc--;
                dbprintf(D_INFO, "Parsed proto arg: %s offset %x\r\n",
                        parg->var->name, parg->bp_offset);
        }

        // Eat closing )
        lexer_match_next(TOK_RBRACE);

        // Prototype finished.
        // Now check to see if we already have a matching one in the module
        proto_it = proto_already_exists(t, fn_name);
        if(proto_it) return proto_it;

        g_params = args;

        proto = ast_proto_create(fn_name, args, argc);

        // Move new proto to front of the ll
        list_to_front(proto, get_g_module()->prototypes);

        return proto;
}

int
check_is_entry_fnc(struct ast_func *fnc)
{
        return strcmp(fnc->proto->name, "main") == 0;
}

struct ast_func *
parse_fnc(void)
{
        struct ast_func *fnc;
        struct ast_item *body;
        struct ast_proto *proto;

        // def ident ( args, ... )
        proto = parse_proto(TOK_FNC);
        body = parse_block();
        if (!proto || !body) return NULL;

        fnc = ast_func_create(proto, body);
        fnc->locals = g_locals;

        // Move new function to front of function ll list
        list_to_front(fnc, get_g_module()->functions);

        // Finished parsing,
        // clear global lists
        g_locals = g_params = NULL;

        dbprintf(D_INFO, "Parsed def: %s argc: %d\r\n",
                proto->name, proto->argc);

        if (check_is_entry_fnc(fnc)) {
                get_g_module()->entry = fnc;
        }

        return fnc;
}

struct ast_lvar *
check_str_in_scope(struct list_item *scope, char *ident)
{
        struct ast_lvar *vc;
        list_for_each(scope) {
                vc = (struct ast_lvar *) scope->value;
                if (!vc)
                        return NULL;

                if (strcmp(ident, vc->var->name) == 0)
                        return vc;
        }
        return NULL;
}

struct ast_lvar *
get_var(char *ident)
{
        struct ast_lvar *v;

        // Check if v is a parameter to the function
        v = check_str_in_scope(g_params, ident);
        if (v) return v;

        // Must be a local variable
        return check_str_in_scope(g_locals, ident);
}

int
check_call(struct ast_call *call)
{
        // Check call to prototypes
        struct ast_proto *p = get_g_module()->prototypes;

        list_for_each(p) {
                if (strcmp(call->callee, p->name) == 0) {
                        // number of args must match
                        // Ignore data types as we dont really have any
                        if ((call->argc == p->argc)) {
                                call->proto = p;
                                return true;
                        }
                        // bad number of args
                        break;
                }
        }

        return false;
}


struct ast_item *
parse_call(char *ident)
{
        struct list_item *args;
        int              argc;
        struct ast_call *call;
        struct ast_item *arg_expr;

        args = zalloc(args);
        argc = 0;

        lexer_match_next(TOK_LBRACE);
        if (!lexer_match(TOK_RBRACE)) {
                while (1) {
                        arg_expr = parse_expr();
                        if (!arg_expr) {
                                args = NULL;
                                break;
                        }

                        append_ll_item(args, arg_expr);
                        argc++;

                        if (lexer_match(TOK_RBRACE)) break;

                        lexer_match_next(TOK_COMMA);
                }
        }

        lexer_match_next(TOK_RBRACE);

        // Create the call ast item
        call = ast_call_create(ident, args, argc);
        if (check_call(call)) {
                return ast_expr_create(call, AST_CALL);
        } else {
                dbprintf(D_ERR, "ERR: Undefined reference to: %s\r\n", ident);
                return NULL;
        }
}

struct ast_item *
parse_assignment(char *ident)
{
        struct ast_lvar *v;
        struct ast_item *val;
        struct ast_assign *a;

        lexer_match_next(TOK_ASSIGNMENT);
        v = get_var(ident);
        if (!v) {
                dbprintf(D_ERR,
                        "ERR: Var assignment ident '%s' not found in scope\r\n",
                        ident);
                return NULL;
        }

        val = parse_expr();
        if (!val) return NULL;

        a = zalloc(a);
        a->var = v;
        a->val = val;

        return ast_expr_create(a, AST_ASSIGNMENT);
}

struct ast_item *
parse_ident(void)
{
        struct ast_lvar *v;
        char *ident;

        ident = LEXER_GET_STR();
        lexer_match_next(TOK_ID);

        // call expr: <ident>(...)
        if (lexer_match(TOK_LBRACE)) {
                return parse_call(ident);
        }

        if (lexer_match(TOK_ASSIGNMENT)) {
                return parse_assignment(ident);
        }

        // else its a variable reference
        v = get_var(ident);
        if (!v) {
                dbprintf(D_ERR,
                        "Variable '%s' is not in the current scope!\r\n",
                        ident);
                return NULL;
        }

        return ast_expr_create(v, AST_VAR_REF);
}

struct ast_item *
parse_num(void)
{
        struct ast_num *ret;
        ret = ast_num_create(LEXER_GET_NUM());
        lexer_eat();
        return ast_expr_create(ret, AST_NUM);
}

struct ast_item *
parse_paren(void)
{
        struct ast_item *result;

        // TODO: check lexer_token == (
        lexer_eat();

        // Parse inside the (...)
        result = parse_expr();

        if (!lexer_match(TOK_RBRACE)) {
                dbprintf(D_ERR,
                        "ERR: Missing closing ) paren in expr\r\n");
                return NULL;
        }

        lexer_eat();
        return result;

}

struct ast_item *
parse_var(void)
{
        char            *ident  = NULL;
        struct ast_lvar *v      = NULL;
        struct ast_item *nvar   = NULL;

        dbprintf(D_INFO, "Parsing variable\r\n");

        lexer_match_next(TOK_VARIABLE);
        lexer_match(TOK_ID);
        ident = LEXER_GET_STR();
        lexer_match_next(TOK_ID);

        // If variable not found, create new local decl
        if ((v = get_var(ident)) == NULL) {
                v = ast_lvar_create(ast_var_create(ident, dtINT));
                nvar = ast_expr_create(v, AST_LOCAL_VAR);
                g_locals = append_ll_item_head(g_locals, v);
        }

        // If there is an assignment after it,
        // Its not a decl so its an assignment
        if (lexer_match(TOK_ASSIGNMENT)) {
                if (nvar) {
                        nvar->next = parse_assignment(v->var->name);
                        if (nvar->next == NULL)
                                return NULL;
                        return nvar;
                } else {
                        nvar = parse_assignment(v->var->name);
                }
        }

        // If no new_var created, its just a reference
        if (nvar == NULL) {
                nvar = ast_expr_create(v, AST_VAR_REF);
        }

        return nvar;
}

struct ast_item *
parse_cstring(void)
{
        struct ast_cstring *string;
        char *asciz = NULL;

        lexer_match_req(TOK_DQUOTE);

        asciz = parse_ascii();
        if(!asciz) return NULL;

        string = zalloc(string);
        string->string    = asciz;
        string->string_id = NEW_GUID();
        lexer_match_next(TOK_DQUOTE);

        dbprintf(D_INFO, "Found CSTRING: '%s'\r\n", string->string);

        // Add string to global string list
        get_g_module()->strings =
                append_ll_item_head(get_g_module()->strings, string);

        return ast_expr_create(string, AST_CSTRING);
}

struct ast_item *
parse_deref(void)
{
        struct ast_deref *deref;
        struct ast_item  *deref_expr;

        lexer_match_next(TOK_DEREF);
        deref_expr = parse_expr();
        if(deref_expr == NULL) { return NULL; }

        deref = zalloc(deref);
        deref->item = deref_expr;
        return ast_expr_create(deref, AST_DEREF);
}

struct ast_item *
parse_primary(void)
{
        switch (lexer_token()) {
        case TOK_VARIABLE:
                return parse_var();
        case TOK_ID:
                return parse_ident();
        case TOK_NUM:
                return parse_num();
        case TOK_LBRACE:
                return parse_paren();

        case TOK_DQUOTE:
                return parse_cstring();

        case TOK_DEREF:
                return parse_deref();

        // TODO: Decide if this should be an expr
        case ';':
                // For now, eat it
                lexer_eat();
                // and return the next primary
                return parse_primary();
        default:
                dbprintf(D_ERR, "Unknown primary token: %d %c\r\n",
                        lexer_token(), lexer_token());
                return NULL;
        }
}

struct ast_item *
parse_bin_rhs(int min_prec, struct ast_item *lhs)
{
        struct ast_item *rhs;
        int cur_tok_prec;
        int next_tok_prec;
        int bin_op;

        while (1) {
                cur_tok_prec = get_tok_prec();
                if (cur_tok_prec < min_prec) return lhs;

                bin_op = lexer_token();
                
                // eat bin op
                lexer_eat();
                
                rhs = parse_primary();
                if (!rhs) return NULL;

                next_tok_prec = get_tok_prec();
                if (cur_tok_prec < next_tok_prec) {
                        rhs = parse_bin_rhs(cur_tok_prec + 1, rhs);
                        if (!rhs) return NULL;
                }

                lhs = ast_expr_create(ast_bin_create((char) bin_op, lhs, rhs), AST_BIN);
        }
}

struct ast_item *
parse_if_expr(void)
{
        // if '(' <expr> ')' '{' <expr> '}'
        struct ast_item *cond = NULL;
        struct ast_item *then = NULL;
        struct ast_item *els  = NULL;

        // if '('
        lexer_match_next(TOK_IF);
        lexer_match_next(TOK_LBRACE);

        // <cond> ')'
        cond = parse_expr();
        lexer_match_next(TOK_RBRACE);

        // '{' <body> '}'
        then = parse_block();

        // Else is optional, so check for it
        // else '{' <body> '}'
        if (lexer_match(TOK_ELSE)) {
                lexer_match_next(TOK_ELSE);
                lexer_match_next(TOK_LCBRACE);
                els = parse_block();
                lexer_match_next(TOK_RCBRACE);
        }

        return ast_expr_create(ast_if_create(cond, then, els), AST_IF);
}

struct ast_item *
parse_for_expr(void)
{
        // for '(' <expr> <expr> <expr> ) { <expr> }
        struct ast_item *start,
                        *cond,
                        *step,
                        *body;

        lexer_match_next(TOK_FOR);
        lexer_match_next(TOK_LBRACE);
        start = parse_expr();
        cond = parse_expr();
        step = parse_expr();
        lexer_match_next(TOK_RBRACE);

        lexer_match_next(TOK_LCBRACE);
        body = parse_block();
        lexer_match_next(TOK_RCBRACE);

        return ast_expr_create(ast_for_create(start, cond, step, body), AST_FOR);
}

struct ast_item *
parse_port_uart1(void)
{
        // UART1 '(' <expr> ')'
        struct ast_item *val;
        struct ast_expr *uart_ast;

        // Parse it
        lexer_match_next(TOK_PORT_UART1);
        lexer_match_next(TOK_LBRACE);
        val = parse_expr();
        lexer_match_next(TOK_RBRACE);

        // AST it
        uart_ast = zalloc(uart_ast);
        uart_ast->val = val;
        return ast_expr_create(uart_ast, AST_UART);
}

struct ast_item *
parse_while_expr(void)
{
        // while ( <expr> ) { <expr> }
        struct ast_item  *cond;
        struct ast_item  *body;
        struct ast_while *w;

        // Parse it
        lexer_match_next(TOK_WHILE);
        lexer_match_next(TOK_LBRACE);
        cond = parse_expr();
        if(!cond) { return NULL; }
        lexer_match_next(TOK_RBRACE);

        body = parse_block();
        if(!cond) { return NULL; }

        // AST it
        w = zalloc(w);
        w->cond = cond;
        w->body = body;
        return ast_expr_create(w, AST_WHILE);
}

struct ast_item *
parse_expr(void)
{
        struct ast_item *lhs;

        if (lexer_match(TOK_IF))
                return parse_if_expr();

        if (lexer_match(TOK_FOR))
                return parse_for_expr();

        if(lexer_match(TOK_WHILE))
                return parse_while_expr();

        if(lexer_match(TOK_PORT_UART1))
                return parse_port_uart1();


        lhs = parse_primary();
        if (!lhs) return NULL;

        return parse_bin_rhs(0, lhs);
}

struct ast_item *
parse_block(void)
{
        // A block is a list of multiple <expr> with
        // in chronological order
        struct ast_item *start  = NULL;
        struct ast_item *last   = NULL;
        int has_braces          = 0;

        has_braces = lexer_match(TOK_LCBRACE);
        if(has_braces) lexer_match_next(TOK_LCBRACE);

        while (!lexer_match(TOK_RCBRACE)) {
                struct ast_item *e = parse_expr();
                if (!e) return NULL;

                // Add to back of list
                if (start == NULL) {
                        start = e;
                        while (e->next) e = e->next;
                        last = e;
                } else {
                        last->next = e;
                        while (e->next) e = e->next;
                        last = e;
                }
        }

        if(has_braces) lexer_match_next(TOK_RCBRACE);

        // Return the start of the list
        return start;
}

char *
parse_ascii(void)
{
        char *buf;
        int buf_i = 0;
        const int ASCIZ_MAX_LENGTH = 256;

        lexer_match_req(TOK_DQUOTE);
        buf = malloc(ASCIZ_MAX_LENGTH);

        while(LEXER_GET_CHAR() != '"') {
                if(buf_i < ASCIZ_MAX_LENGTH-1) {
                        buf[buf_i++] = (char)LEXER_GET_CHAR();
                        lexer_fgetc();
                }
        }
        // Null terminate the asciz string
        buf[buf_i] = 0;

        // eat the TOK_DQUOTE
        lexer_eat();

        return buf;
}

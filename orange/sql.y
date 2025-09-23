
%{
#include <stdio.h>
#include <stdlib.h>
#include "vdbe.h"

int yylex(void);
extern int yylineno;
extern char* yytext;    // 当前token文本
int yyerror(const char *s) {
    fprintf(stderr, "Syntax error at line %d: %s near '%s'\n", 
            yylineno, s, yytext);
    return 0;
}

%}

%union {
    int ival;
    char *sval;
}
%token T_DELETE T_FROM T_WHERE T_SEMI  T_LT 
%token <ival> T_INT
%token <sval> T_IDENT   // 表名、列名等
%type <sval> stmt table condition where_clause column
%type <ival> value
%%
stmt    :   T_DELETE T_FROM table where_clause T_SEMI {
                emit(OP_OPEN, $3, 0);
                emit(OP_DELETE, NULL, 0);
                emit(OP_HALT, NULL, 0);
            }
        ;
table   :   T_IDENT { $$ = $1; }
        ;
where_clause : T_WHERE condition { $$ = $2; }
        ;
condition   : column T_LT value {
                emit(OP_WHERE, $1, $3);
            }
        ;
column  : T_IDENT { $$ = $1; }
        ;
value   :   T_INT { $$ = $1; }
        ;
%%

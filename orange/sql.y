
/*====声明===*/
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
void yyerror(char* s, ...);
void emit(char* s, ...); // 产生逆波兰表达式, printf打印
%}

%union {
    int intval;
    char *strval;
    double floatval;
    int subtok;
}
/*名字和字面值*/
%token <strval> NAME
%token <strval> STRING
%token <intval> INTNUM

/*====规则===*/
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

/*===c代码===*/

void yyerror(char* s, ...)
{
        fprintf(stderr, "error:%s\n", );
}

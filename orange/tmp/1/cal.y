%{
#include <stdio.h>
#include "cal.h"
#include <stdlib.h>

int yylex(void);  // flex

%}

/* 声明类型*/
%union {
    struct AST* a;
    double d;
}

%token <d> NUMBER
%token EOL

%type <a> factor expr term

%%
calclist: { printf("calclist matched space.\n"); } /* 可以空*/ 
    | calclist expr EOL { 
        printf("= %.4g\n", eval($2)); /**/
        treefree($2);
        printf("> ");
         } 
    | calclist EOL {
        printf("> "); /* 空行或者注释*/
    }
    ;
expr: factor
    | expr '+' factor {  printf("expr matched\n"); $$ = newast('+', $1, $3);}
    | expr '-' factor { $$ = newast('-', $1, $3); }
    ;
factor: term {printf("factor matched\n");}
    | factor '*' term { $$ = newast('*', $1, $3); }
    | factor '/' term { $$ = newast('/', $1, $3); }
    ;
term: NUMBER {  printf("term matched: %f\n", $1); $$ = newnum($1); }
    | '|' term { $$= newast('|', $2, NULL); }
    | '(' expr ')' { $$ = $2; }
    | '-' term { $$ = newast('M', $2, NULL); }
    ;
%%

%{
#include <stdio.h>
#include <stdlib.h>
#include "cal.h"

int yylex(void);  // flex

%}

/* 声明类型*/
%union {
    struct AST* a;
    double d;
    struct symbol* s;
    struct symlist* sl;
    int fn; 
}

%token <d> NUMBER
%token <s> NAME
%token <fn> FUNC
%token EOL
%token IF THEN ELSE DO WHILE LET

%nonassoc <fn> CMP /* cmp 是一个函数，没有结核性*/
%right '='
%left '+' '-'
%left '*' '/'
%nonassoc '|' UMINUS /*优先级最高，没有结合性*/


%type <a> exp stmt list explist
%type <sl> symlist

%start calclist /* 定义了顶层规则， 所以无需放在最上面*/
%%

/*stmt语句： 一段控制流 或者 一句表达式。*/
stmt: IF exp THEN list { $$ = newflow('I', $2, $4, NULL); }
    | IF exp THEN list ELSE list { $$ = newflow('I', $2, $4, $6); }
    | WHILE exp DO list { $$ = newflow('W', $2, $4, NULL); }
    | exp
    ;
/*list语句列表：*/
list: { $$ = NULL; }
    | stmt ';' list { if ($3 == NULL) 
                        $$ = $1; 
                    else 
                        $$ = newast('L', $1, $3);
                    }
    ;
exp: exp CMP exp { $$ = newcmp($2, $1, $3); }
    | exp '+' exp { $$ = newast('+', $1, $3); }
    | exp '-' exp { $$ = newast('-', $1, $3); }
    | exp '*' exp { $$ = newast('*', $1, $3); }
    | exp '/' exp { $$ = newast('/', $1, $3); }
    | '|' exp { $$ = newast('|', $2, NULL); }
    | '(' exp ')' { $$ = $2; }
    | NAME { $$ = newref($1); }
    | NAME '=' exp  { $$ = newasgn($1, $3); }
    | NUMBER { $$ = newnum($1); }
    | FUNC '(' explist ')' { $$ = newfunc($1, $3); }
    | NAME '(' explist ')' { $$ = newcall($1, $3); }
    ;
explist: exp
    | exp ',' explist { $$ = newast('L', $1, $3);} 
    ;
symlist: NAME { $$ = newsymlist($1, NULL); }
    | NAME ',' symlist { $$ = newsymlist($1, $3); }
    ;



calclist: { printf("calclist matched space.\n"); } /* 可以空*/ 
    | calclist stmt EOL { 
        printf("= %.4g\n", eval($2)); /**/
        treefree($2);
        printf("> ");
         } 
    | calclist LET NAME '(' symlist ')' '=' list EOL {
        dodef($3, $5, $8);
        printf("Defined %s.\n", $3->name);
    }
    | calclist  EOL {
        printf("> "); /* 空行或者注释*/
    }
    ;

%%

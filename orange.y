%debug

/*====声明===*/
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "orange.h"
int yylex(void);  // flex
void yyerror(char* s, ...);
extern struct StmtList* root;
%}

%union {
    char *strval;
                int intval;
        struct SelectStmt* selectStmtVal;
        struct Expr* exprVal;
        struct TableRef* tabRefVal;
        struct ExprList* exprListVal;
        struct Stmt* stmtVal;
        struct StmtList* stmtListVal;    
}
%token EOL
%token <strval> NAME
%token <intval> INTNUM 
%token SELECT FROM
%type <exprVal> expr
%type <exprListVal> expr_list
%type <stmtVal> stmt
%type <selectStmtVal> select_stmt
%type <stmtListVal> stmt_list
%type <tabRefVal> table_ref

%start input
/*====规则===*/
%%
input: stmt_list  EOL {  }
        | EOL {}
        ;
stmt_list: stmt   { stmtListAdd(root, $1);  $$ = root;}
        | stmt_list stmt  { stmtListAdd($1, $2); $$ = $1; }
        ;

stmt: select_stmt { $$ = newStmt(STMT_SELECT, $1);}
        ;
select_stmt: SELECT expr_list FROM table_ref ';' {
                $$ = newSelectStmt($2, $4);
        }
        ;
table_ref: NAME { $$ = newTableRef($1); }
        ;
expr: NAME { $$ = newExpr($1); }
        ;

/*变长表达式列表*/
expr_list: expr { $$ = newExprList($1); }
        | expr_list ',' expr   { exprListAdd($1, $3); $$ = $1; }
        ;

%%



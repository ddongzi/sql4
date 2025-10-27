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
        struct CreateStmt* createStmtVal;
        struct Expr* exprVal;
        struct TableRef* tabRefVal;
        struct ExprList* exprListVal;
        struct Stmt* stmtVal;
        struct StmtList* stmtListVal;    
}
%token EOL
%token <strval> NAME
%token <intval> INTNUM 
%token SELECT FROM CREATE TABLE
%type <exprVal> expr
%type <exprListVal> expr_list
%type <stmtVal> stmt
%type <selectStmtVal> select_stmt
%type <createStmtVal> create_table_stmt
%type <stmtListVal> stmt_list
%type <tabRefVal> table_ref

%start input
/*====规则===*/
%%
input: stmt_list  {  }
        |  {}
        ;
stmt_list: stmt   { stmtListAdd($1);  $$ = root;}
        | stmt_list stmt  { stmtListAdd($2); $$ = $1; }
        ;

stmt: select_stmt { $$ = newStmt(STMT_SELECT, $1);}
        | create_table_stmt { $$ = newStmt(STMT_CREATE, $1);}
        ;
select_stmt: SELECT expr_list FROM table_ref ';' {
                $$ = newSelectStmt($2, $4);
        }
        ;
table_ref: NAME { $$ = newTableRef($1); }
        ;
expr: NAME { $$ = newExpr($1); }
        ;
/*表达式列表*/
expr_list: expr { $$ = newExprList($1); }
        | expr_list ',' expr   { exprListAdd($1, $3); $$ = $1; }
        ;

create_table_stmt: CREATE TABLE table_ref '(' expr_list ')' ';' {
                $$ = newCreateStmt($5, $3);
        }
        ;

%%



#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sql.h"
#include "sql.tab.h"

struct StmtList* root;

struct Stmt* newStmt(enum StmtType type, void* st)
{
    struct Stmt* s = malloc(sizeof(struct Stmt));
    s->type = type;
    s->st = st;
    return s;
}

void stmtListAdd(struct StmtList* stmts, struct Stmt* item)
{
    stmts->items = realloc(stmts->items, sizeof(struct Stmt*) * (stmts->nstmt + 1));
    stmts->items[stmts->nstmt] = item;
    stmts->nstmt += 1;
}
struct SelectStmt* newSelectStmt(struct ExprList* exprls, struct TableRef* tabref)
{
    struct SelectStmt* selectstmt = malloc(sizeof(struct SelectStmt));
    selectstmt->col_list = exprls;
    selectstmt->table_ref = tabref;
    return selectstmt;
}
struct Expr* newExpr(char* name)
{
    struct Expr* expr = malloc(sizeof(struct Expr));
    expr->name = strdup(name);
    return expr;
}
struct TableRef* newTableRef(char* name)
{
    struct TableRef* tabref = malloc(sizeof(struct TableRef));
    tabref->name = strdup(name);
    return tabref;
}
struct ExprList* newExprList(struct Expr* item)
{
    struct ExprList* exprs = malloc(sizeof(struct ExprList));
    exprs->items = malloc(sizeof(struct Expr*));
    exprs->items[0] = item;
    exprs->nexpr = 1;
    return exprs;
}
void exprListAdd(struct ExprList* exprs, struct Expr* item)
{
    exprs->items = realloc(exprs->items, sizeof(struct Expr*) * (exprs->nexpr + 1));
    exprs->items[exprs->nexpr] = item;
    exprs->nexpr += 1;
}
static void printIdent(int level)
{
    for (int i = 0; i < level; i++)
    {
        printf("  ");
    }
    
}
static void printExpr(struct Expr* expr, int level)
{
    printIdent(level);
    printf("└─ Expr: [%s]\n", expr->name);  
}
static void printExprList(struct ExprList* exprs, int level)
{
    printIdent(level);
    printf("└─ ExprList: \n");
    for (int i = 0; i < exprs->nexpr; i++){
        printExpr(exprs->items[i], level + 1);
    }
}
static void printTableRef(struct TableRef* tabref, int level)
{
    printIdent(level);
    printf("└─ TableRef: [%s]\n", tabref->name);
}
static void printSelectStmt(struct SelectStmt* selectst, int level)
{
    printIdent(level);
    printf("└─ SelectStmt: \n");
    printExprList(selectst->col_list, level+1);
    printTableRef(selectst->table_ref, level + 1);
}
static void printStmt(struct Stmt * st, int level)
{
    switch (st->type)
    {
    case STMT_SELECT:
        printSelectStmt((struct SelectStmt*)(st->st), level);
        break;
    
    default:
        break;
    }
}
static void printStmtList(struct StmtList* stlist)
{
    int level = 0;
    printIdent(level);
    printf("StmtList:\n");
    for (int i = 0; i < stlist->nstmt; i++)
    {   
        printStmt(stlist->items[i], 1);
    }
    
}
void yyerror(char* s, ...)
{
        fprintf(stderr, "error:%s\n", s);
}
int main(int argc, char** argv)
{
    root = malloc(sizeof(struct StmtList));
    root->nstmt = 0;
    yydebug = 1;
    yyparse();
    printStmtList(root);
    return 0;
}

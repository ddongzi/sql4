#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vdbe.h"
#include "orange.h"
#include "build/orange.tab.h"
#include "build/orange_lex.yy.h"

struct StmtList* root;

struct Stmt* newStmt(enum StmtType type, void* st)
{
    struct Stmt* s = malloc(sizeof(struct Stmt));
    s->type = type;
    s->st = st;
    return s;
}

void stmtListAdd(struct Stmt* item)
{
    printf("(debug) stmtlistadd\n");
    root->items = realloc(root->items, sizeof(struct Stmt*) * (root->nstmt + 1));
    root->items[root->nstmt] = item;
    root->nstmt += 1;
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
struct CreateStmt* newCreateStmt(struct ExprList* col_list,struct TableRef* tabref)
{
        struct CreateStmt* createst = malloc(sizeof(struct CreateStmt));
    createst->col_list = col_list;
    createst->table_ref = tabref;
    return createst;
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
static void printCreateStmt(struct CreateStmt* createst, int level)
{
    printIdent(level);
    printf("└─ CreateStmt: \n");
    printExprList(createst->col_list, level+1);
    printTableRef(createst->table_ref, level + 1);
}
static void printStmt(struct Stmt * st, int level)
{
    switch (st->type)
    {
    case STMT_SELECT:
        printSelectStmt((struct SelectStmt*)(st->st), level);
        break;
    case STMT_CREATE:
        printCreateStmt((struct CreateStmt*)(st->st), level);
        break;
    default:
        break;
    }
}
static void printStmtList()
{
    int level = 0;
    printIdent(level);
    printf("StmtList:\n");
    for (int i = 0; i < root->nstmt; i++)
    {   
        printStmt(root->items[i], 1);
    }
    
}
void orange_free()
{

}
void yyerror(char* s, ...)
{
    fprintf(stderr, "error:%s\n", s);
}
void orange_parse(SqlPrepareContext* sqlctx)
{
    // flex没有生成头文件，所以这里报提示
    printf("Orange parse sql: %s\n", sqlctx->sql);
    YY_BUFFER_STATE buffer = yy_scan_string(sqlctx->sql);
    root = malloc(sizeof(struct StmtList));
    root->nstmt = 0;
    yydebug = 1;
    yyparse();
    printf("(debug) root nstmt [%d]", root->nstmt);
    sqlctx->ast = root;
    printStmtList();
    yy_delete_buffer(buffer);
}
// // for test!
// void yyerror(char* s, ...)
// {
//         fprintf(stderr, "error:%s\n", s);
// }
// int main(int argc, char** argv)
// {
//     root = malloc(sizeof(struct StmtList));
//     root->nstmt = 0;
//     yydebug = 1;
//     yyparse();
//     printStmtList(root);
//     return 0;
// }

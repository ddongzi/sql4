#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vdbe.h"
#include "oranger.h"
#include "build/orange.tab.h"
#include "build/orange_lex.yy.h"
extern int yylex_destroy(void);

// declare
void freeExprList(struct ExprList* exprs);
void freeTableRef(struct TableRef* tabref);

struct StmtList* root;

struct Stmt* newStmt(enum StmtType type, void* st)
{
    struct Stmt* s = malloc(sizeof(struct Stmt));
    if (s == NULL) {
        printf("new stmt malloc err!!\n");
    }
    s->type = type;
    s->st = st;
    return s;
}

void stmtListAdd(struct Stmt* item)
{
    assert(root);
    printf("(debug) here5, %d nstmt\n", root->nstmt);
    printf("(debug) here5, %p pointer\n", root->items);
    root->items = realloc(root->items, sizeof(struct Stmt*) * (root->nstmt + 1));
    printf("(debug) here6\n");
    
    assert(root->items);
    root->items[root->nstmt] = item;
    root->nstmt += 1;
    printf("(debug) here3\n");
}
struct SelectStmt* newSelectStmt(struct ExprList* exprls, struct TableRef* tabref)
{
    struct SelectStmt* selectstmt = malloc(sizeof(struct SelectStmt));
    selectstmt->col_list = exprls;
    selectstmt->table_ref = tabref;
    return selectstmt;
}
void freeSelectStmt(struct SelectStmt* selectstmt)
{
    freeExprList(selectstmt->col_list);
    freeTableRef(selectstmt->table_ref);
    free(selectstmt);
}
struct Expr* newExpr(char* name)
{
    struct Expr* expr = malloc(sizeof(struct Expr));
    expr->name = strdup(name);
    return expr;
}
void freeExpr(struct Expr* expr)
{
    free(expr->name);
    free(expr);
}
struct TableRef* newTableRef(char* name)
{
    struct TableRef* tabref = malloc(sizeof(struct TableRef));
    tabref->name = strdup(name);
    return tabref;
}
void freeTableRef(struct TableRef* tabref)
{
    free(tabref->name);
    free(tabref);
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
void freeCreateStmt(struct CreateStmt* createst)
{
    freeExprList(createst->col_list);
    freeTableRef(createst->table_ref);
    free(createst);
}

void exprListAdd(struct ExprList* exprs, struct Expr* item)
{
    exprs->items = realloc(exprs->items, sizeof(struct Expr*) * (exprs->nexpr + 1));
    exprs->items[exprs->nexpr] = item;
    exprs->nexpr += 1;
}
void freeExprList(struct ExprList* exprs)
{
    for (size_t i = 0; i < exprs->nexpr; i++)
    {
        freeExpr(exprs->items[i]);
    }
    free(exprs);
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
// 销毁ast
void orange_destroy(SqlPrepareContext* sqlctx)
{
    AST* ast = sqlctx->ast;
    for (size_t i = 0; i < ast->nstmt; i++)
    {
        struct Stmt* st = ast->items[i];
        switch (st->type)
        {
        case STMT_CREATE:
            freeCreateStmt((struct CreateStmt*)st->st);
            break;
        case STMT_SELECT:
            freeSelectStmt((struct SelectStmt*)st->st);
            break;
        default:
            break;
        }
    }
    free(ast);    
    printf("Orange has destroyed.\n");
}
void yyerror(char* s, ...)
{
    fprintf(stderr, "error:%s\n", s);
}
void orange_parse(SqlPrepareContext* sqlctx)
{
    printf("Orange parse sql: %s\n", sqlctx->sql);
    YY_BUFFER_STATE buffer = yy_scan_string(sqlctx->sql);
    root = malloc(sizeof(struct StmtList));
    root->nstmt = 0;
    root->items = NULL;
    yydebug = 1;
    yyparse();
    printf("(debug) root nstmt [%d]", root->nstmt);
    sqlctx->ast = root;
    printStmtList();
    yy_delete_buffer(buffer);
    yylex_destroy();
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

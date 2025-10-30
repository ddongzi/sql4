#ifndef ORANGE_H
#define ORANGE_H
#include <stdarg.h>
#include "vdbe.h"

// 嵌套结构体本身就是树
struct Expr {
    char* name;
};
struct TableRef {
    char* name;
};
struct ExprList {
    struct Expr* *items;
    int nexpr;
};
struct SelectStmt {
    struct ExprList* col_list;     // Expr columns
    struct TableRef* table_ref;    // tbref
};
struct CreateStmt {
    struct ExprList* col_list;
    struct TableRef* table_ref;
};
enum StmtType {
    STMT_SELECT,
    STMT_CREATE,
};
struct Stmt {
    enum StmtType type;
    void* st;
};
struct StmtList {
    struct Stmt** items;
    int nstmt;
};
extern struct StmtList* root;
typedef struct StmtList AST; // 对外类型

// 目前 SELECT a,b from tb1;
struct Stmt* newStmt(enum StmtType type, void* st);
void stmtListAdd(struct Stmt* item); 

struct SelectStmt* newSelectStmt(struct ExprList*, struct TableRef*);
struct Expr* newExpr(char* name);
struct TableRef* newTableRef(char* name);
struct ExprList* newExprList(struct Expr* item); // 一个个添加，刚开始是一个
void exprListAdd(struct ExprList* exprs, struct Expr* item); // 一个个添加，刚开始是一个

struct CreateStmt* newCreateStmt(struct ExprList* col_list,struct TableRef* );

void orange_parse(SqlPrepareContext* sqlctx);
void yyerror(char* s, ...);
void orange_destroy(SqlPrepareContext* sqlctx);


#endif
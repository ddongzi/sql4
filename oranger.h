#ifndef ORANGE_H
#define ORANGE_H
#include <stdarg.h>
#include "vdbe.h"

// 嵌套结构体本身就是树
// abc,123
enum ExprType {
    EXPR_INT,
    EXPR_STRING,
};
struct Expr {
    enum ExprType type;
    union 
    {
        int ival;
        char* sval;
    };
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
struct InsertStmt {
    struct ExprList* col_list;
    struct TableRef* table_ref;
    struct ExprList* val_list;
};
enum StmtType {
    STMT_SELECT,
    STMT_CREATE,
    STMT_INSERT,
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
struct Expr* newStringExpr(char* s);
struct Expr* newIntExpr(int i);
struct TableRef* newTableRef(char* name);
struct ExprList* newExprList(struct Expr* item); // 一个个添加，刚开始是一个
void exprListAdd(struct ExprList* exprs, struct Expr* item); // 一个个添加，刚开始是一个

struct CreateStmt* newCreateStmt(struct ExprList* col_list,struct TableRef* );

struct InsertStmt* newInsertStmt(struct TableRef* , struct ExprList* col_list, struct ExprList* val_list);

void orange_parse(SqlPrepareContext* sqlctx);
void yyerror(char* s, ...);
void orange_destroy(SqlPrepareContext* sqlctx);


#endif
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
enum StmtType {
    STMT_SELECT,
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

// 目前 SELECT a,b from tb1;
struct Stmt* newStmt(enum StmtType type, void* st);
void stmtListAdd(struct StmtList* stmts, struct Stmt* item); 

struct SelectStmt* newSelectStmt(struct ExprList*, struct TableRef*);
struct Expr* newExpr(char* name);
struct TableRef* newTableRef(char* name);
struct ExprList* newExprList(struct Expr* item); // 一个个添加，刚开始是一个
void exprListAdd(struct ExprList* exprs, struct Expr* item); // 一个个添加，刚开始是一个


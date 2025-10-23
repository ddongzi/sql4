#include "cal.tab.h"
/* */
extern int yylineno;
void yyerror(char* s, ...);
int yyparse(void); // bison

/**
 * 节点类型：
 * + - * / |
 * 比较操作符, 01, 02 ..
 * M
 * L 表达式或者语句列表？
 * I if
 * W while
 * N 符号引用
 * = 赋值
 * F 内置函数调用
 * C 用户函数调用
 */
struct AST {
    int nodetype;
    struct AST* left;
    struct AST* right;
};

// 内置函数
enum bifs { 
    B_SQRT = 1,
    B_EXP ,
    B_LOG,
    B_PRINT,
};

// 参数链表
struct symlist {
    struct symbol* sym;
    struct symlist* next;
};
// 每个符号都可以有一个变量（名字/值），还可以有一个函数
struct symbol{
    char* name;
    double value;
    struct AST* func;
    struct symlist* syms;
};
#define NHASH 123
extern struct symbol symtable[NHASH]; // 符号表
struct symbol* lookup(char*);

struct symlist* newsymlist(struct symbol* sym, struct symlist* next);
void symlistfree(struct symlist* sl);



/*各类节点 */
struct fncall {
    int nodetype;
    struct AST* l; // 函数
    enum bifs functype;
};
struct ufncall {
    int nodetype;
    struct AST* l; // 参数列表
    struct symbol* s;
};
struct flow { // if-then-else, do-while
    int nodetype;
    struct AST* cond; // if/while cond
    struct AST* tl; // do
    struct AST* el; // else
};
struct numval { // 
    int nodetype; 
    double number;
};
struct symref { // 符号引用
    int nodetype;
    struct symbol* s;
};
struct symasgn { // 赋值
    int nodetype;
    struct symbol* s;
    struct AST* v;
};

struct AST* newast(int nodetype, struct AST* l, struct AST *r);
struct AST* newcmp(int cmptype, struct AST* l, struct AST *r);
struct AST* newfunc(int functype, struct AST* l);
struct AST* newcall(struct symbol*s, struct AST* l);
struct AST* newref(struct symbol*s);
struct AST* newasgn(struct symbol*s, struct AST* v);
struct AST* newflow(int nodetype, struct AST* cond, struct AST* tl, struct AST* el); // if list then list else list 
struct AST* newnum(double d);

// 定义函数
void dodef(struct symbol* name, struct symlist* syms, struct AST* func);
double eval(struct AST * ); // 计算ast
void treefree(struct AST*);
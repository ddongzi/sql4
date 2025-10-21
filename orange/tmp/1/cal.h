/* */
extern int yylineno;
void yyerror(char* s, ...);
int yyparse(void); // bison



struct AST {
    int nodetype;
    struct AST* left;
    struct AST* right;
};

struct numval { // 叶子节点, nodetype=k, 墙砖到AST即为叶子节点
    int nodetype; 
    double number;
};
struct AST* newast(int nodetype, struct AST* l, struct AST *r);
struct AST* newnum(double d); // 叶子节点

double eval(struct AST * ); // 计算ast
void treefree(struct AST*);
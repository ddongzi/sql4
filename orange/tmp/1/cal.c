#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cal.h"
struct AST* newast(int nodetype, struct AST* l, struct AST *r)
{
    struct AST* a = malloc(sizeof(struct AST));
    a->nodetype = nodetype;
    a->left = l;
    a->right = r;
    return a;
}
struct AST* newnum(double d)
{
    struct numval* a = malloc(sizeof(struct numval));
    a->nodetype = 'K';
    a->number = d;
    return (struct AST*) a;
}
double eval(struct AST * a)
{
    double v;
    switch (a->nodetype)
    {
    case 'K':
        v = ((struct numval*)a)->number;
        break;
    case '+':
        v = eval(a->left) + eval(a->right);
        break;
    case '-':
        v = eval(a->left) - eval(a->right);
        break;
    case '*':
        v = eval(a->left) * eval(a->right);
        break;
    case '/':
        v = eval(a->left) / eval(a->right);
        break;
    case '|':
        v = eval(a->left); 
        if (v < 0) v = -v;
        break;
    case 'M':
        v = - eval(a->left);
        break;
    default:
        printf("internal error: bad node %c\n", a->nodetype);
        break;
    }
    return v;
}
void treefree(struct AST* a)
{
    switch (a->nodetype)
    {
    case 'K':
        free(a);
        break;
    case '|':
    case 'M':
        treefree(a->left);
        break;
    case '+':
    case '-':
    case '*':
    case '/':
        treefree(a->left);
        treefree(a->right);
        break;
    default:
        printf("internal error: free bad node %c\n", a->nodetype);
        break;
    }
}
void yyerror(char* s, ...)
{
    va_list ap;
    va_start(ap, s);
    fprintf(stderr, "%d: error", yylineno);
    fprintf(stderr, s, ap);
    fprintf(stderr, "\n");
}
int main(int argc, char const *argv[])
{
    printf("> ");
    yyparse();
    return 0;
}

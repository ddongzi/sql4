#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "cal.h"
#include <string.h>
#include <math.h>
struct symbol symtable[NHASH];
static double callbuiltin(struct fncall* f)
{
    enum bifs functype = f->functype;
    double v = eval(f->l);
    switch (functype)
    {
    case B_SQRT:
        return sqrt(v);
    case B_EXP:
        return exp(v);
    case B_LOG:
        return log(v);
    case B_PRINT:
        printf("=%4.4g\n", v);
        return v;
    default:
        yyerror("Unkonwn built-in function %d", functype);
        return 0.0;
    }
}
static double calluser(struct ufncall* uf)
{
    struct symbol* fsym = uf->s;
    struct symlist* sl; // 形参
    struct AST* args = uf->l; // 实参
    double *oldval, *newval; // 参数值：
    double v;
    int nargs;

    if (!fsym->func) {
        yyerror("call to undefined function. %s", fsym->name);
        return 0;
    }

    sl = fsym->syms;
    for ( nargs = 0; sl ; sl = sl->next)
    {
        nargs++;
    }
    oldval = (double* )malloc(nargs * sizeof(double));
    newval = (double* )malloc(nargs * sizeof(double));
    for (int i = 0; i < nargs; i++)
    {
        if (args->nodetype == 'L') {    
            newval[i] = eval(args->left);
            args = args->right;
        } else {
            newval[i] = eval(args);
            args = NULL;
        }
    }

    sl = fsym->syms;
    for (int i = 0; i < nargs; i++)
    {
        struct symbol* s = sl->sym;
        oldval[i] = s->value;
        s->value = newval[i];
        sl = sl->next;
    }
    free(newval);

    v = eval(fsym->func);
    sl = fsym->syms;
    for (int i = 0; i < nargs; i++)
    {
        struct symbol* s = sl->sym;
        s->value = oldval[i];
        sl = sl->next;  
    }
    free(oldval);
    return v;    
}

// symbol的 哈希值
static unsigned int symhash(char *sym)
{
    unsigned int hash = 0;
    unsigned char c;
    while (c = *sym++)
    {
        hash = hash * 9 ^ c;
    }
    return hash;
}

struct symbol* lookup(char* sym)
{
    struct symbol* sp = &symtable[ symhash(sym) % NHASH];
    int scount = NHASH;
    while (--scount >= 0)
    {
        if (sp->name && !strcmp(sp->name, sym)) return sp;
        if (!sp->name) {
            // 表里没有。添加
            sp->name = strdup(sym);
            sp->value = 0;
            sp->syms = NULL;
            sp->func = NULL;
            return sp;
        }
        if (++sp >= symtable + NHASH) sp = symtable;    // 循环尝试
    }
    yyerror("symbol table overflow.\n");
    abort();
}

void dodef(struct symbol* name, struct symlist* syms, struct AST* func)
{
    if (name->syms) symlistfree(name->syms);
    if (name->func) treefree(name->func);
    name->syms = syms;
    name->func = func;
}

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
struct AST* newcmp(int cmptype, struct AST* l, struct AST *r)
{
    struct AST* a = malloc(sizeof(struct AST));
    a->nodetype = '0' + cmptype;
    a->left = l;
    a->right = r;
    return a;
}
struct AST* newfunc(int functype, struct AST* l)
{
    struct fncall* a = malloc(sizeof(struct fncall));
    a->nodetype = 'F';
    a->functype = functype;
    a->l = l;
    return (struct AST*) a;
}
struct AST* newcall(struct symbol*s, struct AST* l)
{
    struct ufncall* a = malloc(sizeof(struct ufncall));
    a->nodetype = 'C';
    a->l = l;
    a->s = s;
    return (struct AST*) a;
}
struct AST* newref(struct symbol*s)
{
    struct symref* a = malloc(sizeof(struct symref));
    a->nodetype = 'N';
    a->s = s;
    return (struct AST*) a;
}
// a = a+21* 
struct AST* newasgn(struct symbol*s, struct AST* v)
{
    struct symasgn* a = malloc(sizeof(struct symasgn));
    a->nodetype = '=';
    a->s = s;
    a->v = v;
    return (struct AST*) a;
}
struct AST* newflow(int nodetype, struct AST* cond, struct AST* tl, struct AST* el) // if list then list else list
{
    struct flow* a = malloc(sizeof(struct flow));
    a->nodetype = nodetype;
    a->cond = cond;
    a->tl = tl;
    a->el = el;
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
    case 'N':
        v = ((struct symref* )a)->s->value;
        break;
    case '=': // TODO
        v = ((struct symasgn* )a)->s->value;
        eval(((struct symasgn* )a)->v);
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
    
    case '1': v = (eval(a->left) > eval(a->right)) ? 1: 0; break;
    case '2': v = (eval(a->left) < eval(a->right)) ? 1: 0; break;
    case '3': v = (eval(a->left) != eval(a->right)) ? 1: 0; break;
    case '4': v = (eval(a->left) == eval(a->right)) ? 1: 0; break;
    case '5': v = (eval(a->left) >= eval(a->right)) ? 1: 0; break;
    case '6': v = (eval(a->left) <= eval(a->right)) ? 1: 0; break;

    case 'I':
        if (eval(((struct flow*)a)->cond) != 0) {
            if (((struct flow*)a)->tl) {
                v = eval(((struct flow*)a)->tl);
            } else {
                v = 0.0;
            }
        } else {
            if (((struct flow*)a)->el) {
                v = eval(((struct flow*)a)->el);
            } else {
                v = 0.0;
            } 
        }
        break;
    case 'W':
        v = 0.0;
        if (((struct flow*)a)->tl) {
            while (eval(((struct flow*)a)->cond) != 0) {
                v = eval(((struct flow*)a)->tl);
            }
        }
        break;

    case 'L':
        eval(a->left);
        v = eval(a->right);
        break;
    case 'F':
        v = callbuiltin((struct fncall*)a);
        break;
    case 'C':
        v = calluser((struct ufncall*)a);
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
    /** 没有子树 */
    case 'K': 
    case 'N':
        free(a);
        break;
    
    case '|':
    case 'M':
    case 'C':
    case 'F':
        treefree(a->left);
        break;
    // 两颗子数 
    case '+':
    case '-':
    case '*':
    case '/':
    case '1': // cmp
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
        treefree(a->left);
        treefree(a->right);
        break;
    
    case '=':
        free(((struct symasgn*)a)->v);
        break;

    case 'I':
    case 'W':
        treefree(((struct flow*)a)->cond);
        if (((struct flow*)a)->tl) 
            treefree(((struct flow*)a)->tl);
        if (((struct flow*)a)->el) 
            treefree(((struct flow*)a)->el);
        break;

    default:
        printf("internal error: free bad node %c\n", a->nodetype);
        break;
    }
}
struct symlist* newsymlist(struct symbol* sym, struct symlist* next)
{
    struct symlist* l = malloc(sizeof(struct symlist));
    l->sym = sym;
    l->next = next;
    return l;
}
void symlistfree(struct symlist* sl)
{
    struct symlist* nl;
    while (sl)
    {
        nl = sl->next;
        free(sl);
        sl = nl;
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

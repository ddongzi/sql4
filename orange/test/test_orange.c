#include "orange.h"
#include "orange.tab.h"

// for test!
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

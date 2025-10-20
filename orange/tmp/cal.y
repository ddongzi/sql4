%{
#include <stdio.h>
%}

%token NUMBER
%token ADD SUB MUL DIV ABS
%token EOL

%%
calclist: /* 空规则: 从输入开头匹配*/
    | calclist expr EOL { printf("=%d\n", $2); } 
    ;
expr: factor
    | expr ADD factor { $$ = $1 + $3; }
    | expr SUB factor { $$ = $1 - $3; }
    ;
factor: term
    | factor MUL term { $$ = $1 * $3; }
    | factor DIV term { $$ = $1 / $3; }
    ;
term: NUMBER
    | ABS term { $$=$2 > 0? $2:-$2;}
    ;
%%
void yyerror(char* s)
{
    fprintf(stderr, "error: %s\n", s);
}
int main(int argc, char** argv)
{
    yyparse();
}

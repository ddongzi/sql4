%option noyywrap nodefault yylineno case-insensitive

%{
#include "y.tab.h" /* 包含语法分析中 记号, yylval*/
#include <string.h>
#include <stdarg.h>
void yyerror(char* s, ...);
%}

%x COMMENT /*起始状态：处理c注释*/
%s BTWMODE /*起始状态：共享BTWMODE状态，后续and关键字使用*/

%%
SELECT { return T_SELECT }
"DELETE" { return T_DELETE; }
"FROM" { return T_FROM; }
"WHERE" { return T_WHERE; }
/*数字*/
[a-zA-Z_][a-zA-Z_0-9]*  { yylval.sval = strdup(yytext); return T_IDENT; }
[0-9]+  { yylval.ival = atoi(yytext); return T_INT; }
"<" { return T_LT; }
";" { return T_SEMI; }
[ \t\n]+    {;}   //忽略
.   { return yytext[0]; }
%%

int main(int argc, char **argv)
{
    yylex();
}
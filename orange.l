%option noyywrap nodefault yylineno case-insensitive

%{
#include <string.h>
#include <stdarg.h>
#include "build/orange.tab.h"
void yyerror(char* s, ...);
%}

%%

SELECT {  return SELECT;}
"*" |
"," |
";"    {  return yytext[0];}
FROM {  return FROM; }

[a-zA-Z_][a-zA-Z_0-9]*  { printf("flex: %s\n", yytext); yylval.strval = yytext; return NAME;}

[0-9]+  {   yylval.intval = atoi(yytext); return INTNUM; }

[ \t]+    {  }   //忽略


\n {  return EOL;} 

.    { printf("mstery ignore %c\n", yytext[0]); }

%%



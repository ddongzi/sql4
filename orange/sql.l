%{
#include "y.tab.h"
#include <string.h>
%}

%%
"DELETE" { return T_DELETE; }
"FROM" { return T_FROM; }
"WHERE" { return T_WHERE; }
[a-zA-Z_][a-zA-Z_0-9]*  { yylval.sval = strdup(yytext); return T_IDENT; }
[0-9]+  { yylval.ival = atoi(yytext); return T_INT; }
"<" { return T_LT; }
";" { return T_SEMI; }
[ \t\n]+    {;}   //忽略
.   { return yytext[0]; }
%%
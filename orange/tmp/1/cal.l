%{
#include "cal.tab.h"    
#include "cal.h"
%}
EXP ([Ee][-+]?[0-9]+)
%%
"+" | 
"-" |  
"*" | 
"/" | 
")" | 
"(" | 
"|"  {printf("flex: opcode %c\n", yytext[0]); return yytext[0]; }

[0-9]+"."[0-9]*{EXP}? | 
"."?[0-9]+{EXP}?  { printf("flex: number.\n"); yylval.d = atof(yytext); return NUMBER; }
\n { printf("flex: \\n \n"); return EOL; }
[ \t] { }
"//".*
. { yyerror("Mystery character %c\n", *yytext); }
%%

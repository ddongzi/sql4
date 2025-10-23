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
"=" | 
"," | 
";" | 
"|"  {printf("flex: opcode %c\n", yytext[0]); return yytext[0]; }

">" { yylval.fn = 1; return CMP; }
"<" { yylval.fn = 2; return CMP; }
"<>" { yylval.fn = 3; return CMP; }
"==" { yylval.fn = 4; return CMP; }
">=" { yylval.fn = 5; return CMP; }
"<=" { yylval.fn = 6; return CMP; }

"if" { return IF; }
"then" { return THEN; }
"else" { return ELSE; }
"while" {return WHILE; }
"do" {return DO;}
"let" {return LET;}

"sqrt" { yylval.fn = B_SQRT ;return FUNC; }
"exp" { yylval.fn = B_EXP ;return FUNC; }
"log" { yylval.fn = B_LOG ;return FUNC; }
"print" { yylval.fn = B_PRINT;return FUNC; }

[a-zA-Z][a-zA-Z0-9]* { yylval.s = lookup(yytext); return NAME; } 

[0-9]+"."[0-9]*{EXP}? | 
"."?[0-9]+{EXP}?  { printf("flex: number.\n"); yylval.d = atof(yytext); return NUMBER; }

\n { printf("flex: \\n \n"); return EOL; }

[ \t] 

"//".*

. { yyerror("Mystery character %c\n", *yytext); }

%%

orange: orange.l orange.y oranger.h
	flex orange.l
	bison -d -Wcounterexamples  orange.y 
	gcc -o $@ orange.tab.c lex.yy.c orange.c -lfl
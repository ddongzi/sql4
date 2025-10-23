sql: sql.l sql.y sql.h
	flex sql.l
	bison -d -Wcounterexamples  sql.y 
	gcc -o $@ sql.tab.c lex.yy.c sql.c -lfl
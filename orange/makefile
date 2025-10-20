sql: sql.l sql.y
	bison -d sql.y
	flex sql.l
	gcc -o $@ sql.tab.c lex.yy.c -lfl
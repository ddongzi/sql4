vdbe: lex.yy.o y.tab.o vdbe.c
		gcc -o sql lex.yy.o y.tab.o vdbe.c -ll
lex.yy.c: sql.l y.tab.c
	flex sql.l
y.tab.c: sql.y
	bison -vdty sql.y

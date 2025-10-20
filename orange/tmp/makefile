cal: cal.y cal.l
	bison -d cal.y
	flex cal.l
	gcc -o $@ cal.tab.c lex.yy.c -lfl
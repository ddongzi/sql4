cal: cal.y cal.l cal.h
	flex cal.l 
	bison -d cal.y
	gcc -o $@ cal.tab.c lex.yy.c cal.c -lfl
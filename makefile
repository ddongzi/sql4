sql4: btree.c bytecode.c db.c main.c orange.c pager.c vdbe.c\
	sql4code.h sql4limit.h table.h\
	build/orange.tab.c build/orange_lex.yy.c
	gcc -o sql4 btree.c bytecode.c db.c main.c orange.c pager.c vdbe.c\
		sql4code.h sql4limit.h table.h\
		build/orange.tab.c build/orange_lex.yy.c\
		-Ibuild -I. -lfl 
build/orange.tab.c: orange.y
	mkdir -p build
	bison -d -Wcounterexamples -o build/orange.tab.c orange.y
build/orange_lex.yy.c: orange.l build/orange.tab.c
	mkdir -p build
	flex --header-file=build/orange_lex.yy.h -o build/orange_lex.yy.c orange.l
clean:
	rm -rf build sql4

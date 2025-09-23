https://www.sqlite.org/arch.html
learn sqlite, build your own

# Modules
## pager_t:
1. file r/w
- locate page
- dirty page
2. cache manage
- LRU
3. transcation
- BEGIN, COMMIT, ROLLBACK
4. journal mode
- rollback journal (tradtion)
- WAL
5. consistency
- crash-safe
- ato



An "Abstract Syntax Tree" or AST is a data structure that describes a program or statement in some kind of formal language. In our context, the formal language is SQL. 


The SQL language parser for SQLite is generated using a code-generator program called "Lemon". The Lemon program reads a grammar of the input language and emits C-code to implement a parser for that language. 


LALR(1)：
- LR: left-to-right scan.
- LR(1): 向前看一个token就能决定解析动作，
- LALR : look ahead LR. LR(1)的优化

lemon: .y -> c-code

SQLite works by translating each SQL statement into bytecode and then running that bytecode. A prepared statement in SQLite is mostly just the bytecode needed to implement the corresponding SQL. The sqlite3_prepare_v2() interface is a compiler that translates SQL into bytecode. The sqlite3_step() interface is the virtual machine that runs the bytecode contained within the prepared statement.


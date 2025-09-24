

typedef struct Intstruction Intstruction;
enum { OP_DELETE, OP_OPEN, OP_WHERE, OP_HALT };
void emit(int opcode, char* arg1, int arg2);
typedef struct sql_statement sql_statement;


/**
 * Virtual DataBase Engine
 */
#include "vdbe.h"
#include <stdio.h>
#include "y.tab.h"
#include <stdint.h>
union P4_t
{   
    int32_t i32;
    int64_t il64;
    double f64; // 64-bit float value
    const char* s;  // string
    const char* blob;   // Binary large object 二进制大对象。“\x2b11..”
    // ....
};

struct Intstruction {
    int opcode;
    int32_t p1;
    int32_t p2;
    int32_t p3;
    union P4_t p4;

};
struct Intstruction iss[100];
int size = 0;
const char* opcode_str[] = {
    "OP_DELETE", "OP_OPEN", "OP_WHERE", "OP_HALT"
};
void emit(int opcode, int32_t p1, int32_t p2, int32_t p3, union P4_t p4)
{
    iss[size].opcode = opcode;
    iss[size].p1 = p1;
    iss[size].p2 = p2;
    iss[size].p3 = p3;
    iss[size].p4 = p4;
    size++;
}

void format_printf(int addr, const char* opcode, 
    char* p1, int p2, const char* comment
    )
{
    printf("%6d %12s %4s %4d %20s\n", 
        addr, opcode, p1, p2, comment
    );  
}
int main()
{
    int retcode = yyparse();
    if (retcode == 0) {
        printf("%6s %12s %4s %4s %20s\n", 
            "addr", "opcode", "p1", "p2", "comment"
        );
        printf("%6s %12s %4s %4s %20s\n",
            "------", "------------", "----",  "----", 
            "------------------------"
        );
        for(int i = 0; i < size; i++) {
            format_printf(i, opcode_str[iss[i].opcode],
                iss[i].arg1 ? iss[i].arg1 : "NULL",
                iss[i].arg2, "comment_"
            );
        }
    } else {
        printf("err: yyparse retcode %d", retcode);
    }

    return retcode;
}
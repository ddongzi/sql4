
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

typedef struct  {
    int addr;
    int opcode;
    int32_t p1;
    int32_t p2;
    int32_t p3;
    union P4_t p4;
} Intstruction;
enum OPCode{ 
    Init,
    OpenRead,
    Rewind,
    Column,
    ResultRow,
    Next,
    Halt,
    Transaction,
    Goto,
};

typedef struct{
    // 指令数组
    size_t ints_size;
    Intstruction* ints;
} IntstructionList;

// 寄存器，存储临时数据
typedef struct {
    uint8_t flags; // 标志位， TODO 
    union {
        int32_t i32;
        int64_t il64;
        double f64;
        char* s;
    } value;
} Register;
extern Register g_registers[16]; // 声明vdb全局寄存器

Intstruction* vdbe_new_ins(int opcode, int32_t p1, int32_t p2, int32_t p3, union P4_t p4);

IntstructionList* vdbe_new_inslist();
void vdbe_inslist_add(IntstructionList*, Intstruction*);
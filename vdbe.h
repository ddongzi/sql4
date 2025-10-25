#ifndef VDBE_H
#define VDBE_H

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

typedef struct StmtList AST; // 向前声明

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
    size_t nints;
    Intstruction* *ints;
} InstructionList;

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

// 全局处理的上下文，从input输入，到编译 ast， 到字节码列表
typedef struct {
    char* sql;  // sql语句
    AST* ast;   // 对应语法树
    InstructionList* inslist; // 对应字节码列表
} SqlPrepareContext;

void vdbe_run(SqlPrepareContext* );

Intstruction* vdbe_new_ins(int opcode, int32_t p1, int32_t p2, int32_t p3, union P4_t p4);

InstructionList* vdbe_new_inslist();
void vdbe_inslist_add(InstructionList*, Intstruction*);

#endif
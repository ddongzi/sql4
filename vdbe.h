#ifndef VDBE_H
#define VDBE_H

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "pager.h"
#include "db.h"
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
} Instruction;
enum OPCode{ 
    OP_Init,
    OP_CreateBtree,
    OP_OpenRead,
    OP_Rewind,
    OP_Column,
    OP_ResultRow,
    OP_Next,
    OP_Halt,
    OP_String,
    OP_Integer,
    OP_Copy,
    OP_MakeRecord,
    OP_Transaction,
    OP_Goto,
    OP_OpenWrite,
    OP_Insert,
    OP_NewRowid,
    OP_SeekRowid,
    OP_Rowid,
};

typedef struct{
    // 指令数组
    size_t nints;
    Instruction* *ints;
} InstructionList;

typedef struct {
    uint8_t* data; // <type><len><data>...
    int n;
} Row;

typedef struct {
    Row** data; 
    int nrow;
} ResultBuffer;

// 全局处理的上下文，从input输入，到编译 ast， 到字节码列表
typedef struct {
    char* sql;  // sql语句
    AST* ast;   // 对应语法树
    InstructionList* inslist; // 对应字节码列表
    DB* db; // master和pager 常用
    
    ResultBuffer* buffer; // resultrow返回存放
} SqlPrepareContext;

void vdbe_run(SqlPrepareContext* );
void vdbe_destroy(SqlPrepareContext* sqlctx);

Instruction* vdbe_new_ins(int opcode, int32_t p1, int32_t p2, int32_t p3, union P4_t p4);

InstructionList* vdbe_new_inslist();
void vdbe_inslist_add(InstructionList*, Instruction*);

#endif
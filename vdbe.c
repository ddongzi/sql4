/**
 * Virtual DataBase Engine
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "btree.h"
#include "sql4code.h"
#include "vdbe.h"
#include "bytecode.h"

#define MAX_OPEN_CURSOR 128 // vdbe运行时候最多维持这么多个cursor打开
static Cursor* g_vdb_cursors[MAX_OPEN_CURSOR];

enum RegisterFlag {
    REG_I32,
    REG_IL64,
    REG_F64,
    REG_STR,
    REG_BYTES
};
// 寄存器，存储临时数据
typedef struct {
    uint8_t flags; // 标志位， 后续才能正确读写/
    union {
        int32_t i32; // 
        int64_t il64; // 
        double f64; // 
        char* s; // string
        uint8_t* bytes; // bytes
    } value;
    int n; // 为了bytes，需要存储长度. str也是为了方便
} Register;
#define REGISTER_SIZE 64 // 最多维持16个寄存器
Register g_registers[REGISTER_SIZE]; // 声明vdb全局寄存器

Intstruction* vdbe_new_ins(int opcode, int32_t p1, int32_t p2, int32_t p3, union P4_t p4)
{
    Intstruction* ins = malloc(sizeof(Intstruction));
    ins->opcode = opcode;
    ins->p1 = p1;
    ins->p2 = p2;
    ins->p3 = p3;
    ins->p4 = p4;
    return ins;
}
InstructionList* vdbe_new_inslist()
{
    InstructionList* inslist = malloc(sizeof(InstructionList));
    inslist->nints = 0;
    inslist->ints = NULL;
    return inslist;
}
void vdbe_inslist_add(InstructionList* inslist, Intstruction* ins)
{
    inslist->ints = realloc(inslist->ints, sizeof(Intstruction*) * (inslist->nints + 1));
    inslist->ints[inslist->nints] = ins;
    inslist->nints += 1;
}


void print_inslist(InstructionList* inslist)
{

    printf("Instruction List:\n");
    printf("%4s %10s %6s %6s %6s %6s\n", "Addr", "Opcode", "P1", "P2", "P3", "P4");
    Intstruction *ins;
    for (int i = 0; i < inslist->nints; i++)
    {
        ins = inslist->ints[i];
        printf("%4d ", i);
        switch (ins->opcode)
            {
            case Init:
                printf("%10s %6d %6d %6d %d\n", "Init", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case OpenRead:
                printf("%10s %6d %6d %6d %d\n", "OpenRead", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Rewind:
                printf("%10s %6d %6d %6d %d\n", "Rewind", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Column:
                printf("%10s %6d %6d %6d %d\n", "Column", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case ResultRow:
                printf("%10s %6d %6d %6d %d\n", "ResultRow", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Next:
                printf("%10s %6d %6d %6d %d\n", "Next", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Halt:
                printf("%10s %6d %6d %6d %d\n", "Halt", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Transaction:
                printf("%10s %6d %6d %6d %d\n", "Transaction", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Goto:
                printf("%10s %6d %6d %6d %d\n", "Goto", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case OpenWrite:
                printf("%10s %6d %6d %6d %d\n", "OpenWrite", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case MakeRecord:
                printf("%10s %6d %6d %6d %d\n", "MakeRecord", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Insert:
                printf("%10s %6d %6d %6d %d\n", "Insert", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case String:
                printf("%10s %6d %6d %6d %s\n", "String", ins->p1, ins->p2, ins->p3, ins->p4.s);
                break;
            case NewRowid:
                printf("%10s %6d %6d %6d %d\n", "NewRowid", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            default:
                printf("%10s ", "unknown");
                sql4_errno = VDBE_UNKOWN_OPCODE_ERR;
                break;
            }
        
    }

}
static void execute_init(SqlPrepareContext* sqlctx, Intstruction* ins)
{
    printf("Execute init ins.\n");
}
static void execute_openread(SqlPrepareContext* sqlctx, Intstruction* ins)
{
    printf("Execute openread ins.\n");
}

// 为表访问创建cursor
static void execute_openwrite(SqlPrepareContext* sqlctx, Intstruction* ins)
{
    printf("Execute openwrite ins.\n");
    uint32_t root_pagenum = ins->p2;
    Cursor* cursor = btree_cursor_start(btree_get(root_pagenum, sqlctx->pager));
    // 需要与cursor编号绑定。
    g_vdb_cursors[ins->p1] = cursor;
}
// 字符串设置寄存器
static void execute_string(SqlPrepareContext* sqlctx, Intstruction* ins)
{
    printf("Execute string ins.\n");
    g_registers[ins->p2].n = strlen(ins->p4.s);
    g_registers[ins->p2].flags = REG_STR;
    g_registers[ins->p2].value.s = strdup(ins->p4.s);
}
// 组装序列化bytes，
static void execute_makerecord(SqlPrepareContext* sqlctx, Intstruction* ins)
{
    // lengthi的长度目前就是 2个字节
    // TODO 需要把cell中的data序列化和反序列化。 格式为：<length1><data1><length2><data2>..
    size_t bytesize = 0;
    for (size_t i = ins->p1; i < ins->p2; i++)
    {
        bytesize += 2; // <length>
        bytesize += g_registers[i].n; // <data>
    }
    uint8_t* bytes = malloc(bytesize); // 序列化
    size_t bi = 0;
    for (size_t i = ins->p1; i < ins->p2; i++)
    {
        switch (g_registers[i].flags)
        {
        case REG_I32:
            bytes[bi++] = 0;
            bytes[bi++] = 4;
            bytes[bi++] = g_registers[i].value.i32 & 0xff000000>> 24;
            bytes[bi++] = g_registers[i].value.i32 & 0x00ff0000>> 16;
            bytes[bi++] = g_registers[i].value.i32 & 0x0000ff00>> 8;
            bytes[bi++] = g_registers[i].value.i32 & 0x000000ff;
            break;
        case REG_STR:
            bytes[bi++] = strlen(g_registers[i].value.s) & 0xff00 >> 8;
            bytes[bi++] = strlen(g_registers[i].value.s) & 0x00ff;
            memcpy(bytes + bi, g_registers[i].value.s, g_registers[i].n);
            bi += g_registers[i].n;
            break;
        default:
            break;
        }
    }
    g_registers[ins->p3].flags = REG_BYTES;
    g_registers[ins->p3].value.bytes = bytes; 
    g_registers[ins->p3].n = bytesize;
}

static void execute_insert(SqlPrepareContext* sqlctx, Intstruction* ins)
{
    printf("Execute insert ins.\n");
    Cursor* cursor = g_vdb_cursors[ins->p1];
    uint8_t* bytes = g_registers[ins->p2].value.bytes;
    int bytesize = g_registers[ins->p2].n;
    int rowid = g_registers[ins->p3].value.i32;
    //
    btree_insert(cursor->btree, rowid, bytes, bytesize);
}
static void execute_newrowid(SqlPrepareContext* sqlctx, Intstruction* ins)
{
    printf("Execute newrowid ins.\n");
    Cursor* cursor = g_vdb_cursors[ins->p1];
    // TODO create new rowid
    int rowid = btree_get_newrowid(cursor->btree); // 获取btree最右边孩子的key，
    g_registers[ins->p2].flags = REG_I32;
    g_registers[ins->p2].value.i32 = rowid;
    g_registers[ins->p2].n = 4;
}
// 运行字节码
void vdbe_run(SqlPrepareContext *sqlctx)
{
    printf("VDBE run for sql: [%s]\n", sqlctx->sql);
    Intstruction *ins;
    InstructionList *inslist = sqlctx->inslist;
    print_inslist(inslist);
    printf("Begin execute instructions:\n");    
    for (size_t pc = 0; pc < inslist->nints; )
    {
        ins = sqlctx->inslist->ints[pc];
        switch (ins->opcode)
        {
        case Init:
            execute_init(sqlctx, ins);
            pc++;
            break;
        case OpenRead:
            pc++;
            break;
        case Rewind:
            pc++;
            break;
            ;
        case Column:
            pc++;
            break;
        case ResultRow:
            pc++;
            break;
        case Next:
            pc++;
            break;
        case Halt:
            pc++;
            break;
        case Transaction:
            pc++;
            break;
        case Goto:
            pc = ins->p1;
            break;
        case OpenWrite:
            execute_openwrite(sqlctx, ins);
            pc++;
            break;
        case String:
            execute_string(sqlctx, ins);
            pc++;
            break;
        case MakeRecord:
            execute_makerecord(sqlctx, ins);
            pc++;
            break;
        case Insert:
            execute_insert(sqlctx, ins);
            pc++;
            break;
        case NewRowid:
            execute_newrowid(sqlctx, ins);
            pc++;
            break;
        default:
            printf("run unkown opcode [%d]", ins->opcode);
            break;
        }
    }
    // debug 临时 保证以前程序正常
    printf("Vdbe run ok.\n");
}
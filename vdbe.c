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
static size_t g_pc = 0; // 全局，可以跳转
static bool g_halt_flag = false;

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

Instruction* vdbe_new_ins(int opcode, int32_t p1, int32_t p2, int32_t p3, union P4_t p4)
{
    Instruction* ins = malloc(sizeof(Instruction));
    ins->opcode = opcode;
    ins->p1 = p1;
    ins->p2 = p2;
    ins->p3 = p3;
    ins->p4 = p4;
    return ins;
}
void free_ins(Instruction* ins)
{
    free(ins);
}
InstructionList* vdbe_new_inslist()
{
    InstructionList* inslist = malloc(sizeof(InstructionList));
    inslist->nints = 0;
    inslist->ints = NULL;
    return inslist;
}
void vdbe_inslist_add(InstructionList* inslist, Instruction* ins)
{
    inslist->ints = realloc(inslist->ints, sizeof(Instruction*) * (inslist->nints + 1));
    inslist->ints[inslist->nints] = ins;
    inslist->nints += 1;
}
void free_inslist(InstructionList* inslist)
{
    for (size_t i = 0; i < inslist->nints; i++)
    {
        free_ins(inslist->ints[i]);
    }
    free(inslist);
}

void print_inslist(InstructionList* inslist)
{

    printf("Instruction List:\n");
    printf("%4s %10s %6s %6s %6s %6s\n", "Addr", "Opcode", "P1", "P2", "P3", "P4");
    Instruction *ins;
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
            case CreateBtree:
                printf("%10s %6d %6d %6d %d\n", "CreateBtree", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break;
            case Copy:
                printf("%10s %6d %6d %6d %d\n", "Copy", ins->p1, ins->p2, ins->p3, ins->p4.i32);
                break; 
            default:
                printf("%10s ", "unknown");
                sql4_errno = VDBE_UNKOWN_OPCODE_ERR;
                break;
            }
        
    }

}
static void execute_init(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("Execute init ins.\n");
    g_halt_flag = false;
}
static void execute_rewind(SqlPrepareContext* sqlctx, Instruction* ins)
{
    Cursor* cursor = g_vdb_cursors[ins->p1]; // 本来就是指向第一行的，不需要移动到第一行
    // TODO 为了语义正确，实际上应该让cursor 调用函数移动到第一行，

    // 如果表为空直接 跳转到ins.p2
    
}
static void execute_next(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("execute next\n");
    Cursor* cursor = g_vdb_cursors[ins->p1]; 
    btree_cursor_advance(cursor);
    if (!cursor->end_of_table) {
        g_pc = ins->p2; // 目前， 执行的pc 和 生成的pc是一致的。
    }
}
static void execute_halt(SqlPrepareContext* sqlctx, Instruction* ins)
{
    // 重置vdbe状态, 释放打开的cursor
    g_halt_flag = true;
    printf("execute halt\n");
    g_pc = 0;
    for (size_t i = 0; i < MAX_OPEN_CURSOR; i++)
    {
        if (g_vdb_cursors[i] != NULL) {
            // FREE cursor
            btree_cursor_free(g_vdb_cursors[i]);
            g_vdb_cursors[i] = NULL;
        }
    }
    memset(g_registers, 0, sizeof(Register) * REGISTER_SIZE);
}


// 把col返回的内容放在buffer
static void execute_resultrow(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("execute resultrow\n");
    ResultBuffer* result = sqlctx->buffer;
    result->nrow += 1;
    result->data = realloc(result->data, result->nrow * sizeof(Row*)); // 新增一行
    // TODO 默认一行最长
    result->data[result->nrow - 1] = malloc(sizeof(Row)); // 为新的一行分配内存
    // 拼接
    Row* row = result->data[result->nrow - 1];
    int bdi = 0;
    for (size_t i = ins->p1; i <= ins->p2; i++)
    {
        // <len1><d1><len2><d2>
        uint8_t* data = g_registers[i].value.bytes;
        int n = g_registers[i].n;
        row->n += n;
        row->data = realloc(row->data, row->n);
        memcpy(row->data + row->n - n, data, n);
        printf("(debug) add reg[%ld], [%d]bytes to row\n", i, n);

    }
}
static void free_row(Row* row)
{
    free(row->data);
    free(row);
}
static void free_resultbuf(ResultBuffer* resbuf)
{
    for (size_t i = 0; i < resbuf->nrow; i++)
    {
        free_row(resbuf->data[i]);
    }
    free(resbuf);
}

static void execute_column(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("execute column\n");
    Cursor* cursor = g_vdb_cursors[ins->p1];
    int coli = ins->p2;
    // 读取coli的内容 到 ins.p2寄存器
    // 目前都是直接全部读出来忽略表结构
    uint8_t* data = btree_cursor_value(cursor);
    int j = 0;
    for (size_t i = 0; i < coli; i++)
    {
        int len = (data[j] << 8 | data[j+1]);
        if (i == coli - 1) {
            uint8_t* tmp = malloc(2 + len);
            memcpy(tmp, data + j, 2 + len);
            g_registers[ins->p3].value.bytes = tmp;
            g_registers[ins->p3].flags = REG_BYTES;
            g_registers[ins->p3].n = 2 + len;

        } 
        j += 2 + len;
    }
}

// 创建cursor
static void execute_openread(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("Execute openread ins.\n");
    uint32_t root_pagenum = ins->p2;
    Cursor* cursor = btree_cursor_start(btree_get(root_pagenum, sqlctx->db->pager));
    // 需要与cursor编号绑定。
    g_vdb_cursors[ins->p1] = cursor;
}

// 为表访问创建cursor
static void execute_openwrite(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("Execute openwrite ins.\n");
    uint32_t root_pagenum = ins->p2;
    Cursor* cursor = btree_cursor_start(btree_get(root_pagenum, sqlctx->db->pager));
    // 需要与cursor编号绑定。
    g_vdb_cursors[ins->p1] = cursor;
}
// 字符串设置寄存器
static void execute_string(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("Execute string ins.\n");
    g_registers[ins->p2].n = strlen(ins->p4.s);
    g_registers[ins->p2].flags = REG_STR;
    g_registers[ins->p2].value.s = strdup(ins->p4.s);
}
// 组装序列化bytes，
static void execute_makerecord(SqlPrepareContext* sqlctx, Instruction* ins)
{
    // 格式: <len1><data1><len2><data2>...
    size_t bytesize = 0;
    for (size_t i = ins->p1; i <= ins->p2; i++) {
        bytesize += 2; // length field
        bytesize += g_registers[i].n;
    }

    uint8_t* bytes = malloc(bytesize);
    if (!bytes) {
        fprintf(stderr, "malloc failed in makerecord\n");
        return;
    }

    size_t bi = 0;
    for (size_t i = ins->p1; i <= ins->p2; i++) {
        switch (g_registers[i].flags)
        {
        case REG_I32: {
            uint16_t len = 4;
            int32_t v = g_registers[i].value.i32;
            bytes[bi++] = (len >> 8) & 0xff;
            bytes[bi++] = len & 0xff;
            bytes[bi++] = (v >> 24) & 0xff;
            bytes[bi++] = (v >> 16) & 0xff;
            bytes[bi++] = (v >> 8) & 0xff;
            bytes[bi++] = v & 0xff;
            break;
        }
        case REG_STR: {
            uint16_t len = g_registers[i].n;
            bytes[bi++] = (len >> 8) & 0xff;
            bytes[bi++] = len & 0xff;
            if (g_registers[i].value.s && len > 0) {
                memcpy(bytes + bi, g_registers[i].value.s, len);
                bi += len;
            }
            break;
        }
        default:
            fprintf(stderr, "makerecord: unsupported reg type %d\n", g_registers[i].flags);
            break;
        }
    }

    g_registers[ins->p3].flags = REG_BYTES;
    g_registers[ins->p3].value.bytes = bytes; 
    g_registers[ins->p3].n = bytesize;
}


static void execute_insert(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("Execute insert ins.\n");
    Cursor* cursor = g_vdb_cursors[ins->p1];
    uint8_t* bytes = g_registers[ins->p2].value.bytes;
    int bytesize = g_registers[ins->p2].n;
    int rowid = g_registers[ins->p3].value.i32;
    //
    btree_insert(cursor->btree, rowid, bytes, bytesize);
}
static void execute_createbtree(SqlPrepareContext* sqlctx, Instruction* ins)
{
    // 创建一个空树，添加到pager
    int root_pagenum = pager_get_unused_pagenum(sqlctx->db->pager);
    pager_add_page(sqlctx->db->pager, root_pagenum);
    g_registers[ins->p1].value.i32 = root_pagenum;
    g_registers[ins->p1].flags = REG_I32;
    g_registers[ins->p1].n = 4;
}
static void execute_copy(SqlPrepareContext* sqlctx, Instruction* ins)
{
    memcpy(&g_registers[ins->p2], &g_registers[ins->p1], sizeof(Register));
}

static void execute_newrowid(SqlPrepareContext* sqlctx, Instruction* ins)
{
    printf("Execute newrowid ins.\n");
    Cursor* cursor = g_vdb_cursors[ins->p1];
    int rowid = btree_get_newrowid(cursor->btree); // 获取btree最右边孩子的key，
    g_registers[ins->p2].flags = REG_I32;
    g_registers[ins->p2].value.i32 = rowid + 1;
    g_registers[ins->p2].n = 4;
}
// 运行字节码
void vdbe_run(SqlPrepareContext *sqlctx)
{
    printf("VDBE run for sql: [%s]\n", sqlctx->sql);
    Instruction *ins;
    InstructionList *inslist = sqlctx->inslist;
    print_inslist(inslist);
    printf("Begin execute instructions:\n");   
    // 按理来说，halt时候就可以直接return了 
    for (; g_pc < inslist->nints && !g_halt_flag; )
    {
        ins = sqlctx->inslist->ints[g_pc];
        switch (ins->opcode)
        {
        case Init:
            execute_init(sqlctx, ins);
            g_pc++;
            break;
        case OpenRead:
            execute_openread(sqlctx, ins);
            g_pc++;
            break;
        case Rewind:
            execute_rewind(sqlctx, ins);
            g_pc++;
            break;
            ;
        case Column:
            execute_column(sqlctx, ins);
            g_pc++;
            break;
        case ResultRow:
            execute_resultrow(sqlctx, ins);
            g_pc++;
            break;
        case Next:
            execute_next(sqlctx, ins);
            g_pc++;
            break;
        case Halt:
            execute_halt(sqlctx, ins);
            g_pc++;
            break;
        case Transaction:
            g_pc++;
            break;
        case Goto:
            g_pc = ins->p1;
            break;
        case OpenWrite:
            execute_openwrite(sqlctx, ins);
            g_pc++;
            break;
        case String:
            execute_string(sqlctx, ins);
            g_pc++;
            break;
        case MakeRecord:
            execute_makerecord(sqlctx, ins);
            g_pc++;
            break;
        case Insert:
            execute_insert(sqlctx, ins);
            g_pc++;
            break;
        case NewRowid:
            execute_newrowid(sqlctx, ins);
            g_pc++;
            break;
        case CreateBtree:
            execute_createbtree(sqlctx, ins);
            g_pc++;
            break;
        case Copy:
            execute_copy(sqlctx, ins);
            g_pc++;
            break;
        default:
            printf("run unkown opcode [%d]", ins->opcode);
            break;
        }
    }
    // debug 临时 保证以前程序正常
    printf("Vdbe run ok.\n");
}
void vdbe_destroy(SqlPrepareContext* sqlctx)
{
    free_inslist(sqlctx->inslist);
    free_resultbuf(sqlctx->buffer);
    free(sqlctx->sql);
    printf("vdbe destroyed.\n");
}


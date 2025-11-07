#include "oranger.h"
#include "sql4code.h"
#include "btree.h"
#include "table.h"
#include "bytecode.h"
#include "vdbe.h"
#include "db.h"
#include <string.h>

extern DB* g_db;
// 编译时候的寄存器编号，仅仅是逻辑占位。在执行时候可以直接映射到数组index，也可以进行循环复用
static int nex_reg_num = 0;
// 同理，cursornum也是占位
static int next_cursor_num = 0; 

 Instruction* bytecode_init()
{
    Instruction* ins = vdbe_new_ins(Init, 0, 0, 0, (union P4_t){0});
    return ins;
}
/**
 * 为新的tree分配一个页码
 * p1: 页码存放的寄存器
 * p2: 0 not used
 * p3: 0 not used
 * p4: 0 not used
 */
Instruction* bytecode_createbtree()
{
    Instruction* ins = vdbe_new_ins(CreateBtree, nex_reg_num++, 0, 0, (union P4_t){0});
    return ins;
}

// TODO 还是需要把把master的一些表的元信息 缓存到内存结构中， 这个时候不经过字节码 直接取btree
/**
 * p1: 游标编号
 * p2: master表的rootpagenum
 * p3: 0 not used
 * p4: 0 not used
 */
Instruction* bytecode_openwrite()
{
    Instruction* ins = vdbe_new_ins(OpenWrite, next_cursor_num++,
         g_db->master->tree->root_page_num,
         0, (union P4_t){0});
    return ins;
}
/*
* P1: cursor编号
* P2: root pagenum
* p3: 0 not used
* p4: 0 not used 
*/
Instruction* bytecode_openread(char* tabname)
{

    Instruction* ins = vdbe_new_ins(OpenRead, next_cursor_num++, 
        db_get_table(g_db, tabname)->tree->root_page_num,
        0, (union P4_t){0});
    return ins;
}
/**
 * 游标移动到第一行
 * P1: cursor编号
 * P2: 表为空时候跳转地址
 * P3: 0 not used
 * P4: 0 not used
*/
Instruction* bytecode_rewind(int cursor_num)
{
    Instruction* ins = vdbe_new_ins(Rewind, cursor_num, 0, 0, (union P4_t){0});
    return ins;
}
/**
 * P1: cursor编号
 * P2: 列，从1开始
 * P3: 目标寄存器
 * P4: 0 not used
 */
Instruction* bytecode_column(int p1, int p2)
{
    Instruction* ins = vdbe_new_ins(Column, p1, p2, nex_reg_num++, (union P4_t){0});
    return ins;
}
/** cursor移动到下一行
 * P1: cursor编号
 * P2: 循环的跳转地址。还有下一行时候跳转
 * P3: 0 not used
 * P4: 0 not used
 */
 Instruction* bytecode_next(int p1, int p2)
{
    Instruction* ins = vdbe_new_ins(Next, p1, p2, 0, (union P4_t){0});
    return ins;
}
/**
 * P1: 0 not used
 * P2: 0 not used
 * P3: 0 not used
 * P4: 0 not used
 */
 Instruction* bytecode_halt()
{
    // 结束指令流运行
    Instruction* ins = vdbe_new_ins(Halt, 0, 0, 0, (union P4_t){0});
    return ins;
}
    /**
     * P1: 0 not used
     * P2: 寄存器编号
     * P3: 0 not used
     * P4: char* 
     */
Instruction* bytecode_string(char* s)
{
    union P4_t p4 = {.s = s};
    Instruction* ins = vdbe_new_ins(String, 0, nex_reg_num++, 0, p4);
    return ins;
}
/**
 * p1: src register num
 * p2: dest register num
 * p3: 0 not used
 * p4: 0 not used
 */
Instruction* bytecode_copy(uint32_t p1)
{
    Instruction* ins = vdbe_new_ins(Copy, p1, nex_reg_num++, 0,(union P4_t){0} );
    return ins;
}


/**
 * P1: 起始寄存器
 * P2: 终止寄存器（与sqlite不同）
 * P3：目标寄存器
 * P4: 0 not used
 */
Instruction* bytecode_mkrecord(int32_t p1, int32_t p2)
{
     Instruction* ins = vdbe_new_ins(MakeRecord, p1, p2, nex_reg_num++, (union P4_t){0});
    return ins;   
}
/**
 * 将打包的记录插入表。不含一些信息，如rootpagenum
 * P1: cursor 编号
 * p2: mkrecord的寄存器编号
 * p3: rowid的寄存器编号
 * p4: 0 not used
 */
Instruction* bytecode_insert(uint32_t p1, int32_t p2, int32_t p3)
{
    Instruction* ins = vdbe_new_ins(Insert, p1,  p2, p3, (union P4_t){0});
    return ins;   
}

/**
 * 获取newrowid
 * p1: openwrite 的 cursor编号
 * p2: 新rowid目标寄存器
 * p3: 0 not used
 * p4: 0 not used
 */
Instruction*  bytecode_newrowid(int cursor_num)
{
    Instruction* ins = vdbe_new_ins(NewRowid, cursor_num, nex_reg_num++, 0, (union P4_t){0});
    return ins;   
}
/**
 * 合并寄存器1到2 的内容，为一行， 我们不会把这个内容放在寄存器，执行时候会在vdbe缓冲中
 * p1: 寄存器编号1
 * p2: 寄存器编号2
 * p3: 0 not used
 * p4: 0 not used
 */
Instruction*  bytecode_resultrow(int p1, int p2)
{   
    Instruction* ins = vdbe_new_ins(ResultRow, p1, p2, 0, (union P4_t){0});
    
    return ins;   
}
 void bytecode_select(struct SelectStmt* selectst, SqlPrepareContext* sqlctx)
{
    uint32_t nins = 0;
    
    InstructionList* inslist = sqlctx->inslist;
    char* tabname = selectst->table_ref->name;
    Instruction* ins;
    ins = bytecode_init();
    vdbe_inslist_add(inslist, ins);
    nins++;

    Instruction* openread_ins = bytecode_openread(tabname);
    vdbe_inslist_add(inslist, openread_ins);
    nins++;

    Instruction* rewind_ins = bytecode_rewind(ins->p1);
    vdbe_inslist_add(inslist, rewind_ins); 
    nins++;

    int col_start_addr = nins;
    int col_reg1 = 0, col_reg2 = 0;
    Table* tabmeta = db_get_table(sqlctx->db, tabname);
    for (int i = 0; i < selectst->col_list->nexpr; i++)
    {
        int colk = 0;
        struct Expr* expr = selectst->col_list->items[i];
        // 查找列名对应元信息的列id
        for (size_t j = 0; j < tabmeta->ncol; j++)
        {
            if (strcmp(expr->name, tabmeta->cols[j]) == 0) {
                colk = j;
                break;
            }
        }
        
        Instruction* col_ins = bytecode_column(openread_ins->p1, colk + 1);
        vdbe_inslist_add(inslist, col_ins);
        if (col_ins->p3 < col_reg1) col_reg1 = col_ins->p3;
        if (col_ins->p3 > col_reg2) col_reg2 = col_ins->p3;
        nins++;
    } 
    Instruction* resultrow_ins = bytecode_resultrow(col_reg1, col_reg2);
    vdbe_inslist_add(inslist, resultrow_ins);
    nins++;   
    
    Instruction* next_ins = bytecode_next(openread_ins->p1, col_start_addr);
    vdbe_inslist_add(inslist, next_ins); 
    nins++;

    Instruction* halt_ins = bytecode_halt();
    vdbe_inslist_add(inslist, halt_ins);
    uint32_t haltaddr = nins;

    nins++;

    rewind_ins->p2 = haltaddr;
}



void bytecode_create_table(struct CreateStmt* creatst,  SqlPrepareContext* sqlctx)
{
    // Init
    // CreateBtree 得到一个新的page
    // Transcation 开启事务
    // OpenWrite 打开master
    // NewRowid 为master表生成新rowid
    // String8 类型type：table
    // String8 表名：users
    // String8 sql原文
    // MakeRecord 组装记录 (type,name,tbl_name,rootpage,sql)
    // Insert 写入master表
    // Close 关闭cursor
    // Commit 提交事务
    // Halt
    InstructionList* inslist = sqlctx->inslist;
    Instruction* ins;
    ins = bytecode_init();
    vdbe_inslist_add(inslist, ins);

    Instruction* createbtree_ins = bytecode_createbtree();
    vdbe_inslist_add(inslist, createbtree_ins);

    Instruction* openwt_ins = bytecode_openwrite();
    vdbe_inslist_add(inslist, openwt_ins);
    
    Instruction* newrowid_ins = bytecode_newrowid(openwt_ins->p1);
    vdbe_inslist_add(inslist, newrowid_ins);

    Instruction* typeins = bytecode_string("table");
    vdbe_inslist_add(inslist, typeins);
    
    Instruction* name_ins = bytecode_string(creatst->table_ref->name);
    vdbe_inslist_add(inslist, name_ins);

    Instruction* tblname_ins = bytecode_string(creatst->table_ref->name);
    vdbe_inslist_add(inslist, tblname_ins);

    Instruction* copy_ins = bytecode_copy(createbtree_ins->p1);
    vdbe_inslist_add(inslist, copy_ins);
    
    Instruction* sql_ins = bytecode_string(sqlctx->sql);
    vdbe_inslist_add(inslist, sql_ins);

    Instruction* mkrcrd_ins = bytecode_mkrecord(typeins->p2, sql_ins->p2);
    vdbe_inslist_add(inslist, mkrcrd_ins);

    ins = bytecode_insert(openwt_ins->p1, mkrcrd_ins->p3, newrowid_ins->p2);
    vdbe_inslist_add(inslist, ins);
    
    ins = bytecode_halt();
    vdbe_inslist_add(inslist, ins);
}

// 将root ast翻译成指令， 放在inslist，
void bytecode_generate(SqlPrepareContext* sqlctx)
{
    printf("Bytecode generate for sql: [%s]\n", sqlctx->sql);
    AST* root = sqlctx->ast;
    InstructionList* inslist = vdbe_new_inslist();
    sqlctx->inslist = inslist;
    
    for (int i = 0; i < root->nstmt; i++)
    {
        struct Stmt* stmt = root->items[i];
        switch (stmt->type)
        {
        case STMT_SELECT:
            bytecode_select((struct SelectStmt* )(stmt->st), sqlctx);
            break;
        case STMT_CREATE:
            bytecode_create_table((struct CreateStmt*)(stmt->st), sqlctx);
            break;
        default:
            break;
        }
    }
}
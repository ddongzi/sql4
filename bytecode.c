#include "orange.h"
#include "sql4code.h"
#include "btree.h"
#include "table.h"
#include "bytecode.h"
#include "vdbe.h"
#include "db.h"

extern DB* g_db;
// 编译时候的寄存器编号，仅仅是逻辑占位。在执行时候可以直接映射到数组index，也可以进行循环复用
static int nex_reg_num = 0;
// 同理，cursornum也是占位
static int next_cursor_num = 0; 

 Intstruction* bytecode_init()
{
    Intstruction* ins = vdbe_new_ins(Init, 0, 0, 0, (union P4_t){0});
    return ins;
}
// TODO 还是需要把把master的一些表的元信息 缓存到内存结构中， 这个时候不经过字节码 直接取btree
/**
 * p1: 游标编号
 * p2: master表的rootpagenum
 * p3: 0 not used
 * p4: 0 not used
 */
Intstruction* bytecode_openwrite()
{
    Intstruction* ins = vdbe_new_ins(OpenWrite, next_cursor_num++,
         g_db->master->tree->root_page_num,
         0, (union P4_t){0});
    return ins;
}
Intstruction* bytecode_openread(char* tabname)
{
    // TODO 不应该创建游标获取，而是通过元信息

    /*
     * P1: cursor编号
     * P2: root pagenum
     * p3: 0 not used
     * p4: 0 not used 
     */
    Intstruction* ins = vdbe_new_ins(OpenRead, next_cursor_num++, 
        db_get_table(g_db, tabname)->tree->root_page_num,
        0, (union P4_t){0});
    return ins;
}
 Intstruction* bytecode_resultrow()
 {
    /**
     * P1: 寄存器序号
     * P2: 寄存器序号
     * p3: 0 not used
     * p4: 0 not used 
     */
    Intstruction* ins = vdbe_new_ins(ResultRow, 0, 1, 0, (union P4_t){0});
    return ins;
 }
 Intstruction* bytecode_rewind()
{
    /**
     * P1: cursor编号
     * P2: 表为空时候跳转地址
     * P3: 0 not used
     * P4: 0 not used
    */
    Intstruction* ins = vdbe_new_ins(Rewind, 0, 0, 0, (union P4_t){0});
    return ins;
}
 Intstruction* bytecode_column(char* colname)
{
    /**
     * P1: cursor编号
     * P2: 列，从1开始
     * P3: 目标寄存器
     * P4: 0 not used
     */
    // 运行column指令，会把列名放入目标寄存器
    Intstruction* ins = vdbe_new_ins(Column, 0, 1, 0, (union P4_t){0});
    return ins;
}
 Intstruction* bytecode_next()
{
    /**
     * P1: cursor编号
     * P2: 跳转地址。 如果还有下一行，就跳转 继续读
     * P3: 0 not used
     * P4: 0 not used
     */
    Intstruction* ins = vdbe_new_ins(Next, 0, 0, 0, (union P4_t){0});
    return ins;
}
 Intstruction* bytecode_halt()
{
    /**
     * P1: 0 not used
     * P2: 0 not used
     * P3: 0 not used
     * P4: 0 not used
     */
    // 结束指令流运行
    Intstruction* ins = vdbe_new_ins(Halt, 0, 0, 0, (union P4_t){0});
    return ins;
}
    /**
     * P1: 0 not used
     * P2: 寄存器编号
     * P3: 0 not used
     * P4: char* 
     */
Intstruction* bytecode_string(char* s)
{
    union P4_t p4 = {.s = s};
    Intstruction* ins = vdbe_new_ins(String, 0, nex_reg_num++, 0, p4);
    return ins;
}
/**
 * P1: 起始寄存器
 * P2: 打包的列数
 * P3：目标寄存器
 * P4: 0 not used
 */
Intstruction* bytecode_mkrecord(int32_t p1, int32_t columns)
{
     Intstruction* ins = vdbe_new_ins(MakeRecord, p1, columns, nex_reg_num++, (union P4_t){0});
    return ins;   
}
/**
 * 将打包的记录插入表
 * P1: cursor 编号
 * p2: mkrecord的寄存器编号
 * p3: rowid的寄存器编号
 * p4: 0 not used
 */
Intstruction* bytecode_insert(uint32_t p1, int32_t p2, int32_t p3)
{
    Intstruction* ins = vdbe_new_ins(Insert, p1,  p2, p3, (union P4_t){0});
    return ins;   
}

/**
 * 获取newrowid
 * p1: openwrite 的 cursor编号
 * p2: 新rowid目标寄存器
 * p3: 0 not used
 * p4: 0 not used
 */
Intstruction*  bytecode_newrowid(int cursor_num)
{
    Intstruction* ins = vdbe_new_ins(NewRowid, cursor_num, nex_reg_num++, 0, (union P4_t){0});
    return ins;   
}
 void bytecode_select(struct SelectStmt* selectst, SqlPrepareContext* sqlctx)
{
    InstructionList* inslist = sqlctx->inslist;
    char* tabname = selectst->table_ref->name;
    Intstruction* ins;
    ins = bytecode_init();
    vdbe_inslist_add(inslist, ins);

    ins = bytecode_openread(tabname);
    vdbe_inslist_add(inslist, ins);

    ins = bytecode_rewind();
    vdbe_inslist_add(inslist, ins); // TODO p2 待定，需要halt地址

    for (int i = 0; i < selectst->col_list->nexpr; i++)
    {
        struct Expr* expr = selectst->col_list->items[i];
        ins = bytecode_column(expr->name);
        vdbe_inslist_add(inslist, ins);
    }    
    ins = bytecode_next();
    vdbe_inslist_add(inslist, ins); // TODO p2 待定

    ins = bytecode_halt();
    vdbe_inslist_add(inslist, ins);
}



void bytecode_create_table(struct CreateStmt* creatst,  SqlPrepareContext* sqlctx)
{
    // Init
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
    Intstruction* ins;
    ins = bytecode_init();
    vdbe_inslist_add(inslist, ins);

    Intstruction* openwt_ins = bytecode_openwrite();
    vdbe_inslist_add(inslist, openwt_ins);
    
    Intstruction* newrowid_ins = bytecode_newrowid(openwt_ins->p1);
    vdbe_inslist_add(inslist, newrowid_ins);

    Intstruction* typeins = bytecode_string("table");
    vdbe_inslist_add(inslist, typeins);
    
    Intstruction* tabname_ins = bytecode_string(creatst->table_ref->name);
    vdbe_inslist_add(inslist, tabname_ins);
    
    Intstruction* sql_ins = bytecode_string(sqlctx->sql);
    vdbe_inslist_add(inslist, sql_ins);
    
    Intstruction* mkrcrd_ins = bytecode_mkrecord(typeins->p2, sql_ins->p2 - typeins->p2 + 1);
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
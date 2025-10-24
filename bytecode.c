#include "orange/sql.h"
#include "sql4code.h"
#include "vdbe.h"
#include "btree.h"
#include "table.h"

static SQL4_CODE exprlist(struct ExprList* exprls,  struct InstructionList* inslist)
{
    for (int i = 0; i < exprls->nexpr; i++)
    {
        // 最小了
        struct Expr* expr = exprls->items[i];
    }
    
}
static Intstruction* init()
{
    Intstruction* ins = vdbe_new_ins(Init, 0, 0, 0, (union P4_t){0});
}
static Intstruction* openread(char* tabname)
{
    Cursor* csr = btree_cursor_start(g_table->tree);
    int pagenum = csr->btree->root_page_num;
    /*
     * P1: cursor编号
     * P2: root pagenum
     * p3: 0 not used
     * p4: 0 not used 
     */
    Intstruction* ins = vdbe_new_ins(OpenRead, 0, pagenum, 0, (union P4_t){0});
    return ins;
}
static Intstruction* rewind()
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
static Intstruction* column(char* colname)
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
static Intstruction* next()
{
    /**
     * P1: cursor编号
     * P2: 跳转地址。 如果还有下一行，就跳转 继续读
     * P3: 0 not used
     * P4: 0 not used
     */
    Intstruction* ins = vdbe_new_ins(Next, 0, 0, 0, (union P4_t){0});
    return next;
}
static Intstruction* halt()
{
    /**
     * P1: 0 not used
     * P2: 0 not used
     * P3: 0 not used
     * P4: 0 not used
     */
    // 结束指令流运行
    Intstruction* ins = vdbe_new_ins(Halt, 0, 0, 0, (union P4_t){0});
    return next;
}
static void select(struct SelectStmt* selectst, struct InstructionList* inslist)
{
    char* tabname = selectst->table_ref->name;
    Intstruction* ins;
    ins = init();
    vdbe_inslist_add(inslist, ins);

    ins = openread(tabname);
    vdbe_inslist_add(inslist, ins);

    ins = rewind();
    vdbe_inslist_add(inslist, ins); // TODO p2 待定，需要halt地址

    for (int i = 0; i < selectst->col_list->nexpr; i++)
    {
        struct Expr* expr = selectst->col_list->items[i];
        ins = column(expr->name);
        vdbe_inslist_add(inslist, ins);
    }    
    ins = next();
    vdbe_inslist_add(inslist, ins); // TODO p2 待定

    ins = halt();
    vdbe_inslist_add(inslist, ins);
}


// 将root ast翻译成指令， 放在inslist，
void bytecode_generate(struct StmtList* root, struct InstructionList* inslist)
{
    for (int i = 0; i < root->nstmt; i++)
    {
        struct Stmt* stmt = root->items[i];
        switch (stmt->type)
        {
        case STMT_SELECT:
            select((struct SelectStmt* )(stmt->st), inslist);
            break;
        
        default:
            break;
        }
    }
    
}
/**
 * Virtual DataBase Engine
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "btree.h"
#include "sql4code.h"
#include "vdbe.h"
#include "bytecode.h"

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
                printf("%10s ", "Init");
                break;
            case OpenRead:
                printf("%10s ", "OpenRead");
                break;
            case Rewind:
                printf("%10s ", "Rewind");
                break;
            case Column:
                printf("%10s ", "Column");
                break;
            case ResultRow:
                printf("%10s ", "ResultRow");
                break;
            case Next:
                printf("%10s ", "Next");
                break;
            case Halt:
                printf("%10s ", "Halt");
                break;
            case Transaction:
                printf("%10s ", "Transaction");
                break;
            case Goto:
                printf("%10s ", "Goto");
                break;
            default:
                printf("%10s ", "unknown");
                sql4_errno = VDBE_UNKOWN_OPCODE_ERR;
                break;
            }
        printf("%6d %6d %6d %6d\n", ins->p1, ins->p2, ins->p3, ins->p4.i32);
        
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
// 运行字节码
void vdbe_run(SqlPrepareContext *sqlctx)
{
    printf("VDBE run for sql: [%s]\n", sqlctx->sql);
    Intstruction *ins;
    InstructionList *inslist = sqlctx->inslist;
    print_inslist(inslist);
    printf("Begin execute instructions:\n");    
    for (size_t pc = 0; pc < inslist->nints; pc++)
    {
        ins = sqlctx->inslist->ints[pc];
        switch (ins->opcode)
        {
        case Init:
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
        default:
            break;
        }
    }
    // debug 临时 保证以前程序正常
    printf("Vdbe run ok.\n");
}
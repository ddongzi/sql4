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


void print_ins(int addr, const char *opcode,
                   char *p1, int p2, const char *comment)
{
    printf("%6d %12s %4s %4d %20s\n",
           addr, opcode, p1, p2, comment);
}

// 运行字节码
void vdbe_run(SqlPrepareContext *sqlctx)
{
    Intstruction *ins;
    InstructionList *inslist = sqlctx->inslist;
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
}
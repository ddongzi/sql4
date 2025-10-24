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

void emit(int opcode, int32_t p1, int32_t p2, int32_t p3, union P4_t p4)
{
    iss[size].opcode = opcode;
    iss[size].p1 = p1;
    iss[size].p2 = p2;
    iss[size].p3 = p3;
    iss[size].p4 = p4;
    size++;
}

void format_printf(int addr, const char* opcode, 
    char* p1, int p2, const char* comment
    )
{
    printf("%6d %12s %4s %4d %20s\n", 
        addr, opcode, p1, p2, comment
    );  
}


// TODO 通过complier 转化sql语句为 bytecode, 放在vdbe执行。

/*
 * virtual machines
 * */
SQL4_CODE execute_statement(sql_statement *statement, Table *table)
{
    switch (statement->type) {
        case STATEMENT_INSERT:
            return execute_insert(statement, table);
        case STATEMENT_SELECT:
            return execute_select(statement, table);
        case STATEMENT_DELETE:
            return execute_delete(statement, table);
        default:
            perror("unkown execute type");
            return EXECUTE_UNKNOWN_ERR;
    }
}



/**
 * @brief 执行select
 */
SQL4_CODE execute_select(sql_statement * statement, Table * table)
{
    Row row;
    size_t selectsize;
    uint8_t** data;

    if (btree_select(table->tree, &selectsize, data) != BTREE_SELECT_SUCCESS) {
        fprintf(stderr, "btree select err: %d", sql4_errno);
        return EXECUTE_ERR;
    }
    // TODO data print
    return EXECUTE_SUCCESS;
}

/**
 * @brief 执行insert
 */
SQL4_CODE execute_insert(sql_statement* statement, Table* table)
{
    Row* row = &(statement->row);
    uint32_t key = row->time;
    if (btree_insert(table->tree, key, row, sizeof(row)) != BTREE_INSERT_SUCCESS) {
        fprintf(stderr, "btree insert error: %d", sql4_errno);
        return EXECUTE_ERR;
    }
    return EXECUTE_SUCCESS;
}

/**
 * @brief 通过id找到cell，请0文本，移除cell
 * 
 * @param statement 
 * @param table 
 * @return SQL4_CODE 
 */
SQL4_CODE execute_delete(sql_statement *statement, Table *table)
{
    time_t time = statement->row.time;
    time_t key = time;

    if (btree_delete(table->tree, key) != BTREE_DELETE_SUCCESS) {
        fprintf(stderr, "btree delete err: %d", sql4_errno);
    }
    return EXECUTE_SUCCESS;
}

// 运行字节码
void vdbe_run()
{
    bytecode_generate();
}
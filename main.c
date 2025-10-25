//
// Created by dong on 4/25/24.
//
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>
#include "pager.h"
#include "sql4limit.h"
#include "btree.h"
#include "vdbe.h"
#include "table.h"
#include "orange.h"
#include "bytecode.h"
#include "db.h"

uint16_t sql4_errno = 0; // 初始hua
Table* g_table = NULL; 

/* 用户输入以封号末尾 + 换行 结束。 */
char* read_input()
{
    char* input = malloc(1024);
    size_t input_len = 0;

    size_t len = 0;
    char* line = NULL;
    ssize_t nread = 0;
    while (true)
    {   
        nread = getline(&line, &len, stdin);
        strncat(input, line, nread - 1); // 不加换行符号            
        if (input_len + nread < 1024
             && line[nread - 2] == ';'
             && line[nread - 1] == '\n') {
            break;
        }
        input_len += nread - 1;
    }
    return strdup(input);
}
// AST, 字节码生成
void prepare_sqlctx(char* sql, SqlPrepareContext *sqlctx)
{
    sqlctx->sql = strdup(sql);
    sqlctx->ast = NULL; // 在orange_parse分配设置
    sqlctx->inslist = NULL; // 在bytecode_generate分配设置
    orange_parse(sqlctx); // 解释， 设置ast
    bytecode_generate(sqlctx);// 生成字节码
}
// 执行sql语句（对应字节码）
void execute_sqlctx(SqlPrepareContext* sqlctx)
{
    vdbe_run(sqlctx);
}
// TODO 临时，单表支持
/*处理源命令*/
void do_meta_command(char* input, Table *table)
{
    if (strcmp(input, ".exit") == 0) {
        db_close(table);
        exit(EXIT_SUCCESS);
    } else if (strcmp(input, ".btree") == 0) {
        printf("Tree:\n");
        btree_print(table->tree);
    } else if (strcmp(input, ".help") == 0) {
        printf("Help:\n");
        printf("Meta command:\n");
        printf("\t.exit [] exit\n");
        printf("\t.btree [] btree\n");
        printf("\t.help [] help\n");
        printf("SQL command:\n");
        printf("\tselect []\n");
        printf("\tinsert [id][][] \n");
        printf("\tdelete [index]\n");

    } else {
        sql4_errno = EXECUTE_ERR;
    }
}

extern Table* g_table;

/**
 *
sqlite> .exit
 * @param argc
 * @param argv
 * @return
 */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Must supply a  db filename.\n");
        exit(EXIT_FAILURE);
    }
    char *file_name = argv[1];
    g_table = db_open(file_name);
    //printf("PAGES: %u, ROWS-PAGE: %u, ROW-SIZE :%u\n", TABLE_MAX_PAGES, ROWS_PER_PAGE, ROW_SIZE);
    char *input = NULL;
    while (true) {
        printf("db > ");
        input = read_input();

        // .开头的 源命令
        if (input[0] == '.') {
            // 源命令
            do_meta_command(input, g_table);
            continue;
        }
        // 非源命令 sql语句
        SqlPrepareContext* sqlctx = malloc(sizeof(SqlPrepareContext));
        prepare_sqlctx(input, sqlctx);
        execute_sqlctx(sqlctx);
    }
    return 0;
}

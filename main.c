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
#define FIELD_SIZE(type, field) sizeof(((type*)0)->field)
#define FIELD_OFFSET(type, field) offsetof(type, field)


/* 与getline交互*/
typedef struct {
    char *buffer;
    size_t buffer_length;
    size_t input_length;
} input_buffer_t;

/*源命令 结果*/
typedef enum {
    META_COMMAND_SUCCESS, // 源命令识别成功
    META_COMMAND_UNRECOGNIZED_COMMAND // 。开始， 但是命令不存在
} meta_cmd_res;

/*sql命令 结果*/
typedef enum {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_SYNTAX_ERROR,
    PREPARE_STRING_TOO_LONG,
} prepare_res_type;

/*sql 命令类型*/
typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_DELETE
} statement_type;







void close_input_buffer(input_buffer_t* input_buffer);


/*
 * 1. 缓存刷新到盘
 * 2. 关闭db file
 * 3. 释放table， pager
 * */
void db_close(Table *table)
{
    Pager *pager = btree_get_pager(table->tree);
    assert(pager);
    assert(table);

    for (uint32_t i = 0; i < pager_get_num_pages(pager); ++i) {
        if (pager_get_page(pager, i) == NULL) {
            continue;
        }
        pager_flush(pager, i);
        pager_free_page(pager, i);
    }

    int res = close(pager_get_fd(pager));
    if (res == -1) {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; ++i) {
        pager_free_page(pager, i);
    }
    free(pager);
    free(table);
}



void print_row(Row *row)
{
    printf("(%d, %s, %s)\n", row->time, row->source, row->content);
}

/* 从内存读,  source 为 一行的起始地址*/
void deserialize_row(void* src, Row* destination) {
    memcpy(&(destination->time), src +  FIELD_OFFSET(Row, time), FIELD_SIZE(Row, time));
    memcpy(&(destination->source), src + FIELD_OFFSET(Row, source), FIELD_SIZE(Row, source));
    memcpy(&(destination->content), src +  FIELD_OFFSET(Row, content),  FIELD_SIZE(Row, content));
}




/* db初始化
 * 1. 打开db file
 * 2. 初始化pager
 * 3. 初始化table
 * 4. 初始化page0 ， 根节点
 * */
Table *db_open(const char *file_name)
{
    Pager *pager = pager_open(file_name);
    uint32_t num_rows = pager_get_file_length(pager) / sizeof(Row);

    Table *table = (Table *) malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;

    if (pager_get_num_pages(pager) == 0) {
        // 分配page0 , 初始化根节点
        leaf_node_t *root_node = (leaf_node_t*)pager_get_page(pager, 0);
        initialize_leaf_node(root_node, 0);
        root_node->meta.is_root = 1;

    }
    return table;
}


/*处理源命令*/
meta_cmd_res do_meta_command(input_buffer_t *input_buffer, Table *table)
{
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        close_input_buffer(input_buffer);
        db_close(table);
        exit(EXIT_SUCCESS);
    } else if (strcmp(input_buffer->buffer, ".constants") == 0) {
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".table") == 0) {
        printf("Table:\n");
        print_table(table);
        return META_COMMAND_SUCCESS;
    }else if (strcmp(input_buffer->buffer, ".btree") == 0) {
        printf("Tree:\n");
        print_tree(table->pager, 0, 0);
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".help") == 0) {
        printf("Help:\n");
        printf("Meta command:\n");
        printf("\t.exit [] exit\n");
        printf("\t.constants [] constants\n");
        printf("\t.btree [] btree\n");
        printf("\t.table [] btree\n");
        printf("\t.help [] help\n");

        printf("SQL command:\n");
        printf("\tselect []\n");
        printf("\tinsert [time_t][source][content] \n");
        printf("\tdelete [index]\n");

        return META_COMMAND_SUCCESS;
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

input_buffer_t* new_input_buffer()
{
    input_buffer_t* input_buffer = (input_buffer_t*)malloc(sizeof(input_buffer_t));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}
void close_input_buffer(input_buffer_t* input_buffer)
{
    assert(input_buffer);
    assert(input_buffer->buffer);
    free(input_buffer->buffer);
    free(input_buffer);
}
void read_input(input_buffer_t *input_buffer)
{
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
    if (bytes_read < 0) {
        printf("Error reading input");
        exit(EXIT_FAILURE);
    }
    // 去除换行符号
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = '\0';
}
// 打印提示
void print_prompt()
{
    printf("db > ");
}







/**
 *

 ~ sqlite3
SQLite version 3.16.0 2016-11-04 19:09:39
Enter ".help" for usage hints.
Connected to a transient in-memory database.
Use ".open FILENAME" to reopen on a persistent database.
sqlite> create table users (id int, source varchar(255), content varchar(255));
sqlite> .tables
users
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
    input_buffer_t *input_buffer = new_input_buffer();
    //printf("PAGES: %u, ROWS-PAGE: %u, ROW-SIZE :%u\n", TABLE_MAX_PAGES, ROWS_PER_PAGE, ROW_SIZE);
    while (true) {
        /*1. Making a Simple REPL*/
        print_prompt();
        read_input(input_buffer);
        if (input_buffer->buffer[0] == '.') {
            // 源命令
            switch (do_meta_command(input_buffer, table)) {
                case META_COMMAND_SUCCESS:
                    continue;
                case META_COMMAND_UNRECOGNIZED_COMMAND:
                    printf("Unrecognized command `%s` \n", input_buffer->buffer);
                    continue;
            }
        }
        sql_statement statement;
        switch (prepare_statement(input_buffer, &statement)) {
            // 拿到SQL 就去处理
            case PREPARE_SUCCESS:
                break;
            case PREPARE_SYNTAX_ERROR:
                printf("Syntax error. Could not parse statement.\n");
                continue;
            case PREPARE_UNRECOGNIZED_STATEMENT:
                printf("Unrecognized SQL command `%s` \n", input_buffer->buffer);
                continue;
            case PREPARE_STRING_TOO_LONG:
                printf("String is too long.\n");
                continue;

        }

        switch (execute_statement(&statement, table)) {
            case EXECUTE_SUCCESS:
                printf("Executed.\n");
                break;
            case EXECUTE_TABLE_FULL:
                printf("Error: Table full.\n");
                break;
            case EXECUTE_DUPLICATE_KEY:
                printf("Error: Duplicate key.\n");
                break;
        }
    }
    return 0;
}

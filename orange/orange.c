/**
 * TODO 参考书中 分析sql写法。
 */
#include "../sql4code.h"
#include "y.tab.h"
#include <stdio.h>

// 运行时候生成在lex.yy.c
typedef struct yy_buffer_state *YY_BUFFER_STATE;
YY_BUFFER_STATE yy_scan_string(const char *str);
void yy_delete_buffer(YY_BUFFER_STATE buffer);

/**
 * Parser: sql text -> sql statement(instructions) 
 */
/* 处理insert*/
prepare_res_type prepare_insert(input_buffer_t* input_buffer, sql_statement *statement)
{
    statement->type = STATEMENT_INSERT;
    char *keyword = strtok(input_buffer->buffer, " ");
    char *time_str = strtok(NULL, " ");
    char *source = strtok(NULL, " ");
    char *content = strtok(NULL, " ");

    if (time_str == NULL || source == NULL || content == NULL) {
        return PREPARE_SYNTAX_ERROR;
    }
    time_t time = atoi(time_str);

    if (strlen(source) > COLUMN_SOURCE_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(content) > COLUMN_CONTENT_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }
    statement->row.time = time;
    strcpy(statement->row.source, source);
    strcpy(statement->row.content, content);
    return PREPARE_SUCCESS;
}

prepare_res_type prepare_delete(input_buffer_t *input_buffer, sql_statement *statement)
{
    statement->type = STATEMENT_DELETE;
    char *keyword = strtok(input_buffer->buffer, " ");
    char *time_str = strtok(NULL, " ");
    time_t time = atoi(time_str);

    statement->row.time = time;
    return PREPARE_SUCCESS;
}
/* 填充 statement*/
/*
 * insert 1 cstack foo@bar.com
*/
prepare_res_type prepare_statement(input_buffer_t *input_buffer, sql_statement *statement)
{
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        return prepare_insert(input_buffer, statement);
    }
    if (strncmp(input_buffer->buffer, "select", 6) == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    if (strncmp(input_buffer->buffer, "delete", 6) == 0) {
        statement->type = STATEMENT_DELETE;
        return prepare_delete(input_buffer, statement);
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}


SQL4_CODE parse(const char* sql_text)
{
    YY_BUFFER_STATE buf = yy_scan_string(sql_text);
    int retcode = yyparse();
    yy_delete_buffer(buf);
    return COMPLIER_PARSE_SUCCESS;
}
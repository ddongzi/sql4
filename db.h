#ifndef DB_H
#define DB_H
#include "table.h"
#include "pager.h"


// 一个数据库一个pager
typedef struct {
    Pager* pager;
    Table** tabs; // 元信息
    int ntab;
    Table* master; // master表 type name tbl_name root_page sql
} DB;

extern DB* g_db;
// 获取table元信息
Table* db_get_table(DB* db, char* name);
void db_close(DB* db);
DB *db_open(const char *file_name);

#endif
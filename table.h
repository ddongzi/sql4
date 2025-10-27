#ifndef TABLE_H
#define TABLE_H

#include "btree.h"

// TODO 目前是固定字段
typedef  struct {
    int id;    // 日志时间
    char* name;
    char* addr;
} Row;

// 存储表的元信息，不存储数据，来自启动时候master读取
// 属于静态结构，不像pager那样动态操作，
typedef struct  {
    BTree* tree;    // 对应一个tree 。 可以考虑只留下rootpagenum
    char* name;
    char** cols;   // 列字段
    int ncol; // 列字段数量
} Table;

// 
Table* init_table(uint32_t root_pgnum, char* name, char** cols, int ncol);

#endif // TABLE_H
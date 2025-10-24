#include "btree.h"

// TODO 目前是固定字段
typedef  struct {
    int id;    // 日志时间
    char* name;
    char* addr;
} Row;


typedef struct  {
    BTree* tree;    // 对应一个tree
    Row* row; // 表行格式
} Table;


// TODO 临时，仅支持单表
extern Table * g_table;

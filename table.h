/**
 * table对btree的rowdata作出解释。
 * - 字段名字、顺序、类型
 * - 
 */
#ifndef TABLE_H
#define TABLE_H
#include <stdbool.h>
#include "btree.h"

// 存储表的元信息，不存储数据，来自启动时候master读取
// 属于静态结构，不像pager那样动态操作，
typedef enum  {
    COL_INT,
    COL_STRING,
} ColumnType;
typedef struct {
    char* name;
    ColumnType type;
    // 其他列属性，如顺序index
    int index;  // 
} Column;
typedef struct  {
    BTree* tree;    // 对应一个tree 。 可以考虑只留下rootpagenum
    char* name;
    Column* columns;
    int ncol; // 列字段数量
} Table;

// 
void free_table(Table* table);

// 通过列名，返回对应index
int table_get_column_index(Table* tab, char* name);

// TODO 从vdbe中拆出， 涉及 列顺序、缺省列、拼接
// 需要生成/解释 一行bytes数据 <len1><data1><len2><data2>..
// select 一行
// insert 一行
// select all

char* table_select(Table* tab, uint8_t bytes, int nb, int cols[], int ncol);


// TODO 对操作进行验证，如insert字段存在

#endif // TABLE_H
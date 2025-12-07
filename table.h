/**
 * table对btree的rowdata作出解释。
 * - 字段名字、顺序、类型
 * - 
 */
#ifndef TABLE_H
#define TABLE_H
#include <stdbool.h>
#include "btree.h"
#include "sql4limit.h"


// GO : 关于<len><data> 重新开发为 <type><len><data>， 加入type字段后，大幅减少自己的处理逻辑，更简洁安全
//      1. 不需要获取tab元信息也能正确解析数据

// 对于btree中celldata，与table行对应：生成、转换、解释、用户输出。
// 因此对于不同的操作，我们给予不同结构体来标识。
// btree-celldata：bytes[CELL_DATA_SIZE] -> RawRow: <len><data><len>... 
// 有类型的行/字段data：TypedRow: {TypedCell, ...}，  TypedCell: { type, union data} 
// 用户输出行/字段： UserRow {char* display[], ncol}

typedef struct {
    uint8_t bytes[ROW_SIZE];
    int nbyte;
} RawRow;

// 存储表的元信息，不存储数据，来自启动时候master读取
// 属于静态结构，不像pager那样动态操作，
typedef enum  {
    COL_INT,
    COL_STRING,
    COL_BLOB,
} ColumnType;
typedef struct {
    char* name;
    ColumnType type;
    // 其他列属性，如顺序index
    int index;  // 
} Column;

typedef struct {
    ColumnType type;
    union {
        int i;
        char* text;
        // blob
        struct {
            uint8_t* ptr;
            int len;
        } blob;
    } v;
} TypedCell;

typedef struct {
    TypedCell cells[MAX_COL];   //
    int ncell; // 对于一个table， 这里一个row的ncell是确定的
} TypedRow;

// 对于typedrow（真实存储）进行逻辑转换：列排序、默认值缺失
typedef struct {
    TypedCell cells[MAX_COL];   //
    int ncell; // 这里是不固定的， 比如select, insert 部分字段，
} LogicRow;

// 用户输出row：可以考虑移动到其他文件
typedef struct {
    char* disp[MAX_COL];
    int ncol;
} OutputRow;

typedef struct  {
    BTree* tree;    // 对应一个tree 。 可以考虑只留下rootpagenum
    char* name;
    Column* columns;
    int ncol; // 列字段数量
} Table;


RawRow* bytes_to_rawrow(uint8_t bytes[], int nbyte);
// 对于原始bytes, 我们需要知道. 每个cell对应哪个column 得到type
TypedRow* rawrow_to_typedrow(Table* tabmeta, RawRow* rrow);
// typedrow -> logicrow
LogicRow* typderow_to_logicrow(Table* tabmeta, TypedRow* trow);
OutputRow* logicrow_to_outputrow(LogicRow* logrow);
OutputRow* bytes_to_outputrow(Table* tabmeta, uint8_t bytes[], int nbyte);

// 为了避免每次for i （execute column）， 我们使用rawlayout一次完成，对于一行数据.
typedef struct {
    int ncol;
    int offsets[MAX_COL]; // 每个字段data的偏移
    int sizes[MAX_COL]; // 每个字段len
} RawRowLayout;



// 
void free_table(Table* table);

// 通过列名，返回对应index
int table_get_column_index(Table* tab, char* name);

// TODO 从vdbe中拆出， 涉及 列顺序、缺省列、拼接
// 需要生成/解释 一行bytes数据 <len1><data1><len2><data2>..
// select 一行
// insert 一行
// select all

// char* table_select(Table* tab, uint8_t bytes, int nb, int cols[], int ncol);


// TODO 对操作进行验证，如insert字段存在

#endif // TABLE_H
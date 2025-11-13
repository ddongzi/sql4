#include "table.h"
#include "sql4code.h"
int table_get_column_index(Table* tab, char* name)
{
  // 查找列名对应元信息的列index
    for (size_t j = 0; j < tab->ncol; j++)
    {
        Column col = tab->columns[j];
        // TODO 或许应该对col的词法分析更多一层，只能是STRING？
        if (strcmp(name, tab->columns[j].name) == 0) {
            return col.index;
        }
    }
    sql4_errno = TABLE_COLUMN_NOT_EXIST_ERR;
    return -1;
}
// 从bytes解析出指定colindex的数据
static char* table_select_column_by_index(Table* tab, uint8_t bytes, int nb, int colidx)
{
    int colk = 0;
    while (condition)
    {
        int len = ?
        // 通用来说，不能区分 len部分占用多少字节，取决于字段类型。 所以必须是 <type><len><data>
        // insert时候需要增加type字段。 目前2字节够用
    }
        
}
/**
 * 将一行bytes数据，解析成table一些字段。 返回 结果字符串
 * 
 * @param cols 列index
 */
char* table_select(Table* tab, uint8_t bytes, int nb, int cols[], int ncol)
{
    char s[256];
    assert(ncol < tab->ncol);
    for (size_t i = 0; i < ncol; i++)
    {
        Column col = tab->columns[cols[i]];
        // 得到col字段数据

    }
        
}
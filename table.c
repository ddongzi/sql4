#include "table.h"
#include "sql4code.h"
#include <string.h>
int table_get_column_index(Table* tab, char* name)
{
  // 查找列名对应元信息的列index
    for (size_t j = 0; j < tab->ncol; j++)
    {
        Column col = tab->columns[j];
        // TODO 或许应该对col的词法分析更多一层，只能是STRING？
        if (strcmp(name, col.name) == 0) {
            return col.index;
        }
    }
    sql4_errno = TABLE_COLUMN_NOT_EXIST_ERR;
    return -1;
}
// 从bytes解析出指定colindex的数据
static char* table_select_column_by_index(Table* tab, uint8_t bytes[], int nbyte, int colidx)
{
    char* res = NULL;
    int colk = 0;
    for (size_t i = 0; i < nbyte; )
    {
        // 通用来说，不能区分 len部分占用多少字节，取决于字段类型。 必须是 <type><len><data>
        // 但是目前而言， 不需要，Len都是2字节够用了。
        // insert时候需要增加type字段。 
        int len = bytes[i] << 8 | bytes[i + 1];
        i += 2;
        if (colk == colidx) {
            res = realloc(res, len + 1);
            memcpy(res, bytes + i, len);
            res[len] = '\0';
            return res;
        }
        i += len;
    }
    sql4_errno = INTERNAL_BAD_ERR;
    return res;        
}


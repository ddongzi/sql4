
#ifndef SQL4LIMIT_H
#define SQL4LIMIT_H
/**
 * In SQL4, size and length are strongly related.
 * We should define them in the same file for proper control.
 */
#define PAGE_SIZE  1024 // page_size(default: 1k)

#define CELL_DATA_SIZE 256
#define ROW_SIZE (CELL_DATA_SIZE)
#define MAX_COL 16 // 最大字段数

#endif

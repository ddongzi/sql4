#include <stdbool.h>
#include "sql4code.h"

typedef struct BTree BTree;
typedef struct cursor_t cursor_t;

pager_t* btree_get_pager(BTree* tree);

cursor_t *btree_cursor_start(BTree *tree);
cursor_t *btree_cursor_find(BTree *tree, uint32_t key);
void btree_cursor_advance(cursor_t *cursor);
uint8_t* btree_cursor_value(cursor_t *cursor);
bool btree_cursor_is_end_of_table(cursor_t* cursor);

SQL4_CODE btree_insert(BTree* tree, uint32_t key, uint8_t* data, size_t datalen);
SQL4_CODE btree_select(BTree* tree, size_t* selectsize, uint8_t** data);
SQL4_CODE btree_delete(BTree* tree, uint32_t key);

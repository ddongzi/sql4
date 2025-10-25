#ifndef BTREE_H
#define BTREE_H

#include <stdbool.h>
#include "pager.h"

#include "sql4code.h"
/* 根据 architecture of sqlite. Backend: btree=>pager=>os*/

/**
 * btree实现是通过偏移量做的，没有常规的真正的树结构体；
 * Btree只是保存btree的基本信息，提供外部接口而已。
 */
typedef struct  {
    uint32_t root_page_num; // B-tree 根节点所在页
    Pager* pager;
}BTree;
/* ============ BTree cursor ============ */
/* 代表在table中的位置， #pagenum#cellnum。  也是Btree 的cell位置
 * 1. 指向表起始和表尾部
 * 2. 通过cursor 进行insert、select、delete、update， search for a ID , then cursor pointing this ID row。
 * */
typedef struct {
    int id; // TODO 编号可以用于多表操作
    BTree *btree;
    uint32_t page_num; //
    uint32_t cell_num;
    bool end_of_table; // 帮助insert
}Cursor ;

BTree* btree_create(uint32_t root_pagenum, Pager* pager);

Cursor *btree_cursor_start(BTree *tree);
Cursor *btree_cursor_find(BTree *tree, uint32_t key);
void btree_cursor_advance(Cursor *cursor);
uint8_t* btree_cursor_value(Cursor *cursor);

SQL4_CODE btree_insert(BTree* tree, uint32_t key, uint8_t* data, size_t datalen);
SQL4_CODE btree_select(BTree* tree, size_t* selectsize, uint8_t** data);
SQL4_CODE btree_delete(BTree* tree, uint32_t key);

void btree_print(BTree * tree);

#endif
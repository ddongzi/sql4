
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "db.h"
#include "btree.h"
/*
 * 1. 缓存刷新到盘
 * 2. 关闭db file
 * 3. 释放table， pager
 * */
void db_close(Table *table)
{
    printf("db closing..\n");
    Pager *pager = table->tree->pager;
    assert(pager);
    assert(table);

    for (uint32_t i = 0; i < pager->num_pages; ++i) {
        if (pager_get_page(pager, i) == NULL) {
            continue;
        }
        pager_flush(pager, i);
        pager_free_page(pager, i);
    }
    free(pager);
    free(table);
    printf("db closed.\n");
}
/* db初始化
 * 1. 打开db file
 * 2. 初始化pager
 * 3. 初始化table
 * 4. 初始化page0 ， 根节点
 * */
Table *db_open(const char *file_name)
{
    Pager *pager = pager_open(file_name);
    Table *table = (Table *) malloc(sizeof(Table));
    table->tree = btree_create(0,pager); 
    // TODO 没必要吗？ 但是原来main分支代码有
    // if (pager->num_pages == 0) {
    //     // 分配page0 , 初始化根节点
    //     ?
    //     leaf_node_t *root_node = (leaf_node_t*)pager_get_page(pager, 0);
    //     initialize_leaf_node(root_node, 0);
    //     root_node->meta.is_root = 1;

    // }
    return table;
}



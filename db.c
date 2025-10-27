#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "sql4code.h"
#include "db.h"
#include "btree.h"
DB* g_db = NULL; // 目前仅支持一个库
Table* db_get_table(DB* db, char* name)
{
    if (strcmp(name, "master") == 0) {
        return db->master;
    }
    for (int i = 0; i < db->ntab; i++)
    {
        if (strcmp(db->tabs[i]->name, name) == 0) {
            return db->tabs[i];
        }
    }
    sql4_errno = TABLE_NOT_EXIST_ERR;
    return NULL;
}

/*
 * 1. 缓存刷新到盘
 * 2. 关闭db file
 * 3. 释放table， pager
 * */
void db_close(DB* db)
{
    printf("db closing..\n");
    Pager *pager = db->pager;
    assert(pager);

    for (uint32_t i = 0; i < pager->num_pages; ++i) {
        if (pager_get_page(pager, i) == NULL) {
            continue;
        }
        pager_flush(pager, i);
        pager_free_page(pager, i);
    }
    free(pager);
    printf("db closed.\n");
}
// 初始化db的table master结构
void init_master(DB* db)
{
    Table* master = malloc(sizeof(Table));
    // type(16) name(16) tbl_name(16) root_page(4) sql(128)  
    master->ncol = 5;
    master->cols = malloc(sizeof(char*) * master->ncol);
    master->cols[0] = "type";
    master->cols[1] = "name";
    master->cols[2] = "tbl_name";
    master->cols[3] = "root_pagenum";
    master->cols[4] = "sql";
    master->tree = btree_get(0, db->pager);
    db->master = master;
    printf("init master metainfo\n");
}
/* db初始化
 * 1. 打开db file
 * 2. 初始化pager
 * 3. 初始化table
 * 4. 初始化page0 ， 根节点
 * */
DB* db_open(const char *file_name)
{
    DB* db = malloc(sizeof(DB));
    Pager *pager = pager_open(file_name);

    db->pager = pager;
    printf("(debug) pager open ok.\n");

    // page0 是master页
    // master 元信息内存结构。
    init_master(db);
    if (pager->num_pages == 0) {
        // db空的
        printf("db is empty.\n");
        db->ntab = 0;
        db->tabs = NULL;
    } else {
        // db不空。 
        BTree* tree = btree_get(0, pager);
        printf("(debug) btree page0 ok.\n");
        size_t ntabs;
        uint8_t** data;
        btree_select(tree, &ntabs, data);
        printf("(debug) btree select page0 ok.\n");
        db->tabs = malloc(sizeof(Table*) *ntabs);
        printf("Db open, master load %ld tabs\n", ntabs);
        for (size_t i = 0; i < ntabs; i++)
        {
            // 先固定长度，仅仅支持字段类型定长
            // type(16) name(16) tbl_name(16) root_page(4) sql(128)  
        }
    }

    



    // if (pager->num_pages == 0) {
    //     // 分配page0 , 初始化根节点
    //     ?
    //     leaf_node_t *root_node = (leaf_node_t*)pager_get_page(pager, 0);
    //     initialize_leaf_node(root_node, 0);
    //     root_node->meta.is_root = 1;

    // }
    return db;
}



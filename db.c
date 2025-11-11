#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "sql4code.h"
#include "db.h"
#include "btree.h"
#include "table.h"

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
void free_table(Table* table)
{
    for (size_t i = 0; i < table->ncol; i++)
    {
        free(table->cols[i]);
    }
    free(table);
}

/*
 * 1. 缓存刷新到盘
 * 2. 关闭db file
 * 3. 释放table， pager
 * */
void db_close(DB* db)
{
    printf("db closing..\n");
    pager_free(db->pager);
    free_table(db->master);
    for (size_t i = 0; i < db->ntab; i++)
    {
        free_table(db->tabs[i]);
    }
    free(db);
    printf("db closed.\n");
}
// 初始化db的table master结构
void init_master(DB* db)
{
    Table* master = malloc(sizeof(Table));
    // type name tbl_name root_page sql  
    master->ncol = 5;
    master->cols = malloc(sizeof(char*) * master->ncol);
    // 为了table结构一致，后续易free， cols[i] 使用堆上内存，而非字符串常量
    master->cols[0] = strdup("type");
    master->cols[1] = strdup("name");
    master->cols[2] = strdup("tbl_name");
    master->cols[3] = strdup("root_pagenum");
    master->cols[4] = strdup("sql");
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
        size_t ntabs;
        uint8_t** data = NULL;
        btree_select(tree, &ntabs, &data);
        db->tabs = malloc(sizeof(Table*) *ntabs);
        db->ntab = ntabs;
        printf("Db open, master load %ld tabs\n", ntabs);
        printf("Table master:\n[id] | name | root_pagenum | cols\n");
        for (size_t i = 0; i < ntabs; i++)
        {
            // 解析表的元信息
            // type name tbl_name root_page sql
            db->tabs[i] = malloc(sizeof(Table));
            Table* tab = db->tabs[i];
            uint8_t* tabmeta = data[i];

            int k = 0;
            int typelen = tabmeta[k] << 8 | tabmeta[k + 1];
            k += 2 + typelen;

            int namelen = tabmeta[k] << 8 | tabmeta[k + 1];
            k += 2;
            tab->name = malloc(namelen + 1);
            memcpy(tab->name, tabmeta + k, namelen);
            tab->name[namelen] = '\0';
            k += namelen;
            // 暂时没有用
            int tblnamelen = tabmeta[k] << 8 | tabmeta[k + 1];
            k += 2;
            k += tblnamelen;

            int root_pagenum_len = tabmeta[k] << 8 | tabmeta[k + 1];
            assert(root_pagenum_len == 4);
            k += 2;
            int root_pagenum = (tabmeta[k] << 24) | (tabmeta[k + 1] << 16) 
                | (tabmeta[k + 2] << 8) | (tabmeta[k + 3]);
            tab->tree = btree_get(root_pagenum, db->pager);
            k += 4;

            // 从sql中解析列名
            int sql_len = (tabmeta[k] << 8) | (tabmeta[k + 1]);
            k += 2;
            char* sql = malloc(sql_len + 1);
            memcpy(sql, tabmeta + k, sql_len);
            sql[sql_len] = '\0';
            char* start = strchr(sql, '(');
            char* end = strchr(sql, ')');
            tab->cols = NULL;
            tab->ncol = 0;
            if (start && end && end > start) {
                char cols[256];
                size_t len = end - start - 1;
                strncpy(cols, start + 1, len);
                cols[len] = '\0';
                // 拆分
                char* token = strtok(cols, ",");
                while (token)
                {
                    tab->ncol ++;
                    tab->cols = realloc(tab->cols, tab->ncol * sizeof(char*));
                    tab->cols[tab->ncol - 1] = strdup(token);
                    token = strtok(NULL, ",");
                }
            }
            
            // print table meta
            printf("[%ld] | %s | %d | cols: ", i, tab->name, tab->tree->root_page_num);
            for (size_t i = 0; i < tab->ncol; i++)
            {
                printf("%s ", tab->cols[i]);
            }
            free(tabmeta);
            printf("\n");
        }
        free(data);
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



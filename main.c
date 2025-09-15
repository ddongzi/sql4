//
// Created by dong on 4/25/24.
//
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>

#define FIELD_SIZE(type, field) sizeof(((type*)0)->field)
#define FIELD_OFFSET(type, field) offsetof(type, field)

/* 与getline交互*/
typedef struct {
    char *buffer;
    size_t buffer_length;
    size_t input_length;
} input_buffer_t;

/*源命令 结果*/
typedef enum {
    META_COMMAND_SUCCESS, // 源命令识别成功
    META_COMMAND_UNRECOGNIZED_COMMAND // 。开始， 但是命令不存在
} meta_cmd_res;

/*sql命令 结果*/
typedef enum {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_SYNTAX_ERROR,
    PREPARE_STRING_TOO_LONG,
} prepare_res_type;

/*sql 命令类型*/
typedef enum {
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_DELETE
} statement_type;

#define COLUMN_SOURCE_SIZE 32
#define COLUMN_CONTENT_SIZE 128
typedef struct {
    time_t time;    // 日志时间
    char source[COLUMN_SOURCE_SIZE + 1];    // 日志来源
    char content[COLUMN_CONTENT_SIZE + 1];  // 日志内容
} row_t;

typedef struct {
    statement_type type;
    row_t row;
} sql_statement;

// 命令执行结果
typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
    EXECUTE_DUPLICATE_KEY,
    EXECUTE_UNKNOWN_ERR
} execute_res;


void serialize_row(row_t* source, void* destination);

/* B+Tree  原配置*/
#define BTREE_M 3 // TODO 暂时没有用，因为叶子节点没有限制
#define PAGE_SIZE  1024 // 每个页  个字节
#define BTREE_HEIGHT 4  // 三层 0，1，2， 3
#define TABLE_MAX_PAGES 160 // 根据M和Height等比求和
#define INTERNAL_NODE_MAX_CELLS BTREE_M // 
#define INTERNAL_NODE_MIN_CELLS 1 // 最小为1个cell，两个孩子
#define LEAF_NODE_MAX_CELLS BTREE_M


#define INVALID_PAGE_NUM UINT8_MAX  // internal node为空节点 : right child page_num 为 INVALID_PAGE_NUM



/*使用 pager 访问页缓存和文件*/
typedef struct {
    int fd;
    uint32_t file_length; // 追加写。 写的起始位置. 是page_size 整数倍
    uint32_t num_pages; // 页个数
    void *pages[TABLE_MAX_PAGES];
} pager_t;

/* 单表 : page数组*/
typedef struct {
    uint32_t root_page_num; // B-tree 根节点所在页
    pager_t* pager;
} table_t;

/* 代表在table中的位置， #pagenum#cellnum
 * 1. 指向表起始和表尾部
 * 2. 通过cursor 进行insert、select、delete、update， search for a ID , then cursor pointing this ID row。
 * */
typedef struct {
    table_t *table;
    uint32_t page_num; //
    uint32_t cell_num;
    bool end_of_table; // 帮助insert
} cursor_t;

/* 对应关系： 一个节点对应一个页。 根节点头部存储子节点page_num来定位。*/

#define NODE_INTERNAL 0
#define NODE_LEAF 1

/* ============ Common header layout ============*/
/*
 * | Node Type (uint8_t)      | 1 byte    | 0表示内部节点，1表示叶子节点
 * | Is Root (uint8_t)        | 1 byte    | 
 * | Parent Pointer (uint32_t)| 1 byte   | root节点parent是Invalid_page_num
 * | Page num                 | 1 byte   | 
 * | right_page_num             | 1 byte   | 叶子/内部 节点
 * | left_page_Num             | 1 byte   | 
 * |--------------------------------------|
 * | Total Header Size        | 4 bytes   |
 * =========================================
 */
typedef struct {
    uint8_t node_type;
    uint8_t is_root;
    uint8_t parent_page_num; // TODO 肯定不够，大约需要14位以上标识
    uint8_t page_num;
} common_header_t;

/* ============ Leaf Node Structure ============ */
/* 
 * ============ Leaf Node Header Layout ============ 
 * | Number of Cells                  | 1 byte    | 设置有效cell
 * ============ Leaf Node Body Layout #0 cell ====== 
 * | Key                              | 4 bytes    | 
 * | Value (ROW_SIZE)                 | ROW_SIZE   |
 * ============ Leaf Node Body Layout #1 cell ====== 
 * | Key                              | 4 bytes    | 
 * | Value (ROW_SIZE)                 | ROW_SIZE   |
 * |     .....................................     |
 * ================================================
 * | Space for Cells                  | PAGE_SIZE - 14 bytes |
 */

/* 叶子满节点分裂时候，分一半给新兄弟叶子*/
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;
const uint32_t LEAF_NODE_MIN_CELLS = LEAF_NODE_RIGHT_SPLIT_COUNT; // 最小cell数

typedef struct {
    uint32_t key;
    row_t row;
} leaf_node_cell_t;
typedef struct {
    common_header_t meta;

    uint8_t next_leaf_page_num;
    uint8_t num_cells;
    leaf_node_cell_t cells[LEAF_NODE_MAX_CELLS];
    unsigned char padding[PAGE_SIZE - sizeof(common_header_t) -3 * sizeof(uint8_t) - sizeof(leaf_node_cell_t)]; // 填充剩余page空间
} leaf_node_t;

cursor_t *leaf_node_find(table_t *table, uint8_t page_num, uint32_t key);

/* ============ Internal Node Structure ============ */
/* 
 *  ============ Internal Node Header Layout  ====== 
 * | Number of Keys        | 1 bytes    | 
 * | Right Child Pointer     | 1 bytes    | 
 *  ============ Internal Node Body Layout #0 cell ====== 
 * | Child Pointer        | 1 bytes    | 
 * | Key (uint32_t)                  | 4 bytes    | 孩子节点树key最值
 *  ============ Internal Node Body Layout #1 cell ====== 
 * | Child Pointer        | 1 bytes    | 
 * | Key (uint32_t)                  | 4 bytes    | 
 */


typedef struct {
    uint8_t child_page_num;
    uint32_t key;
} internal_node_cell_t;
typedef struct {
    common_header_t meta;

    uint8_t num_cells;
    uint8_t right_child_page_num;
    internal_node_cell_t cells[INTERNAL_NODE_MAX_CELLS];
    unsigned char padding[PAGE_SIZE - sizeof(common_header_t)- 2 * sizeof(uint8_t) -sizeof(leaf_node_cell_t)];
} internal_node_t;

typedef union {
    internal_node_t internal;
    leaf_node_t leaf;
}node_t;

void create_new_root(table_t *table, uint8_t right_child_page_num);
uint8_t internal_node_child(internal_node_t* node, uint8_t cell_num);
cursor_t* internal_node_find(table_t *table, uint8_t page_num, uint32_t key);
void internal_node_insert(table_t * table, uint8_t parent_page_num, uint8_t child_page_num);
void internal_node_split_and_insert(table_t * table, uint8_t parent_page_num, uint8_t child_page_num);
void initialize_internal_node(internal_node_t *node, uint8_t page_num);
void update_internal_node_key(internal_node_t *node, uint32_t old_key, uint32_t new_key);
uint8_t internal_node_find_child(internal_node_t* node, uint32_t key);
uint8_t* node_parent(node_t* node);


/* 在表中根据key找到 内部节点或者叶子节点， cursor*/
cursor_t *table_find(table_t *table, uint32_t key);
void *get_page(pager_t* pager, uint8_t page_num);
uint8_t get_unused_page_num(pager_t *pager);

void print_constants() ;
void indent(uint32_t level);  // 递归调用表现tab
void print_tree(pager_t * pager, uint8_t page_num, uint32_t indentation_level); // 打印tree
void print_table(table_t *table);

void leaf_node_split_and_insert(cursor_t *cursor, uint32_t key, row_t *val);

/**
 * @brief 父亲pagenum
 * 
 * @param node 
 * @return uint8_t* 
 */
uint8_t* node_parent(node_t *node)
{
    common_header_t *meta = (common_header_t *)node;
    return &meta->parent_page_num;
}

uint8_t get_node_type(node_t *node)
{
    common_header_t *meta = (common_header_t *)node;
    return node->internal.meta.node_type;
}
void set_node_type(node_t *node, uint8_t type)
{
    common_header_t *meta = (common_header_t *)node;
    meta->node_type = type;
}

void set_node_root(node_t *node, uint8_t is_root)
{
    common_header_t *meta = (common_header_t *)node;
    meta->is_root = is_root;
}

/**
 * @brief 获取node在parent的cellNUm
 * 
 * @param table 
 * @param node 
 * @return int ： 如果是最右结点，返回numcells表示
 */
uint8_t get_cellnum_in_parent(table_t *table, node_t *node)
{
    uint8_t parent_page_num = *node_parent(node);
    if (parent_page_num == INVALID_PAGE_NUM) perror("error, pagenum invalid");

    internal_node_t *parent = (internal_node_t *)get_page(table->pager, parent_page_num);
    
    if (parent->right_child_page_num == ((common_header_t *)node)->page_num) 
        return parent->num_cells;
    uint8_t i;
    for ( i = 0; i < parent->num_cells; i++) {
        if (parent->cells[i].child_page_num == ((common_header_t*)node)->page_num) {
            return i;
        }
    }
    perror("unkown .");
    return -1;
}

/**
 * @brief 获取左兄弟的pagenum,  不允许跨父
 * 
 * @param node 
 * @return uint8_t 
 */
uint8_t get_left_page_num(table_t *table, node_t *node)
{
    uint8_t parent_page_num = *node_parent(node);
    if (parent_page_num == INVALID_PAGE_NUM) perror("error, pagenum invalid");

    internal_node_t *parent = (internal_node_t *)get_page(table->pager, parent_page_num);
    
    if (parent->right_child_page_num == ((common_header_t *)node)->page_num) 
        return parent->cells[parent->num_cells - 1].child_page_num;
    
    uint8_t i;
    for ( i = 0; i < parent->num_cells; i++) {
        if (parent->cells[i].child_page_num == ((common_header_t*)node)->page_num) {
            break;
        }
    }
    if (i > 0) 
        return parent->cells[i - 1].child_page_num;
    if (i == 0) 
        return INVALID_PAGE_NUM;
    perror("unkown .");
    return INVALID_PAGE_NUM;
}

uint8_t get_right_page_num(table_t *table, node_t *node)
{
    uint8_t parent_page_num = *node_parent(node);
    if (parent_page_num == INVALID_PAGE_NUM) perror("error, pagenum invalid");

    internal_node_t *parent = (internal_node_t *)get_page(table->pager, parent_page_num);
    
    if (parent->right_child_page_num == ((common_header_t *)node)->page_num) 
        return INVALID_PAGE_NUM;
    
    uint8_t i;
    for ( i = 0; i < parent->cells; i++) {
        if (parent->cells[i].child_page_num == ((common_header_t*)node)->page_num) {
            break;
        }
    }
    if (i < parent->num_cells - 1) 
        return parent->cells[i + 1].child_page_num;
    if (i == parent->num_cells - 1) 
        return parent->right_child_page_num;
    
    perror("unkown .");
    return INVALID_PAGE_NUM;
}


/**
 * @brief 返回节点（树）的最大key
 *      如果是叶子节点：获取最右边元素的key
 *      如果是内部节点：递归向右获取
 * @param pager 
 * @param node 
 * @return uint32_t 
 */
uint32_t get_node_max_key(pager_t *pager, node_t *node)
{
    if (get_node_type(node) == NODE_LEAF) {
        return node->leaf.cells[node->leaf.num_cells - 1].key;
    }

    node_t* right_child = (node_t *)get_page(pager, node->internal.right_child_page_num);
    return get_node_max_key(pager, right_child);
}



/**
 * @brief 获取内部节点#cell_num的pagenum
 * 
 * @param node : 内部节点
 * @param cell_num : cell_num=num_cells 时候，表示获取最右孩子
 * @return uint8_t : 有效的pagenum 
 */
uint8_t internal_node_child(internal_node_t* node, uint8_t cell_num)
{
    uint32_t num_keys = node->num_cells;

    if (cell_num > num_keys) {
        printf("Tried to access child_num %d> num_keys %d\n", cell_num, num_keys);
        exit(EXIT_FAILURE);
    } else if (cell_num == num_keys) {
        if (node->right_child_page_num == INVALID_PAGE_NUM) {
            printf("Tried to access right child of node , but was invalid page.\n");
            exit(EXIT_FAILURE);
        }
        return node->right_child_page_num;
    } else {

        if (node->cells[cell_num].child_page_num  == INVALID_PAGE_NUM) {
            printf("Tried to access child %d of node , but was invalid page.\n", cell_num);
            exit(EXIT_FAILURE);
        }
        return node->cells[cell_num].child_page_num  ;
    }
}

/**
 * @brief 在内部节点中，找到包含key所在节点对应的#cell_num
 * 
 * @param node 
 * @param key 
 * @return uin8_t : 不论是否找得到==key的节点，返回的#cell_num一定是key所在节点（树）对应的
 * @warning 不涉及最右孩子pointer
 */
uint8_t internal_node_find_child(internal_node_t* node, uint32_t key)
{
    uint32_t num_keys = node->num_cells;

    uint32_t low = 0;
    uint32_t high = num_keys;
    uint32_t mid, mid_key;
    while (low < high) {
        mid = (low + high) / 2;
        mid_key = node->cells[mid].key;
        if (mid_key >= key) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return low;
}
/**
 * @brief 在内部节点内，递归找到key所在的#pagenum#cellnum(叶子节点)
 * 
 * @param table 
 * @param page_num 
 * @param key 
 * @return cursor_t* 
 * @warning 
 */
cursor_t* internal_node_find(table_t *table, uint8_t page_num, uint32_t key)
{
    internal_node_t *node = (internal_node_t *)get_page(table->pager, page_num);

    // 找到key所在节点树
    uint8_t child_cell_num = internal_node_find_child(node, key);
    uint8_t child_page_num = internal_node_child(node, child_cell_num);

    node_t *child = (node_t *)get_page(table->pager, child_page_num);
    switch (get_node_type(child)) {
        case NODE_LEAF:
            return leaf_node_find(table, child_page_num, key);
        case NODE_INTERNAL:
            internal_node_find(table, child_page_num, key);
            break;
        default:
            perror("unkown node type");
            return NULL;
    }
}
/**
 * @brief parent内部节点已满，需要分裂parent节点，插入cell指向#child_page_num
 * 
 * @param table 
 * @param parent_page_num 
 * @param child_page_num 
 */
void internal_node_split_and_insert(table_t * table, uint8_t parent_page_num, uint8_t child_page_num)
{
    uint8_t old_page_num = parent_page_num;
    internal_node_t * old_node = (internal_node_t *)get_page(table->pager, old_page_num);
    uint32_t old_max = get_node_max_key(table->pager, (node_t*)old_node);

    node_t* child =  (node_t *)get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table->pager, child);

    uint8_t new_page_num = get_unused_page_num(table->pager);

    uint8_t splitting_root = old_node->meta.is_root; // 记录被分裂节点是否是根节点， 如果是根节点，还需要创建root
    
    internal_node_t* parent;
    internal_node_t* new_node;

    /* 创建新节点，与parent建联 */
    if (splitting_root) {
        // 如果没有parent，内部创建一个新root，并且新节点作为他的右边孩子。
        create_new_root(table, new_page_num);
        parent = (internal_node_t *)get_page(table->pager, table->root_page_num); 
        // 重建root后，重新获取节点
        old_page_num = internal_node_child(parent, 0); 
        old_node =(internal_node_t *)get_page(table->pager, old_page_num); 
        new_node = (internal_node_t *)get_page(table->pager, new_page_num);
    } else {
        // 如果有parent, 把新节点与parent建联即可。
        parent = (internal_node_t *)get_page(table->pager, old_node->meta.parent_page_num);
        new_node = (internal_node_t *)get_page(table->pager, new_page_num);

        initialize_internal_node(new_node, new_page_num);
        new_node->meta.parent_page_num = old_node->meta.parent_page_num;
    }

    /* 原节点分裂到一部分到新节点。*/
    // 1. 先将最右孩子给 新节点
    uint32_t old_num_keys = old_node->num_cells;
    uint8_t cur_page_num = old_node->right_child_page_num; // 先指向最右侧孩子
    node_t* cur_node = (node_t* )get_page(table->pager, cur_page_num);

    internal_node_insert(table, new_page_num, cur_page_num); //
    *node_parent(cur_node) = new_page_num;
    old_node->right_child_page_num = INVALID_PAGE_NUM; // 原来page 最右侧孩子指向空

    // 2. 挪一半cells给新节点
    for (int i = INTERNAL_NODE_MAX_CELLS - 1; i > INTERNAL_NODE_MAX_CELLS/2; --i) {
        cur_page_num = old_node->cells[i].child_page_num;
        internal_node_insert(table, new_page_num, cur_page_num);

        cur_node = (node_t *)get_page(table->pager, cur_page_num);
        *node_parent(cur_node) = new_page_num;

        old_node->num_cells--;
    }

    // 3. 设置原node最右边cell为rightchild，并从cell中移除
    old_node->right_child_page_num = old_node->cells[old_node->num_cells - 1].child_page_num;
    old_node->num_cells--;

    /* 插入cell操作*/
    // 4. 新插入节点进行关联
    uint32_t max_after_split = get_node_max_key(table->pager, (node_t*)old_node);
    uint8_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

    internal_node_insert(table, destination_page_num, child_page_num);
    *node_parent(child) = destination_page_num;

    /* 更新原节点、新节点和父亲节点cell */
    // 5. 
    update_internal_node_key(parent, old_max, get_node_max_key(table->pager, (node_t*)old_node)); // 更新父节点
    if (!splitting_root) {
        // 如果原节点是根节点，新节点是作为新root的rightchild，不需要更新cell. 否则要更新
        internal_node_insert(table, parent->meta.page_num, new_page_num);
    }

}


void update_internal_node_key(internal_node_t *node, uint32_t old_key, uint32_t new_key)
{
    uint8_t cell_num = internal_node_find_child(node, old_key);
    node->cells[cell_num].key = new_key;
}

/**
 * @brief 内部parent节点插入一个cell，指向childpagenum
 * 
 * @param table 
 * @param parent_page_num :该parent插入
 * @param child_page_num : 
 */
void internal_node_insert(table_t * table, uint8_t parent_page_num, uint8_t child_page_num)
{
    internal_node_t *parent =(internal_node_t *) get_page(table->pager, parent_page_num);
    node_t *child = (node_t *)get_page(table->pager, child_page_num);

    uint32_t child_max_key = get_node_max_key(table->pager, child);
    uint32_t index = internal_node_find_child(parent, child_max_key); // 要插入的位置，插入max_key

    uint32_t original_num_keys = parent->num_cells;

    if (original_num_keys >= INTERNAL_NODE_MAX_CELLS) {
        // 1. 当前parent节点容量超了， 需要分割 parent节点处理
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = parent->right_child_page_num;

    if (right_child_page_num == INVALID_PAGE_NUM) {
        // 2. parent为空节点 : 设置right child 即可
        parent->right_child_page_num = child_page_num;
        return;
    }

    // 3. parent节点不为空也不满
    node_t *right_child = (node_t *)get_page(table->pager, right_child_page_num);
    parent->num_cells = original_num_keys + 1;

    if (child_max_key > get_node_max_key(table->pager, right_child)) {
        // 3.1 如果插入节点大于最右侧孩子， 代替最右孩子字段， 将最右孩子写到内部节点最后中
        parent->cells[original_num_keys].child_page_num = right_child_page_num;
        parent->cells[original_num_keys].key = get_node_max_key(table->pager, right_child);
        parent->right_child_page_num = child_page_num;
    } else {
        // 3.2 插入节点要插入cells中，插入位置index
        for (uint32_t i = original_num_keys; i > index; --i) {
            memcpy(&parent->cells[i], &parent->cells[i - 1], sizeof(internal_node_cell_t));
        }
        parent->cells[index].child_page_num = child_page_num;
        parent->cells[index].key = child_max_key;
    }
}


/**
 * @brief 内部节点安全移除一个cell
 * 
 * @param cursor 
 * @param cell_num 
 */
void internal_node_remove_cell(table_t* table, internal_node_t *node, uint8_t cell_num)
{
    uint32_t old_max_key = get_node_max_key(table->pager, (node_t*)node);
    // 向前覆盖
    for (size_t i = cell_num + 1; i < node->num_cells; i++) {
        memcpy(&node->cells[i - 1], &node->cells[i], sizeof(internal_node_cell_t));
    }
    memset(&node->cells[node->num_cells], 0, sizeof(internal_node_cell_t));
    node->num_cells--;

    if (cell_num == node->num_cells - 1) {
        // 需要更新parent中的值
        if (node->meta.parent_page_num != INVALID_PAGE_NUM) {
            internal_node_t* parent = (internal_node_t*)get_page(table->pager, node->meta.parent_page_num);
            update_internal_node_key(parent, old_max_key, get_node_max_key(table->pager, (node_t*)node));
        }
    }
    
}


/**
 * @brief 合并内部兄弟结点，移除cellnum
 * @param node 
 * @param merge_to_left 
 */
void internal_node_merge_and_remove(table_t *table, internal_node_t *node,uint8_t cell_num, bool merge_to_left)
{
    internal_node_t *merge_node = NULL;
    uint32_t merge_node_old_max_key;
    pager_t* pager = table->pager;

    // 断链
    node->cells[cell_num].child_page_num = INVALID_PAGE_NUM;

    // 如果往左合并，需要向右摆动：前结点最右孩子作为第一个孩子。
    // 如果往右合并，需要向左摆动

    if (merge_to_left) {
        merge_node = (internal_node_t*)get_page(table->pager, get_left_page_num(table, node));
        merge_node_old_max_key = get_node_max_key(pager, merge_node);

        // 向右摆动
        for (size_t i = cell_num; i > 0; i--) {
            memcpy(&node->cells[i], &node->cells[i - 1], sizeof(internal_node_cell_t));
        }
        node->cells[0].child_page_num = merge_node->right_child_page_num;
        node_t* right_node = (node_t*)get_page(table->pager, merge_node->right_child_page_num);
        node->cells[0].key = get_node_max_key(table->pager, right_node);
        merge_node->right_child_page_num = node->right_child_page_num;

        // 合并
        memcpy(&merge_node->cells[merge_node->num_cells], &node->cells, node->num_cells * sizeof(internal_node_cell_t));
        
    } else {
        merge_node = (internal_node_t*)get_page(table->pager, get_right_page_num(table, node));
        merge_node_old_max_key = get_node_max_key(pager, merge_node);

        // 向左摆动
        for (size_t i = cell_num; i < node->num_cells - 1; i++) {
            memcpy(&node->cells[i], &node->cells[i + 1], sizeof(internal_node_cell_t));
        }
        node->cells[node->num_cells - 1].child_page_num = node->right_child_page_num;
        node_t* right_node = (node_t*)get_page(table->pager, node->right_child_page_num);
        node->cells[node->num_cells - 1].key = get_node_max_key(table->pager, right_node);
        node->right_child_page_num = INVALID_PAGE_NUM;

        memmove(&merge_node->cells[node->num_cells], merge_node->cells, node->num_cells * sizeof(internal_node_cell_t));
        memcpy(merge_node->cells, node->cells, node->num_cells * sizeof(internal_node_cell_t));
    }
    merge_node->num_cells += node->num_cells;
    
    
    // 更新子节点: 的父亲
    for (size_t i = 0; i < merge_node->num_cells; i++) {
        node_t* child = (node_t*)get_page(pager, merge_node->cells[i].child_page_num);
        *node_parent(child) = merge_node->meta.page_num;
    }
    node_t* right_child = (node_t*)get_page(pager, merge_node->right_child_page_num);
    *node_parent(right_child) = merge_node->meta.page_num;
    

    // 删除父亲结点中的cell
    node_t* parent = (node_t*)get_page(pager, node->meta.parent_page_num);
    internal_node_remove(table, parent, get_cellnum_in_parent(table, (node_t*)node));


}
/**
 * @brief node内部节点移除一个cell
 * 
 * @param node 
 * @param key 
 */
void internal_node_remove(table_t *table, internal_node_t *node, uint8_t cell_num)
{

    // 1. 节点够删除cell
    if (node->num_cells - 1 >= INTERNAL_NODE_MIN_CELLS) {
        // 可以安全移除
        internal_node_remove_cell(table, node, cell_num);
        return;
    } 
    // 如果是根节点,不够删除，即只有1个cell，
    if (node->meta.is_root) {
        common_header_t meta = node->meta;
        node_t *new_root = (node_t*)get_page(table->pager, node->right_child_page_num);
        memcpy(node, new_root, sizeof(node_t));
        memcpy(&node->meta, &meta, sizeof(common_header_t));

        if (cell_num == 0) {
            // 让右孩子作为新的根
            if (get_node_type(new_root) == NODE_INTERNAL) {
                internal_node_t* right_node = &new_root->internal;
                // 右孩子的孩子指向新的父亲
                for (size_t i = 0; i < right_node->num_cells; i++) {
                    node_t *child = (node_t*)get_page(table->pager, right_node->cells[i].child_page_num);
                    *node_parent(child) = meta.page_num;
                }
                node_t* right_r_child =  (node_t*)get_page(table->pager, right_node->right_child_page_num);
                *node_parent(right_r_child) = meta.page_num;
            } else {
                // 如果是叶子结点
                node->meta.node_type = NODE_LEAF;
            }


        } else if (cell_num == 1) {
            // 让左孩子作为新的根
            if (get_node_type(new_root) == NODE_INTERNAL) {
                internal_node_t* left_node = &new_root->internal;
                // 左孩子的孩子指向新的父亲
                for (size_t i = 0; i < left_node->num_cells; i++) {
                    node_t *child = (node_t*)get_page(table->pager, left_node->cells[i].child_page_num);
                    *node_parent(child) = meta.page_num;
                }
                node_t* left_r_child =  (node_t*)get_page(table->pager, left_node->right_child_page_num);
                *node_parent(left_r_child) = meta.page_num;
            } else {
                node->meta.node_type = NODE_LEAF;
            }

        } else {
            perror("unknown root num cells");
        }
        return;

    }


    // 2. 不够，需要从兄弟结点挪
    uint8_t right_page_num = get_right_page_num(table, (node_t*)node);
    uint8_t left_page_num = get_left_page_num(table, (node_t*)node);
    uint8_t parent_page_num = node->meta.parent_page_num;
    internal_node_t* next = NULL;
    internal_node_t* prev = NULL;
    internal_node_t* parent = NULL;
    if (right_page_num != INVALID_PAGE_NUM) next = (internal_node_t*)get_page(table->pager, right_page_num);
    if (left_page_num != INVALID_PAGE_NUM) prev = (internal_node_t*)get_page(table->pager, left_page_num);
    if (parent_page_num != INVALID_PAGE_NUM) parent = (internal_node_t*)get_page(table->pager, parent_page_num);

    uint8_t num_cells = node->num_cells;

    if (next != NULL && next->num_cells - 1 >= INTERNAL_NODE_MIN_CELLS) {
        uint32_t old_max_key = get_node_max_key(table->pager, (node_t*) node);
        // 1. 删除cell
        for (size_t i = cell_num + 1; i < num_cells; i++) {
            memcpy(&node->cells[i - 1], &node->cells[i], sizeof(internal_node_cell_t));
        }
        // 2. 将node右孩子作为next#0cell的child， next#0cell的孩子作为最右
        node->cells[node->num_cells - 1].child_page_num = node->right_child_page_num;
        node_t* temp_node = (node_t*)get_page(table->pager, node->right_child_page_num);
        node->cells[node->num_cells - 1].key = get_node_max_key( table->pager, temp_node);

        node->right_child_page_num = next->cells[0].child_page_num;

        node_t *next_first_child = (node_t*)get_page(table->pager, next->cells[0].child_page_num);
        *node_parent(next_first_child) = node->meta.page_num;

        // 3. 删除next第一个cell
        internal_node_remove_cell(table, next, 0);
        // 4. 更新parent
        if (parent == NULL) printf("need !");
        update_internal_node_key(parent, old_max_key, get_node_max_key(table->pager, (node_t*) node));
        return;
    } 
    if (prev != NULL && prev->num_cells - 1 >= INTERNAL_NODE_MIN_CELLS) {
        uint32_t old_max_key = get_node_max_key(table->pager, (node_t*) prev);

        // 1. node [0..cellNUm] 后移，空出第一个
        for (size_t i = cell_num; i >0; i--) {
            memcpy(&node->cells[i], &node->cells[i - 1], sizeof(internal_node_cell_t));
        }
        // 2. prev结点的最右孩子指向第一个cell
        node->cells[0].child_page_num = prev->right_child_page_num;
        node_t *right_node = (node_t *)get_page(table->pager, prev->right_child_page_num);
        node->cells[0].key = get_node_max_key(table->pager, right_node);

        *node_parent(right_node) = node->meta.page_num;

        // 3. prev结点移除最右孩子
        // 3.1 先移除最后一个cell， 让最后一个cell座位右孩子
        node_t *temp_node = (node_t*)get_page(table->pager, prev->cells[prev->num_cells - 1].child_page_num);
        internal_node_remove_cell(table, prev, prev->num_cells - 1);
        prev->right_child_page_num = ((common_header_t *)temp_node)->page_num;
        update_internal_node_key(parent, old_max_key, get_node_max_key(table->pager, prev));
        return;
    }

    // 3. 兄弟结点不够借，需要合并
    if (next != NULL) {
        internal_node_merge_and_remove(table, node, cell_num, false);
        return;
    }
    if (prev != NULL) {
        internal_node_merge_and_remove(table, node, cell_num, true);
        return;
    }
        

}
/**
 * @brief 初始化内部节点
 * 
 * @param node 
 */
void initialize_internal_node(internal_node_t *node, uint8_t page_num)
{
    // memset(node, '\0', sizeof(internal_node_t));

    node->meta.node_type = NODE_INTERNAL;
    node->meta.is_root = 0;
    node->meta.page_num = page_num;    
    node->meta.parent_page_num = INVALID_PAGE_NUM;

    node->num_cells = 0;
    node->right_child_page_num = INVALID_PAGE_NUM;

}


/* 初始化叶子节点， cell_num 为0*/
void initialize_leaf_node(leaf_node_t *node, uint8_t page_num)
{

    node->meta.node_type = NODE_LEAF;
    node->meta.is_root = 0;
    node->meta.page_num = page_num;
    node->meta.parent_page_num = INVALID_PAGE_NUM;

    node->next_leaf_page_num = INVALID_PAGE_NUM;
    node->num_cells = 0;
}
/**
 * @brief 在叶子节点#pagenum中，查找key的#cell_num，
 * 
 * @param table 
 * @param page_num 
 * @param key 
 * @return cursor_t* 
 */
cursor_t *leaf_node_find(table_t *table, uint8_t page_num, uint32_t key)
{
    leaf_node_t *node = (leaf_node_t *)get_page(table->pager, page_num);
    uint8_t num_cells = node->num_cells;

    cursor_t *cursor = (cursor_t *) malloc(sizeof(cursor_t));
    cursor->table = table;
    cursor->page_num = page_num;

    // 二分搜索
    uint32_t low = 0;
    uint32_t high = num_cells;
    uint32_t mid = 0, mid_key = 0;
    while (low != high) {
        mid = (low + high) / 2;
        mid_key = node->cells[mid].key;
        if (mid_key == key) {
            cursor->cell_num = mid;
            return cursor;
        } else if (mid_key > key) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    // 当前节点没有找到key， 返回正确位置（可insert)
    cursor->cell_num = low;
    return cursor;
}

/* 在cursor位置，插入一行数据 */
void leaf_node_insert(cursor_t *cursor, uint32_t key, row_t *row)
{
    leaf_node_t *node = (leaf_node_t*)get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = node->num_cells;
    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        // 节点满
        leaf_node_split_and_insert(cursor, key, row);
        return;
    }
    if (cursor->cell_num < num_cells) {
        // 【】【cell_num】【】【】 在cell_num位置插入， 有序即向后移动
        for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
             memcpy(&node->cells[i], &node->cells[i - 1], sizeof(leaf_node_cell_t));
        }
    }
    
    node->num_cells += 1;
    node->cells[cursor->cell_num].key = key;
    serialize_row(row, &node->cells[cursor->cell_num].row);

}

/**
 * @brief 叶子节点已满时，需要对叶子节点分裂，然后插入
 * 
 * @param cursor 
 * @param key 
 * @param val 
 * @details 1. 创建一个新next_leaf节点
 *  2.如果分裂节点是根叶子节点，则创建一个新的根内部节点
 *  3.如果分裂节点是非根的叶子节点，则指向父内部节点的insert
 */

void leaf_node_split_and_insert(cursor_t *cursor, uint32_t key, row_t *val)
{
    leaf_node_t *old_node= (leaf_node_t *)get_page(cursor->table->pager, cursor->page_num);
    uint32_t old_max = get_node_max_key(cursor->table->pager, (node_t*)old_node);

    // 1. 创建一个新叶子节点：作为兄弟节点放在右邻居
    uint8_t new_page_num = get_unused_page_num(cursor->table->pager);
    leaf_node_t *new_node = (leaf_node_t *)get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node, new_page_num);
    new_node->meta.parent_page_num = old_node->meta.parent_page_num;
    new_node->next_leaf_page_num = old_node->next_leaf_page_num;
    old_node->next_leaf_page_num = new_page_num;

    // 2. 将每个cell 转移到新位置, 包括新插入cell。更新cell相关元数据
    // old_node为超满节点，此时个数为 LEAF_NODE_MAX_CELLS + 1
    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
        leaf_node_t *destination_node;
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
            destination_node = new_node;
        } else {
            destination_node = old_node;
        }
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        leaf_node_cell_t *destination_cell = &destination_node->cells[i % LEAF_NODE_LEFT_SPLIT_COUNT];

        if (i == cursor->cell_num) {
            serialize_row(val, &destination_cell->row);
            destination_cell->key = key;
        } else if (i > cursor->cell_num) {
            //
            memcpy(destination_cell, &old_node->cells[i - 1], sizeof(leaf_node_cell_t));
        } else {
            memcpy(destination_cell, &old_node->cells[i], sizeof(leaf_node_cell_t));
        }
    }
    old_node->num_cells = LEAF_NODE_LEFT_SPLIT_COUNT;
    new_node->num_cells = LEAF_NODE_RIGHT_SPLIT_COUNT;


    // 2.更新parent。创建新cell指向新叶子；更新原节点对应cell
    if (old_node->meta.is_root) {
        // 原节点是根节点没有parent， 需要创建一个新的parent
        return create_new_root(cursor->table, new_page_num);
    } else {
        // 原节点有parent
        // 1. 更新原节点对应cell中的ket
        uint8_t parent_page_num = old_node->meta.parent_page_num;
        uint32_t new_max = get_node_max_key(cursor->table->pager, (node_t*)old_node);
        internal_node_t *parent = (internal_node_t *)get_page(cursor->table->pager, parent_page_num);
        update_internal_node_key(parent, old_max, new_max);

        // 2. 父亲节点创建新cell指向新叶子节点
        internal_node_insert(cursor->table, parent_page_num, new_page_num);
        return;
    }
}

/**
 * @brief 在叶子节点内部安全移除cell. 
 * 
 * @param node 
 * @param cell_num 
 */
void leaf_node_remove_cell(table_t *table, leaf_node_t *node, uint32_t cell_num)
{
    uint32_t old_max_Key = get_node_max_key(table->pager, (node_t*)node);
    uint8_t num_cells = node->num_cells;
    for (size_t i = cell_num; i < node->num_cells; i++) {
        memcpy(&node->cells[i], &node->cells[i + 1], sizeof(leaf_node_cell_t));
    }
    node->num_cells -= 1;
    memset(&node->cells[node->num_cells], 0, sizeof(leaf_node_cell_t)); 

    if (cell_num == num_cells -1 && node->meta.parent_page_num != INVALID_PAGE_NUM) {
        // 如果移除cell的是最右边的， 并且node不是最右孩子，还需要更新parent cell中的key
        internal_node_t *parent = (internal_node_t*) get_page(table->pager, node->meta.parent_page_num);
        if (parent->right_child_page_num != node->meta.page_num) {
            update_internal_node_key(parent, old_max_Key, get_node_max_key(table->pager, (node_t*)node));
        }
    }
}


/**
 * @brief 释放page
 * 
 * @param node 
 */
void free_node(void *node)
{
    memset(node, 0, PAGE_SIZE);
    // TODO page回收
}

/**
 * @brief 叶子结点需要合并才能删除自己的cell
 *  合并相邻两个叶子节点 : 把Node2的cells追加到node1后面, 删除node2的cell
 * @param node1 
 * @param node2 
 */
leaf_node_t* leaf_node_merge_and_remove(cursor_t* cursor, bool merge_to_left)
{
    table_t *table = cursor->table;
    pager_t* pager = cursor->table->pager;
    uint8_t cell_num = cursor->cell_num;

    leaf_node_t* node = (leaf_node_t* )get_page(cursor->table->pager, cursor->page_num);
    leaf_node_t *merge_node = NULL;
    uint32_t merge_node_old_max_key;
    uint32_t node_old_max_key = get_node_max_key(pager, node);

    // 内部安全移除cell
    leaf_node_remove_cell(table, node, cell_num);
    
    if (merge_to_left) {
        merge_node = (leaf_node_t* )get_page(cursor->table->pager, get_left_page_num(table, (node_t*)node));
        merge_node_old_max_key = get_node_max_key(pager, merge_node);

        // 2. 合并到左边
        memcpy(&merge_node->cells[merge_node->num_cells], node->cells, node->num_cells * sizeof(leaf_node_cell_t));
    } else {
        merge_node = (leaf_node_t* )get_page(cursor->table->pager, get_right_page_num(table, (node_t*)node));
        merge_node_old_max_key = get_node_max_key(pager, merge_node);

        // 1. 

        // 1.先向右腾出位置
        memmove(merge_node->cells + node->num_cells, merge_node->cells, merge_node->num_cells * sizeof(leaf_node_cell_t));
        // 2. 拷贝到merge node
        memcpy(merge_node->cells, node->cells, node->num_cells * sizeof(leaf_node_cell_t));
    }
    merge_node->num_cells += node->num_cells;

    internal_node_t* parent = ( internal_node_t *)get_page(table->pager, node->meta.parent_page_num);
    update_internal_node_key(parent, merge_node_old_max_key, get_node_max_key(table->pager, (node_t*)merge_node));

    /* 更新父亲节点: 删除对应cell*/
    // 1. 如果node是最右孩子，删除mergenode对应cell， 将mergenode设置为右孩子
    if (node->meta.page_num == parent->right_child_page_num) {
        uint8_t parent_remove_cell_num = get_cellnum_in_parent(table, (node_t*)merge_node);
        // node处填充mergenode内容， mergenode无用删除
        memcpy(node->cells, merge_node->cells, merge_node->num_cells * sizeof(leaf_node_cell_t));
        node->num_cells = merge_node->num_cells;

        internal_node_remove(table, parent, parent_remove_cell_num);
        

    } else {
        // 2. 如果node不是最右孩子，直接移除对应cell, 更新node1对应cell
        uint8_t parent_remove_cell_num = get_cellnum_in_parent(table, node);
        internal_node_remove(table, parent,  parent_remove_cell_num );
    } 

    return merge_node;
}
/**
 * @brief 移除#cursor的cell, 
 * 
 * @param cursor 
 */
void leaf_node_remove(cursor_t *cursor)
{   
    leaf_node_t * node = (leaf_node_t*)get_page(cursor->table->pager, cursor->page_num);
    uint32_t node_old_max_key = get_node_max_key(cursor->table->pager, (node_t*)node);
    leaf_node_cell_t *remove_cell = &node->cells[cursor->cell_num];
    uint8_t num_cells = node->num_cells;
    if ((num_cells - 1 ) >= LEAF_NODE_MIN_CELLS || node->meta.is_root) {
        // 1. 不产生下溢，直接删除
        leaf_node_remove_cell(cursor->table, node, cursor->cell_num);
        return;
    } else {
        // 2. 产生下溢出: 
        // 2.1 向兄弟节点借最邻近的cell
        uint8_t prev_leaf_pagenum = get_left_page_num(cursor->table, (node_t*)node);
        uint8_t next_leaf_pagenum = get_right_page_num(cursor->table, (node_t*)node);
        leaf_node_t* prev_leaf = NULL;
        leaf_node_t* next_leaf = NULL;
        uint8_t parent_page_num = node->meta.parent_page_num;
        internal_node_t* parent = NULL;
        if (parent_page_num != INVALID_PAGE_NUM) {
            parent = (internal_node_t*)get_page(cursor->table->pager, parent_page_num);
        }
        if (prev_leaf_pagenum != INVALID_PAGE_NUM) {
            prev_leaf = (leaf_node_t*)get_page(cursor->table->pager, prev_leaf_pagenum);
        }
        if (next_leaf_pagenum != INVALID_PAGE_NUM ) {
            next_leaf = (leaf_node_t*)get_page(cursor->table->pager, next_leaf_pagenum);
        }
        // prev节点可以借一个
        if (prev_leaf != NULL && prev_leaf->num_cells - 1 >= LEAF_NODE_MIN_CELLS) {
            // 1.节点移除cell，[0-cellnum]往右偏移一个,让出第一个cell未知 
            for (size_t i = cursor->cell_num; i > 0; i--) {
                memcpy(&node->cells[i], &node->cells[i - 1], sizeof(leaf_node_cell_t));
            }
            // 2. 将prev最后的cell复制到 节点的第一个cell
            leaf_node_cell_t* last_cell = &prev_leaf->cells[prev_leaf->num_cells - 1];
            memcpy(&node->cells[0], last_cell, sizeof(leaf_node_cell_t));
            // 3. prev节点移除第一个cell，需要更新parent
            leaf_node_remove_cell(cursor->table, prev_leaf, prev_leaf->num_cells - 1);

            // 更新node结点对应父节点
            update_internal_node_key(parent, node_old_max_key, get_node_max_key(cursor->table->pager, node));

            return;
        } else if (next_leaf != NULL && next_leaf->num_cells - 1 >= LEAF_NODE_MIN_CELLS) {
            // 与借prev节点类似。
            uint32_t old_max_key = get_node_max_key(cursor->table->pager, (node_t*)node);
            // 1.节点移除cell，[cellnum:]往左偏移一个,让出最后一个cell 
            for (size_t i = cursor->cell_num + 1 ; i < next_leaf->num_cells; i++) {
                memcpy(&node->cells[i - 1], &node->cells[i], sizeof(leaf_node_cell_t));
            }
            // 2. 将next第一个的cell复制到 节点的最后cell, 需要更新parent中的key
            leaf_node_cell_t* first_cell = &next_leaf->cells[0];
            memcpy(&node->cells[node->num_cells - 1], first_cell, sizeof(leaf_node_cell_t));
            internal_node_t *parent = (internal_node_t*)get_page(cursor->table->pager, node->meta.parent_page_num);
            update_internal_node_key(parent, old_max_key, get_node_max_key(cursor->table->pager, (node_t*)node));
            // 3. next节点移除第一个cell
            leaf_node_remove_cell(cursor->table, next_leaf, 0);
            return;
        }

        // 没有兄弟借： 移除cell后，向兄弟节点合并
        if (prev_leaf != NULL && prev_leaf->meta.parent_page_num == node->meta.parent_page_num) {
            leaf_node_merge_and_remove(cursor, true);
            return;
        } else if (next_leaf != NULL && next_leaf->meta.parent_page_num == node->meta.parent_page_num){
            leaf_node_merge_and_remove(cursor, false);
            return;
        }
        perror("Leaf node remove error");

    }
}

/**
 * @brief 内部更新，重建root
 *  
 * @param table 
 * @param right_child_page_num : the property of new root
 * @details 原来的root（pagenum）不变，新创建一个left_child,把原来root的内容给left_child. root空出来重新初始化
 */
void create_new_root(table_t *table, uint8_t right_child_page_num)
{
    node_t *root = (node_t *)get_page(table->pager, table->root_page_num);
    node_t *right_child =(node_t *) get_page(table->pager, right_child_page_num);

    uint8_t left_child_page_num = get_unused_page_num(table->pager);
    node_t *left_child=(node_t *) get_page(table->pager, left_child_page_num);

    if (get_node_type(root) == NODE_INTERNAL) {
        initialize_internal_node((internal_node_t *)right_child, right_child_page_num);
        initialize_internal_node((internal_node_t *)left_child, left_child_page_num);
    }

    /* 新的leftchild 操作*/
    // 1. 将root原有内容复制到left_child
    memcpy(left_child, root, PAGE_SIZE);

    // 2.重置leftchild 元信息
    set_node_root(left_child, 0);
    ((common_header_t *)left_child)->page_num = left_child_page_num;
    ((common_header_t *)left_child)->parent_page_num = 0;

    if (get_node_type(left_child) == NODE_INTERNAL) {
        // 3. 更新内部cell，将每个cell的parent修改为left child */
        node_t *child;
        for (int i = 0; i < left_child->internal.num_cells; ++i) {
            child = (node_t *)get_page(table->pager,  left_child->internal.cells[i].child_page_num);
            *node_parent(child) = left_child_page_num;
        }
        child = get_page(table->pager, left_child->internal.right_child_page_num);
        *node_parent(child) = left_child_page_num;
    }

    /* 兄弟节点 */


    /* 根节点（父亲）：重新初始化 */
    internal_node_t *new_root = &root->internal;
    initialize_internal_node(new_root, table->root_page_num);
    new_root->meta.is_root = 1;

    new_root->num_cells = 1; 
    new_root->cells[0].child_page_num = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(table->pager, left_child);
    new_root->cells[0].key = left_child_max_key;
    new_root->right_child_page_num = right_child_page_num;
    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;
    *node_parent(root) = INVALID_PAGE_NUM; // 根节点的父亲parent_page_num是非法的
}

void close_input_buffer(input_buffer_t* input_buffer);

/*==============cursor ====================*/
/* 创建一个cursor指向 表第一个记录。*/
cursor_t *table_start(table_t *table)
{
    // 返回最小的key数据 所在cursor
    cursor_t *cursor = table_find(table, 0);
    leaf_node_t *node = (leaf_node_t*)get_page(table->pager, cursor->page_num);
    uint32_t num_cells = node->num_cells;
    cursor->end_of_table = (num_cells == 0);

    return cursor;
}

/**
 * @brief 找到key的位置，#pagenum#cellnum（叶子节点）
 * 
 * @param table 
 * @param key 
 * @return cursor_t* 
 */
cursor_t *table_find(table_t *table, uint32_t key)
{
    uint8_t root_page_num = table->root_page_num;
    node_t *root_node = (node_t*)get_page(table->pager, root_page_num);

    if (get_node_type(root_node) == NODE_LEAF) {
        return leaf_node_find(table, root_page_num, key);
    } else {
        return internal_node_find(table, root_page_num, key);
    }
}
/* 总是假设 0..num_pages-1 已经被分配，*/
uint8_t get_unused_page_num(pager_t *pager)
{
    return pager->num_pages;
}

/**
 * @brief 节点获取必须通过getpage，不可以通过pager.pagers直接访问
 * 
 * @param pager ：pages管理
 * @param page_num ：
 * @return void* ：一个page
 */
void *get_page(pager_t* pager, uint8_t page_num)
{
    if (page_num >= TABLE_MAX_PAGES) {
        printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
               TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL) {
        // 未命中缓存， 分配内存，从文件读
        void *page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE; // 获取page个数

        // We might save a partial page at the end of the file.
        if (pager->file_length % PAGE_SIZE) {
            num_pages += 1;
        }

        if (page_num < num_pages) {
            lseek(pager->fd, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->fd, page, PAGE_SIZE);
            if (bytes_read == -1) {
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }
        pager->pages[page_num] = page;
        if (page_num >= pager->num_pages) {
            pager->num_pages = page_num + 1;
        }
    }
    return pager->pages[page_num];
}
/* 将page刷盘
 * 整页刷盘， cell不会跨page
 * */
void pager_flush(pager_t *pager, uint8_t page_num)
{
#ifdef DEBUG
    // printf("pager flush %u\n", page_num);
#endif
    if (pager->pages[page_num] == NULL) {
        printf("Tried to flush null page.\n");
        exit(EXIT_FAILURE);
    }
    off_t offset = lseek(pager->fd, page_num * PAGE_SIZE, SEEK_SET);
    if (offset == -1) {
        printf("Error seeking: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    ssize_t bytes_written = write(pager->fd, pager->pages[page_num], PAGE_SIZE);
    if (bytes_written == -1) {
        printf("Error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}
/*
 * 1. 缓存刷新到盘
 * 2. 关闭db file
 * 3. 释放table， pager
 * */
void db_close(table_t *table)
{
    pager_t *pager = table->pager;
    assert(pager);
    assert(table);

    for (uint32_t i = 0; i < pager->num_pages; ++i) {
        if (pager->pages[i] == NULL) {
            continue;
        }
        pager_flush(pager, i);
        assert(pager->pages[i]);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    int res = close(pager->fd);
    if (res == -1) {
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; ++i) {
        void *page = pager->pages[i];
        if (page) {
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);
    free(table);
}
/* cursor 记录下移一行*/
void cursor_advance(cursor_t *cursor)
{
    uint8_t page_num = cursor->page_num;
    leaf_node_t *node = (leaf_node_t*)get_page(cursor->table->pager, page_num);
    cursor->cell_num += 1;
    if (cursor->cell_num >= node->num_cells) {
        // 跨节点
        uint32_t next_page_num = node->next_leaf_page_num;
        if (next_page_num == INVALID_PAGE_NUM) {
            cursor->end_of_table = true;
        } else {
            cursor->page_num = next_page_num;
            cursor->cell_num = 0;
        }
    }
}


// 通过cursor返回访问字节位置
void * cursor_value(cursor_t *cursor)
{
    uint8_t page_num = cursor->page_num;
    leaf_node_t *node = (leaf_node_t*)get_page(cursor->table->pager, page_num);
    return &node->cells[cursor->cell_num].row;
}

/* row_t 落入内存*/
void serialize_row(row_t* src, void* destination)
{
    memcpy(destination + FIELD_OFFSET(row_t, time), &(src->time), FIELD_SIZE(row_t, time));
    strncpy(destination + FIELD_OFFSET(row_t, source), src->source, FIELD_SIZE(row_t, source));
    strncpy(destination + FIELD_OFFSET(row_t, content), src->content, FIELD_SIZE(row_t, content));
}

void print_row(row_t *row)
{
    printf("(%d, %s, %s)\n", row->time, row->source, row->content);
}

/* 从内存读,  source 为 一行的起始地址*/
void deserialize_row(void* src, row_t* destination) {
    memcpy(&(destination->time), src +  FIELD_OFFSET(row_t, time), FIELD_SIZE(row_t, time));
    memcpy(&(destination->source), src + FIELD_OFFSET(row_t, source), FIELD_SIZE(row_t, source));
    memcpy(&(destination->content), src +  FIELD_OFFSET(row_t, content),  FIELD_SIZE(row_t, content));
}

/* 执行select， 从table.page内存读出*/
execute_res execute_select(sql_statement * statement, table_t * table)
{
    row_t row;
    cursor_t *cursor = table_start(table);
    while (!cursor->end_of_table) {
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }
    free(cursor);
    return EXECUTE_SUCCESS;
}

/* 执行insert， 写入table.page内存*/
execute_res execute_insert(sql_statement* statement, table_t* table)
{
    row_t* row = &(statement->row);
    uint32_t key = row->time;
    cursor_t *cursor = table_find(table, key);
    
    leaf_node_t* node = (leaf_node_t*)get_page(table->pager, cursor->page_num);

    if (cursor->cell_num < node->num_cells) {
        if (key == node->cells[cursor->cell_num].key) {
            return EXECUTE_DUPLICATE_KEY;
        }
    }

    leaf_node_insert(cursor, row->time, row);
    free(cursor);
    return EXECUTE_SUCCESS;
}

/**
 * @brief 通过id找到cell，请0文本，移除cell
 * 
 * @param statement 
 * @param table 
 * @return execute_res 
 */
execute_res execute_delete(sql_statement *statement, table_t *table)
{
    time_t time = statement->row.time;
    time_t key = time;

    cursor_t* cursor =table_find(table, key);

    // 1. 
    leaf_node_t *node = (leaf_node_t*)get_page(table->pager, cursor->page_num);
    leaf_node_remove(cursor);

    free(cursor);
    return EXECUTE_SUCCESS;
}
/*
 * virtual machines
 * */
execute_res execute_statement(sql_statement *statement, table_t *table)
{
    switch (statement->type) {
        case STATEMENT_INSERT:
            return execute_insert(statement, table);
        case STATEMENT_SELECT:
            return execute_select(statement, table);
        case STATEMENT_DELETE:
            return execute_delete(statement, table);
        default:
            perror("unkown execute type");
            return EXECUTE_UNKNOWN_ERR;
    }
}

/*
 * 打开文件， 初始化pager
 * */
pager_t *pager_open(const char *file_name)
{
    // 文件读写， 文件所有者读写权限
    int fd = open(file_name, O_RDWR|O_CREAT, S_IWUSR|S_IRUSR);
    if (fd < 0) {
        printf("Unable to open file.\n");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(fd, 0, SEEK_END);
    if (file_length == -1) {
        perror("can't seek");
    }
    pager_t *pager = (pager_t *) malloc(sizeof(pager_t));
    pager->fd = fd;
    pager->file_length = file_length;
    pager->num_pages = file_length / PAGE_SIZE; // 初始化为最大page个数
    if (file_length % PAGE_SIZE != 0) {
        printf("DB file is not a whole number of pages. corrupt file.\n");
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; ++i) {
        pager->pages[i] = NULL;
    }
    
    return pager;
}

/* db初始化
 * 1. 打开db file
 * 2. 初始化pager
 * 3. 初始化table
 * 4. 初始化page0 ， 根节点
 * */
table_t *db_open(const char *file_name)
{
    pager_t *pager = pager_open(file_name);
    uint32_t num_rows = pager->file_length / sizeof(row_t);

    table_t *table = (table_t *) malloc(sizeof(table_t));
    table->pager = pager;
    table->root_page_num = 0;

    if (pager->num_pages == 0) {
        // 分配page0 , 初始化根节点
        leaf_node_t *root_node = (leaf_node_t*)get_page(pager, 0);
        initialize_leaf_node(root_node, 0);
        root_node->meta.is_root = 1;

    }
    return table;
}

/* 处理insert*/
prepare_res_type prepare_insert(input_buffer_t* input_buffer, sql_statement *statement)
{
    statement->type = STATEMENT_INSERT;
    char *keyword = strtok(input_buffer->buffer, " ");
    char *time_str = strtok(NULL, " ");
    char *source = strtok(NULL, " ");
    char *content = strtok(NULL, " ");

    if (time_str == NULL || source == NULL || content == NULL) {
        return PREPARE_SYNTAX_ERROR;
    }
    time_t time = atoi(time_str);

    if (strlen(source) > COLUMN_SOURCE_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(content) > COLUMN_CONTENT_SIZE) {
        return PREPARE_STRING_TOO_LONG;
    }
    statement->row.time = time;
    strcpy(statement->row.source, source);
    strcpy(statement->row.content, content);
    return PREPARE_SUCCESS;
}

prepare_res_type prepare_delete(input_buffer_t *input_buffer, sql_statement *statement)
{
    statement->type = STATEMENT_DELETE;
    char *keyword = strtok(input_buffer->buffer, " ");
    char *time_str = strtok(NULL, " ");
    time_t time = atoi(time_str);

    statement->row.time = time;
    return PREPARE_SUCCESS;
}
/* 填充 statement*/
/*
 * insert 1 cstack foo@bar.com
*/
prepare_res_type prepare_statement(input_buffer_t *input_buffer, sql_statement *statement)
{
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        return prepare_insert(input_buffer, statement);
    }
    if (strncmp(input_buffer->buffer, "select", 6) == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    if (strncmp(input_buffer->buffer, "delete", 6) == 0) {
        statement->type = STATEMENT_DELETE;
        return prepare_delete(input_buffer, statement);
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

/*处理源命令*/
meta_cmd_res do_meta_command(input_buffer_t *input_buffer, table_t *table)
{
    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        close_input_buffer(input_buffer);
        db_close(table);
        exit(EXIT_SUCCESS);
    } else if (strcmp(input_buffer->buffer, ".constants") == 0) {
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".table") == 0) {
        printf("Table:\n");
        print_table(table);
        return META_COMMAND_SUCCESS;
    }else if (strcmp(input_buffer->buffer, ".btree") == 0) {
        printf("Tree:\n");
        print_tree(table->pager, 0, 0);
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".help") == 0) {
        printf("Help:\n");
        printf("Meta command:\n");
        printf("\t.exit [] exit\n");
        printf("\t.constants [] constants\n");
        printf("\t.btree [] btree\n");
        printf("\t.table [] btree\n");
        printf("\t.help [] help\n");

        printf("SQL command:\n");
        printf("\tselect []\n");
        printf("\tinsert [time_t][source][content] \n");
        printf("\tdelete [index]\n");

        return META_COMMAND_SUCCESS;
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

input_buffer_t* new_input_buffer()
{
    input_buffer_t* input_buffer = (input_buffer_t*)malloc(sizeof(input_buffer_t));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}
void close_input_buffer(input_buffer_t* input_buffer)
{
    assert(input_buffer);
    assert(input_buffer->buffer);
    free(input_buffer->buffer);
    free(input_buffer);
}
void read_input(input_buffer_t *input_buffer)
{
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
    if (bytes_read < 0) {
        printf("Error reading input");
        exit(EXIT_FAILURE);
    }
    // 去除换行符号
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = '\0';
}
// 打印提示
void print_prompt()
{
    printf("db > ");
}
// 打印 Btree 配置
void print_constants() {
    printf("tree m:%u\n", BTREE_M);
    printf("tree height:%u \n", BTREE_HEIGHT);
    printf("max pages : %u\n", TABLE_MAX_PAGES);

    printf("Page size: %d\n", PAGE_SIZE);
    printf("Row size: %d\n", sizeof(row_t));

    printf("max rows in leafnode (theory)：%u\n", (PAGE_SIZE - sizeof(common_header_t)) / sizeof(leaf_node_cell_t));
    printf("Total rows(theory): %u\n", LEAF_NODE_MAX_CELLS *  (PAGE_SIZE - sizeof(common_header_t)) / sizeof(leaf_node_cell_t));

    printf("internal node numbers: %d\n");
    printf("leaf node numbers: 27\n");

    printf("INTERNAL_NODE_MAX_CELLS : %d\n", INTERNAL_NODE_MAX_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
    printf("LEAF_NODE_MIN_CELLS: %d\n", LEAF_NODE_MIN_CELLS);
    printf("LEAF_NODE_CELL_SIZE: %d\n", sizeof(leaf_node_cell_t));
    
}


/**
 * @brief 打印table内存信息
 * 
 * @param table 
 */
void print_table(table_t * table)
{
    node_t *node;
    for (size_t i = 0; i < table->pager->num_pages; i++) {
        /* code */
        node = (node_t*)get_page(table->pager, i);

        printf("---------------------------------------\n");
        if (get_node_type(node) == NODE_INTERNAL) {
            printf("+ nodetype: %u, isroot : %u, parentpagenum : %u, pagenum : %u, ",
                node->internal.meta.node_type,
                node->internal.meta.is_root,
                node->internal.meta.parent_page_num,
                node->internal.meta.page_num
            );
            printf(" numcells : %u, rightchild : %u \n",
                node->internal.num_cells,
                node->internal.right_child_page_num
            );
            for (size_t j = 0; j < node->internal.num_cells; j++) {
                printf("| childpagenum : %u, key : %u",
                    node->internal.cells[j].child_page_num,
                    node->internal.cells[j].key
                );
            }
            
        } else {
            printf("+ nodetype: %u, isroot : %u, parentpagenum : %u, pagenum : %u ",
                node->leaf.meta.node_type,
                node->leaf.meta.is_root,
                node->leaf.meta.parent_page_num,
                node->leaf.meta.page_num
            );
            printf(" numcells : %u\n",
                node->leaf.num_cells
            );
            for (size_t j = 0; j < node->leaf.num_cells; j++) {
                printf("| key : %u",
                    node->leaf.cells[j].key
                );
            }
        }
        printf("\n---------------------------------------\n");
    }
    
}

void indent(uint32_t level)
{
    for (uint32_t i = 0; i < level; ++i) {
        printf("  ");
    }
}
// 打印BTREE
void print_tree(pager_t * pager, uint8_t page_num, uint32_t indentation_level)
{
    node_t *node = (node_t*)get_page(pager, page_num);
    uint32_t num_keys, child_page_num;

    switch (get_node_type(node)) {
        case NODE_INTERNAL:
            num_keys = node->internal.num_cells;
            indent(indentation_level);
            printf("+ Internal Node (num_keys %d) page_num (%d)\n", num_keys, page_num);
            if (num_keys > 0) {
                for (uint32_t  i = 0; i < num_keys; ++i) {
                    child_page_num = node->internal.cells[i].child_page_num;
                    print_tree(pager, child_page_num, indentation_level + 1);
                    indent(indentation_level + 1);
                    printf("+ Internal cell_(%d) key %d\n", i, node->internal.cells[i].key);
                }
                child_page_num = node->internal.right_child_page_num;
                print_tree(pager, child_page_num, indentation_level + 1);
            }
            printf("\n");
            break;
        case NODE_LEAF:
            num_keys = node->leaf.num_cells;
            indent(indentation_level);
            printf("- Leaf Node(num_cells %d) page_num (%d)\n", num_keys, page_num);
            for (uint32_t i = 0; i < num_keys; ++i) {
                indent(indentation_level + 1);
                printf("- Leaf cell_(%d) key %u\n", i,node->leaf.cells[i].key);
            }
            break;
    }

}



/**
 *

 ~ sqlite3
SQLite version 3.16.0 2016-11-04 19:09:39
Enter ".help" for usage hints.
Connected to a transient in-memory database.
Use ".open FILENAME" to reopen on a persistent database.
sqlite> create table users (id int, source varchar(255), content varchar(255));
sqlite> .tables
users
sqlite> .exit
 * @param argc
 * @param argv
 * @return
 */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Must supply a  db filename.\n");
        exit(EXIT_FAILURE);
    }
    char *file_name = argv[1];
    table_t *table = db_open(file_name);
    input_buffer_t *input_buffer = new_input_buffer();
    //printf("PAGES: %u, ROWS-PAGE: %u, ROW-SIZE :%u\n", TABLE_MAX_PAGES, ROWS_PER_PAGE, ROW_SIZE);
    while (true) {
        /*1. Making a Simple REPL*/
        print_prompt();
        read_input(input_buffer);
        if (input_buffer->buffer[0] == '.') {
            // 源命令
            switch (do_meta_command(input_buffer, table)) {
                case META_COMMAND_SUCCESS:
                    continue;
                case META_COMMAND_UNRECOGNIZED_COMMAND:
                    printf("Unrecognized command `%s` \n", input_buffer->buffer);
                    continue;
            }
        }
        sql_statement statement;
        switch (prepare_statement(input_buffer, &statement)) {
            // 拿到SQL 就去处理
            case PREPARE_SUCCESS:
                break;
            case PREPARE_SYNTAX_ERROR:
                printf("Syntax error. Could not parse statement.\n");
                continue;
            case PREPARE_UNRECOGNIZED_STATEMENT:
                printf("Unrecognized SQL command `%s` \n", input_buffer->buffer);
                continue;
            case PREPARE_STRING_TOO_LONG:
                printf("String is too long.\n");
                continue;

        }

        switch (execute_statement(&statement, table)) {
            case EXECUTE_SUCCESS:
                printf("Executed.\n");
                break;
            case EXECUTE_TABLE_FULL:
                printf("Error: Table full.\n");
                break;
            case EXECUTE_DUPLICATE_KEY:
                printf("Error: Duplicate key.\n");
                break;
        }
    }
    return 0;
}

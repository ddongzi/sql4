#include "btree.h"
#include <stdint.h>
#include "sql4limit.h"
#include <stdbool.h>
#include "pager.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define NODE_INTERNAL 1
#define NODE_LEAF 0 // 空白页 都是0， 默认应该是leaf

/* B+Tree  config */
#define BTREE_M 3 // TODO 暂时没有用，因为叶子节点没有限制
#define BTREE_HEIGHT 4  // 三层 0，1，2， 3
#define INTERNAL_NODE_MAX_CELLS BTREE_M // 
#define INTERNAL_NODE_MIN_CELLS 1 // 最小为1个cell，两个孩子
#define LEAF_NODE_MAX_CELLS BTREE_M

#define INVALID_PAGE_NUM UINT8_MAX  // internal node为空节点 : right child page_num 为 INVALID_PAGE_NUM
#define CELL_DATA_SIZE 256

/* ============ Common header layout ============*/
/*
 * | Node Type (uint8_t)      | 1 byte    | 1表示内部节点，0表示叶子节点
 * | Is Root (uint8_t)        | 1 byte    | 0
 * | Parent Pointer (uint8_t) | 1 byte   | root节点parent是Invalid_page_num
 * | Page num                 | 1 byte   | 
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
 * | Number of Cells                  | 1 byte     | 
 * ============ Leaf Node Body Layout #0 cell ====== 
 * | Key                              | 4 bytes    | 
 * | Value (CELL_DATA_SIZE)           | CELL_DATA_SIZE   |
 * ============ Leaf Node Body Layout #1 cell ====== 
 * | Key                              | 4 bytes    | 
 * | Value (CELL_DATA_SIZE)           | CELL_DATA_SIZE   |
 * |     .....................................     |
 * ================================================
 * | Space for Cells                  | PAGE_SIZE - ~ |
 */

/* 叶子满节点分裂时候，分一半给新兄弟叶子*/
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2;
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;
const uint32_t LEAF_NODE_MIN_CELLS = LEAF_NODE_RIGHT_SPLIT_COUNT; // 最小cell数

typedef struct {
    uint32_t key;
    uint8_t data[CELL_DATA_SIZE]; // 直接映射文件存储,最大存储CELL_DATA_SIZE，不能使用指针。 
} leaf_node_cell_t;

typedef struct {
    common_header_t meta;
    uint8_t next_leaf_page_num;
    uint8_t num_cells;
    leaf_node_cell_t cells[LEAF_NODE_MAX_CELLS];
    unsigned char padding[PAGE_SIZE - sizeof(common_header_t) -3 * sizeof(uint8_t) - sizeof(leaf_node_cell_t)]; // 填充剩余page空间
} leaf_node_t;


/* ============ Internal Node Structure ============ */
/* 
 *  ============ Internal Node Header Layout  ====== 
 * | Number of cells        | 1 byte    | 
 * | Right Child Pointer   | 1 byte    | 
 *  ============ Internal Node Body Layout #0 cell ====== 
 * | Child Pointer        | 1 byte     | 
 * | Key (uint32_t)       | 4 bytes    | 孩子节点树key最值
 *  ============ Internal Node Body Layout #1 cell ====== 
 * | Child Pointer        | 1 byte     | 
 * | Key (uint32_t)       | 4 bytes    | 
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


void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, 
    void* data, size_t datalen);
void internal_node_remove(BTree* tree, internal_node_t *node, uint8_t cell_num);
Cursor *leaf_node_find(BTree *tree, uint8_t page_num, uint32_t key);
void create_new_root(BTree *tree, uint8_t right_child_page_num);
void initialize_internal_node(internal_node_t *node, uint8_t page_num);
void internal_node_insert(BTree * tree, 
    uint8_t parent_page_num, uint8_t child_page_num);
void update_internal_node_key(internal_node_t *node, 
    uint32_t old_key, uint32_t new_key);





uint8_t get_node_parent(node_t *node)
{
    common_header_t *meta = (common_header_t *)node;
    return meta->parent_page_num;
}
void set_node_parent(node_t* node, uint8_t parent_page_num)
{
    common_header_t *meta = (common_header_t *)node;
    meta->parent_page_num = parent_page_num;
}
uint8_t get_node_type(node_t *node)
{
    common_header_t *meta = (common_header_t *)node;
    return meta->node_type;
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
 * @return uint8_t ： 如果是最右结点，返回numcells表示
 */
uint8_t get_cellnum_in_parent(BTree *tree, node_t *node)
{
    uint8_t parent_page_num = get_node_parent(node);
    if (parent_page_num == INVALID_PAGE_NUM) perror("error, pagenum invalid");

    internal_node_t *parent = (internal_node_t *)pager_get_page(tree->pager, parent_page_num);
    
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
uint8_t get_left_page_num(BTree *tree, node_t *node)
{
    uint8_t parent_page_num = get_node_parent(node);
    if (parent_page_num == INVALID_PAGE_NUM) perror("error, pagenum invalid");

    internal_node_t *parent = (internal_node_t *)pager_get_page(tree->pager, parent_page_num);
    
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
/**
 * @brief 获取右兄弟的pagenum,  不允许跨父
 * 
 * @param node 
 * @return uint8_t 
 */
uint8_t get_right_page_num(BTree *tree, node_t *node)
{
    uint8_t parent_page_num = get_node_parent(node);
    if (parent_page_num == INVALID_PAGE_NUM) perror("error, pagenum invalid");

    internal_node_t *parent = (internal_node_t *)pager_get_page(tree->pager, parent_page_num);
    
    if (parent->right_child_page_num == ((common_header_t *)node)->page_num) 
        return INVALID_PAGE_NUM;
    
    uint8_t i;
    for ( i = 0; i < parent->num_cells; i++) {
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
uint32_t get_node_max_key(BTree* tree, node_t *node)
{
    if (get_node_type(node) == NODE_LEAF) {
        return node->leaf.cells[node->leaf.num_cells - 1].key;
    }

    node_t* right_child = (node_t *)pager_get_page(tree->pager, node->internal.right_child_page_num);
    return get_node_max_key(tree, right_child);
}


/*============internal node function=================*/
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
 * @return Cursor* 
 * @warning 
 */
Cursor* internal_node_find(BTree *tree, uint8_t page_num, uint32_t key)
{
    internal_node_t *node = (internal_node_t *)pager_get_page(tree->pager, page_num);

    // 找到key所在节点树
    uint8_t child_cell_num = internal_node_find_child(node, key);
    uint8_t child_page_num = internal_node_child(node, child_cell_num);

    node_t *child = (node_t *)pager_get_page(tree->pager, child_page_num);
    switch (get_node_type(child)) {
        case NODE_LEAF:
            return leaf_node_find(tree, child_page_num, key);
        case NODE_INTERNAL:
            return internal_node_find(tree, child_page_num, key);
            break;
        default:
            sql4_errno = BTREE_UNKOWN_NODETYPE_ERR;
            perror("unkown node type");
            return NULL;
    }
}

/**
 * @brief 在叶子节点#pagenum中，查找key的#cell_num，
 * 
 * @param table 
 * @param page_num 
 * @param key 
 * @return Cursor* 
 */
Cursor *leaf_node_find(BTree *tree, uint8_t page_num, uint32_t key)
{
    leaf_node_t *node = (leaf_node_t *)pager_get_page(tree->pager, page_num);
    uint8_t num_cells = node->num_cells;

    Cursor *cursor = (Cursor *) malloc(sizeof(Cursor));
    cursor->btree = tree;
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

/**
 * @brief parent内部节点已满，需要分裂parent节点，插入cell指向#child_page_num
 * 
 * @param table 
 * @param parent_page_num 
 * @param child_page_num 
 */
void internal_node_split_and_insert(BTree * tree, uint8_t parent_page_num, uint8_t child_page_num)
{
    uint8_t old_page_num = parent_page_num;
    internal_node_t * old_node = (internal_node_t *)pager_get_page(tree->pager, old_page_num);
    uint32_t old_max = get_node_max_key(tree, (node_t*)old_node);

    node_t* child =  (node_t *)pager_get_page(tree->pager, child_page_num);
    uint32_t child_max = get_node_max_key(tree, child);

    uint8_t new_page_num = pager_get_unused_pagenum(tree->pager);

    uint8_t splitting_root = old_node->meta.is_root; // 记录被分裂节点是否是根节点， 如果是根节点，还需要创建root
    
    internal_node_t* parent;
    internal_node_t* new_node;

    /* 创建新节点，与parent建联 */
    if (splitting_root) {
        // 如果没有parent，内部创建一个新root，并且新节点作为他的右边孩子。
        create_new_root(tree, new_page_num);
        parent = (internal_node_t *)pager_get_page(tree->pager, tree->root_page_num); 
        // 重建root后，重新获取节点
        old_page_num = internal_node_child(parent, 0); 
        old_node =(internal_node_t *)pager_get_page(tree->pager, old_page_num); 
        new_node = (internal_node_t *)pager_get_page(tree->pager, new_page_num);
    } else {
        // 如果有parent, 把新节点与parent建联即可。
        parent = (internal_node_t *)pager_get_page(tree->pager, old_node->meta.parent_page_num);
        new_node = (internal_node_t *)pager_get_page(tree->pager, new_page_num);

        initialize_internal_node(new_node, new_page_num);
        new_node->meta.parent_page_num = old_node->meta.parent_page_num;
    }

    /* 原节点分裂到一部分到新节点。*/
    // 1. 先将最右孩子给 新节点
    uint32_t old_num_keys = old_node->num_cells;
    uint8_t cur_page_num = old_node->right_child_page_num; // 先指向最右侧孩子
    node_t* cur_node = (node_t* )pager_get_page(tree->pager, cur_page_num);

    internal_node_insert(tree, new_page_num, cur_page_num); //
    set_node_parent(cur_node, new_page_num);
    old_node->right_child_page_num = INVALID_PAGE_NUM; // 原来page 最右侧孩子指向空

    // 2. 挪一半cells给新节点
    for (int i = INTERNAL_NODE_MAX_CELLS - 1; i > INTERNAL_NODE_MAX_CELLS/2; --i) {
        cur_page_num = old_node->cells[i].child_page_num;
        internal_node_insert(tree, new_page_num, cur_page_num);

        cur_node = (node_t *)pager_get_page(tree->pager, cur_page_num);
        set_node_parent(cur_node, new_page_num);

        old_node->num_cells--;
    }

    // 3. 设置原node最右边cell为rightchild，并从cell中移除
    old_node->right_child_page_num = old_node->cells[old_node->num_cells - 1].child_page_num;
    old_node->num_cells--;

    /* 插入cell操作*/
    // 4. 新插入节点进行关联
    uint32_t max_after_split = get_node_max_key(tree, (node_t*)old_node);
    uint8_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

    internal_node_insert(tree, destination_page_num, child_page_num);
    set_node_parent(child, destination_page_num);

    /* 更新原节点、新节点和父亲节点cell */
    // 5. 
    update_internal_node_key(parent, old_max, get_node_max_key(tree, (node_t*)old_node)); // 更新父节点
    if (!splitting_root) {
        // 如果原节点是根节点，新节点是作为新root的rightchild，不需要更新cell. 否则要更新
        internal_node_insert(tree, parent->meta.page_num, new_page_num);
    }

}


/**
 * @brief 内部更新，重建root
 *  
 * @param tree 
 * @param right_child_page_num : the property of new root
 * @details 原来的root（pagenum）不变，新创建一个left_child,把原来root的内容给left_child. root空出来重新初始化
 */
void create_new_root(BTree *tree, uint8_t right_child_page_num)
{
    node_t *root = (node_t *)pager_get_page(tree->pager, tree->root_page_num);
    node_t *right_child =(node_t *) pager_get_page(tree->pager, right_child_page_num);

    uint8_t left_child_page_num = pager_get_unused_pagenum(tree->pager);
    node_t *left_child=(node_t *) pager_get_page(tree->pager, left_child_page_num);

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
            child = (node_t *)pager_get_page(tree->pager,  left_child->internal.cells[i].child_page_num);
            set_node_parent(child, left_child_page_num);
        }
        child = pager_get_page(tree->pager, left_child->internal.right_child_page_num);
        set_node_parent(child, left_child_page_num);
    }

    /* 兄弟节点 */


    /* 根节点（父亲）：重新初始化 */
    internal_node_t *new_root = &root->internal;
    initialize_internal_node(new_root, tree->root_page_num);
    new_root->meta.is_root = 1;

    new_root->num_cells = 1; 
    new_root->cells[0].child_page_num = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(tree, left_child);
    new_root->cells[0].key = left_child_max_key;
    new_root->right_child_page_num = right_child_page_num;
    set_node_parent(left_child, tree->root_page_num);
    set_node_parent(right_child, tree->root_page_num);
    set_node_parent(root, INVALID_PAGE_NUM);
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

/**
 * @brief 向内部parent节点插入一个cell，指向childpagenum
 * 
 * @param table 
 * @param parent_page_num :该parent插入
 * @param child_page_num : 
 */
void internal_node_insert(BTree * tree, uint8_t parent_page_num, uint8_t child_page_num)
{
    internal_node_t *parent =(internal_node_t *) pager_get_page(tree->pager, parent_page_num);
    node_t *child = (node_t *)pager_get_page(tree->pager, child_page_num);

    uint32_t child_max_key = get_node_max_key(tree, child);
    uint32_t index = internal_node_find_child(parent, child_max_key); // 要插入的位置，插入max_key

    uint32_t original_num_keys = parent->num_cells;

    if (original_num_keys >= INTERNAL_NODE_MAX_CELLS) {
        // 1. 当前parent节点容量超了， 需要分割 parent节点处理
        internal_node_split_and_insert(tree, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = parent->right_child_page_num;

    if (right_child_page_num == INVALID_PAGE_NUM) {
        // 2. parent为空节点 : 设置right child 即可
        parent->right_child_page_num = child_page_num;
        return;
    }

    // 3. parent节点不为空也不满
    node_t *right_child = (node_t *)pager_get_page(tree->pager, right_child_page_num);
    parent->num_cells = original_num_keys + 1;

    if (child_max_key > get_node_max_key(tree, right_child)) {
        // 3.1 如果插入节点大于最右侧孩子， 代替最右孩子字段， 将最右孩子写到内部节点最后中
        parent->cells[original_num_keys].child_page_num = right_child_page_num;
        parent->cells[original_num_keys].key = get_node_max_key(tree, right_child);
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
 * @brief TODO 更新node key???? 
 */
void update_internal_node_key(internal_node_t *node, uint32_t old_key, uint32_t new_key)
{
    uint8_t cell_num = internal_node_find_child(node, old_key);
    node->cells[cell_num].key = new_key;
}


/**
 * @brief 内部节点安全移除一个cell
 * 
 * @param cursor 
 * @param cell_num 
 */
void internal_node_remove_cell(BTree* tree, internal_node_t *node, uint8_t cell_num)
{
    uint32_t old_max_key = get_node_max_key(tree, (node_t*)node);
    // 向前覆盖
    for (size_t i = cell_num + 1; i < node->num_cells; i++) {
        memcpy(&node->cells[i - 1], &node->cells[i], sizeof(internal_node_cell_t));
    }
    memset(&node->cells[node->num_cells], 0, sizeof(internal_node_cell_t));
    node->num_cells--;

    if (cell_num == node->num_cells - 1) {
        // 需要更新parent中的值
        if (node->meta.parent_page_num != INVALID_PAGE_NUM) {
            internal_node_t* parent = (internal_node_t*)pager_get_page(tree->pager, node->meta.parent_page_num);
            update_internal_node_key(parent, old_max_key, get_node_max_key(tree, (node_t*)node));
        }
    }
    
}


/**
 * @brief 合并内部兄弟结点，移除cellnum
 * @param node 
 * @param merge_to_left 
 */
void internal_node_merge_and_remove(BTree* tree, internal_node_t *node,uint8_t cell_num, bool merge_to_left)
{
    internal_node_t *merge_node = NULL;
    uint32_t merge_node_old_max_key;
    Pager* pager = tree->pager;

    // 断链
    node->cells[cell_num].child_page_num = INVALID_PAGE_NUM;

    // 如果往左合并，需要向右摆动：前结点最右孩子作为第一个孩子。
    // 如果往右合并，需要向左摆动

    if (merge_to_left) {
        merge_node = (internal_node_t*)pager_get_page(tree->pager, 
            get_left_page_num(tree, (node_t*)node)
        );
        merge_node_old_max_key = get_node_max_key(tree,(node_t*)merge_node);

        // 向右摆动
        for (size_t i = cell_num; i > 0; i--) {
            memcpy(&node->cells[i], &node->cells[i - 1], sizeof(internal_node_cell_t));
        }
        node->cells[0].child_page_num = merge_node->right_child_page_num;
        node_t* right_node = (node_t*)pager_get_page(tree->pager, merge_node->right_child_page_num);
        node->cells[0].key = get_node_max_key(tree, right_node);
        merge_node->right_child_page_num = node->right_child_page_num;

        // 合并
        memcpy(&merge_node->cells[merge_node->num_cells], 
            &node->cells, 
            node->num_cells * sizeof(internal_node_cell_t)
        );
        
    } else {
        merge_node = (internal_node_t*)pager_get_page(tree->pager, 
            get_right_page_num(tree, (node_t*)node)
        );
        merge_node_old_max_key = get_node_max_key(tree, (node_t*)merge_node);

        // 向左摆动
        for (size_t i = cell_num; i < node->num_cells - 1; i++) {
            memcpy(&node->cells[i], &node->cells[i + 1], sizeof(internal_node_cell_t));
        }
        node->cells[node->num_cells - 1].child_page_num = node->right_child_page_num;
        node_t* right_node = (node_t*)pager_get_page(tree->pager, node->right_child_page_num);
        node->cells[node->num_cells - 1].key = get_node_max_key(tree, right_node);
        node->right_child_page_num = INVALID_PAGE_NUM;

        memmove(&merge_node->cells[node->num_cells], merge_node->cells, node->num_cells * sizeof(internal_node_cell_t));
        memcpy(merge_node->cells, node->cells, node->num_cells * sizeof(internal_node_cell_t));
    }
    merge_node->num_cells += node->num_cells;
    
    
    // 更新子节点: 的父亲
    for (size_t i = 0; i < merge_node->num_cells; i++) {
        node_t* child = (node_t*)pager_get_page(pager, merge_node->cells[i].child_page_num);
        set_node_parent(child, merge_node->meta.page_num);
    }
    node_t* right_child = (node_t*)pager_get_page(pager, merge_node->right_child_page_num);
    set_node_parent(right_child, merge_node->meta.page_num);
    

    // 删除父亲结点中的cell
    node_t* parent = (node_t*)pager_get_page(pager, node->meta.parent_page_num);
    internal_node_remove(tree, 
        (internal_node_t*)parent, 
        get_cellnum_in_parent(tree, (node_t*)node)
    );

}


/**
 * @brief node内部节点移除一个cell
 * 
 * @param node 
 * @param key 
 */
void internal_node_remove(BTree* tree, internal_node_t *node, uint8_t cell_num)
{

    // 1. 节点够删除cell
    if (node->num_cells - 1 >= INTERNAL_NODE_MIN_CELLS) {
        // 可以安全移除
        internal_node_remove_cell(tree, node, cell_num);
        return;
    } 
    // 如果是根节点,不够删除，即只有1个cell，
    if (node->meta.is_root) {
        common_header_t meta = node->meta;
        node_t *new_root = (node_t*)pager_get_page(tree->pager, node->right_child_page_num);
        memcpy(node, new_root, sizeof(node_t));
        memcpy(&node->meta, &meta, sizeof(common_header_t));

        if (cell_num == 0) {
            // 让右孩子作为新的根
            if (get_node_type(new_root) == NODE_INTERNAL) {
                internal_node_t* right_node = &new_root->internal;
                // 右孩子的孩子指向新的父亲
                for (size_t i = 0; i < right_node->num_cells; i++) {
                    node_t *child = (node_t*)pager_get_page(tree->pager, right_node->cells[i].child_page_num);
                    set_node_parent(child, meta.page_num);
                }
                node_t* right_r_child =  (node_t*)pager_get_page(tree->pager, right_node->right_child_page_num);
                set_node_parent(right_r_child, meta.page_num);
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
                    node_t *child = (node_t*)pager_get_page(tree->pager, left_node->cells[i].child_page_num);
                    set_node_parent(child, meta.page_num);
                }
                node_t* left_r_child =  (node_t*)pager_get_page(tree->pager, left_node->right_child_page_num);
                set_node_parent(left_r_child, meta.page_num);
            } else {
                node->meta.node_type = NODE_LEAF;
            }

        } else {
            perror("unknown root num cells");
        }
        return;

    }


    // 2. 不够，需要从兄弟结点挪
    uint8_t right_page_num = get_right_page_num(tree, (node_t*)node);
    uint8_t left_page_num = get_left_page_num(tree, (node_t*)node);
    uint8_t parent_page_num = node->meta.parent_page_num;
    internal_node_t* next = NULL;
    internal_node_t* prev = NULL;
    internal_node_t* parent = NULL;
    if (right_page_num != INVALID_PAGE_NUM) next = (internal_node_t*)pager_get_page(tree->pager, right_page_num);
    if (left_page_num != INVALID_PAGE_NUM) prev = (internal_node_t*)pager_get_page(tree->pager, left_page_num);
    if (parent_page_num != INVALID_PAGE_NUM) parent = (internal_node_t*)pager_get_page(tree->pager, parent_page_num);
    uint8_t num_cells = node->num_cells;

    if (next != NULL && next->num_cells - 1 >= INTERNAL_NODE_MIN_CELLS) {
        uint32_t old_max_key = get_node_max_key(tree, (node_t*) node);
        // 1. 删除cell
        for (size_t i = cell_num + 1; i < num_cells; i++) {
            memcpy(&node->cells[i - 1], &node->cells[i], sizeof(internal_node_cell_t));
        }
        // 2. 将node右孩子作为next#0cell的child， next#0cell的孩子作为最右
        node->cells[node->num_cells - 1].child_page_num = node->right_child_page_num;
        node_t* temp_node = (node_t*)pager_get_page(tree->pager, node->right_child_page_num);
        node->cells[node->num_cells - 1].key = get_node_max_key(tree, temp_node);

        node->right_child_page_num = next->cells[0].child_page_num;

        node_t *next_first_child = (node_t*)pager_get_page(tree->pager, next->cells[0].child_page_num);
        set_node_parent(next_first_child, node->meta.page_num);

        // 3. 删除next第一个cell
        internal_node_remove_cell(tree, next, 0);
        // 4. 更新parent
        if (parent == NULL) printf("need !");
        update_internal_node_key(parent, old_max_key, get_node_max_key(tree, (node_t*) node));
        return;
    } 
    if (prev != NULL && prev->num_cells - 1 >= INTERNAL_NODE_MIN_CELLS) {
        uint32_t old_max_key = get_node_max_key(tree, (node_t*) prev);

        // 1. node [0..cellNUm] 后移，空出第一个
        for (size_t i = cell_num; i >0; i--) {
            memcpy(&node->cells[i], &node->cells[i - 1], sizeof(internal_node_cell_t));
        }
        // 2. prev结点的最右孩子指向第一个cell
        node->cells[0].child_page_num = prev->right_child_page_num;
        node_t *right_node = (node_t *)pager_get_page(tree->pager, prev->right_child_page_num);
        node->cells[0].key = get_node_max_key(tree, right_node);

        set_node_parent(right_node, node->meta.page_num);

        // 3. prev结点移除最右孩子
        // 3.1 先移除最后一个cell， 让最后一个cell座位右孩子
        node_t *temp_node = (node_t*)pager_get_page(tree->pager, prev->cells[prev->num_cells - 1].child_page_num);
        internal_node_remove_cell(tree, prev, prev->num_cells - 1);
        prev->right_child_page_num = ((common_header_t *)temp_node)->page_num;
        update_internal_node_key(parent, old_max_key, get_node_max_key(tree, (node_t*)prev));
        return;
    }

    // 3. 兄弟结点不够借，需要合并
    if (next != NULL) {
        internal_node_merge_and_remove(tree, node, cell_num, false);
        return;
    }
    if (prev != NULL) {
        internal_node_merge_and_remove(tree, node, cell_num, true);
        return;
    }
}


/**
 * @brief 初始化叶子节点
 */
void initialize_leaf_node(leaf_node_t *node, uint8_t page_num)
{

    node->meta.node_type = NODE_LEAF;
    node->meta.is_root = 0;
    node->meta.page_num = page_num;
    node->meta.parent_page_num = INVALID_PAGE_NUM;

    node->next_leaf_page_num = INVALID_PAGE_NUM;
    node->num_cells = 0;
}

/* ===========cursor ============ */
/**
 * @brief  在cursor位置，插入数据。 btree无需感知。 外部传入的是row .
 */
void leaf_node_insert(Cursor *cursor, uint32_t key, void* data, size_t datalen)
{
    leaf_node_t *node = (leaf_node_t*)pager_get_page(cursor->btree->pager, cursor->page_num);
    uint32_t num_cells = node->num_cells;
    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        // 节点满
        leaf_node_split_and_insert(cursor, key, data, datalen);
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
    // TODO 可以突破限制马？ 
    assert(datalen < CELL_DATA_SIZE);
    memcpy(&node->cells[cursor->cell_num].data, data, datalen);
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

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, void* data, size_t datalen)
{
    leaf_node_t *old_node= (leaf_node_t *)pager_get_page(cursor->btree->pager, cursor->page_num);
    uint32_t old_max = get_node_max_key(cursor->btree, (node_t*)old_node);

    // 1. 创建一个新叶子节点：作为兄弟节点放在右邻居
    uint8_t new_page_num = pager_get_unused_pagenum(cursor->btree->pager);
    leaf_node_t *new_node = (leaf_node_t *)pager_get_page(cursor->btree->pager, new_page_num);
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
            memcpy(&destination_cell->data, data, datalen);
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
        return create_new_root(cursor->btree, new_page_num);
    } else {
        // 原节点有parent
        // 1. 更新原节点对应cell中的ket
        uint8_t parent_page_num = old_node->meta.parent_page_num;
        uint32_t new_max = get_node_max_key(cursor->btree, (node_t*)old_node);
        internal_node_t *parent = (internal_node_t *)pager_get_page(cursor->btree->pager, parent_page_num);
        update_internal_node_key(parent, old_max, new_max);

        // 2. 父亲节点创建新cell指向新叶子节点
        internal_node_insert(cursor->btree, parent_page_num, new_page_num);
        return;
    }
}

/**
 * @brief 在叶子节点内部安全移除cell. 
 * 
 * @param node 
 * @param cell_num 
 */
void leaf_node_remove_cell(BTree *tree, leaf_node_t *node, uint32_t cell_num)
{
    uint32_t old_max_Key = get_node_max_key(tree, (node_t*)node);
    uint8_t num_cells = node->num_cells;
    for (size_t i = cell_num; i < node->num_cells; i++) {
        memcpy(&node->cells[i], &node->cells[i + 1], sizeof(leaf_node_cell_t));
    }
    node->num_cells -= 1;
    memset(&node->cells[node->num_cells], 0, sizeof(leaf_node_cell_t)); 

    if (cell_num == num_cells -1 && node->meta.parent_page_num != INVALID_PAGE_NUM) {
        // 如果移除cell的是最右边的， 并且node不是最右孩子，还需要更新parent cell中的key
        internal_node_t *parent = (internal_node_t*) pager_get_page(tree->pager, node->meta.parent_page_num);
        if (parent->right_child_page_num != node->meta.page_num) {
            update_internal_node_key(parent, old_max_Key, get_node_max_key(tree, (node_t*)node));
        }
    }
}

/**
 * @brief 叶子结点需要合并才能删除自己的cell
 *  合并相邻两个叶子节点 : 把Node2的cells追加到node1后面, 删除node2的cell
 * @param node1 
 * @param node2 
 */
leaf_node_t* leaf_node_merge_and_remove(Cursor* cursor, bool merge_to_left)
{
    BTree* btree = cursor->btree;
    Pager* pager = cursor->btree->pager;
    uint8_t cell_num = cursor->cell_num;

    leaf_node_t* node = (leaf_node_t* )pager_get_page(cursor->btree->pager, cursor->page_num);
    leaf_node_t *merge_node = NULL;
    uint32_t merge_node_old_max_key;
    uint32_t node_old_max_key = get_node_max_key(btree, (node_t*)node);

    // 内部安全移除cell
    leaf_node_remove_cell(cursor->btree, node, cell_num);
    
    if (merge_to_left) {
        merge_node = (leaf_node_t* )pager_get_page(cursor->btree->pager, get_left_page_num(btree, (node_t*)node));
        merge_node_old_max_key = get_node_max_key(cursor->btree, (node_t*)merge_node);

        // 2. 合并到左边
        memcpy(&merge_node->cells[merge_node->num_cells], node->cells, node->num_cells * sizeof(leaf_node_cell_t));
    } else {
        merge_node = (leaf_node_t* )pager_get_page(cursor->btree->pager, get_right_page_num(btree, (node_t*)node));
        merge_node_old_max_key = get_node_max_key(cursor->btree, (node_t*)merge_node);

        // 1. 

        // 1.先向右腾出位置
        memmove(merge_node->cells + node->num_cells, merge_node->cells, merge_node->num_cells * sizeof(leaf_node_cell_t));
        // 2. 拷贝到merge node
        memcpy(merge_node->cells, node->cells, node->num_cells * sizeof(leaf_node_cell_t));
    }
    merge_node->num_cells += node->num_cells;

    internal_node_t* parent = ( internal_node_t *)pager_get_page(btree->pager, node->meta.parent_page_num);
    update_internal_node_key(parent, merge_node_old_max_key, get_node_max_key(btree, (node_t*)merge_node));

    /* 更新父亲节点: 删除对应cell*/
    // 1. 如果node是最右孩子，删除mergenode对应cell， 将mergenode设置为右孩子
    if (node->meta.page_num == parent->right_child_page_num) {
        uint8_t parent_remove_cell_num = get_cellnum_in_parent(btree, (node_t*)merge_node);
        // node处填充mergenode内容， mergenode无用删除
        memcpy(node->cells, merge_node->cells, merge_node->num_cells * sizeof(leaf_node_cell_t));
        node->num_cells = merge_node->num_cells;

        internal_node_remove(btree, parent, parent_remove_cell_num);
        

    } else {
        // 2. 如果node不是最右孩子，直接移除对应cell, 更新node1对应cell
        uint8_t parent_remove_cell_num = get_cellnum_in_parent(btree, (node_t*)node);
        internal_node_remove(btree, parent,  parent_remove_cell_num );
    } 

    return merge_node;
}

/**
 * @brief 移除cursor的cell, 
 * 
 * @param cursor 
 */
void leaf_node_remove(Cursor *cursor)
{   
    leaf_node_t * node = (leaf_node_t*)pager_get_page(cursor->btree->pager, cursor->page_num);
    uint32_t node_old_max_key = get_node_max_key(cursor->btree, (node_t*)node);
    uint8_t num_cells = node->num_cells;
    if ((num_cells - 1 ) >= LEAF_NODE_MIN_CELLS || node->meta.is_root) {
        // 1. 不产生下溢，直接删除
        leaf_node_remove_cell(cursor->btree, node, cursor->cell_num);
        return;
    } else {
        // 2. 产生下溢出: 
        // 2.1 向兄弟节点借最邻近的cell
        uint8_t prev_leaf_pagenum = get_left_page_num(cursor->btree, (node_t*)node);
        uint8_t next_leaf_pagenum = get_right_page_num(cursor->btree, (node_t*)node);
        leaf_node_t* prev_leaf = NULL;
        leaf_node_t* next_leaf = NULL;
        uint8_t parent_page_num = node->meta.parent_page_num;
        internal_node_t* parent = NULL;
        if (parent_page_num != INVALID_PAGE_NUM) {
            parent = (internal_node_t*)pager_get_page(cursor->btree->pager, parent_page_num);
        }
        if (prev_leaf_pagenum != INVALID_PAGE_NUM) {
            prev_leaf = (leaf_node_t*)pager_get_page(cursor->btree->pager, prev_leaf_pagenum);
        }
        if (next_leaf_pagenum != INVALID_PAGE_NUM ) {
            next_leaf = (leaf_node_t*)pager_get_page(cursor->btree->pager, next_leaf_pagenum);
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
            leaf_node_remove_cell(cursor->btree, prev_leaf, prev_leaf->num_cells - 1);

            // 更新node结点对应父节点
            update_internal_node_key(parent, node_old_max_key, get_node_max_key(cursor->btree, (node_t*)node));

            return;
        } else if (next_leaf != NULL && next_leaf->num_cells - 1 >= LEAF_NODE_MIN_CELLS) {
            // 与借prev节点类似。
            uint32_t old_max_key = get_node_max_key(cursor->btree, (node_t*)node);
            // 1.节点移除cell，[cellnum:]往左偏移一个,让出最后一个cell 
            for (size_t i = cursor->cell_num + 1 ; i < next_leaf->num_cells; i++) {
                memcpy(&node->cells[i - 1], &node->cells[i], sizeof(leaf_node_cell_t));
            }
            // 2. 将next第一个的cell复制到 节点的最后cell, 需要更新parent中的key
            leaf_node_cell_t* first_cell = &next_leaf->cells[0];
            memcpy(&node->cells[node->num_cells - 1], first_cell, sizeof(leaf_node_cell_t));
            internal_node_t *parent = (internal_node_t*)pager_get_page(cursor->btree->pager, node->meta.parent_page_num);
            update_internal_node_key(parent, old_max_key, get_node_max_key(cursor->btree, (node_t*)node));
            // 3. next节点移除第一个cell
            leaf_node_remove_cell(cursor->btree, next_leaf, 0);
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
 * @brief 创建一个cursor指向第一个cell
 */
Cursor *btree_cursor_start(BTree *tree)
{
    // 返回最小的key数据 所在cursor
    Cursor *cursor = btree_cursor_find(tree, 0);
    leaf_node_t *node = (leaf_node_t*)pager_get_page(tree->pager, cursor->page_num);
    uint32_t num_cells = node->num_cells;
    cursor->end_of_table = (num_cells == 0);

    return cursor;
}
/**
 * @brief 找到key的位置，#pagenum#cellnum（叶子节点）
 * 
 * @param table 
 * @param key 
 * @return Cursor* 
 */
Cursor *btree_cursor_find(BTree *tree, uint32_t key)
{
    uint8_t root_page_num = tree->root_page_num;
    node_t *root_node = (node_t*)pager_get_page(tree->pager, root_page_num);

    if (get_node_type(root_node) == NODE_LEAF) {
        return leaf_node_find(tree, root_page_num, key);
    } else {
        return internal_node_find(tree, root_page_num, key);
    }
}
/**
 * @brief cursor 移动到一个cell
 */
void btree_cursor_advance(Cursor *cursor)
{
    uint8_t page_num = cursor->page_num;
    leaf_node_t *node = (leaf_node_t*)pager_get_page(cursor->btree->pager, page_num);
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
/**
 * @brief 通过cursor返回data指针。 不感知datalen
 */
uint8_t* btree_cursor_value(Cursor *cursor)
{
    uint8_t page_num = cursor->page_num;
    leaf_node_t *node = (leaf_node_t*)pager_get_page(cursor->btree->pager, page_num);
    uint8_t* row = malloc(CELL_DATA_SIZE);
    memcpy(row, &node->cells[cursor->cell_num].data, CELL_DATA_SIZE);
    return row;
}
void btree_cursor_free(Cursor* cursor)
{
    free(cursor);
}


/**
 * 打印 tree limit
 */
void btree_print_config(BTree* tree)
 {
    printf("BTree Configuration:\n");
    printf("tree m:%u\n", BTREE_M);
    printf("tree height:%u \n", BTREE_HEIGHT);

    printf("Page size: %d\n", PAGE_SIZE);
    printf("cell data size: %ld\n", sizeof(CELL_DATA_SIZE));

    printf("max rows in leafnode (theory)：%lu\n", (PAGE_SIZE - sizeof(common_header_t)) / sizeof(leaf_node_cell_t));
    printf("Total rows(theory): %lu\n", LEAF_NODE_MAX_CELLS *  (PAGE_SIZE - sizeof(common_header_t)) / sizeof(leaf_node_cell_t));

    printf("internal node numbers: \n");
    printf("leaf node numbers: 27\n");

    printf("INTERNAL_NODE_MAX_CELLS : %d\n", INTERNAL_NODE_MAX_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
    printf("LEAF_NODE_MIN_CELLS: %d\n", LEAF_NODE_MIN_CELLS);
    printf("LEAF_NODE_CELL_SIZE: %ld\n", sizeof(leaf_node_cell_t));
}
/**
 * @brief 打印btree详细
 * 
 * @param table 
 */
void btree_print(BTree * tree)
{
    btree_print_config(tree);

    node_t *node;
    Pager* pager = tree->pager;
    for (size_t i = 0; i < pager->num_pages; i++) {
        /* code */
        node = (node_t*)pager_get_page(tree->pager, i);

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

/* ==========btree============== */

/**
 * @brief 插入
 */
SQL4_CODE btree_insert(BTree* tree, uint32_t key, uint8_t* data, size_t datalen)
{
    printf("Btree [%d] insert : key [%d] datalen [%d]\n", tree->root_page_num, key, datalen); 
    Cursor *cursor = btree_cursor_find(tree, key);
    leaf_node_t* node = (leaf_node_t*)pager_get_page(tree->pager, cursor->page_num);

    if (cursor->cell_num < node->num_cells) {
        if (key == node->cells[cursor->cell_num].key) {
            return BTREE_DUPLICATE_KEY;
        }
    }

    leaf_node_insert(cursor, key, data, datalen);
    free(cursor);
    return BTREE_INSERT_SUCCESS;
}
/**
 * @brief select all
 * @param [out] selectsize 返回的行数
 * @param [out] data 二维数组，每行是对应数据
 */
SQL4_CODE btree_select(BTree* tree, size_t* selectsize, uint8_t*** data)
{
    Cursor *cursor = btree_cursor_start(tree);
    size_t retsize = 0;
    uint8_t** retdata = NULL;
    while (!cursor->end_of_table) {
        btree_cursor_advance(cursor);
        retsize++;
    }
    free(cursor);
    if (retsize == 0) {
        *selectsize = 0;
        data = NULL;
        return BTREE_SELECT_SUCCESS;
    }
    retdata = malloc(retsize * sizeof(uint8_t*));
    cursor = btree_cursor_start(tree);
    retsize = 0;
    while (!cursor->end_of_table) {
        retdata[retsize++] = btree_cursor_value(cursor);
        btree_cursor_advance(cursor);
    }
    
    free(cursor);
    
    *selectsize = retsize;
    *data = retdata;

    return BTREE_SELECT_SUCCESS;
}
/**
 * @brief delete
 */
SQL4_CODE btree_delete(BTree* tree, uint32_t key)
{
    Cursor* cursor = btree_cursor_find(tree, key);

    // 1. 
    leaf_node_remove(cursor);

    free(cursor);
    return BTREE_DELETE_SUCCESS;
}
// 对一个空白树，做最基本的设置，
void btree_init(uint32_t root_pagenum, Pager* pager)
{
    printf("init a blank btree\n");
    // 空白页是 root， 是leaf 
    node_t *root = (node_t*)pager_get_page(pager, root_pagenum);
    root->leaf.meta.is_root = 1;
    root->leaf.meta.node_type = NODE_LEAF;
    root->leaf.meta.page_num = root_pagenum;
    root->leaf.meta.parent_page_num = INVALID_PAGE_NUM;
    root->leaf.num_cells = 0;
    root->leaf.next_leaf_page_num = INVALID_PAGE_NUM;
    pager_flush(pager, root_pagenum);
}

// 获取一个树（rootpagenum）。可能树空、可能不空
BTree* btree_get(uint32_t root_pagenum, Pager* pager)
{
    assert(pager);
    BTree* tree = malloc(sizeof(BTree));
    tree->root_page_num = root_pagenum;
    tree->pager = pager;
    // 检测该tree是否是新的，即table是否空, 如果空，pager要扩容
    if (!pager_has_page(pager, root_pagenum)) {
        pager_add_page(pager, root_pagenum);
    }
    return tree;
}

// 返回树的最大key, 即最右孩子的key
int btree_get_newrowid(BTree* tree)
{
    node_t* root = (node_t*)pager_get_page(tree->pager, tree->root_page_num);
    return get_node_max_key(tree, root) + 1;
}

bool btree_is_empty(BTree* tree)
{
    // todo  未使用的函数
    return true;
}
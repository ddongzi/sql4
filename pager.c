#include "pager.h"
#include <unistd.h>
#include "btree.h"

void* pager_get_pages(Pager* p)
{
    return p->pages;
}

/* page */
void pager_free_page(Pager* p, uint32_t page_num)
{
    void* page = pager_get_page(p, page_num);
    free(page);
    p->pages[page_num] = NULL;
}

/* 总是假设 0..num_pages-1 已经被分配，*/
uint8_t pager_get_unused_pagenum(Pager *pager)
{
    return pager->num_pages;
}
// 是否有这个页， 默认，如果>=numpages 就没有
bool pager_has_page(Pager* p, uint32_t page_num)
{
    return page_num < p->num_pages;
}
// 目前仅支持顺序扩充+1. 即参数page_num == p->num_pages
void pager_add_page(Pager* p, uint32_t page_num) {
    assert(page_num == p->num_pages);
    // 文件扩展一个页的大小. 访问不能超越文件长度
    ftruncate(p->fd, (p->num_pages + 1) * PAGE_SIZE);
    p->num_pages += 1;
    p->pages = realloc(p->pages, sizeof(void*) * (p->num_pages));   
    printf("pager add page [%d] done\n", page_num); 
    // 对btree做一些最基本的初始化设置：leafnode
    btree_init(page_num, p);
}

/**
 * @brief 节点获取必须通过getpage，不可以通过pager.pagers直接访问
 * 
 * @param pager ：pages管理
 * @param page_num ：
 * @return void* ：一个page
 */
void *pager_get_page(Pager* pager, uint8_t page_num)
{
    if (page_num >= pager->num_pages) {
        printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
               pager->num_pages);
        return NULL;
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

/*
 * 打开文件， 初始化pager
 * */
Pager *pager_open(const char *file_name)
{
    // TODO
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
    // TODO db如果是固定大小，PAGES是固定的； 如果是动态增长，那么pager中pages就是变化的。
    Pager *pager = (Pager *) malloc(sizeof(Pager));
    pager->fd = fd;
    pager->file_length = file_length;
    pager->num_pages = file_length / PAGE_SIZE; // 
    if (file_length % PAGE_SIZE != 0) {
        printf("DB file is not a whole number of pages. corrupt file.\n");
        exit(EXIT_FAILURE);
    }
    pager->pages = malloc(sizeof(void*) * pager->num_pages);
    for (uint32_t i = 0; i < pager->num_pages; ++i) {
        pager->pages[i] = NULL;
    }
    
    return pager;
}


/* 将page刷盘
 * 整页刷盘， cell不会跨page
 * */
void pager_flush(Pager *pager, uint8_t page_num)
{
    printf("pager flush [%u]\n", page_num);
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
void pager_free(Pager * pager)
{
    assert(pager);
    for (uint32_t i = 0; i < pager->num_pages; ++i) {
        if (pager_get_page(pager, i) == NULL) {
            continue;
        }
        pager_flush(pager, i);
        pager_free_page(pager, i);
    }
    printf("pager free ok");
}
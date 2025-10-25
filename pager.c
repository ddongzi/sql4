#include "pager.h"


void* pager_get_pages(Pager* p)
{
    return p->pages;
}

/* page */
void pager_free_page(Pager* p, uint32_t page_num)
{
    void* page = pager_get_page(p, page_num);
    assert(page);
    free(page);
    page = NULL;
}

/* 总是假设 0..num_pages-1 已经被分配，*/
uint8_t pager_get_unused_pagenum(Pager *pager)
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
void *pager_get_page(Pager* pager, uint8_t page_num)
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

/*
 * 打开文件， 初始化pager
 * */
Pager *pager_open(const char *file_name)
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
    Pager *pager = (Pager *) malloc(sizeof(Pager));
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


/* 将page刷盘
 * 整页刷盘， cell不会跨page
 * */
void pager_flush(Pager *pager, uint8_t page_num)
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
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "sql4limit.h"

/*使用 pager 访问页缓存和文件*/
typedef struct {
    int fd;
    uint32_t file_length; // 追加写。 写的起始位置. 是page_size 整数倍
    uint32_t num_pages; // 页个数
    void *pages[TABLE_MAX_PAGES];
} Pager;

Pager *pager_open(const char *file_name);

int pager_get_fd(Pager* p);
uint32_t pager_get_file_length(Pager* p);
uint32_t pager_get_num_pages(Pager* p);

/* page */
void* pager_get_page(Pager* p, uint32_t page_num);
void pager_free_page(Pager* p, uint32_t page_num);
uint8_t pager_get_unused_page_num(Pager *pager);

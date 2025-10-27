#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include "sql4limit.h"

/*使用 pager 访问页缓存和文件*/
// 每个db文件持有一个pager
typedef struct {
    int fd;
    uint32_t file_length; // 追加写。 写的起始位置. 是page_size 整数倍
    uint32_t num_pages; // 页个数,
    void* *pages; // void* 指向一个PAGESIZE的内存，即页缓存
} Pager;

Pager *pager_open(const char *file_name);
void *pager_get_page(Pager* pager, uint8_t page_num);
void pager_flush(Pager *pager, uint8_t page_num);
void pager_add_page(Pager* p, uint32_t page_num);

/* page */
void pager_free_page(Pager* p, uint32_t page_num);
uint8_t pager_get_unused_pagenum(Pager *pager);
bool pager_has_page(Pager* p, uint32_t page_num);

#endif // PAGER_H
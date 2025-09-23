#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "sql4limit.h"

typedef struct pager_t pager_t;

pager_t *pager_open(const char *file_name);

int pager_get_fd(pager_t* p);
uint32_t pager_get_file_length(pager_t* p);
uint32_t pager_get_num_pages(pager_t* p);

/* page */
void* pager_get_page(pager_t* p, uint32_t page_num);
void pager_free_page(pager_t* p, uint32_t page_num);
uint8_t pager_get_unused_page_num(pager_t *pager);

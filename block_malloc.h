#ifndef BLOCK_MALLOC_H
#define BLOCK_MALLOC_H

#include <stddef.h>

struct block {
    size_t size;
    struct block *next;
    int free;
};

void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);
void my_heap_info(void);

#endif

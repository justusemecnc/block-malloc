#include "block_malloc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#define ALIGN 16
#define PAGE_SIZE 4096
#define HEAP_CHUNK (64 * 1024)
#define MAX_REGIONS 32

static size_t alignUp(size_t n, size_t align)
{
    return (n + align - 1) & ~(align - 1);
}

#define PAYLOAD_OFFSET alignUp(sizeof(struct block), ALIGN)
#define MIN_BLOCK_SIZE (PAYLOAD_OFFSET + ALIGN)

typedef struct {
    void *base;
    size_t size;
} heapRegion;

static struct block *freeList = NULL;
static heapRegion regions[MAX_REGIONS];
static int regionCount = 0;

static void *blockPayload(struct block *b)
{
    return (char *)b + PAYLOAD_OFFSET;
}

static size_t payloadCapacity(struct block *b)
{
    return b->size - PAYLOAD_OFFSET;
}

static size_t blockSizeForPayload(size_t payload)
{
    return alignUp(PAYLOAD_OFFSET + alignUp(payload, ALIGN), ALIGN);
}

static struct block *blockFromPtr(void *ptr)
{
    if (!ptr) {
        return NULL;
    }

    return (struct block *)((char *)ptr - PAYLOAD_OFFSET);
}

static int regionContains(heapRegion *region, struct block *b)
{
    uintptr_t start = (uintptr_t)region->base;
    uintptr_t end = start + region->size;
    uintptr_t addr = (uintptr_t)b;

    return addr >= start && addr < end;
}

static struct block *findRegionBlock(void *ptr)
{
    struct block *b = blockFromPtr(ptr);
    if (!b) {
        return NULL;
    }

    for (int i = 0; i < regionCount; i++) {
        if (regionContains(&regions[i], b)) {
            return b;
        }
    }

    return NULL;
}

static void removeFromFreeList(struct block *target)
{
    struct block **cursor = &freeList;

    while (*cursor) {
        if (*cursor == target) {
            *cursor = target->next;
            target->next = NULL;
            return;
        }
        cursor = &(*cursor)->next;
    }
}

static void pushFreeList(struct block *b)
{
    b->free = 1;
    b->next = freeList;
    freeList = b;
}

#ifdef _WIN32
static void *mapMemory(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
#else
static void *mapMemory(size_t size)
{
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}
#endif

static int growHeap(size_t minBlockSize)
{
    if (regionCount >= MAX_REGIONS) {
        return 0;
    }

    size_t regionSize = HEAP_CHUNK;
    if (minBlockSize > regionSize) {
        regionSize = alignUp(minBlockSize, PAGE_SIZE);
    }

    void *base = mapMemory(regionSize);
#ifdef _WIN32
    if (!base) {
        return 0;
    }
#else
    if (base == MAP_FAILED) {
        return 0;
    }
#endif

    struct block *head = (struct block *)base;
    head->size = regionSize;
    head->free = 1;
    head->next = NULL;

    regions[regionCount].base = base;
    regions[regionCount].size = regionSize;
    regionCount++;

    pushFreeList(head);
    return 1;
}

static struct block *findPrevBlock(heapRegion *region, struct block *current)
{
    struct block *cursor = (struct block *)region->base;
    struct block *prev = NULL;
    uintptr_t end = (uintptr_t)region->base + region->size;

    while ((uintptr_t)cursor < end && cursor != current) {
        if (cursor->size == 0) {
            break;
        }
        prev = cursor;
        cursor = (struct block *)((char *)cursor + cursor->size);
    }

    return prev;
}

static struct block *nextPhysicalBlock(heapRegion *region, struct block *current)
{
    struct block *next = (struct block *)((char *)current + current->size);
    uintptr_t end = (uintptr_t)region->base + region->size;

    if ((uintptr_t)next >= end) {
        return NULL;
    }

    return next;
}

static void coalesceBlock(struct block *b)
{
    heapRegion *region = NULL;

    for (int i = 0; i < regionCount; i++) {
        if (regionContains(&regions[i], b)) {
            region = &regions[i];
            break;
        }
    }

    if (!region) {
        return;
    }

    struct block *prev = findPrevBlock(region, b);

    if (prev && prev->free) {
        removeFromFreeList(prev);
        prev->size += b->size;
        b = prev;
    }

    struct block *next = nextPhysicalBlock(region, b);
    if (next && next->free) {
        removeFromFreeList(next);
        b->size += next->size;
    }

    pushFreeList(b);
}

static void splitBlock(struct block *b, size_t neededSize)
{
    if (b->size - neededSize < MIN_BLOCK_SIZE) {
        return;
    }

    struct block *remainder =
        (struct block *)((char *)b + neededSize);
    remainder->size = b->size - neededSize;
    remainder->free = 1;
    remainder->next = NULL;
    b->size = neededSize;

    pushFreeList(remainder);
}

void *my_malloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size_t needed = blockSizeForPayload(size);

    for (int attempt = 0; attempt < 2; attempt++) {
        struct block *cursor = freeList;

        while (cursor) {
            if (cursor->size >= needed) {
                removeFromFreeList(cursor);
                splitBlock(cursor, needed);
                cursor->free = 0;
                cursor->next = NULL;
                return blockPayload(cursor);
            }

            cursor = cursor->next;
        }

        if (!growHeap(needed)) {
            return NULL;
        }
    }

    return NULL;
}

void my_free(void *ptr)
{
    if (!ptr) {
        return;
    }

    struct block *b = findRegionBlock(ptr);
    if (!b || b->free) {
        return;
    }

    b->free = 1;
    b->next = NULL;
    coalesceBlock(b);
}

void *my_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    if (nmemb > SIZE_MAX / size) {
        return NULL;
    }

    size_t total = nmemb * size;
    void *ptr = my_malloc(total);
    if (!ptr) {
        return NULL;
    }

    memset(ptr, 0, total);
    return ptr;
}

void *my_realloc(void *ptr, size_t size)
{
    if (!ptr) {
        return my_malloc(size);
    }

    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    struct block *b = findRegionBlock(ptr);
    if (!b) {
        return NULL;
    }

    size_t oldPayload = payloadCapacity(b);
    size_t needed = blockSizeForPayload(size);

    if (needed <= b->size) {
        splitBlock(b, needed);
        return ptr;
    }

    heapRegion *region = NULL;
    for (int i = 0; i < regionCount; i++) {
        if (regionContains(&regions[i], b)) {
            region = &regions[i];
            break;
        }
    }

    if (region) {
        struct block *next = nextPhysicalBlock(region, b);
        if (next && next->free && b->size + next->size >= needed) {
            removeFromFreeList(next);
            b->size += next->size;
            splitBlock(b, needed);
            return ptr;
        }
    }

    void *fresh = my_malloc(size);
    if (!fresh) {
        return NULL;
    }

    size_t copySize = oldPayload < size ? oldPayload : size;
    memcpy(fresh, ptr, copySize);
    my_free(ptr);
    return fresh;
}

void my_heap_info(void)
{
    size_t total = 0;
    size_t freeBytes = 0;
    size_t usedBytes = 0;

    for (int i = 0; i < regionCount; i++) {
        total += regions[i].size;
    }

    for (struct block *cursor = freeList; cursor; cursor = cursor->next) {
        freeBytes += cursor->size;
    }

    for (int i = 0; i < regionCount; i++) {
        struct block *cursor = (struct block *)regions[i].base;
        uintptr_t end = (uintptr_t)regions[i].base + regions[i].size;

        while ((uintptr_t)cursor < end && cursor->size > 0) {
            if (!cursor->free) {
                usedBytes += cursor->size;
            }
            cursor = (struct block *)((char *)cursor + cursor->size);
        }
    }

    printf("heap total: %zu bytes\n", total);
    printf("heap free:  %zu bytes\n", freeBytes);
    printf("heap used:  %zu bytes\n", usedBytes);
}

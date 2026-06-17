#include "block_malloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALIGN 16

static int testsRun = 0;
static int testsFailed = 0;

static void check(int condition, const char *name)
{
    testsRun++;
    if (!condition) {
        testsFailed++;
        printf("FAIL: %s\n", name);
    } else {
        printf("PASS: %s\n", name);
    }
}

static void testBasicAllocFree(void)
{
    char *buf = my_malloc(64);
    check(buf != NULL, "basic alloc returns pointer");

    memset(buf, 0xAB, 64);
    check(buf[0] == (char)0xAB && buf[63] == (char)0xAB, "basic alloc writable");

    my_free(buf);
    check(1, "basic free does not crash");
}

static void testMultipleAllocs(void)
{
    void *a = my_malloc(32);
    void *b = my_malloc(64);
    void *c = my_malloc(128);

    check(a != NULL && b != NULL && c != NULL, "multiple allocs succeed");
    check(a != b && b != c && a != c, "multiple allocs distinct");

    my_free(a);
    my_free(b);
    my_free(c);
}

static void testCoalescing(void)
{
    void *a = my_malloc(256);
    void *b = my_malloc(256);
    void *c = my_malloc(256);

    check(a && b && c, "coalesce setup allocs");

    my_free(a);
    my_free(c);
    my_free(b);

    void *merged = my_malloc(700);
    check(merged != NULL, "coalesced block large enough");

    my_free(merged);
}

static void testSplitting(void)
{
    void *big = my_malloc(512);
    check(big != NULL, "split setup alloc");

    my_free(big);

    void *smallA = my_malloc(128);
    void *smallB = my_malloc(128);

    check(smallA != NULL && smallB != NULL, "split produces two blocks");
    check(smallA != smallB, "split blocks are distinct");

    my_free(smallA);
    my_free(smallB);
}

static void testCalloc(void)
{
    int *values = my_calloc(16, sizeof(int));
    check(values != NULL, "calloc returns pointer");

    int allZero = 1;
    for (int i = 0; i < 16; i++) {
        if (values[i] != 0) {
            allZero = 0;
            break;
        }
    }
    check(allZero, "calloc zero-fills memory");

    my_free(values);
}

static void testRealloc(void)
{
    char *buf = my_malloc(32);
    check(buf != NULL, "realloc setup alloc");
    strcpy(buf, "hello");

    char *grown = my_realloc(buf, 128);
    check(grown != NULL, "realloc grow succeeds");
    check(strcmp(grown, "hello") == 0, "realloc preserves data");

    char *shrunk = my_realloc(grown, 8);
    check(shrunk != NULL, "realloc shrink succeeds");
    check(strncmp(shrunk, "hello", 5) == 0, "realloc shrink keeps prefix");

    void *fresh = my_realloc(NULL, 16);
    check(fresh != NULL, "realloc NULL acts like malloc");

    void *gone = my_realloc(fresh, 0);
    check(gone == NULL, "realloc size zero frees");

    my_free(shrunk);
}

static void testHeapInfo(void)
{
    void *a = my_malloc(64);
    void *b = my_malloc(128);
    check(a && b, "heap info setup");

    printf("--- heap info ---\n");
    my_heap_info();
    printf("-----------------\n");

    check(1, "heap info smoke");

    my_free(a);
    my_free(b);
}

static void testAlignment(void)
{
    void *ptr = my_malloc(1);
    check(ptr != NULL, "alignment alloc");
    check(((uintptr_t)ptr % ALIGN) == 0, "pointer is 16-byte aligned");
    my_free(ptr);
}

int main(void)
{
    testBasicAllocFree();
    testMultipleAllocs();
    testCoalescing();
    testSplitting();
    testCalloc();
    testRealloc();
    testHeapInfo();
    testAlignment();

    printf("\n%d tests run, %d failed\n", testsRun, testsFailed);
    return testsFailed == 0 ? 0 : 1;
}

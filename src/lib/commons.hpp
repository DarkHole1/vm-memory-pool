#pragma once
#include <iostream>
#include <mutex>
#include <atomic>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/mman.h>

#define CHECK(X, MSG)       \
    if (!(X))               \
    {                       \
        perror((MSG));      \
        exit(EXIT_FAILURE); \
    }

// For x != 0
#define CEILING_DIV(X, Y) (1 + (((X) - 1) / (Y)))

struct PoolEntry
{
    std::atomic<void*> start, end;
};

struct
{
    int max = 0;
    struct PoolEntry *list = nullptr;
} pools;

void init_handler(int max_pools);
void add_pool(void *start, void *end);
void remove_pool(void *start);

template <class T>
static void init_pool(unsigned size, unsigned element_size, T &pool)
{
    auto page_size = sysconf(_SC_PAGESIZE);
    auto pool_size = CEILING_DIV(size, page_size) * page_size;
    auto protected_page_size = CEILING_DIV(element_size, page_size) * page_size;
    pool.size = protected_page_size + pool_size;
    pool.start = mmap(nullptr, pool.size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    CHECK(pool.start != (void *)-1, "Cannot create mmap");
    CHECK(mprotect(pool.start, protected_page_size, PROT_NONE) == 0, "Cannot protect first page");

    pool.current = reinterpret_cast<char *>(pool.start) + pool.size;
    add_pool(pool.start, reinterpret_cast<char *>(pool.start) + pool.size);
};

template <class T>
static void release_pool(T &pool)
{
    remove_pool(pool.start);
    CHECK(munmap(pool.start, pool.size) == 0, "Cannot call munmap");
};

void get_usage(struct rusage &usage);

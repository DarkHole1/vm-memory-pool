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

struct PoolEntry
{
    std::atomic<bool> is_used;
    void *start;
    void *end;
};

struct
{
    std::mutex m;
    std::atomic<int> count = 0;
    std::atomic<int> used_count = 0;
    int max = 0;
    struct PoolEntry *list = nullptr;
} pools;

void init_handler(int max_pools);
void add_pool(void *start, void *end);
void remove_pool(void *start);

template <class T>
static void init_pool(unsigned size, T &pool)
{
    auto page_size = sysconf(_SC_PAGESIZE);
    auto half_size = size / page_size;
    pool.size = (half_size * 2 + 1) * page_size;
    pool.start = mmap(nullptr, pool.size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    CHECK(pool.start != (void *)-1, "Cannot create mmap");
    CHECK(mprotect(pool.start, half_size * page_size, PROT_NONE) == 0, "Cannot protect first page");

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

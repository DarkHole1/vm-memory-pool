#pragma once
#include <iostream>
#include <mutex>
#include <atomic>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/resource.h>

#define CHECK(X, MSG)       \
    if (!(X))               \
    {                       \
        perror((MSG));      \
        exit(EXIT_FAILURE); \
    }

struct PoolEntry
{
    void *start;
    void *end;
};

struct
{
    std::mutex m;
    std::atomic<int> count = 0;
    int max = 0;
    struct PoolEntry *list = nullptr;
} pools;

struct sigaction newsa = {};
struct sigaction oldsa = {};

const char *DIGIT_TABLE = "0123456789abcdef";
static void overflow_handler([[maybe_unused]] int signum, siginfo_t *info, [[maybe_unused]] void *context)
{
    void *fault_address = info->si_addr;

    int pool = -1;
    for (int i = 0; i < pools.count; i++)
    {
        if (pools.list[i].start <= fault_address && pools.list[i].end > fault_address)
        {
            pool = i;
            break;
        }
    }

    if (pool >= 0)
    {
        char msg[60] = "Overflow occured\nAddress: 0x1234567812345678\nIn pool: 0000\n";
        for (uintptr_t i = 0, cur = reinterpret_cast<uintptr_t>(fault_address); i < 16; i++, cur /= 16)
        {
            msg[43 - i] = DIGIT_TABLE[cur % 16];
        }

        for (int i = 0, cur = pool; i < 4; i++, cur /= 10)
        {
            msg[57 - i] = DIGIT_TABLE[cur % 10];
        }

        write(STDERR_FILENO, msg, 59);
        exit(EXIT_FAILURE);
    }
    else
    {
        sigaction(signum, &oldsa, NULL);
        raise(signum);
        sigaction(signum, &newsa, NULL);
    }
}

void init_handler(int max_pools)
{
    newsa.sa_sigaction = &overflow_handler;
    sigemptyset(&newsa.sa_mask);
    newsa.sa_flags = SA_SIGINFO;
    CHECK(sigaction(SIGSEGV, &newsa, &oldsa) != -1, "Cannot install SIGSEGV handler");
    pools.list = reinterpret_cast<PoolEntry *>(malloc(max_pools * sizeof(PoolEntry)));
    pools.max = max_pools;
}

void add_pool(void *start, void *end)
{
    std::unique_lock lock(pools.m);
    auto new_pool_id = pools.count.load();
    CHECK(new_pool_id < pools.max, "Too many pools");

    pools.list[new_pool_id] = {
        .start = start,
        .end = end};
    pools.count.store(new_pool_id + 1, std::memory_order_release);
}

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
    CHECK(munmap(pool.start, pool.size) == 0, "Cannot call munmap");
};

static void get_usage(struct rusage &usage)
{
    CHECK(!getrusage(RUSAGE_SELF, &usage), "Cannot get usage");
}
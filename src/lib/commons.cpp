#include <iostream>
#include <atomic>
#include <signal.h>
#include "commons.hpp"

static struct sigaction newsa = {};
static struct sigaction oldsa = {};

const char *DIGIT_TABLE = "0123456789abcdef";
static void overflow_handler([[maybe_unused]] int signum, siginfo_t *info, [[maybe_unused]] void *context)
{
    void *fault_address = info->si_addr;

    pools.used_count.fetch_add(1, std::memory_order_acquire);
    int pool = -1;
    for (int i = 0; i < pools.count.load(std::memory_order_acquire); i++)
    {
        PoolEntry &entry = pools.list[i];
        if (entry.is_used.load(std::memory_order_acquire) && entry.start <= fault_address && entry.end > fault_address)
        {
            pool = i;
            break;
        }
    }
    pools.used_count.fetch_sub(1, std::memory_order_release);

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
    // Do not allocate while handler is active
    while (pools.used_count != 0)
        ;

    for (int i = 0; i < pools.max; i++)
    {
        PoolEntry &entry = pools.list[i];
        if (!entry.is_used)
        {
            entry.start = start;
            entry.end = end;
            entry.is_used.store(true, std::memory_order_release);
            return;
        }
    }

    CHECK(false, "Too many pools");
}

void remove_pool(void *start)
{
    std::unique_lock lock(pools.m);
    for (int i = 0; i < pools.max; i++)
    {
        PoolEntry &entry = pools.list[i];
        if (entry.is_used && entry.start == start)
        {
            entry.is_used.store(false, std::memory_order_release);
            return;
        }
    }
    CHECK(false, "Removing non-existing pool");
}

void get_usage(struct rusage &usage)
{
    CHECK(!getrusage(RUSAGE_SELF, &usage), "Cannot get usage");
}
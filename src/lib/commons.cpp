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

    int pool = -1;
    for (int i = 0; i < pools.max; i++)
    {
        PoolEntry &entry = pools.list[i];
        void *e_start = entry.start;
        void *e_end = entry.end;
        if (e_start != nullptr && e_end != nullptr && 
            entry.start <= fault_address && entry.end > fault_address)
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
    for (int i = 0; i < max_pools; i++)
    {
        pools.list[i].start = nullptr;
        pools.list[i].end = nullptr;
    }
}

void add_pool(void *start, void *end)
{
    NEXT_LOOP:
    while (true)
    {
        for (int i = 0; i < pools.max; i++)
        {
            auto &entry = pools.list[i];
            void *e_start = entry.start.load();
            void *e_end = entry.end.load();
            if (e_start == nullptr && e_end == nullptr)
            {
                if(!entry.end.compare_exchange_weak(e_end, end)) {
                    goto NEXT_LOOP;
                }

                entry.start.store(start);
                return;
            }
        }
        
        CHECK(false, "Too many pools");
    }
}

void remove_pool(void *start)
{
    NEXT_LOOP:
    while (true)
    {
        for (int i = 0; i < pools.max; i++)
        {
            auto &entry = pools.list[i];
            void *e_start = entry.start.load();
            if (e_start == start)
            {
                if(!entry.start.compare_exchange_weak(e_start, nullptr)) {
                    goto NEXT_LOOP;
                }

                entry.end.store(nullptr);
                return;
            }
        }
        
        CHECK(false, "Removing non-existing pool");
    }
}

void get_usage(struct rusage &usage)
{
    CHECK(!getrusage(RUSAGE_SELF, &usage), "Cannot get usage");
}
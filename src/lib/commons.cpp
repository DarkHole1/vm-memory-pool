#include <iostream>
#include <mutex>
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
    int count = 0;
    struct PoolEntry *list = nullptr;
} pools;

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
            msg[58 - i] = DIGIT_TABLE[cur % 10];
        }

        write(STDERR_FILENO, msg, 59);
        exit(EXIT_FAILURE);
    }
    else
    {
        // TODO: Pass down
    }

    exit(EXIT_FAILURE);
}

static void get_usage(struct rusage &usage)
{
    CHECK(!getrusage(RUSAGE_SELF, &usage), "Cannot get usage");
}
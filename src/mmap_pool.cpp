#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>

using namespace std;

static void get_usage(struct rusage &usage)
{
  if (getrusage(RUSAGE_SELF, &usage))
  {
    perror("Cannot get usage");
    exit(EXIT_SUCCESS);
  }
}

struct Node
{
  Node *next;
  unsigned node_id;
};

struct
{
  void *start;
  void *current;
  size_t size;
} pool;

static void overflow_handler(int signum, siginfo_t *info, void *context)
{
  void *fault_address = info->si_addr;

  fprintf(stderr, "Overflow occured\n");
  fprintf(stderr, "Address: %p\n", fault_address);

  exit(EXIT_FAILURE);
}

static void init_pool(unsigned size)
{
  auto page_size = sysconf(_SC_PAGESIZE);
  pool.size = size + page_size;
  pool.start = mmap(nullptr, pool.size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (pool.start == (void *)-1)
  {
    perror("Cannot create mmap");
    exit(EXIT_SUCCESS);
  }

  if (mprotect(pool.start, page_size, PROT_NONE) != 0)
  {
    perror("Cannot protect first page");
    exit(EXIT_SUCCESS);
  }

  pool.current = reinterpret_cast<char *>(pool.start) + pool.size;
}

static void release_pool()
{
  if (munmap(pool.start, pool.size) != 0)
  {
    perror("Cannot call munmap");
    exit(EXIT_SUCCESS);
  }
}

static void *alloc_pool(unsigned n)
{
  auto result = reinterpret_cast<char *>(pool.current) - n;
  pool.current = result;
  return result;
}

static inline Node *create_list(unsigned n)
{
  init_pool(n * sizeof(Node));

  Node *list = nullptr;
  for (unsigned i = 0; i < n; i++)
  {
    Node *new_node = reinterpret_cast<Node *>(alloc_pool(sizeof(Node)));
    *new_node = Node({list, i});
    list = new_node;
  }
  return list;
}

static inline void delete_list(Node *list)
{
  release_pool();
}

static inline void test(unsigned n)
{
  struct rusage start, finish;
  get_usage(start);

  struct sigaction sa = {};
  sa.sa_sigaction = &overflow_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO;
  if (sigaction(SIGSEGV, &sa, NULL) == -1)
  {
    perror("Cannot install SIGSEGV handler");
    exit(EXIT_SUCCESS);
  }

  delete_list(create_list(n));

  get_usage(finish);

  struct timeval diff;
  timersub(&finish.ru_utime, &start.ru_utime, &diff);
  uint64_t time_used = diff.tv_sec * 1000000 + diff.tv_usec;
  cout << "Time used: " << time_used << " usec\n";

  uint64_t mem_used = (finish.ru_maxrss - start.ru_maxrss) * 1024;
  cout << "Memory used: " << mem_used << " bytes\n";

  auto mem_required = n * sizeof(Node);
  auto overhead = (mem_used >= mem_required ? mem_used - mem_required : 0) * double(100) / mem_used;
  cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
       << overhead << "%\n";
}

int main(const int argc, const char *argv[])
{
  test(10000000);
  return EXIT_SUCCESS;
}
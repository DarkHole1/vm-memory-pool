#include <iostream>
#include <iomanip>
#include <mutex>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>
#include "lib/commons.cpp"

using namespace std;

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
} thread_local pool;

static void init_pool(unsigned size)
{
  auto page_size = sysconf(_SC_PAGESIZE);
  pool.size = size + page_size;
  pool.start = mmap(nullptr, pool.size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  CHECK(pool.start != (void *)-1, "Cannot create mmap");
  CHECK(mprotect(pool.start, page_size, PROT_NONE) == 0, "Cannot protect first page");

  pool.current = reinterpret_cast<char *>(pool.start) + pool.size;

  add_pool(pool.start, reinterpret_cast<char *>(pool.start) + pool.size);
}

static void release_pool()
{
  CHECK(munmap(pool.start, pool.size) == 0, "Cannot call munmap");
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

static inline void delete_list([[maybe_unused]] Node *list)
{
  release_pool();
}

static inline void *test_handler(void *n)
{
  delete_list(create_list(reinterpret_cast<unsigned long>(n)));
  return nullptr;
}

static inline void test(unsigned n, int m)
{
  struct rusage start, finish;
  get_usage(start);

  init_handler(m);

  pthread_t *threads = reinterpret_cast<pthread_t *>(malloc(m * sizeof(pthread_t)));
  for (int i = 0; i < m; i++)
  {
    pthread_create(&threads[i], NULL, test_handler, reinterpret_cast<void *>(n));
  }

  for (int i = 0; i < m; i++)
  {
    pthread_join(threads[i], NULL);
  }

  get_usage(finish);

  struct timeval diff;
  timersub(&finish.ru_utime, &start.ru_utime, &diff);
  uint64_t time_used = diff.tv_sec * 1000000 + diff.tv_usec;
  cout << "Time used: " << time_used << " usec\n";

  uint64_t mem_used = (finish.ru_maxrss - start.ru_maxrss) * 1024;
  cout << "Memory used: " << mem_used << " bytes\n";

  auto mem_required = n * m * sizeof(Node);
  auto overhead = (mem_used >= mem_required ? mem_used - mem_required : 0) * double(100) / mem_used;
  cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
       << overhead << "%\n";
}

int main(const int argc, const char *argv[])
{
  if (argc < 2)
  {
    cout << "Usage: program [THREADS]\n";
    exit(EXIT_SUCCESS);
  }

  int m = atoi(argv[1]);
  test(10000000, m);
  return EXIT_SUCCESS;
}
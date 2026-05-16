#include <iostream>
#include <iomanip>
#include <atomic>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "lib/commons.hpp"

using namespace std;

struct Node
{
  Node *next;
  unsigned node_id;
};

struct
{
  void *start;
  atomic<char *> current;
  size_t size;
} pool;

static inline void *alloc_pool(unsigned n)
{
  return pool.current.fetch_sub(n, memory_order_relaxed) - n;
}

static inline Node *create_list(unsigned long n)
{
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

  init_handler(1);

  init_pool(n * m * sizeof(Node) + m * sizeof(pthread_t), max(sizeof(Node), sizeof(pthread_t)), pool);

  pthread_t *threads = reinterpret_cast<pthread_t *>(alloc_pool(m * sizeof(pthread_t)));

  for (int i = 0; i < m; i++)
  {
    pthread_create(&threads[i], NULL, test_handler, reinterpret_cast<void *>(n));
  }

  for (int i = 0; i < m; i++)
  {
    pthread_join(threads[i], NULL);
  }

  release_pool(pool);

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
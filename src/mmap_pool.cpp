#include <iostream>
#include <iomanip>
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
} pool;

static inline void *alloc_pool(unsigned n)
{
  auto result = reinterpret_cast<char *>(pool.current) - n;
  pool.current = result;
  return result;
}

static inline Node *create_list(unsigned n)
{
  init_pool(n * sizeof(Node), pool);

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
  release_pool(pool);
}

static inline void test(unsigned n)
{
  struct rusage start, finish;
  get_usage(start);

  init_handler(1);

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

int main([[maybe_unused]] const int argc, [[maybe_unused]] const char *argv[])
{
  test(10000000);
  return EXIT_SUCCESS;
}
# vm-memory-pool

# Build

```bash
make
```

Built programs at `dist`.

# Programs

| Name                    | Description                                      |
| ----------------------- | ------------------------------------------------ |
| `original`              | Original program, uses `malloc`                  |
| `mmap_pool`             | Linear memory pool backed by `mmap`              |
| `mmap_mutex_pool`       | Multithread `mmap` pool with mutex               |
| `mmap_nonblocking_pool` | Multithread `mmap` pool with atomics             |
| `mmap_tls_pool`         | Multithread `mmap` pool with one pool for thread |

# Results

| Name                       | Time (usec) | Memory (bytes) | Overhead (%) |
| -------------------------- | ----------: | -------------: | -----------: |
| `original`                 |      287045 |      317452288 |         49.6 |
| `mmap_pool`                |       11155 |      157401088 |          0.0 |
| `mmap_mutex_pool 16`       |    26478277 |     2557689856 |          0.0 |
| `mmap_nonblocking_pool 16` |   112194953 |     2557607936 |          0.0 |
| `mmap_tls_pool`            |      881676 |     1291038720 |          0.0 |

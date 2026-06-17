# Block Malloc

A custom heap allocator in C with first-fit allocation, block splitting, and coalescing. Built to understand what happens below `malloc`.

## What it does

- Implements `my_malloc`, `my_free`, `my_calloc`, `my_realloc`, and `my_heap_info`
- Requests memory from the OS directly (`mmap` on Unix, `VirtualAlloc` on Windows)
- Maintains a free list with first-fit search
- Splits oversized blocks and merges adjacent free blocks on free
- Includes unit tests for alloc, coalesce, split, calloc, realloc, and alignment

## Requirements

- A C compiler (GCC, Clang, or MSVC)
- `make` on Linux/macOS/WSL (optional on Windows)

## Run locally

1. Clone the project:

   ```bash
   git clone https://github.com/justusemecnc/block-malloc.git
   cd block-malloc
   ```

2. Build and run the tests:

   Linux / macOS / WSL:

   ```bash
   make test
   ```

   Windows with MinGW:

   ```bash
   gcc -Wall -Wextra -std=c11 -o test.exe block_malloc.c test.c
   test.exe
   ```

   Windows with MSVC:

   ```bash
   cl /W4 /TC block_malloc.c test.c /Fe:test.exe
   test.exe
   ```
// memory.h - memory manager (contiguous allocation with first/best/worst fit).

#ifndef MINIOS_MEMORY_H
#define MINIOS_MEMORY_H

#include <string>
#include <vector>
using namespace std;

// Gives process `pid` a free block of `size` KB using the current fit strategy.
// Returns the start address, or -1 if no free block is big enough.
int allocate_memory(int pid, int size);

// Frees every block owned by `pid` and joins neighbouring free blocks.
void free_memory(int pid);

// Slides all used blocks to the start of memory so the free space becomes one block.
void compact_memory();

// Calculates used KB, free KB, the biggest free hole and the fragmentation percentage.
void memory_info(int *used, int *free_total, int *largest_free, int *frag_percent);

// Shell commands: "memmap", "fit first|best|worst", "compact".
void cmd_memmap();
void cmd_fit(vector<string> &args);
void cmd_compact();

#endif

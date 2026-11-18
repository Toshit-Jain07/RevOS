// memory.cpp - memory manager: first/best/worst fit, free + coalescing, compaction, memmap.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "memory.h"
#include "kernel_log.h"
#include "types.h"
#include "globals.h"

// What it is: the memory allocator.
// What it does: looks for a free block of at least `size` KB using the chosen
// fit strategy, splits it if it is bigger than needed, gives it to process
// `pid`, and returns the start address. Returns -1 if no free block is big enough.
int allocate_memory(int pid, int size) {
    int chosen = -1;
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].pid != -1 || blocks[i].size < size) {
            continue;   // used, or too small
        }
        if (mem_fit == FIT_FIRST) {
            chosen = i;
            break;
        }
        if (chosen == -1) {
            chosen = i;
        } else if (mem_fit == FIT_BEST && blocks[i].size < blocks[chosen].size) {
            chosen = i;
        } else if (mem_fit == FIT_WORST && blocks[i].size > blocks[chosen].size) {
            chosen = i;
        }
    }
    if (chosen == -1) {
        return -1;
    }

    int start = blocks[chosen].start;
    if (blocks[chosen].size > size) {
        if (block_count >= MAX_BLOCKS) {
            return -1;
        }
        // shift later blocks one place to the right to make room for the leftover piece
        for (int j = block_count; j > chosen + 1; j--) {
            blocks[j] = blocks[j - 1];
        }
        blocks[chosen + 1].start = start + size;
        blocks[chosen + 1].size = blocks[chosen].size - size;
        blocks[chosen + 1].pid = -1;
        block_count++;
        blocks[chosen].size = size;
    }
    blocks[chosen].pid = pid;
    return start;
}

// What it is: the memory "free" operation.
// What it does: marks every block owned by `pid` as free, then joins
// neighbouring free blocks into one bigger free block (coalescing).
void free_memory(int pid) {
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].pid == pid) {
            blocks[i].pid = -1;
        }
    }
    int i = 0;
    while (i < block_count - 1) {
        if (blocks[i].pid == -1 && blocks[i + 1].pid == -1) {
            blocks[i].size += blocks[i + 1].size;
            for (int j = i + 1; j < block_count - 1; j++) {
                blocks[j] = blocks[j + 1];
            }
            block_count--;
        } else {
            i++;
        }
    }
}

// What it is: the defragmenter.
// What it does: slides all used blocks to the start of memory, in the same
// order, so all free space becomes one big block at the end.
void compact_memory() {
    Block newblocks[MAX_BLOCKS];
    int n = 0;
    int next = 0;
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].pid != -1) {
            newblocks[n].start = next;
            newblocks[n].size = blocks[i].size;
            newblocks[n].pid = blocks[i].pid;
            procs[blocks[i].pid].mem_start = next;
            next += blocks[i].size;
            n++;
        }
    }
    if (next < TOTAL_MEM) {
        newblocks[n].start = next;
        newblocks[n].size = TOTAL_MEM - next;
        newblocks[n].pid = -1;
        n++;
    }
    for (int i = 0; i < n; i++) {
        blocks[i] = newblocks[i];
    }
    block_count = n;
}

// What it is: a memory report calculator.
// What it does: fills in how much memory is used, how much is free, the size
// of the biggest free hole, and the external fragmentation percentage
// (the share of free memory that is NOT in the biggest hole).
void memory_info(int *used, int *free_total, int *largest_free, int *frag_percent) {
    *used = 0;
    *free_total = 0;
    *largest_free = 0;
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].pid == -1) {
            *free_total += blocks[i].size;
            if (blocks[i].size > *largest_free) {
                *largest_free = blocks[i].size;
            }
        } else {
            *used += blocks[i].size;
        }
    }
    if (*free_total == 0) {
        *frag_percent = 0;
    } else {
        *frag_percent = (*free_total - *largest_free) * 100 / *free_total;
    }
}

// Command "memmap": draws memory as a bar of characters (each = 4 KB) where a
// digit/letter is the owning process and '.' is free, then lists every block
// and the fragmentation numbers.
void cmd_memmap() {
    printf("Memory map  (each cell = %d KB, total = %d KB, strategy = %s)\n",
           CELL_SIZE, TOTAL_MEM, fit_names[mem_fit]);
    printf("[");
    for (int cell = 0; cell < TOTAL_MEM / CELL_SIZE; cell++) {
        int addr = cell * CELL_SIZE;
        char c = '.';
        for (int i = 0; i < block_count; i++) {
            if (addr >= blocks[i].start && addr < blocks[i].start + blocks[i].size) {
                if (blocks[i].pid != -1) {
                    if (blocks[i].pid <= 9) {
                        c = '0' + blocks[i].pid;
                    } else {
                        c = 'A' + (blocks[i].pid - 10) % 26;
                    }
                }
            }
        }
        printf("%c", c);
    }
    printf("]\n");
    printf("  Start   End   Size  Owner\n");
    for (int i = 0; i < block_count; i++) {
        printf("  %5d %5d %5d  ", blocks[i].start, blocks[i].start + blocks[i].size - 1, blocks[i].size);
        if (blocks[i].pid == -1) {
            printf("free\n");
        } else {
            printf("P%d (%s)\n", blocks[i].pid, procs[blocks[i].pid].name.c_str());
        }
    }
    int used, free_total, largest, frag;
    memory_info(&used, &free_total, &largest, &frag);
    printf("Used: %d KB (%d%%)   Free: %d KB   Largest free hole: %d KB   External fragmentation: %d%%\n",
           used, used * 100 / TOTAL_MEM, free_total, largest, frag);
}

// Command "fit": shows or changes the placement strategy used for NEW
// processes (first, best or worst).
void cmd_fit(vector<string> &args) {
    if (args.size() < 2) {
        printf("Current strategy: %s   (change with: fit first | best | worst)\n", fit_names[mem_fit]);
        return;
    }
    if (args[1] == "first") {
        mem_fit = FIT_FIRST;
    } else if (args[1] == "best") {
        mem_fit = FIT_BEST;
    } else if (args[1] == "worst") {
        mem_fit = FIT_WORST;
    } else {
        printf("usage: fit first | best | worst\n");
        return;
    }
    printf("Memory strategy is now %s (applies to processes created from now on).\n", fit_names[mem_fit]);
    log_event(string("memory strategy set to ") + fit_names[mem_fit]);
}

// Command "compact": runs the defragmenter and shows fragmentation before and after.
void cmd_compact() {
    int used, free_total, largest, frag_before, frag_after;
    memory_info(&used, &free_total, &largest, &frag_before);
    int largest_before = largest;
    compact_memory();
    memory_info(&used, &free_total, &largest, &frag_after);
    printf("Memory compacted. Fragmentation: %d%% -> %d%%   Largest free hole: %d KB -> %d KB\n",
           frag_before, frag_after, largest_before, largest);
    log_event("memory compacted");
}

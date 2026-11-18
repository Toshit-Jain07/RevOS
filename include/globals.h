// globals.h - the global state of the whole operating system.
//
// MiniOS keeps its tables (processes, memory blocks, locks, files) in global
// variables, like a real kernel keeps its data structures in kernel memory.
// "extern" means: the variable exists, it is defined once in globals.cpp.

#ifndef MINIOS_GLOBALS_H
#define MINIOS_GLOBALS_H

#include "types.h"

// --- process table and scheduler state ---
extern Process procs[MAX_PROCS + 1];   // index = pid, so procs[0] is unused
extern int proc_count;
extern int current_tick;
extern int timeline[MAX_TIME];         // timeline[t] = pid that ran at tick t (0 = idle)
extern int running_pid;                // pid on the CPU now (0 = none)
extern int last_pid_ran;               // pid that used the CPU in the previous tick
extern int algo;
extern int quantum;                    // Round Robin time slice
extern int quantum_used;               // ticks the running process has used of its slice
extern bool aging_on;
extern int busy_ticks;
extern int context_switches;
extern int ready_counter;              // gives each ready-queue entry a number

// --- memory ---
extern Block blocks[MAX_BLOCKS];
extern int block_count;
extern int mem_fit;

// --- resources (locks) ---
extern string res_name[MAX_RES];
extern int res_holder[MAX_RES];        // pid holding the resource, 0 = free
extern int res_count;

// --- file system ---
extern FileNode nodes[MAX_NODES];      // nodes[0] is the root folder "/"
extern int cwd;                        // index of the current folder
extern int delete_counter;

// --- shell ---
extern string username;
extern bool interactive;               // true if a person is typing, false if input comes from a file
extern vector<string> history;
extern time_t boot_time;

#endif

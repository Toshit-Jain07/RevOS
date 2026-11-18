// globals.cpp - the one place where the global variables are defined.
// (See globals.h for what each one means.)

#include "globals.h"

// --- process table and scheduler state ---
Process procs[MAX_PROCS + 1];   // index = pid, so procs[0] is unused
int proc_count;
int current_tick;
int timeline[MAX_TIME];         // timeline[t] = pid that ran at tick t (0 = idle)
int running_pid;                // pid on the CPU now (0 = none)
int last_pid_ran;               // pid that used the CPU in the previous tick
int algo;
int quantum;                    // Round Robin time slice
int quantum_used;               // ticks the running process has used of its slice
bool aging_on;
int busy_ticks;
int context_switches;
int ready_counter;              // gives each ready-queue entry a number

// --- memory ---
Block blocks[MAX_BLOCKS];
int block_count;
int mem_fit;

// --- resources (locks) ---
string res_name[MAX_RES];
int res_holder[MAX_RES];        // pid holding the resource, 0 = free
int res_count;

// --- file system ---
FileNode nodes[MAX_NODES];      // nodes[0] is the root folder "/"
int cwd;                        // index of the current folder
int delete_counter;

// --- shell ---
string username;
bool interactive;               // true if a person is typing, false if input comes from a file
vector<string> history;
time_t boot_time;

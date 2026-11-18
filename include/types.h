// types.h - constants and data structures shared by every MiniOS module.
//
// This file only DESCRIBES things (sizes, states, structs). It holds no logic.
// The variables that use these structs live in globals.h / globals.cpp.

#ifndef MINIOS_TYPES_H
#define MINIOS_TYPES_H

#include <string>
#include <vector>
#include <ctime>

using namespace std;   // keeps the code short and beginner friendly

const int MAX_PROCS = 100;      // most processes the process table can hold (finished ones keep their slot for stats)
const int MAX_RES = 8;          // most named resources (locks) we can have
const int MAX_BLOCKS = 100;     // most memory blocks (used + free)
const int MAX_NODES = 200;      // most files and folders
const int MAX_VERSIONS = 20;    // most versions kept per file; the oldest is dropped automatically
const int KEYFRAME_EVERY = 8;   // every 8th version is stored in full so rebuilding a version stays cheap
const int MAX_TIME = 5000;      // the simulation stops after this many ticks
const int TOTAL_MEM = 256;      // total RAM of MiniOS in KB
const int CELL_SIZE = 4;        // one character of the memory map = 4 KB
const int STARVE_LIMIT = 12;    // doctor warns if a ready process waits longer than this
const int AGING_INTERVAL = 5;   // with aging on, a waiting process gets a boost every 5 ticks
const int GANTT_WINDOW = 60;    // the Gantt chart shows at most the last 60 ticks

// process states
const int ST_UNUSED = 0;
const int ST_READY = 1;
const int ST_RUNNING = 2;
const int ST_BLOCKED = 3;
const int ST_DONE = 4;
const int ST_KILLED = 5;
const char *const state_names[] = { "UNUSED", "READY", "RUNNING", "BLOCKED", "DONE", "KILLED" };

// scheduling algorithms
const int ALGO_FCFS = 0;
const int ALGO_SJF = 1;
const int ALGO_SRTF = 2;
const int ALGO_PRIORITY = 3;
const int ALGO_RR = 4;
const char *const algo_names[] = {
    "FCFS (first come first served)",
    "SJF (shortest job first, non-preemptive)",
    "SRTF (shortest remaining time first, preemptive)",
    "Priority (preemptive, 0 = highest priority)",
    "Round Robin"
};

// memory placement strategies
const int FIT_FIRST = 0;
const int FIT_BEST = 1;
const int FIT_WORST = 2;
const char *const fit_names[] = { "first-fit", "best-fit", "worst-fit" };

// The "Process Control Block": everything the OS knows about one process.
struct Process {
    int pid;
    string name;
    int burst;          // total CPU time the process needs
    int remaining;      // CPU time still left
    int priority;       // 0 is the highest priority, 9 the lowest
    int arrival;        // tick when the process was created
    int mem_size;       // KB of memory it uses
    int mem_start;      // address of its memory block
    int state;
    int ready_seq;      // position in the ready queue (smaller = earlier)
    int last_ran;       // tick when it last used the CPU
    int ready_wait;     // ticks spent waiting in READY since it last ran (used by aging)
    int start_time;     // tick when it first got the CPU (-1 = never)
    int finish_time;    // tick when it finished or was killed
    int waiting_for;    // index of the resource it is blocked on (-1 = none)
};

// One piece of memory. pid == -1 means the piece is free.
struct Block {
    int start;
    int size;
    int pid;
};

// One saved version of a file. To save memory only the first version (and every
// KEYFRAME_EVERY-th one) keeps the full text. The others store a "delta": what
// changed compared with the previous version = keep the first `prefix` bytes and
// the last `suffix` bytes of the previous text and put `data` in between.
struct Version {
    int tick;           // tick at which it was saved
    bool full;          // true: data is the whole text; false: data is only the changed middle part
    int prefix;         // (delta only) bytes kept from the start of the previous version
    int suffix;         // (delta only) bytes kept from the end of the previous version
    string data;
};

// One file or folder of the file system.
struct FileNode {
    bool used;
    bool is_dir;
    bool deleted;               // true = it is in the trash
    int deleted_seq;            // order of deletion (to recover the newest first)
    int deleted_tick;
    string name;
    int parent;                 // index of the folder that holds it
    vector<Version> versions;   // saved versions, stored compactly (see Version)
};

#endif

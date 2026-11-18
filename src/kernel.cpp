// kernel.cpp - kernel start-up: puts every table and setting back to its starting value.

#include "types.h"
#include "globals.h"
#include "kernel.h"

// What it is: the OS "reset button".
// What it does: puts every table, counter and setting back to its starting
// value: no processes, clock at 0, empty memory, no resources, empty file system.
void init_kernel() {
    proc_count = 0;
    for (int i = 0; i <= MAX_PROCS; i++) {
        procs[i].state = ST_UNUSED;
    }
    current_tick = 0;
    for (int t = 0; t < MAX_TIME; t++) {
        timeline[t] = 0;
    }
    running_pid = 0;
    last_pid_ran = 0;
    algo = ALGO_RR;
    quantum = 3;
    quantum_used = 0;
    aging_on = false;
    busy_ticks = 0;
    context_switches = 0;
    ready_counter = 0;

    block_count = 1;
    blocks[0].start = 0;
    blocks[0].size = TOTAL_MEM;
    blocks[0].pid = -1;
    mem_fit = FIT_FIRST;

    res_count = 0;
    for (int r = 0; r < MAX_RES; r++) {
        res_holder[r] = 0;
    }

    for (int i = 0; i < MAX_NODES; i++) {
        nodes[i].used = false;
        nodes[i].versions.clear();
    }
    nodes[0].used = true;
    nodes[0].is_dir = true;
    nodes[0].deleted = false;
    nodes[0].name = "/";
    nodes[0].parent = 0;
    cwd = 0;
    delete_counter = 0;
}

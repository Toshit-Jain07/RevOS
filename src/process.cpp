// process.cpp - process manager: create (run), list (ps), end, kill and crash processes.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "process.h"
#include "memory.h"
#include "resources.h"
#include "kernel_log.h"
#include "types.h"
#include "globals.h"

// What it is: the "process exit" routine of the kernel.
// What it does: ends process `pid` with the final state DONE or KILLED, gives
// back its memory and all resources it held, and writes a line in the log.
void end_process(int pid, int final_state, string reason) {
    procs[pid].waiting_for = -1;
    procs[pid].state = final_state;
    procs[pid].finish_time = current_tick;
    for (int r = 0; r < res_count; r++) {
        if (res_holder[r] == pid) {
            release_resource(r);
        }
    }
    free_memory(pid);
    if (running_pid == pid) {
        running_pid = 0;
        quantum_used = 0;
    }
    log_event("P" + to_string(pid) + " (" + procs[pid].name + ") " + reason);
}

// Command "run <name> <burst> [priority] [memKB]": creates a new process that
// needs <burst> ticks of CPU. Priority is 0-9 (0 = highest, default 5) and
// memory defaults to 16 KB. The process is refused if memory is not available.
void cmd_run(vector<string> &args) {
    if (args.size() < 3) {
        printf("usage: run <name> <burst> [priority 0-9] [memory KB]\n");
        return;
    }
    string name = args[1];
    int burst = atoi(args[2].c_str());
    int priority = 5;
    int mem = 16;
    if (args.size() >= 4) {
        priority = atoi(args[3].c_str());
        if (args[3] != "0" && priority == 0) {
            printf("run: priority must be a number from 0 to 9\n");
            return;
        }
    }
    if (args.size() >= 5) {
        mem = atoi(args[4].c_str());
    }
    if (name.size() > 10) {
        printf("run: name is too long (maximum 10 characters)\n");
        return;
    }
    if (burst < 1 || burst > 500) {
        printf("run: burst must be a number from 1 to 500\n");
        return;
    }
    if (priority < 0 || priority > 9) {
        printf("run: priority must be a number from 0 to 9\n");
        return;
    }
    if (mem < 1 || mem > TOTAL_MEM) {
        printf("run: memory must be from 1 to %d KB\n", TOTAL_MEM);
        return;
    }
    if (proc_count >= MAX_PROCS) {
        printf("run: process table is full (%d processes). Use 'reboot' to start fresh.\n", MAX_PROCS);
        return;
    }

    int pid = proc_count + 1;
    int start = allocate_memory(pid, mem);
    if (start == -1) {
        int used, free_total, largest, frag;
        memory_info(&used, &free_total, &largest, &frag);
        printf("run: out of memory. Needed %d KB in one piece, largest free hole is %d KB (total free %d KB).\n",
               mem, largest, free_total);
        if (free_total >= mem) {
            printf("     There is enough free memory in total, but it is fragmented. Try the 'compact' command.\n");
        }
        log_event("process '" + name + "' refused: out of memory");
        return;
    }

    proc_count = pid;
    procs[pid].pid = pid;
    procs[pid].name = name;
    procs[pid].burst = burst;
    procs[pid].remaining = burst;
    procs[pid].priority = priority;
    procs[pid].arrival = current_tick;
    procs[pid].mem_size = mem;
    procs[pid].mem_start = start;
    procs[pid].state = ST_READY;
    ready_counter++;
    procs[pid].ready_seq = ready_counter;
    procs[pid].last_ran = current_tick;
    procs[pid].ready_wait = 0;
    procs[pid].start_time = -1;
    procs[pid].finish_time = -1;
    procs[pid].waiting_for = -1;
    printf("Created P%d '%s'  burst=%d  priority=%d  memory=%d KB at address %d\n", pid, name.c_str(), burst, priority, mem, start);
    log_event("P" + to_string(pid) + " (" + name + ") created, burst " + to_string(burst) + ", priority " + to_string(priority) + ", " + to_string(mem) + " KB");
}

// Command "ps": prints the process table and the current scheduler settings.
void cmd_ps() {
    printf("Scheduler: %s", algo_names[algo]);
    if (algo == ALGO_RR) {
        printf(", quantum %d", quantum);
    }
    printf(" | aging %s | tick %d\n", aging_on ? "ON" : "off", current_tick);
    if (proc_count == 0) {
        printf("No processes yet. Create one with: run <name> <burst>\n");
        return;
    }
    printf(" PID  NAME        STATE     PRI  BURST  LEFT  ARRIVE  MEM   WAITED\n");
    for (int p = 1; p <= proc_count; p++) {
        int end = current_tick;
        if (procs[p].state == ST_DONE || procs[p].state == ST_KILLED) {
            end = procs[p].finish_time;
        }
        int waited = end - procs[p].arrival - (procs[p].burst - procs[p].remaining);
        printf(" %-4d %-11s %-9s %-4d %-6d %-5d %-7d %-5d %-6d",
               p, procs[p].name.c_str(), state_names[procs[p].state], procs[p].priority,
               procs[p].burst, procs[p].remaining, procs[p].arrival, procs[p].mem_size, waited);
        if (procs[p].state == ST_BLOCKED) {
            printf("  waits for '%s'", res_name[procs[p].waiting_for].c_str());
        }
        printf("\n");
    }
}

// Command "kill <pid>": stops a process and frees its memory and resources.
void cmd_kill(vector<string> &args) {
    if (args.size() < 2) {
        printf("usage: kill <pid>\n");
        return;
    }
    int pid = atoi(args[1].c_str());
    if (pid < 1 || pid > proc_count) {
        printf("kill: no such process '%s'\n", args[1].c_str());
        return;
    }
    if (procs[pid].state == ST_DONE || procs[pid].state == ST_KILLED) {
        printf("kill: P%d has already ended (%s)\n", pid, state_names[procs[pid].state]);
        return;
    }
    printf("Killing P%d (%s)...\n", pid, procs[pid].name.c_str());
    int freed = procs[pid].mem_size;
    end_process(pid, ST_KILLED, "killed by user");
    printf("P%d killed. %d KB of memory freed and its resources released.\n", pid, freed);
}

// Command "crash [pid]": makes a process crash (a simulated segmentation
// fault). Without a pid it picks a random live process. The OS then cleans up
// after it (memory and locks), which proves the kernel survives a
// misbehaving process.
void cmd_crash(vector<string> &args) {
    int alive[MAX_PROCS];
    int n = 0;
    for (int p = 1; p <= proc_count; p++) {
        if (procs[p].state == ST_READY || procs[p].state == ST_RUNNING || procs[p].state == ST_BLOCKED) {
            alive[n] = p;
            n++;
        }
    }
    if (n == 0) {
        printf("crash: there is no live process to crash.\n");
        return;
    }
    int pid = alive[rand() % n];
    if (args.size() >= 2) {
        pid = atoi(args[1].c_str());
        if (pid < 1 || pid > proc_count || procs[pid].state == ST_DONE || procs[pid].state == ST_KILLED) {
            printf("crash: '%s' is not a live process\n", args[1].c_str());
            return;
        }
    }
    int freed = procs[pid].mem_size;
    int held = 0;
    for (int r = 0; r < res_count; r++) {
        if (res_holder[r] == pid) {
            held++;
        }
    }
    printf("*** P%d (%s) crashed: segmentation fault (simulated) ***\n", pid, procs[pid].name.c_str());
    end_process(pid, ST_KILLED, "crashed (simulated SIGSEGV)");
    printf("OS cleanup done: freed %d KB of memory and released %d resource(s). The system is still running.\n", freed, held);
}

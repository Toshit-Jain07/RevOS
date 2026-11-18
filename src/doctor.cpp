// doctor.cpp - the doctor: finds deadlock, starvation, blocked processes and memory problems.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "doctor.h"
#include "process.h"
#include "memory.h"
#include "kernel_log.h"
#include "types.h"
#include "globals.h"

// What it is: the deadlock finder (a wait-for graph cycle search).
// What it does: for every blocked process it follows the chain "I wait for a
// resource -> held by that process -> which waits for ... ". If the chain comes
// back to the starting process, those processes are in a deadlock. It saves the
// processes of the cycle in cycle[] and their count in *len. Processes marked
// in skip[] are ignored (used so the same cycle is not reported twice).
bool find_deadlock(int cycle[], int *len, bool skip[]) {
    for (int s = 1; s <= proc_count; s++) {
        if (procs[s].state != ST_BLOCKED || skip[s]) {
            continue;
        }
        int cur = s;
        int n = 0;
        cycle[n] = s;
        n++;
        bool found = false;
        for (int step = 0; step < MAX_PROCS; step++) {
            int r = procs[cur].waiting_for;
            if (r < 0) {
                break;
            }
            int holder = res_holder[r];
            if (holder == 0) {
                break;
            }
            if (holder == s) {
                found = true;
                break;
            }
            if (procs[holder].state != ST_BLOCKED) {
                break;
            }
            cycle[n] = holder;
            n++;
            cur = holder;
        }
        if (found) {
            *len = n;
            return true;
        }
    }
    return false;
}

// Command "doctor [fix]": checks the health of the whole OS and explains any
// problem in plain English. It looks for: deadlocks, starvation, processes
// that are blocked, high memory fragmentation and nearly full memory.
// With "doctor fix" it also breaks every deadlock by killing the youngest
// (highest pid) process in each cycle.
void cmd_doctor(vector<string> &args) {
    bool fix = (args.size() >= 2 && args[1] == "fix");
    int problems = 0;
    int fixed = 0;
    bool skip[MAX_PROCS + 1];
    for (int i = 0; i <= MAX_PROCS; i++) {
        skip[i] = false;
    }
    int cycle[MAX_PROCS + 1];
    int len = 0;

    printf("=== MiniOS doctor: health check at tick %d ===\n", current_tick);

    // check 1: deadlocks
    while (find_deadlock(cycle, &len, skip)) {
        problems++;
        printf("[DEADLOCK] %d processes are stuck in a circular wait:\n", len);
        for (int i = 0; i < len; i++) {
            int a = cycle[i];
            int b = cycle[(i + 1) % len];
            printf("    P%d (%s) wants '%s', which is held by P%d (%s)\n",
                   a, procs[a].name.c_str(), res_name[procs[a].waiting_for].c_str(), b, procs[b].name.c_str());
        }
        for (int i = 0; i < len; i++) {
            skip[cycle[i]] = true;
        }
        if (fix) {
            int victim = cycle[0];
            for (int i = 0; i < len; i++) {
                if (cycle[i] > victim) {
                    victim = cycle[i];
                }
            }
            printf("    FIX: killing P%d (%s), the youngest process in the cycle, to break the deadlock\n",
                   victim, procs[victim].name.c_str());
            end_process(victim, ST_KILLED, "killed by doctor to break a deadlock");
            fixed++;
        } else {
            printf("    Suggestion: run 'doctor fix', or 'kill' one of them.\n");
        }
    }

    // check 2: starvation
    for (int p = 1; p <= proc_count; p++) {
        if (procs[p].state == ST_READY) {
            int since = procs[p].last_ran;
            if (procs[p].arrival > since) {
                since = procs[p].arrival;
            }
            int waited = current_tick - since;
            if (waited > STARVE_LIMIT) {
                problems++;
                printf("[STARVATION] P%d (%s) has waited %d ticks without getting the CPU.\n", p, procs[p].name.c_str(), waited);
                printf("    Suggestion: turn on 'aging on' or switch to a fairer scheduler like 'sched rr'.\n");
            }
        }
    }

    // check 3: blocked processes that are not in a deadlock
    for (int p = 1; p <= proc_count; p++) {
        if (procs[p].state == ST_BLOCKED && !skip[p]) {
            problems++;
            int r = procs[p].waiting_for;
            printf("[BLOCKED] P%d (%s) waits for '%s' held by P%d. This is not a deadlock; it continues when P%d unlocks it.\n",
                   p, procs[p].name.c_str(), res_name[r].c_str(), res_holder[r], res_holder[r]);
        }
    }

    // check 4: memory
    int used, free_total, largest, frag;
    memory_info(&used, &free_total, &largest, &frag);
    if (free_total > 0 && frag >= 30) {
        problems++;
        printf("[MEMORY] External fragmentation is %d%%: %d KB are free but the biggest hole is only %d KB.\n", frag, free_total, largest);
        printf("    Suggestion: run 'compact'.\n");
    }
    if (used * 100 / TOTAL_MEM >= 90) {
        problems++;
        printf("[MEMORY] Memory is almost full (%d%% used).\n", used * 100 / TOTAL_MEM);
    }

    if (problems == 0) {
        printf("System healthy. No problems found.\n");
    } else if (fix) {
        printf("Summary: %d problem(s) reported, %d deadlock(s) fixed.\n", problems, fixed);
    } else {
        printf("Summary: %d problem(s) found.\n", problems);
    }
    log_event("doctor ran: " + to_string(problems) + " problem(s), " + to_string(fixed) + " fixed");
}

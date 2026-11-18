// reports.cpp - Gantt chart and statistics (the numbers used to compare schedulers).

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "reports.h"
#include "memory.h"
#include "types.h"
#include "globals.h"

// Command "gantt": draws who used the CPU in every tick. One row per process:
// '#' = running, '.' = alive but not running (waiting or blocked), blank =
// not created yet or already finished. Below it, a list of the CPU's
// execution order. Shows only the last 60 ticks if the history is longer.
void cmd_gantt() {
    if (current_tick == 0) {
        printf("Nothing has run yet.\n");
        return;
    }
    int from = 0;
    if (current_tick > GANTT_WINDOW) {
        from = current_tick - GANTT_WINDOW;
    }
    printf("Gantt chart, ticks %d to %d   (# running, . waiting/blocked)\n", from, current_tick);
    printf("     ");
    for (int t = from; t < current_tick; t++) {
        if (t % 10 == 0) {
            printf("%d", (t / 10) % 10);
        } else {
            printf(" ");
        }
    }
    printf("   <- tens of the tick number\n     ");
    for (int t = from; t < current_tick; t++) {
        printf("%d", t % 10);
    }
    printf("   <- last digit of the tick number\n");
    for (int p = 1; p <= proc_count; p++) {
        // skip processes that already ended before the visible window
        if ((procs[p].state == ST_DONE || procs[p].state == ST_KILLED) && procs[p].finish_time <= from) {
            continue;
        }
        printf("P%-3d|", p);
        for (int t = from; t < current_tick; t++) {
            char c;
            if (timeline[t] == p) {
                c = '#';
            } else if (t < procs[p].arrival) {
                c = ' ';
            } else if ((procs[p].state == ST_DONE || procs[p].state == ST_KILLED) && t >= procs[p].finish_time) {
                c = ' ';
            } else {
                c = '.';
            }
            printf("%c", c);
        }
        printf("|  %s\n", procs[p].name.c_str());
    }
    printf("CPU order: ");
    int t = from;
    while (t < current_tick) {
        int who = timeline[t];
        int end = t;
        while (end < current_tick && timeline[end] == who) {
            end++;
        }
        if (who == 0) {
            printf("idle(%d-%d) ", t, end);
        } else {
            printf("P%d(%d-%d) ", who, t, end);
        }
        t = end;
    }
    printf("\n");
}

// Command "stats": prints performance numbers - averages of waiting,
// turnaround and response time for finished processes, CPU utilization,
// throughput, context switches and memory usage.
void cmd_stats() {
    int done = 0;
    int killed = 0;
    int alive = 0;
    int total_wait = 0;
    int total_turn = 0;
    int total_resp = 0;
    for (int p = 1; p <= proc_count; p++) {
        if (procs[p].state == ST_DONE) {
            done++;
            int turn = procs[p].finish_time - procs[p].arrival;
            total_turn += turn;
            total_wait += turn - procs[p].burst;
            total_resp += procs[p].start_time - procs[p].arrival;
        } else if (procs[p].state == ST_KILLED) {
            killed++;
        } else {
            alive++;
        }
    }
    printf("=== MiniOS statistics (tick %d) ===\n", current_tick);
    printf("Scheduler            : %s", algo_names[algo]);
    if (algo == ALGO_RR) {
        printf(", quantum %d", quantum);
    }
    printf("\n");
    printf("Processes            : %d finished, %d killed, %d still alive\n", done, killed, alive);
    if (done > 0) {
        printf("Avg waiting time     : %.2f ticks\n", (double) total_wait / done);
        printf("Avg turnaround time  : %.2f ticks\n", (double) total_turn / done);
        printf("Avg response time    : %.2f ticks\n", (double) total_resp / done);
    } else {
        printf("Avg waiting/turnaround/response: no finished process yet\n");
    }
    if (current_tick > 0) {
        printf("CPU utilization      : %.1f%%  (%d busy ticks of %d)\n", busy_ticks * 100.0 / current_tick, busy_ticks, current_tick);
        printf("Throughput           : %.3f processes per tick\n", (double) done / current_tick);
    } else {
        printf("CPU utilization      : 0.0%%\n");
    }
    printf("Context switches     : %d\n", context_switches);
    int used, free_total, largest, frag;
    memory_info(&used, &free_total, &largest, &frag);
    printf("Memory               : %d of %d KB used (%d%%), fragmentation %d%%\n", used, TOTAL_MEM, used * 100 / TOTAL_MEM, frag);
}

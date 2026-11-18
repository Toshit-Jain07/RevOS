// scheduler.cpp - CPU scheduler: one clock tick at a time, five algorithms, aging.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "scheduler.h"
#include "process.h"
#include "kernel_log.h"
#include "types.h"
#include "globals.h"

// What it is: the heart of the OS - one step of the CPU clock.
// What it does: (1) the chosen algorithm picks which process uses the CPU for
// this tick, (2) that process runs for one tick, (3) the clock moves forward,
// (4) a finished process is cleaned up, (5) waiting processes get older and,
// if aging is on, the long-waiting ones get a priority boost.
// It returns the pid that ran (0 means the CPU was idle).
int run_one_tick() {
    // if the process we thought was running is not running any more (killed or blocked), forget it
    if (running_pid != 0 && procs[running_pid].state != ST_RUNNING) {
        running_pid = 0;
        quantum_used = 0;
    }

    int chosen = 0;

    if (algo == ALGO_FCFS || algo == ALGO_SJF) {
        // non-preemptive: the running process keeps the CPU until it ends or blocks
        if (running_pid != 0) {
            chosen = running_pid;
        } else {
            for (int p = 1; p <= proc_count; p++) {
                if (procs[p].state != ST_READY) {
                    continue;
                }
                if (chosen == 0) {
                    chosen = p;
                } else if (algo == ALGO_FCFS && procs[p].ready_seq < procs[chosen].ready_seq) {
                    chosen = p;
                } else if (algo == ALGO_SJF) {
                    if (procs[p].remaining < procs[chosen].remaining) {
                        chosen = p;
                    } else if (procs[p].remaining == procs[chosen].remaining && procs[p].ready_seq < procs[chosen].ready_seq) {
                        chosen = p;
                    }
                }
            }
        }
    } else if (algo == ALGO_SRTF || algo == ALGO_PRIORITY) {
        // preemptive: every tick we look at ALL ready processes and the running one
        for (int p = 1; p <= proc_count; p++) {
            if (procs[p].state != ST_READY && procs[p].state != ST_RUNNING) {
                continue;
            }
            if (chosen == 0) {
                chosen = p;
                continue;
            }
            int key_p;
            int key_c;
            if (algo == ALGO_SRTF) {
                key_p = procs[p].remaining;
                key_c = procs[chosen].remaining;
            } else {
                key_p = procs[p].priority;
                key_c = procs[chosen].priority;
            }
            if (key_p < key_c) {
                chosen = p;
            } else if (key_p == key_c) {
                // tie: keep the running process (no useless switch), otherwise the one waiting longest
                if (procs[p].state == ST_RUNNING) {
                    chosen = p;
                } else if (procs[chosen].state != ST_RUNNING && procs[p].ready_seq < procs[chosen].ready_seq) {
                    chosen = p;
                }
            }
        }
    } else {
        // Round Robin: each process gets `quantum` ticks, then goes to the back of the queue
        if (running_pid != 0 && quantum_used >= quantum) {
            procs[running_pid].state = ST_READY;
            ready_counter++;
            procs[running_pid].ready_seq = ready_counter;
            procs[running_pid].ready_wait = 0;
            running_pid = 0;
            quantum_used = 0;
        }
        if (running_pid != 0) {
            chosen = running_pid;
        } else {
            for (int p = 1; p <= proc_count; p++) {
                if (procs[p].state != ST_READY) {
                    continue;
                }
                if (chosen == 0 || procs[p].ready_seq < procs[chosen].ready_seq) {
                    chosen = p;
                }
            }
        }
    }

    // if a different process takes over, the old running process goes back to READY
    if (running_pid != 0 && running_pid != chosen) {
        procs[running_pid].state = ST_READY;
        ready_counter++;
        procs[running_pid].ready_seq = ready_counter;
        procs[running_pid].ready_wait = 0;
        log_event("P" + to_string(running_pid) + " preempted by P" + to_string(chosen));
    }

    // let the chosen process use the CPU for one tick
    if (chosen != 0) {
        if (chosen != last_pid_ran) {
            if (last_pid_ran != 0) {
                context_switches++;
            }
            quantum_used = 0;
        }
        procs[chosen].state = ST_RUNNING;
        running_pid = chosen;
        if (procs[chosen].start_time == -1) {
            procs[chosen].start_time = current_tick;
        }
        procs[chosen].remaining--;
        procs[chosen].ready_wait = 0;
        procs[chosen].last_ran = current_tick + 1;
        quantum_used++;
        busy_ticks++;
    }
    timeline[current_tick] = chosen;
    last_pid_ran = chosen;
    current_tick++;

    // did the process just finish?
    if (chosen != 0 && procs[chosen].remaining == 0) {
        end_process(chosen, ST_DONE, "finished");
    }

    // everyone still READY waited one more tick; aging boosts the ones waiting too long
    for (int p = 1; p <= proc_count; p++) {
        if (procs[p].state == ST_READY) {
            procs[p].ready_wait++;
            if (aging_on && procs[p].ready_wait >= AGING_INTERVAL && procs[p].priority > 0) {
                procs[p].priority--;
                procs[p].ready_wait = 0;
                log_event("aging: P" + to_string(p) + " priority boosted to " + to_string(procs[p].priority));
            }
        }
    }
    return chosen;
}

// Command "step [n]": runs the CPU for n ticks (default 1) and prints who ran
// in each tick, so you can watch the scheduler think.
void cmd_step(vector<string> &args) {
    int n = 1;
    if (args.size() >= 2) {
        n = atoi(args[1].c_str());
    }
    if (n < 1 || n > 1000) {
        printf("usage: step [number of ticks, 1-1000]\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        if (current_tick >= MAX_TIME) {
            printf("Simulation limit of %d ticks reached. Use 'reboot' to start again.\n", MAX_TIME);
            return;
        }
        int t = current_tick;
        int pid = run_one_tick();
        if (pid == 0) {
            printf("t=%-4d idle\n", t);
        } else {
            printf("t=%-4d P%d (%s) runs, %d left", t, pid, procs[pid].name.c_str(), procs[pid].remaining);
            if (procs[pid].state == ST_DONE) {
                printf("   -> FINISHED");
            }
            printf("\n");
        }
    }
}

// Command "runall": keeps ticking until no process is READY or RUNNING, then
// prints the Gantt chart. Blocked processes cannot move on their own, so if
// only blocked ones are left it stops and tells you to run the doctor.
void cmd_runall() {
    int ran = 0;
    while (true) {
        int active = 0;
        for (int p = 1; p <= proc_count; p++) {
            if (procs[p].state == ST_READY || procs[p].state == ST_RUNNING) {
                active++;
            }
        }
        if (active == 0) {
            break;
        }
        if (current_tick >= MAX_TIME) {
            printf("Simulation limit of %d ticks reached.\n", MAX_TIME);
            break;
        }
        run_one_tick();
        ran++;
    }
    if (ran == 0) {
        printf("Nothing to run: no READY processes. Create some with 'run'.\n");
    } else {
        printf("Ran %d tick(s). The clock is now at tick %d.\n", ran, current_tick);
    }
    int blocked = 0;
    for (int p = 1; p <= proc_count; p++) {
        if (procs[p].state == ST_BLOCKED) {
            blocked++;
        }
    }
    if (blocked > 0) {
        printf("WARNING: %d process(es) are still BLOCKED and cannot continue. Run 'doctor' to find out why.\n", blocked);
    }
}

// Command "sched": shows or changes the CPU scheduling algorithm. It can be
// changed at any moment, even while processes are half way through.
//   sched fcfs | sjf | srtf | priority | rr [quantum]
void cmd_sched(vector<string> &args) {
    if (args.size() < 2) {
        printf("Current scheduler: %s", algo_names[algo]);
        if (algo == ALGO_RR) {
            printf(", quantum %d", quantum);
        }
        printf("\nChange with: sched fcfs | sjf | srtf | priority | rr [quantum]\n");
        return;
    }
    int new_algo;
    if (args[1] == "fcfs") {
        new_algo = ALGO_FCFS;
    } else if (args[1] == "sjf") {
        new_algo = ALGO_SJF;
    } else if (args[1] == "srtf") {
        new_algo = ALGO_SRTF;
    } else if (args[1] == "priority") {
        new_algo = ALGO_PRIORITY;
    } else if (args[1] == "rr") {
        new_algo = ALGO_RR;
        if (args.size() >= 3) {
            int q = atoi(args[2].c_str());
            if (q < 1 || q > 50) {
                printf("sched: quantum must be from 1 to 50\n");
                return;
            }
            quantum = q;
        }
    } else {
        printf("usage: sched fcfs | sjf | srtf | priority | rr [quantum]\n");
        return;
    }
    algo = new_algo;
    quantum_used = 0;
    printf("Scheduler switched to %s", algo_names[algo]);
    if (algo == ALGO_RR) {
        printf(", quantum %d", quantum);
    }
    printf(" (at tick %d)\n", current_tick);
    log_event(string("scheduler switched to ") + algo_names[algo]);
}

// Command "aging [on|off]": turns the anti-starvation feature on or off. With
// aging on, every process that waits 5 ticks in the ready queue gets its
// priority number lowered by 1 (= more important), so nobody waits forever.
void cmd_aging(vector<string> &args) {
    if (args.size() < 2) {
        printf("Aging is %s. Change with: aging on | off\n", aging_on ? "ON" : "off");
        return;
    }
    if (args[1] == "on") {
        aging_on = true;
    } else if (args[1] == "off") {
        aging_on = false;
    } else {
        printf("usage: aging on | off\n");
        return;
    }
    printf("Aging is now %s.\n", aging_on ? "ON" : "off");
    log_event(string("aging ") + (aging_on ? "enabled" : "disabled"));
}

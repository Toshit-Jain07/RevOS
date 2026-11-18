// scheduler.h - the CPU scheduler (one clock tick at a time).

#ifndef MINIOS_SCHEDULER_H
#define MINIOS_SCHEDULER_H

#include <string>
#include <vector>
using namespace std;

// Runs the CPU for exactly one tick using the current algorithm.
// Returns the pid that ran (0 = the CPU was idle).
int run_one_tick();

// Shell commands: "step", "runall", "sched", "aging".
void cmd_step(vector<string> &args);
void cmd_runall();
void cmd_sched(vector<string> &args);
void cmd_aging(vector<string> &args);

#endif

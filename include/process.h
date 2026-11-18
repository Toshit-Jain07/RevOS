// process.h - process manager: create, list, end, kill and crash processes.

#ifndef MINIOS_PROCESS_H
#define MINIOS_PROCESS_H

#include <string>
#include <vector>
using namespace std;

// Ends process `pid` as DONE or KILLED: frees its memory and releases its locks.
void end_process(int pid, int final_state, string reason);

// Shell commands: "run", "ps", "kill", "crash".
void cmd_run(vector<string> &args);
void cmd_ps();
void cmd_kill(vector<string> &args);
void cmd_crash(vector<string> &args);

#endif

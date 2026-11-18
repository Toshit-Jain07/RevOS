// shell.cpp - the command interpreter: help, dmesg and the dispatcher that runs a typed line.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <ctime>
#include "shell.h"
#include "kernel.h"
#include "kernel_log.h"
#include "memory.h"
#include "resources.h"
#include "process.h"
#include "scheduler.h"
#include "reports.h"
#include "doctor.h"
#include "filesystem.h"
#include "utils.h"
#include "types.h"
#include "globals.h"

// Command "help": prints every command grouped by topic.
void cmd_help() {
    printf("MiniOS commands\n");
    printf("\n PROCESSES & SCHEDULER\n");
    printf("  run <name> <burst> [pri] [memKB]  create a process (pri 0-9, 0 = most important)\n");
    printf("  ps                                show the process table\n");
    printf("  step [n]                          run the CPU for n ticks (default 1)\n");
    printf("  runall                            run until all ready processes finish\n");
    printf("  sched fcfs|sjf|srtf|priority|rr [q]  switch the scheduler (works any time)\n");
    printf("  aging on|off                      anti-starvation priority boost\n");
    printf("  gantt                             draw who used the CPU\n");
    printf("  stats                             waiting/turnaround time, CPU use, ...\n");
    printf("  kill <pid>                        stop a process\n");
    printf("  crash [pid]                       crash a process (random one if no pid)\n");
    printf("\n MEMORY\n");
    printf("  memmap                            draw memory and fragmentation\n");
    printf("  fit first|best|worst              placement strategy for new processes\n");
    printf("  compact                           defragment memory\n");
    printf("\n LOCKS & HEALTH\n");
    printf("  lock <pid> <res>                  process asks for a resource (may block)\n");
    printf("  unlock <pid> <res>                process gives a resource back\n");
    printf("  res                               list resources and who waits for them\n");
    printf("  doctor [fix]                      find deadlock, starvation, fragmentation\n");
    printf("\n FILES (every write keeps a new version)\n");
    printf("  ls  pwd  cd <dir>  mkdir <dir>  rmdir <dir>  touch <file>\n");
    printf("  write <file> <text>  append <file> <text>  cat <file>   (use \\n for a new line)\n");
    printf("  versions <file>                   list all saved versions\n");
    printf("  restore <file> <n>                bring back version n\n");
    printf("  diff <file> <a> <b>               compare two versions\n");
    printf("  rmver <file> <n>                  delete one version      prune <file> <keep>  keep newest N\n");
    printf("  purge <file>                      delete a file for good (no trash)   rmdir -r <dir>\n");
    printf("  fsstat                            memory used by files (stored vs full copies)\n");
    printf("  rm <file>  trash  recover <file>  emptytrash      (rm only moves to trash)\n");
    printf("\n SYSTEM\n");
    printf("  dmesg [n]  history  !n  !!  uptime  whoami  echo <text>  clear  reboot  exit\n");
}

// Command "dmesg [n]": shows the last n lines (default 15) of the kernel log.
void cmd_dmesg(vector<string> &args) {
    int n = 15;
    if (args.size() >= 2) {
        n = atoi(args[1].c_str());
        if (n < 1) {
            n = 15;
        }
    }
    ifstream in("kernel.log");
    vector<string> lines;
    string one_line;
    while (getline(in, one_line)) {
        lines.push_back(one_line);
    }
    int start = 0;
    if ((int) lines.size() > n) {
        start = lines.size() - n;
    }
    for (size_t i = start; i < lines.size(); i++) {
        printf("%s\n", lines[i].c_str());
    }
    if (lines.size() == 0) {
        printf("(kernel log is empty)\n");
    }
}

// What it is: the command interpreter.
// What it does: splits one typed line into words, finds the command in the
// first word and calls the right function. Returns false when the user wants
// to leave the OS, true otherwise.
bool execute_command(string line) {
    vector<string> args;
    istringstream ss(line);
    string word;
    while (ss >> word) {
        args.push_back(word);
    }
    if (args.size() == 0) {
        return true;
    }
    string cmd = args[0];

    if (cmd == "exit" || cmd == "logout" || cmd == "shutdown") {
        return false;
    } else if (cmd == "help") {
        cmd_help();
    } else if (cmd == "run") {
        cmd_run(args);
    } else if (cmd == "ps") {
        cmd_ps();
    } else if (cmd == "step") {
        cmd_step(args);
    } else if (cmd == "runall") {
        cmd_runall();
        if (current_tick > 0) {
            cmd_gantt();
        }
    } else if (cmd == "sched") {
        cmd_sched(args);
    } else if (cmd == "aging") {
        cmd_aging(args);
    } else if (cmd == "gantt") {
        cmd_gantt();
    } else if (cmd == "stats") {
        cmd_stats();
    } else if (cmd == "kill") {
        cmd_kill(args);
    } else if (cmd == "crash") {
        cmd_crash(args);
    } else if (cmd == "memmap") {
        cmd_memmap();
    } else if (cmd == "fit") {
        cmd_fit(args);
    } else if (cmd == "compact") {
        cmd_compact();
    } else if (cmd == "lock") {
        cmd_lock(args);
    } else if (cmd == "unlock") {
        cmd_unlock(args);
    } else if (cmd == "res") {
        cmd_res();
    } else if (cmd == "doctor") {
        cmd_doctor(args);
    } else if (cmd == "ls") {
        cmd_ls();
    } else if (cmd == "pwd") {
        cmd_pwd();
    } else if (cmd == "cd") {
        cmd_cd(args);
    } else if (cmd == "mkdir") {
        cmd_mkdir(args);
    } else if (cmd == "rmdir") {
        cmd_rmdir(args);
    } else if (cmd == "touch") {
        cmd_touch(args);
    } else if (cmd == "write") {
        cmd_write(args, line, false);
    } else if (cmd == "append") {
        cmd_write(args, line, true);
    } else if (cmd == "cat") {
        cmd_cat(args);
    } else if (cmd == "versions") {
        cmd_versions(args);
    } else if (cmd == "restore") {
        cmd_restore(args);
    } else if (cmd == "diff") {
        cmd_diff(args);
    } else if (cmd == "rm") {
        cmd_rm(args);
    } else if (cmd == "trash") {
        cmd_trash();
    } else if (cmd == "recover") {
        cmd_recover(args);
    } else if (cmd == "emptytrash") {
        cmd_emptytrash();
    } else if (cmd == "rmver") {
        cmd_rmver(args);
    } else if (cmd == "prune") {
        cmd_prune(args);
    } else if (cmd == "purge") {
        cmd_purge(args);
    } else if (cmd == "fsstat") {
        cmd_fsstat();
    } else if (cmd == "dmesg") {
        cmd_dmesg(args);
    } else if (cmd == "history") {
        for (size_t i = 0; i < history.size(); i++) {
            printf("  %3d  %s\n", (int) i + 1, history[i].c_str());
        }
    } else if (cmd == "uptime") {
        printf("up %d ticks (simulated CPU time), %d seconds of real time\n", current_tick, (int) difftime(time(NULL), boot_time));
    } else if (cmd == "whoami") {
        printf("%s\n", username.c_str());
    } else if (cmd == "echo") {
        printf("%s\n", get_text_after(line, 1).c_str());
    } else if (cmd == "clear") {
        if (interactive) {
            printf("\033[2J\033[H");
        }
    } else if (cmd == "reboot") {
        init_kernel();
        log_event("---- system rebooted ----");
        printf("Rebooted. All processes, memory, resources and files were reset.\n");
    } else {
        printf("minios: command not found: %s   (type 'help')\n", cmd.c_str());
    }
    return true;
}

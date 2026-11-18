// resources.cpp - named resources (locks): lock, unlock, hand-over to waiting processes.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "resources.h"
#include "kernel_log.h"
#include "types.h"
#include "globals.h"

// What it is: a lookup for named resources (like "printer" or "disk").
// What it does: returns the index of the resource called `name`. If it does
// not exist and `create` is true, it makes a new one. Returns -1 if not found
// (or if there is no room for a new one).
int find_resource(string name, bool create) {
    for (int i = 0; i < res_count; i++) {
        if (res_name[i] == name) {
            return i;
        }
    }
    if (!create || res_count >= MAX_RES) {
        return -1;
    }
    res_name[res_count] = name;
    res_holder[res_count] = 0;
    res_count++;
    return res_count - 1;
}

// What it is: the "unlock" operation inside the kernel.
// What it does: frees resource `r`. If some process was blocked waiting for it,
// the lowest-numbered waiting process gets it and becomes READY again.
void release_resource(int r) {
    res_holder[r] = 0;
    for (int p = 1; p <= proc_count; p++) {
        if (procs[p].state == ST_BLOCKED && procs[p].waiting_for == r) {
            res_holder[r] = p;
            procs[p].waiting_for = -1;
            procs[p].state = ST_READY;
            ready_counter++;
            procs[p].ready_seq = ready_counter;
            procs[p].ready_wait = 0;
            printf("  -> resource '%s' is now granted to P%d, which is READY again\n", res_name[r].c_str(), p);
            log_event("resource '" + res_name[r] + "' granted to P" + to_string(p));
            break;
        }
    }
}

// Command "lock <pid> <resource>": process <pid> asks for a resource. If it is
// free the process gets it. If another process holds it, the process becomes
// BLOCKED until the holder releases it.
void cmd_lock(vector<string> &args) {
    if (args.size() < 3) {
        printf("usage: lock <pid> <resource>\n");
        return;
    }
    int pid = atoi(args[1].c_str());
    if (pid < 1 || pid > proc_count) {
        printf("lock: no such process '%s'\n", args[1].c_str());
        return;
    }
    if (procs[pid].state != ST_READY && procs[pid].state != ST_RUNNING) {
        printf("lock: P%d is %s and cannot request a resource now\n", pid, state_names[procs[pid].state]);
        return;
    }
    int r = find_resource(args[2], true);
    if (r == -1) {
        printf("lock: too many resources (maximum %d)\n", MAX_RES);
        return;
    }
    if (res_holder[r] == pid) {
        printf("P%d already holds '%s'\n", pid, res_name[r].c_str());
    } else if (res_holder[r] == 0) {
        res_holder[r] = pid;
        printf("P%d acquired '%s'\n", pid, res_name[r].c_str());
        log_event("P" + to_string(pid) + " acquired '" + res_name[r] + "'");
    } else {
        procs[pid].state = ST_BLOCKED;
        procs[pid].waiting_for = r;
        if (running_pid == pid) {
            running_pid = 0;
            quantum_used = 0;
        }
        printf("P%d is now BLOCKED: '%s' is held by P%d\n", pid, res_name[r].c_str(), res_holder[r]);
        log_event("P" + to_string(pid) + " blocked waiting for '" + res_name[r] + "' held by P" + to_string(res_holder[r]));
    }
}

// Command "unlock <pid> <resource>": process <pid> gives a resource back.
void cmd_unlock(vector<string> &args) {
    if (args.size() < 3) {
        printf("usage: unlock <pid> <resource>\n");
        return;
    }
    int pid = atoi(args[1].c_str());
    int r = find_resource(args[2], false);
    if (r == -1 || res_holder[r] != pid || pid < 1) {
        printf("unlock: process '%s' does not hold '%s'\n", args[1].c_str(), args[2].c_str());
        return;
    }
    printf("P%d released '%s'\n", pid, res_name[r].c_str());
    log_event("P" + to_string(pid) + " released '" + res_name[r] + "'");
    release_resource(r);
}

// Command "res": lists every resource, who holds it and who is waiting for it.
void cmd_res() {
    if (res_count == 0) {
        printf("No resources yet. Create one by using: lock <pid> <resource>\n");
        return;
    }
    printf("  RESOURCE     HELD BY   WAITING\n");
    for (int r = 0; r < res_count; r++) {
        printf("  %-12s ", res_name[r].c_str());
        if (res_holder[r] == 0) {
            printf("%-9s ", "free");
        } else {
            printf("P%-8d ", res_holder[r]);
        }
        for (int p = 1; p <= proc_count; p++) {
            if (procs[p].state == ST_BLOCKED && procs[p].waiting_for == r) {
                printf("P%d ", p);
            }
        }
        printf("\n");
    }
}

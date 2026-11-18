// kernel_log.cpp - the kernel log (the OS diary written to kernel.log).

#include <cstdio>
#include <string>
#include "kernel_log.h"
#include "types.h"
#include "globals.h"

// What it is: the OS diary.
// What it does: appends one line, stamped with the current tick, to kernel.log.
void log_event(string msg) {
    FILE *f = fopen("kernel.log", "a");
    if (f == NULL) {
        return;
    }
    fprintf(f, "[tick %04d] %s\n", current_tick, msg.c_str());
    fclose(f);
}

// kernel_log.h - the kernel log (the OS diary).

#ifndef MINIOS_KERNEL_LOG_H
#define MINIOS_KERNEL_LOG_H

#include <string>
using namespace std;

// Appends one line, stamped with the current tick, to kernel.log.
void log_event(string msg);

#endif

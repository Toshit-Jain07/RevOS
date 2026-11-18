// doctor.h - the doctor: health checks for the whole OS.

#ifndef MINIOS_DOCTOR_H
#define MINIOS_DOCTOR_H

#include <string>
#include <vector>
using namespace std;

// Looks for a circular wait among blocked processes. Fills cycle[] and *len when it finds one.
bool find_deadlock(int cycle[], int *len, bool skip[]);

// Shell command: "doctor" (report problems) or "doctor fix" (also break deadlocks).
void cmd_doctor(vector<string> &args);

#endif

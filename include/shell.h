// shell.h - the command interpreter.

#ifndef MINIOS_SHELL_H
#define MINIOS_SHELL_H

#include <string>
#include <vector>
using namespace std;

// Shell commands: "help" and "dmesg".
void cmd_help();
void cmd_dmesg(vector<string> &args);

// Runs one typed line. Returns false when the user wants to leave the OS.
bool execute_command(string line);

#endif

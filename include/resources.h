// resources.h - named resources (locks) that processes can hold or wait for.

#ifndef MINIOS_RESOURCES_H
#define MINIOS_RESOURCES_H

#include <string>
#include <vector>
using namespace std;

// Finds the resource called `name` (creating it if `create` is true). Returns -1 if not found.
int find_resource(string name, bool create);

// Frees resource `r` and hands it to the lowest-numbered process waiting for it.
void release_resource(int r);

// Shell commands: "lock <pid> <res>", "unlock <pid> <res>", "res".
void cmd_lock(vector<string> &args);
void cmd_unlock(vector<string> &args);
void cmd_res();

#endif

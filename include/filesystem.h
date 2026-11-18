// filesystem.h - in-memory file system with versioning and a trash bin.

#ifndef MINIOS_FILESYSTEM_H
#define MINIOS_FILESYSTEM_H

#include <string>
#include <vector>
using namespace std;

// Index of the live file/folder `name` in the current folder, or -1.
int find_in_cwd(string name);

// Full path of the current folder, e.g. "/notes".
string get_cwd_path();

// Creates a file or folder in the current folder. Returns its index or -1 if full.
int create_node(string name, bool is_dir);

// Shell commands (folders): "pwd", "ls", "mkdir", "cd", "rmdir".
void cmd_pwd();
void cmd_ls();
void cmd_mkdir(vector<string> &args);
void cmd_cd(vector<string> &args);
void cmd_rmdir(vector<string> &args);   // "rmdir -r <folder>" removes a folder with everything inside

// Shell commands (files): "touch", "write", "append", "cat".
void cmd_touch(vector<string> &args);
void cmd_write(vector<string> &args, string line, bool append);
void cmd_cat(vector<string> &args);

// Shell commands (time travel): "versions", "restore", "diff".
void cmd_versions(vector<string> &args);
void cmd_restore(vector<string> &args);
void cmd_diff(vector<string> &args);

// Shell commands (space and deleting): "rmver", "prune", "purge", "fsstat".
void cmd_rmver(vector<string> &args);
void cmd_prune(vector<string> &args);
void cmd_purge(vector<string> &args);
void cmd_fsstat();

// Shell commands (trash bin): "rm", "trash", "recover", "emptytrash".
void cmd_rm(vector<string> &args);
void cmd_trash();
void cmd_recover(vector<string> &args);
void cmd_emptytrash();

#endif

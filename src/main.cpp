// main.cpp - program entry point: boot screen, login and the main command loop.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <ctime>
#include <unistd.h>
#include "shell.h"
#include "kernel.h"
#include "kernel_log.h"
#include "filesystem.h"
#include "types.h"
#include "globals.h"

// What it is: the start-up animation.
// What it does: prints a fake boot log and the MiniOS banner. If a person is
// typing (slow == true) it pauses between lines to look like a real boot.
void boot_screen(bool slow) {
    const char *boot_lines[] = {
        "[ OK ] Starting MiniOS kernel...",
        "[ OK ] Process table ready      (100 slots)",
        "[ OK ] Memory manager ready     (256 KB RAM)",
        "[ OK ] Scheduler ready          (Round Robin, quantum 3)",
        "[ OK ] File system mounted      (versioning ON)",
        "[ OK ] Doctor service started"
    };
    cout << R"(
  __  __ _       _  ___  ____
 |  \/  (_)_ __ (_)/ _ \/ ___|
 | |\/| | | '_ \| | | | \___ \
 | |  | | | | | | | |_| |___) |
 |_|  |_|_|_| |_|_|\___/|____/
        a tiny operating system
)" << endl;
    for (int i = 0; i < 6; i++) {
        printf("%s\n", boot_lines[i]);
        fflush(stdout);
        if (slow) {
            usleep(250000);
        }
    }
    printf("\n");
}

// What it is: the program entry point - the whole life of MiniOS.
// What it does: boots the OS, asks for a username, then reads one command at a
// time and runs it until you type "exit". If the input comes from a file
// (./minios < demo.txt) every command is echoed so the output reads like a
// transcript. Use "./minios --fast" to skip the boot-animation pauses.
int main(int argc, char *argv[]) {
    interactive = isatty(0);
    bool fast = false;
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "--fast") {
            fast = true;
        }
    }
    srand(time(NULL));
    boot_time = time(NULL);

    FILE *f = fopen("kernel.log", "w");   // start with an empty log
    if (f != NULL) {
        fclose(f);
    }
    init_kernel();
    log_event("kernel booted");
    boot_screen(interactive && !fast);

    // login
    printf("minios login: ");
    fflush(stdout);
    string name_typed;
    if (!getline(cin, name_typed)) {
        printf("\n");
        return 0;
    }
    if (!interactive) {
        printf("%s\n", name_typed.c_str());
    }
    username = "";
    for (size_t i = 0; i < name_typed.size(); i++) {
        if (name_typed[i] != ' ' && name_typed[i] != '\t') {
            username += name_typed[i];
        }
    }
    if (username == "") {
        username = "guest";
    }
    printf("Welcome, %s! Type 'help' to see what you can do.\n\n", username.c_str());
    log_event("user '" + username + "' logged in");

    // main loop: read a line, run it
    string line;
    while (true) {
        printf("%s@minios:%s [t=%d]$ ", username.c_str(), get_cwd_path().c_str(), current_tick);
        fflush(stdout);
        if (!getline(cin, line)) {
            printf("\n");
            break;
        }
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] == '\t' || line[i] == '\r') {
                line[i] = ' ';
            }
        }
        if (!interactive) {
            printf("%s\n", line.c_str());
        }

        // "!n" re-runs command number n from the history, "!!" re-runs the last one
        if (line.size() >= 2 && line[0] == '!') {
            string rest = line.substr(1);
            int n;
            if (rest == "!") {
                n = history.size();
            } else {
                n = atoi(rest.c_str());
            }
            if (n < 1 || n > (int) history.size()) {
                printf("history: no such command\n");
                continue;
            }
            line = history[n - 1];
            printf("(running: %s)\n", line.c_str());
        }

        bool blank = true;
        for (size_t i = 0; i < line.size(); i++) {
            if (line[i] != ' ') {
                blank = false;
            }
        }
        if (!blank) {
            history.push_back(line);
        }

        if (!execute_command(line)) {
            break;
        }
    }
    printf("Shutting down MiniOS. Goodbye, %s!\n", username.c_str());
    log_event("system shut down");
    return 0;
}

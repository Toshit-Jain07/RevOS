// utils.cpp - small text helpers shared by the shell and the file system.

#include <string>
#include "utils.h"

// What it is: a text grabber for commands like "write".
// What it does: returns the part of the typed line AFTER the first `skip`
// words, and turns the two characters \n into a real new line so you can
// write multi-line files in one command.
string get_text_after(string line, int skip) {
    int pos = 0;
    int n = line.size();
    for (int t = 0; t < skip; t++) {
        while (pos < n && line[pos] == ' ') {
            pos++;
        }
        while (pos < n && line[pos] != ' ') {
            pos++;
        }
    }
    while (pos < n && line[pos] == ' ') {
        pos++;
    }
    string rest = line.substr(pos);
    string out = "";
    for (size_t i = 0; i < rest.size(); i++) {
        if (rest[i] == '\\' && i + 1 < rest.size() && rest[i + 1] == 'n') {
            out += '\n';
            i++;
        } else {
            out += rest[i];
        }
    }
    return out;
}

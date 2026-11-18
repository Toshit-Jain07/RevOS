// utils.h - small text helpers.

#ifndef MINIOS_UTILS_H
#define MINIOS_UTILS_H

#include <string>
using namespace std;

// Returns the part of `line` after the first `skip` words, with \n turned into a real new line.
string get_text_after(string line, int skip);

#endif

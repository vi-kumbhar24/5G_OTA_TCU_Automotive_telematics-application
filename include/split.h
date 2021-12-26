#ifndef SPLIT_H
#define SPLIT_H

#include <string.h>
#include "autonet_types.h"
using namespace std;

Strings split(string line, const char *sep) {
    Strings strings;
    char *ln = new char[line.size()+1];
    strcpy(ln, line.c_str());
    char *tok = strtok(ln, sep);
    while (tok != NULL) {
        string str = tok;
        strings.push_back(str);
        tok = strtok(NULL, sep);
    }
    delete[] ln;
    return strings;
}

#endif

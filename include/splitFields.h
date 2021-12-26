#ifndef SPLITFIELDS_H
#define SPLITFIELDS_H

#include <string.h>
#include "autonet_types.h"
using namespace std;

Strings splitFields(string line, const char *sep) {
    Strings strings;
    char *ln = new char[line.size()+1];
    strcpy(ln, line.c_str());
    char *p = ln;
    bool last_was_sep = false;
    while (*p != '\0') {
        last_was_sep = false;
        char *tok = p;
        while ((*p != '\0') and (strchr(sep,*p) == NULL)) {
           p++;
        }
        if (*p != '\0') {
           last_was_sep = true;
           *p = '\0';
           p++;
        }
        string str = tok;
        strings.push_back(str);
    }
    if (last_was_sep) {
        strings.push_back("");
    }
    delete[] ln;
    return strings;
}

#endif

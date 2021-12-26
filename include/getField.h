#ifndef GETFIELD_H
#define GETFIELD_H

#include <string.h>
using namespace std;

string getField(string ln, string sep, int n) {
    string outstr = "";
    char tokstr[ln.size()+1];
    strcpy(tokstr, ln.c_str());
    char *tok = strtok(tokstr, sep.c_str());
    for (; (n > 0) and (tok != NULL); n--) {
        tok = strtok(NULL, sep.c_str());
    }
    if (tok != NULL) {
        outstr = tok;
    }
    return outstr;
}

#endif

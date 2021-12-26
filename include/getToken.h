#ifndef GETTOKEN_H
#define GETTOKEN_H

#include "autonet_types.h"
#include "split.h"
using namespace std;

string getToken(string line, int n) {
    Strings toks = split(line, " \t\n");
    return toks[n];
}

#endif

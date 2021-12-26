#ifndef STR_SYSTEM_H
#define STR_SYSTEM_H

#include <string>
#include <stdlib.h>
using namespace std;

int str_system(string cmd) {
    return system(cmd.c_str());
}

#endif

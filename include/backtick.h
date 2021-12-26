#ifndef BACKTICK_H
#define BACKTICK_H

#include <string>
#include <cstring>
#include <stdio.h>

using namespace std;

string backtick(string command) {
    FILE *pipe = popen(command.c_str(), "r");
    string outstr = "";
    if (pipe) {
        char buffer[128];
        while (!feof(pipe)) {
            if (fgets(buffer, 128, pipe) != NULL) {
                outstr += buffer;
            }
        }
        pclose(pipe);
        if (outstr.size() > 0)  {
            if (outstr.substr(outstr.size()-1) == "\n") {
                outstr.replace(outstr.size()-1, 1, "");
            }
        }
    }
    return outstr;
}

#endif

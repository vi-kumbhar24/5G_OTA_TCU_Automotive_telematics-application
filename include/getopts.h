#ifndef GETOPTS_H
#define GETOPTS_H

#include <map>
#include <string.h>
#include <stdlib.h>
using namespace std;

class GetOpts {
private:
    map<string,string> args;

public:
    GetOpts(int &argc, char *argv[], const char *flags) {
        while (argc > 1) {
            char *arg = argv[1];
            if ( (strlen(arg) == 2) and
                 (arg[0] == '-') ) {
                char flag = arg[1];
                const char *pos = strchr(flags, flag);
                if (pos == NULL) break;
                pos += 1;
                bool hasarg = false;
                if (*pos == ':') {
                    hasarg = true;
                }
                for (int j=2; j < argc; j++) {
                    argv[j-1] = argv[j];
                }
                argc--;
                string flagstr;
                flagstr.assign(1, flag);
                args[flagstr] = "";
                if (hasarg) {
                    if (argc > 1) {
                        args[flagstr] = argv[1];
                        for (int j=2; j < argc; j++) {
                            argv[j-1] = argv[j];
                        }
                        argc--;
                    }
                }
            }
            else break;
        }
    }

    bool exists(string flag) {
        return (args.find(flag) != args.end());
    }

    string get(string flag) {
        string val = "";
        if (exists(flag)) {
            val = args[flag];
        }
        return val;
    }

    int getNumber(string flag) {
        int val = 0;
        if (exists(flag)) {
           string str = args[flag];
           val = atoi(str.c_str());
        }
        return val;
    }
};

#endif

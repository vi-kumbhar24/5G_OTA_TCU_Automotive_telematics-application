#ifndef PGREP_H
#define PGREP_H

#include <fstream>
#include <string>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include "autonet_types.h"
#include "getStringFromFile.h"
#include "split.h"
#include "string_convert.h"
using namespace std;

string pgrep(string comm) {
    string found_pids = "";
    struct dirent *dirp;
    DIR *dp = opendir("/proc");
    if (dp != NULL) {
        while ((dirp = readdir(dp)) != NULL) {
            string pid = dirp->d_name;
            if (pid.find_first_not_of("0123456789") == string::npos) {
                string pname = getStringFromFile("/proc/"+pid+"/comm");
                if (pname == comm) {
                    found_pids += " "+pid;
                }
            }
        }
        closedir(dp);
    }
    else {
        cerr << "Failed to open dir" << endl;
    }
    if (found_pids != "") {
        found_pids.erase(0,1);
    }
    return found_pids;
}

void pkill(string comm, int sig) {
    string pidstr = pgrep(comm);
    if (pidstr != "") {
        Strings pids = split(pidstr, " ");
        for (int i=0; i < pids.size(); i++) {
            kill(sig, stringToInt(pids[i]));
        }
    }
}

#endif

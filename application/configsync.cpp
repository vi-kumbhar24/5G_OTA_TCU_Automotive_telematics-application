#include <iostream>
#include "autonet_files.h"
#include "autonet_types.h"
#include "readAssoc.h"
#include "writeAssoc.h"
#include "getopts.h"
using namespace std;

int main(int argc, char *argv[]) {
    int exitval = 0;
    Assoc config = readAssoc(SyncConfigFile, " ");
    GetOpts opts(argc, argv, "p:s:u:");
    bool change = false;
    if (opts.exists("p")) {
        config["password"] = opts.get("p");
        change = true;
    }
    if (opts.exists("u")) {
        config["username"] = opts.get("u");
        change = true;
    }
    if (opts.exists("s")) {
        config["servers"] = opts.get("s");
        change = true;
    }
    if (change) {
        string tmpfile = string(SyncConfigFile)+".tmp";
        writeAssoc(tmpfile, config, " ");
        rename(tmpfile.c_str(), SyncConfigFile);
    }
    else {
        Assoc::iterator it;
        for (it=config.begin(); it != config.end(); it++) {
            string var = (*it).first;
            string val = config[var];
            cout << var << ":" << val << endl;
        }
    }
    return exitval;
}

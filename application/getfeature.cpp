#include <iostream>
#include <stdlib.h>
#include "my_features.h"
#include "getopts.h"
using namespace std;

int main(int argc, char **argp) {
    int exitval = 0;
    GetOpts opts(argc, argp, "de");
    bool check_enable = opts.exists("e");
    if (argc != 2) {
        cerr << "Usage: getfeature <feature>" << endl;
        exit(1);
    }
    string var = argp[1];
    Features features;
    if (check_enable) {
        if (!features.isEnabled(var)) {
            exitval = 1;
        }
    }
    else if (features.exists(var)) {
        string val = features.getFeature(var);
        cout << var << "=" << val << endl;
    }
    return exitval;
}

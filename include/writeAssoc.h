#ifndef WRITEASSOC_H
#define WRITEASSOC_H

#include <fstream>
#include "autonet_types.h"
using namespace std;

void writeAssoc(string file, Assoc assoc, string sep) {
    ofstream outf(file.c_str());
    if (outf.is_open()) {
        Assoc::iterator it;
        for (it=assoc.begin(); it != assoc.end(); it++) {
            string var = (*it).first;
            string val = assoc[var];
            outf << var << sep << val << endl;
        }
        outf.close();
    }
}

#endif

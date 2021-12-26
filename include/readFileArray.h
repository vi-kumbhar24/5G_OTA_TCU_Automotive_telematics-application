#ifndef READFILEARRAY_H
#define READFILEARRAY_H

#include <fstream>
#include "autonet_types.h"
using namespace std;

Strings readFileArray(string file) {
    Strings outarray;
    string line;
    ifstream fin(file.c_str());
    if (fin.is_open()) {
        while (fin.good()) {
            getline(fin, line);
            if (fin.eof()) break;
            outarray.push_back(line);
        }
        fin.close();
    }
    else {
        cerr << "Failed to open: " << file << endl;
    }
    return(outarray);
}

#endif

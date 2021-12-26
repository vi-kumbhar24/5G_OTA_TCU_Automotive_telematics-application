#ifndef READASSOC_H
#define READASSOC_H

#include <iostream>
#include <fstream>
#include "autonet_types.h"
#include "fileExists.h"
using namespace std;

Assoc readAssoc(string file, const char *sep) {
    Assoc assoc;
    string line;
    if (fileExists(file)) {
        ifstream fin(file.c_str());
        if (fin.is_open()) {
            while (fin.good()) {
                getline(fin, line);
                if (fin.eof()) break;
                if (line.substr(0,1) == "#") continue;
                if (line.find_first_not_of(" ") == string::npos) continue;
                int pos = line.find(sep);
                string var = line;
                string val = "";
                if (pos != string::npos) {
                    var = line.substr(0,pos);
                    val = line.substr(pos+1);
                }
                assoc[var] = val;
            }
            fin.close();
        }
        else {
            cerr << "Failed to open: " << file << endl;
        }
    }
    return assoc;
}

#endif

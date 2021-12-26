#ifndef GREPFILE_H
#define GREPFILE_H

#include <fstream>
using namespace std;

string grepFile(string file, string search) {
    string outline = "";
    ifstream fin(file.c_str());
    if (fin.is_open()) {
        while (fin.good()) {
            string line;
            getline(fin, line);
            if (line.find(search) != string::npos) {
                outline = line;
                break;
            }
        }
        fin.close();
    }
    else {
        cerr << "Failed to open: " << file << endl;
    }
    return outline;
}

#endif

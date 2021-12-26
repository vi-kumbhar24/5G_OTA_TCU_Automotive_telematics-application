#ifndef READFILE_H
#define READFILE_H

#include <fstream>
using namespace std;

string readFile(string file) {
    string output = "";
    if (fileExists(file)) {
        ifstream fin(file.c_str());
        if (fin.is_open()) {
            while (fin.good()) {
                string line;
                getline(fin, line);
                if (!fin.eof()) {
                    output += line + "\n";
                }
            }
            fin.close();
        }
        else {
            cerr << "Failed to open: " << file << endl;
        }
    }
    return output;
}

#endif

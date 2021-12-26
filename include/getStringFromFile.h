#ifndef GETSTRINGFROMFILE_H
#define GETSTRINGFROMFILE_H

#include <fstream>
using namespace std;

string getStringFromFile(string file) {
    string outline = "";
    ifstream fin(file.c_str());
    if (fin.is_open()) {
        if (fin.good()) {
            getline(fin, outline);
        }
        fin.close();
    }
    else {
        cerr << "Failed to open: " << file << endl;
    }
    return outline;
}

#endif

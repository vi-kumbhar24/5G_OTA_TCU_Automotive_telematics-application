#ifndef READBINARYFILE_H
#define READBINARYFILE_H

#include <fstream>
using namespace std;


string readBinaryFile(string file) {
    string str = "";
    if (fileExists(file)) {
        ifstream fin(file.c_str());
        if (fin.is_open()) {
            fin.seekg(0, ios::end);
            int size = fin.tellg();
            fin.seekg(0, ios::beg);
            char *buffer = new char[size];
            fin.read(buffer, size);
            fin.close();
            str.assign(buffer, size);
            delete[] buffer;
        }
        else {
            cerr << "Failed to open: " << file << endl;
        }
    }
    return str;
}

#endif

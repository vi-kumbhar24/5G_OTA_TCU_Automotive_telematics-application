#ifndef WRITEBINARYFILE_H
#define WRITEBINARYFILE_H

#include <fstream>
using namespace std;

void writeBinaryFile(string file, const char *data, int len) {
    ofstream fout(file.c_str());
    if (fout.is_open()) {
        fout.write(data, len);
        fout.close();
    }
}

void writeBinaryFile(string file, string data) {
    writeBinaryFile(file, data.c_str(), data.size());
}

#endif

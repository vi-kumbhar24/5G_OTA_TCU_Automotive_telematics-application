#ifndef WRITESTRINGTOFILE_H
#define WRITESTRINGTOFILE_H

#include <fstream>
using namespace std;

void writeStringToFile(string file, string str) {
     ofstream fout(file.c_str());
     if (fout.is_open()) {
         if (str != "") {
             fout << str << endl;
         }
         fout.close();
     }
}

#endif

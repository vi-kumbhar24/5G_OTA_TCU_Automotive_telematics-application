#ifndef APPENDTOFILE_H
#define APPENDTOFILE_H

#include <fstream>
using namespace std;

void appendToFile(string file, string str) {
     ofstream fout(file.c_str(), ios_base::out|ios_base::app);
     if (fout.is_open()) {
         fout << str << endl;
         fout.close();
     }
}

#endif

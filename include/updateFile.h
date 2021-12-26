#ifndef UPDATEFILE_H
#define UPDATEFILE_H

#include <stdio.h>
#include "writeStringToFile.h"
using namespace std;

void updateFile(string file, string str) {
     string tempfile = file;
     tempfile.append(".tmp");
     writeStringToFile(tempfile, str);
     rename(tempfile.c_str(), file.c_str());
}

#endif

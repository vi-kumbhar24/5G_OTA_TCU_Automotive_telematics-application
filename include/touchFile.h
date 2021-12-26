#ifndef TOUCHFILE_H
#define TOUCHFILE_H

#include "writeStringToFile.h"
using namespace std;

void touchFile(string file) {
     writeStringToFile(file, "");
}

#endif

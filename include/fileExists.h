#ifndef FILE_EXISTS_H
#define FILE_EXISTS_H

#include <sys/stat.h>
using namespace std;

bool fileExists(string file) {
    bool retval = false;
    struct stat st_info;
    int v = stat(file.c_str(), &st_info);
    if (v == 0) {
        retval = true;
    }
    return retval;
}

#endif

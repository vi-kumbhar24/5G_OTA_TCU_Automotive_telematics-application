#ifndef FILESTATS_H
#define FILESTATS_H

#include <sys/stat.h>
#include <sys/types.h>
using namespace std;

time_t fileTime(string file) {
    time_t retval = 0;
    struct stat st_info;
    int v = stat(file.c_str(), &st_info);
    if (v == 0) {
        retval = st_info.st_mtime;
    }
    return retval;
}

off_t fileSize(string file) {
    off_t retval = 0;
    struct stat st_info;
    int v = stat(file.c_str(), &st_info);
    if (v == 0) {
        retval = st_info.st_size;
    }
    return retval;
}

nlink_t fileNLink(string file) {
    nlink_t retval = 0;
    struct stat st_info;
    int v = stat(file.c_str(), &st_info);
    if (v == 0) {
        retval = st_info.st_nlink;
    }
    return retval;
}

#endif

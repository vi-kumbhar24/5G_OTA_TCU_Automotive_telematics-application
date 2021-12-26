#ifndef HIRESTIME_H
#define HIRESTIME_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
using namespace std;

double hiresTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double tod = tv.tv_sec + tv.tv_usec/1000000.;
    return tod;
}

string hiresDateTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    tm *ptm;
    ptm = gmtime(&tv.tv_sec);
    char buffer[30];
    sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d.%06ld",
            ptm->tm_year+1900,ptm->tm_mon+1,ptm->tm_mday,
            ptm->tm_hour,ptm->tm_min,ptm->tm_sec,tv.tv_usec);
    string timestr = buffer;
    return timestr;
}

#endif

#ifndef DATETIME_H
#define DATETIME_H

#include <stdio.h>
using namespace std;

string getDateTime(time_t timeval) {
    tm *ptm;
    ptm = gmtime(&timeval);
    char buffer[20];
    sprintf(buffer, "%04d/%02d/%02d %02d:%02d:%02d",
            ptm->tm_year+1900,ptm->tm_mon+1,ptm->tm_mday,
            ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
    string timestr = buffer;
    return timestr;
}

string getDateTime(void) {
    time_t now = time(NULL);
    return getDateTime(now);
}

#endif

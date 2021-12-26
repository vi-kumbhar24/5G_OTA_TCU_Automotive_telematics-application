#ifndef UPTIME_H
#define UPTIME_H

#include <sys/sysinfo.h>
using namespace std;

unsigned long getUptime() {
    struct sysinfo sysinf;
    sysinfo(&sysinf);
    unsigned long upval = sysinf.uptime;
    return upval;
}

#endif

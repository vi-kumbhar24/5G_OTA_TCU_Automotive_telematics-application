#include <iostream>
#include <string>
#include <time.h>
#include "convertTimezone.h"
using namespace std;

int main(int argc, char **argp) {
    struct tm tm;
    time_t now;
    time(&now);
        localtime_r(&now, &tm);
        printf("%4d/%02d/%02d %02d:%02d:%02d %s\n",
               tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_zone);
        system("date");

    for (int i=1; i < argc; i++) {
setenv("TZ", "UTC", 1);
//unsetenv("TZ");
tzset();
localtime_r(&now, &tm);
        string tzenv = string("TZ=")+argp[i];
//        putenv((char *)(tzenv.c_str()));
        setenv("TZ", argp[i], 1);
tzset();
        localtime_r(&now, &tm);
        printf("%s: %4d/%02d/%02d %02d:%02d:%02d %s\n",
               argp[i],
               tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_zone);
//        system("date");
    }
/*
    Timezones zones;
    for (int i=1; i < argc; i++) {
        string zone = argp[i];
        Timezone tz(zone);
        if (!tz.isValid()) {
            cerr << "Invalid timezone:" << zone << endl;
        }
        else {
            if (zones.find(zone) != zones.end()) {
                cout << "Zone already saved:" << zone << endl;
            }
            else {
                zones[zone] = tz;
            }
        }
    }
    time_t now;
    time(&now);
    Timezones::iterator it;
    for (it=zones.begin(); it != zones.end(); it++) {
        string cur_tz;
        struct tm tm;
        string z = (*it).first;
        zones[z].gettime(now, &tm, cur_tz);
        printf("%s: %4d/%02d/%02d %02d:%02d:%02d %s\n",
               z.c_str(),
               tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec, cur_tz.c_str());
    }
*/
    return 0;
}

/****************************************************************************
 * File: shutdown.cpp
 * 
 * Routine to shutdown the iMX
 *
 * 2014/04/02 rwp
 *    AUT-84: Added code to reset cell module (-r)
 ****************************************************************************
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "getopts.h"
#include "backtick.h"
#include "autonet_files.h"
#include "string_convert.h"
#include "updateFile.h"
#include "vic.h"

using namespace std;

int getResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
void getTime();
void setTime();

VIC vic;
int app_id = 3;
bool notime = false;
bool nosettime = false;
bool debug = false;

int main(int argc, char *argp[]) {
    bool reset = false;
    Byte buffer[256];
    int len = 0;
    vic.openSocket();
    GetOpts opts(argc, argp, "dnrt");
    notime = opts.exists("n");
    nosettime = opts.exists("t");
    debug = opts.exists("d");
    reset = opts.exists("r");
    if (!nosettime) {
        getTime();
        setTime();
    }
    if (argc != 3) {
        cout << "Usage: shutdown <mode> <time>" << endl;
        exit(0);
    }
    string arg1 = argp[1];
    string arg2 = argp[2];
    if (reset) {
        // Pulse ON key
        cout << "Resetting cell modem" << endl;
        buffer[0] = 2;
        vic.sendRequest(app_id, 0x05, 1, buffer);
        len = getExpectedResponse(0x05, buffer, 2, true);
        sleep(2);
        buffer[0] = 2;
        vic.sendRequest(app_id, 0x05, 1, buffer);
        len = getExpectedResponse(0x05, buffer, 2, true);
        sleep(5);
    }
    cout << "Shutting down processes" << endl;
    updateFile(WhosKillingMonitorFile, "shutdown");
    system("/etc/init.d/monitor stop");
    system("pkill -USR1 respawner");
    sleep(1);
    backtick("cell_shell at_ogps=0");
    system("pkill gps_mon");
    system("pkill event_logger");
    sleep(2);
    cout << "Unmounting autonet partitions" << endl;
    system("/etc/init.d/mount_autonet stop");
    sleep(1);
    cout << "Sending shutdown: " << arg1 << " " << arg2 << endl;
    buffer[0] = 5;
    buffer[1] = stringToInt(arg1);
    buffer[2] = stringToInt(arg2);
    vic.sendRequest(app_id, 0x04, 3, buffer);
    len = getExpectedResponse(0x04, buffer, 2, true);
    return 0;
}

int getResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    vic.sendRequest(app_id, cmnd_id);
    int len = getExpectedResponse(cmnd_id, buffer, expected, exact);
    return len;
}

int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    int len = vic.getResp(buffer,5);
    if (len == 0) {
        cerr << "No response to command:" << cmnd_id << endl;
    }
    else if (len < 2) {
        cerr << "Response too short for command:" << cmnd_id << " len=" << len << endl;
        len = 0;
    }
    else if (buffer[0] != app_id) {
        cerr << "Response has wrong app_id:" << app_id << " got:" << (int)buffer[0] << endl;
        len = 0;
    }
    else if (buffer[1] != cmnd_id) {
        cerr << "Response has wrong cmnd_id:" << cmnd_id << " got:" << (int)buffer[1] << endl;
        len = 0;
    }
    else if (exact and (len != expected)) {
        cerr << "Invalid response length for command:" << cmnd_id << " len:" << len << " expected:" << expected << endl;
        len = 0;
    }
    else if (!exact and (len < expected)) {
        cerr << "Response to short for command:" << cmnd_id << " len:" << len << " expected:" << expected << endl;
        len = 0;
    }
    if (debug and (len > 0)) {
        printf("Rcvd:");
        for (int i=0; i < len; i++) {
            printf(" %02X", (int)buffer[i]);
        }
        printf("\n");
    }
    return len;
}

void getTime() {
     struct tm tm;
     vic.getRtc(app_id, &tm);
     printf("Date/time: %04d/%02d/%02d %02d:%02d:%02d\n",
            tm.tm_year, tm.tm_mon, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void setTime() {
    struct tm tm;
    time_t now;
    bool setsystime = false;
    if (!notime) {
        string time_str = backtick("cell_shell gettime 2>/dev/null");
        int year, mon, mday, hour, min, sec;
        int pos = time_str.find("\n");
        time_str.erase(0,pos);
        int num = sscanf(time_str.c_str(), "%d/%d/%d %d:%d:%d",
                         &year, &mon, &mday, &hour, &min, &sec);
        if ((num == 6) and (year > 2000)) {
            tm.tm_year = year - 1900;
            tm.tm_mon = mon - 1;
            tm.tm_mday = mday;
            tm.tm_hour = hour;
            tm.tm_min = min;
            tm.tm_sec = sec;
            now = timegm(&tm);
            setsystime = true;
        }
    }
    if (notime or !setsystime) {
        now = time(NULL);
        gmtime_r(&now, &tm);
        if ((tm.tm_year+1900) < 2000) {
            tm.tm_year = 2000 - 1900;
            setsystime = true;
        }
    }
    if (setsystime) {
        struct timeval tv;
        tv.tv_sec = now;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }
    vic.setRtc(app_id);
}

/***************************************************************************
 * File: vicControl.cpp
 *
 * Test program for talking to VIC
 *
 ***************************************************************************
 * Change log:
 * 2012/08/02 rwp
 *   Moved getVersion code to vic.h
 * 2012/12/20 rwp
 *   Fixed displaying tire pressures if ideal pressure unknown
 * 2013/08/09 rwp
 *   Added battery adjust values
 *   Added "else" for panic and other status
 * 2013/09/05 rwp
 *   Added ignition status and low fuel
 * 2014/04/10 rwp
 *   Added Get Fuel Consumption
 ***************************************************************************
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "getopts.h"
#include "backtick.h"
#include "string_convert.h"
#include "string_printf.h"
#include "autonet_files.h"
#include "my_features.h"
#include "split.h"
#include "vic.h"

using namespace std;

void controlCar(string dev_s, string act_s);
int getResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
void getTime();
void setTime();
void getVersion();
void getStatus();
void getLastStatus();
void decodeStatus(int len, Byte *buffer);
void getTirePressures();
void getOilLife();
void getFuelLevel();
void getVin();
void getSpeed();
void getOdometer();
void getSeats();
void getFaults();
void getBusType();
void decodeBusType(string can, Byte type, Byte speed);
void getQuickScanData();
void ecuLearning();

VIC vic;
int app_id = 1;
bool notime = false;
bool nosettime = false;
bool debug = false;

int main(int argc, char *argp[]) {
    Byte buffer[256];
    Byte notifications[] = {0xF1};
//    vic.openSocket(1,notifications);
    vic.openSocket();
    GetOpts opts(argc, argp, "dnt");
    notime = opts.exists("n");
    nosettime = opts.exists("t");
    debug = opts.exists("d");
    if (argc < 2) {
        cout << "Usage: vicControl command [val1 [val2]]" << endl;
        cout << "Possible commands:" << endl;
        cout << "    setwatchdog <mins>" << endl;
        exit(0);
    }
    if (!nosettime) {
        getTime();
        setTime();
        getTime();
    }
    getVersion();
    for (int idx=1; idx < argc; idx++) {
        string arg1 = argp[idx];
        string arg2 = "";
        string arg3 = "";
        if (argc > (idx+1)) {
            arg2 = argp[idx+1];
        }
        if (argc > (idx+2)) {
            arg3 = argp[idx+2];
        }
        if (arg1 == "setwatchdog") {
            cout << "Setting watchdog: " << arg2 << endl;
            buffer[0] = stringToInt(arg2);
            vic.sendRequest(app_id, 0x07, 1, buffer);
            int len = getExpectedResponse(0x07, buffer, 2, true);
            idx++;
        }
        else {
            cerr << "Invalid command" << endl;
        }
    }
    return 0;
}

int getResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    vic.sendRequest(app_id, cmnd_id);
    int len = getExpectedResponse(cmnd_id, buffer, expected, exact);
    return len;
}

int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    int len;
    while (true) {
        len = vic.getResp(buffer,5);
        if (len == 0) {
            cerr << "No response to command:" << cmnd_id << endl;
        }
        else if (len < 2) {
            cerr << "Response too short for command:" << cmnd_id << " len=" << len << endl;
            len = 0;
        }
        else if (buffer[0] == 0x00) {
            printf("Got notification: %02X\n",buffer[1]);
            continue;
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
        break;
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

void getVersion() {
    vic.getVersion(app_id);
    if (vic.build == "standard") {
        cout << "VIC Version:";
        cout << " HW:" << vic.hw_version << " FW:" << vic.fw_version << endl;
    }
    else {
        cout << "VIC Version: " << vic.build << "-" << vic.version_str << endl;
        cout << "Requested Version: " << vic.requested_build << endl;
    }
    if (vic.loader_version_str != "") {
        cout << "Loader Version: " << vic.loader_version_str << endl;
    }
}


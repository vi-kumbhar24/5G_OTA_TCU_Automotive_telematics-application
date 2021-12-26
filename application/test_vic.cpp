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
void getCounts();

VIC vic;
int app_id = 1;
bool notime = false;
bool nosettime = false;
bool debug = false;
int resp_timeout = 5;

int main(int argc, char *argp[]) {
    Byte buffer[256];
    Byte notifications[] = {0xF1};
//    vic.openSocket(1,notifications);
    vic.openSocket();
    GetOpts opts(argc, argp, "dntT:");
    notime = opts.exists("n");
    nosettime = opts.exists("t");
    debug = opts.exists("d");
    if (opts.exists("T")) {
        resp_timeout = opts.getNumber("T");
    }
    if (argc < 2) {
        cout << "Usage: vicControl command [val1 [val2]]" << endl;
        cout << "Possible commands:" << endl;
        cout << "    cellreset [1|2]" << endl;
        cout << "    setwatchdog <mins>" << endl;
        cout << "    setant [0|1|2|3]" << endl;
        cout << "    setquickscan <mins>" << endl;
        cout << "    shutdown <mode> <time>" << endl;
        cout << "    resetcpu <secs>" << endl;
        cout << "    setbatval <warn> <crit>" << endl;
        cout << "    getstatus" << endl;
        cout << "    getlaststatus" << endl;
        cout << "    gettires" << endl;
        cout << "    getodom" << endl;
        cout << "    getoil" << endl;
        cout << "    getfuel" << endl;
        cout << "    getvin" << endl;
        cout << "    getspeed" << endl;
        cout << "    getseats" << endl;
        cout << "    getfaults" << endl;
        cout << "    getquickscan" << endl;
        cout << "    getbustype(OBD)" << endl;
        cout << "    eculearning" << endl;
        cout << "    crash <impact_sensors> <airbags_deployed>" << endl;
        cout << "    getcounts" << endl;
        exit(0);
    }
    if (!nosettime) {
        getTime();
        setTime();
        getTime();
    }
//    getVersion();
    string arg1 = argp[1];
    string arg2 = "";
    string arg3 = "";
    if (argc > 2) {
        arg2 = argp[2];
    }
    if (argc > 3) {
        arg3 = argp[3];
    }
    if (arg1 == "cellreset") {
        cout << "Controlling cell reset" << endl;
        buffer[0] = stringToInt(arg2);
        vic.sendRequest(app_id, 0x05, 1, buffer);
        int len = getExpectedResponse(0x05, buffer, 2, true);
    }
    else if (arg1 == "setwatchdog") {
        cout << "Setting watchdog: " << arg2 << endl;
        buffer[0] = stringToInt(arg2);
        vic.sendRequest(app_id, 0x07, 1, buffer);
        int len = getExpectedResponse(0x07, buffer, 2, true);
    }
    else if (arg1 == "setant") {
        cout << "Setting antenna: " << arg2 << endl;
        buffer[0] = stringToInt(arg2);
        vic.sendRequest(app_id, 0x09, 1, buffer);
        int len = getExpectedResponse(0x09, buffer, 2, true);
    }
    else if (arg1 == "setquickscan") {
        cout << "Setting quickscan time: " << arg2 << endl;
        int time = stringToInt(arg2);
        buffer[0] = time >> 8;
        buffer[1] = time & 0xFF;
        buffer[2] = 0;
        buffer[3] = 0;
        buffer[4] = 0;
        buffer[5] = 0;
        buffer[6] = 0;
        buffer[7] = 0;
        vic.sendRequest(app_id, 0x0C, 8, buffer);
        int len = getExpectedResponse(0x0C, buffer, 2, true);
    }
    else if (arg1 == "shutdown") {
        cout << "Sending shutdown: " << arg2 << " " << arg3 << endl;
        buffer[0] = 5;
        buffer[1] = stringToInt(arg2);
        buffer[2] = stringToInt(arg3);
        vic.sendRequest(app_id, 0x04, 3, buffer);
        int len = getExpectedResponse(0x04, buffer, 2, true);
    }
    else if (arg1 == "resetcpu") {
        cout << "Resetting CPU: " << arg2 << endl;
        buffer[0] = 5;
        buffer[1] = stringToInt(arg2);
        vic.sendRequest(app_id, 0x08, 2, buffer);
        int len = getExpectedResponse(0x08, buffer, 2, true);
    }
    else if (arg1 == "setbatval") {
        cout << "Setting battery values: " << arg2 << " " << arg3 << endl;
        buffer[0] = stringToInt(arg2);
        buffer[1] = stringToInt(arg3);
        vic.sendRequest(app_id, 0x0B, 2, buffer);
        int len = getExpectedResponse(0x0B, buffer, 2, true);
    }
    else if (arg1 == "crash") {
        cout << "Simulating crash: " << arg2 << " " << arg3 << endl;
        unsigned int impacts = 0;
        unsigned int airbags = 0;
        sscanf(arg2.c_str(), "%x", &impacts);
        sscanf(arg3.c_str(), "%x", &airbags);
        buffer[0] = impacts >> 8;
        buffer[1] = impacts & 0xFF;
        buffer[2] = airbags >> 8;
        buffer[3] = airbags & 0xFF;
        vic.sendRequest(app_id, 0x31, 4, buffer);
        int len = getExpectedResponse(0x31, buffer, 2, true);
    }
    else if (arg1 == "getstatus") {
        getStatus();
    }
    else if (arg1 == "getlaststatus") {
        getLastStatus();
    }
    else if (arg1 == "gettires") {
        getTirePressures();
    }
    else if (arg1 == "getoil") {
        getOilLife();
    }
    else if (arg1 == "getfuel") {
        getFuelLevel();
    }
    else if (arg1 == "getvin") {
        getVin();
    }
    else if (arg1 == "getspeed") {
        getSpeed();
    }
    else if (arg1 == "getodom") {
        getOdometer();
    }
    else if (arg1 == "getseats") {
        getSeats();
    }
    else if (arg1 == "getfaults") {
        getFaults();
    }
    else if (arg1 == "getquickscan") {
        getQuickScanData();
    }
    else if (arg1 == "getbustype") {
        getBusType();
    }
    else if (arg1 == "eculearning") {
        ecuLearning();
    }
    else if (arg1 == "getcounts") {
        getCounts();
    }
    else {
        cerr << "Invalid command" << endl;
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
        len = vic.getResp(buffer,resp_timeout);
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

void getStatus() {
    Byte buffer[256];
    int len = getResponse(0x20, buffer, 9, false);
    if (len > 0) {
        decodeStatus(len, buffer);
    }
}

void getLastStatus() {
    Byte buffer[256];
    int len = getResponse(0x22, buffer, 9, false);
    if (len > 0) {
        decodeStatus(len, buffer);
    }
}

void decodeStatus(int len, Byte *buffer) {
    Byte status = buffer[2];
    Byte locks = buffer[3];
    Byte doors = buffer[4];
    unsigned int ign_time = ((int)buffer[5] << 8) | buffer[6];
    Byte batvalx10 = buffer[7];
    Byte vic_status_code = buffer[8];
    string vic_status = "";
    if (vic_status_code & 0x01) {
        vic_status += ",RtcFail";
    }
    if (vic_status_code & 0x02) {
        vic_status += ",CellPowerFail";
    }
    if (vic_status_code & 0x04) {
        vic_status += ",VicTestFail";
    }
    if (vic_status_code & 0x08) {
        vic_status += ",CanBusFail";
    }
    if (vic_status_code & 0x80) {
        vic_status += ",VicReset";
    }
    if (vic_status == "") {
        vic_status = "OK";
    }
    else {
        vic_status.erase(0, 1);
    }
    cout << "VIC Status: " << vic_status << endl;
    cout << "Car state:" << endl;
    if (status & (1 << 0)) {
        cout << "  ignition on" << endl;
    }
    else {
        cout << "  ignition off for " << ign_time << " min" << endl;
    }
    if (status & (1 << 1)) {
        cout << "  engine on" << endl;
    }
    else {
        cout << "  engine off" << endl;
    }
    if (status & (1 << 2)) {
        cout << "  vehicle is moving" << endl;
    }
    else {
        cout << "  vehicle is stopped" << endl;
    }
    if (status & (1 << 3)) {
        cout << "  emergency occured" << endl;
    }
    if (status & (1 << 4)) {
        cout << "  check engine status set" << endl;
    }
    if (status & (1 << 5)) {
        cout << "  driver's seatbelt fastened" << endl;
    }
    else {
        cout << "  driver's seatbelt unfastened" << endl;
    }
    if (status & (1 << 6)) {
        cout << "  panic is on" << endl;
    }
    else {
        cout << "  panic is off" << endl;
    }
    if (vic.protocol_version >= 0x0229) {
        if (vic_status_code & 0x40) {
            cout << "  CAN A is active" << endl;
        }
        else {
            cout << "  CAN A is not active" << endl;
        }
        if (vic_status_code & 0x20) {
            cout << "  CAN B is active" << endl;
        }
        else {
            cout << "  CAN B is not active" << endl;
        }
    }
    else {
        if (status & (1 << 7)) {
            cout << "  CAN bus is active" << endl;
        }
        else {
            cout << "  CAN bus is not active" << endl;
        }
    }
    string locks_state = "unlocked";
    if (locks == 0xFF) {
        locks_state = "unknown";
    }
    else if (locks == 0x1F) {
       locks_state = "locked";
    }
    else if (locks == 0x1E) {
       locks_state = "driver_unlocked";
    }
    cout << "  locks " << locks_state << endl;
    if (doors == 0xFF) {
        cout << "  doors unknown" << endl;
    }
    else {
        if (doors & (1 << 2)) {
            cout << "  leftdoor opened" << endl;
        }
        else {
            cout << "  leftdoor closed" << endl;
        }
        if (doors & (1 << 3)) {
            cout << "  rightdoor opened" << endl;
        }
        else {
            cout << "  rightdoor closed" << endl;
        }
        if (doors & (1 << 4)) {
            cout << "  trunk opened" << endl;
        }
        else {
            cout << "  trunk closed" << endl;
        }
    }
    float batval = batvalx10 / 10.;
    Features features;
    string pwrcntl = features.getFeature(PwrCntrl);
    Strings pwrvals = split(pwrcntl, "/");
    float batadj = 0.3;
    if (pwrvals.size() > 3) {
        batadj = stringToDouble(pwrvals[3]);
    }
    printf("  batval raw:%.1f adjusted:%.1f\n", batval, batval+batadj);
    if (len > 9) {
        int more_state = (buffer[9] << 8) | buffer[10];
        int car_capabilities = (buffer[11] << 8) | buffer[12];
        if (vic.protocol_version >= 0x0225) {
            int ignition_status = (more_state & 0x0E00) >> 9;
            cout << "  ignition status: " << ignition_status << endl;
            int key_status = (more_state & 0x00C0) >> 6;
            cout << "  key status: " << key_status << endl;
            if (more_state & 0x0100) {
                cout << "  low fuel is set" << endl;
            }
            else {
                cout << "  low fuel is not set" << endl;
            }
        }
//        if (vic.long_version > "000.027.000") {
        if (vic.protocol_version >= 0x0217) {
            cout << "Alarm state " << ((more_state>>13)&0x07) << endl;
        }
        else {
            if (more_state & 0x8000) {
                cout << "Alarm active" << endl;
            }
            else {
                cout << "Alarm not active" << endl;
            }
        }
        string capabilities = "";
        if (car_capabilities & 0x8000) {
            capabilities += " HFM";
        }
        if (car_capabilities & 0x4000) {
            capabilities += " REON";
        }
        if (car_capabilities & 0x2000) {
            capabilities += " RSO";
        }
        if (car_capabilities & 0x1000) {
            capabilities += " RTO";
        }
        if (car_capabilities & 0x0800) {
            capabilities += " RDU";
        }
        if (capabilities == "") {
            capabilities = " none";
        }
        cout << "Capabilities:" << capabilities << endl;
    }
}

void getTirePressures() {
    Byte buffer[64];
    int len = getResponse(0x23, buffer, 9, true);
    if (len > 0) {
        if ( (buffer[4] != 0xFF) and
             (buffer[5] != 0xFF) and
             (buffer[6] != 0xFF) and
             (buffer[7] != 0xFF) ) {
            int front_ideal = buffer[2];
            int rear_ideal = buffer[3];
            int left_front = buffer[4];
            int right_front = buffer[5];
            int left_rear = buffer[6];
            int right_rear = buffer[7];
            cout << "Tire pressures:" << endl;
            if ((buffer[2] != 0xFF) and (buffer[2] != 0xFF)) {
                cout << "  front_ideal: " << front_ideal << "psi" << endl;
                cout << "  rear_ideal: " << rear_ideal << "psi" << endl;
            }
            else {
                cout << "  ideal preasures unknown" << endl;
            }
            cout << "  left_front: " << left_front << "psi" << endl;
            cout << "  right_front: " << right_front << "psi" << endl;
            cout << "  left_rear: " << left_rear << "psi" << endl;
            cout << "  right_rear: " << right_rear << "psi" << endl;
            if (buffer[8] != 0xFF) {
                int spare = buffer[8];
                cout << "  spare: " << spare << "psi" << endl;
            }
            else {
                cout << "  spare: unknown" << endl;
            }
        }
        else {
            cout << "Tire pressures unknown" << endl;
        }
    }
}

void getOilLife() {
    Byte buffer[64];
    int len = getResponse(0x24, buffer, 3, true);
    if (len > 0) {
        if (buffer[2] != 0xFF) {
            int oil_life = buffer[2];
            cout << "Oil life: " << oil_life << "%" << endl;
        }
        else {
            cout << "Oil life: unknown" << endl;
        }
    }
}

void getFuelLevel() {
    Byte buffer[64];
    int len = getResponse(0x25, buffer, 3, true);
    if (len > 0) {
        if (buffer[2] != 0xFF) {
            int liters = buffer[2];
            int gals = (int)(liters * 0.2642 + 0.5);
            cout << "Fuel level: " << liters << "L (" << gals << "gal)" << endl;
        }
        else {
            cout << "Fuel level: unknown" << endl;
        }
    }
    len = getResponse(0x34, buffer, 16, true);
    if (len > 0) {
        unsigned long raw_total = 0;
        for (int i=0; i < 6; i++) {
            raw_total = (raw_total << 8) + buffer[i+2];
        }
        cout << "Raw total uL: " << raw_total << endl;
        int units = buffer[8];
        unsigned int raw_current = (buffer[9] << 8) + buffer[10];
        double ul_total = 0.;
        double ul_current = 0.;
        double lps = 0.;
        if (units == 1) {
            double adj = 0.21668;
            ul_total = raw_total * adj;
            if (raw_current != 0xFFFF) {
                ul_current = raw_current * adj;
                lps = ul_current * 10. / 1000000.;
            }
        }
        else if (units == 2) {
            double adj =  0.0022 * 1000000. / 3600. / 20.;
            ul_total = raw_total * adj;
            if (raw_current != 0xFFFF) {
                ul_current = raw_current * adj;
                lps = raw_current * 0.0022 / 3600.;
            }
        }
        else {
            cout << "Unknown units type" << endl;
        }
        printf("Total uL: %.0f\n", ul_total);
        printf("L/s: %.6f\n", lps);
        long int odom = ((long int)buffer[11]<<16) +
                        ((long int)buffer[12]<<8) +
                        buffer[13];
        if (odom != 0xFFFFFF) {
            double km = odom / 10.;
            double mi = km / 1.60934;
            printf("Odometer: %.1fkm (%.1fmi)\n", km, mi);
        }
        unsigned int speed_vic = ((int)buffer[14]<<8) | buffer[15];
        double mph = 0.0;
        double kph = 0.0;
        if (speed_vic != 0xFFFF) {
            kph = speed_vic / 128.;
            mph = speed_vic / 1.60934;
            printf("Speed: %.0fkph (%.0fmph)\n", kph, mph);
        }
        double kps = kph / 3600.;
        double kpl = 99.9;
        double mpg = 99.9;
        double lp100k = 0.;
        if (raw_current == 0xFFFF) {
            kpl = 0.;
            mpg = 0.;
            lp100k = 99.9;
        }
        else if (raw_current != 0) {
            kpl = kps / lps;
            mpg = kpl * 3.785412 / 1.609344;
            lp100k = 99.9;
            if (kph != 0.0) {
                lp100k = 100. / kpl;
            }
        }
        printf("k/s: %.4f\n", kps);
        printf("k/L: %.2f\n", kpl);
        printf("Instaneous mpg: %.1f\n", mpg);
        printf("Instaneous l/100k: %.1f\n", lp100k);
        printf("rc:%d tuL:%.6f L/s:%.6f k/h:%.0f k/s:%.4f k/L:%.2f m/g:%.1f l/100k:%.1f\n",
                raw_current, ul_total, lps, kph, kps, kpl, mpg, lp100k);
    }
}

void getVin() {
    Byte buffer[64];
    int len = getResponse(0x26, buffer, 3, false);
    if (len > 0) {
        int vin_len = buffer[2];
        if (len != (vin_len+3)) {
            cerr << "Invalid VIN length" << endl;
        }
        else if (vin_len == 0) {
            cout << "VIN: (no vin given)" << endl;
        }
        else {
            char vin[64];
            strncpy(vin, (char *)(&buffer[3]), vin_len);
            vin[vin_len] = '\0';
            cout << "VIN: " << vin << endl;
        }
    }
}

void getSpeed() {
    Byte buffer[64];
    int len = getResponse(0x27, buffer, 4, true);
    if (len > 0) {
        unsigned int speed_vic = ((int)buffer[2]<<8) | buffer[3];
        if (speed_vic != 0xFFFF) {
            double speed = speed_vic / 128.;
            int mph = (int)(speed / 1.60934 + 0.5);
            int kph = (int)(speed + 0.5);
            cout << "Speed: " << kph << "kph (" << mph << "mph)" << endl;
        }
        else {
            cout << "Speed: unknown" << endl;
        }
    }
}

void getOdometer() {
    Byte buffer[64];
    int len = getResponse(0x28, buffer, 5, true);
    if (len > 0) {
        long int odom = ((long int)buffer[2]<<16) +
                        ((long int)buffer[3]<<8) +
                        buffer[4];
        if (odom != 0xFFFFFF) {
            double km = odom / 10.;
            double mi = km / 1.60934;
            printf("Odometer: %.1fkm (%.1fmi)\n", km, mi);
        }
        else {
            cout << "Odometer: unknown" << endl;
        }
    }
}

void getSeats() {
    Byte buffer[64];
    int len = getResponse(0x30, buffer, 4, true);
    if (len > 0) {
        int seats = buffer[2];
        int seatbelts = buffer[3];
        if (seats & 0x01) {
            cout << "Driver seat occupied" << endl;
            if (seatbelts & 0x01) {
                cout << "Driver's seatbelt fastened" << endl;
            }
            else {
                cout << "Driver's seatbelt not fastened" << endl;
            }
        }
        if (seats & 0x02) {
            cout << "Passenger seat occupied" << endl;
            if (seatbelts & 0x02) {
                cout << "Passenger's seatbelt fastened" << endl;
            }
            else {
                cout << "Passenger's seatbelt not fastened" << endl;
            }
        }
    }
}

void getFaults() {
    Byte buffer[128];
    string faults_str = "";
    int idx = 0;
    bool got_faults = false;
    while (true) {
        buffer[0] = 0x00;
        buffer[1] = idx;
        vic.sendRequest(app_id, 0x29, 2, buffer);
        int len = getExpectedResponse(0x29, buffer, 5, false);
        Byte *p = buffer + 4;
        if (len < 5) {
            got_faults = false;
            faults_str = "Invalid response";
            break;
        }
        else if  (*p == 0xFF) {
            got_faults = false;
            faults_str = "unknown";
            break;
        }
        int num_faults = *(p++);
        if (num_faults == 0) {
            break;
        }
        got_faults = true;
        for (int i=0; i < num_faults; i++) {
            if (faults_str != "") {
                faults_str += ",";
            }
            unsigned long fault = 0;
            for (int j=0; j < 3; j++) {
	fault = (fault<<8) | *p;
	len--;
	p++;
    }
    int status = 0x00;
//            if (vic.long_version > "000.027.000") {
    if (vic.protocol_version >= 0x0217) {
	status = *p;
	p++;
	len--;
    }
    int type = fault >> 16;
    int l = 3;
    if ((status&0x0F) == 0) {
	l = 2;
	fault >>= 8;
    }
    type &= 0xC0;
    char code = ' ';
    if (type == 0x00) {
	code = 'P';
    }
    else if (type == 0x40) {
	code = 'C';
    }
    else if (type == 0x80) {
                code = 'B';
            }
            else {
                code = 'U';
            }
            if (l == 2) {
                faults_str += string_printf("%c%04X", code, fault&0x3FFF);
            }
            else {
                faults_str += string_printf("%c%04X-%02X", code, (fault>>8)&0x3FFF, fault&0xFF);
            }
//            if (vic.long_version > "000.027.000") {
            if (vic.protocol_version >= 0x0217) {
                string stattype = "";
                string mil = "off";
                if (status & 0x80) {
                    mil = "on";
                }
                if (status & 0x0F) {
                    if (status & 0x01) {
                        stattype = "Active";
                    }
                    else if (status & 0x04) {
                        stattype = "Pending";
                    }
                    else if (status & 0x08) {
                        stattype = "Stored";
                    }
                }
                else {
                    int tval = status & 0x60;
                    if (tval == 0x20) {
                        stattype = "Stored";
                    }
                    else if (tval == 0x40) {
                        stattype = "Pending";
                    }
                    else if (tval == 0x40) {
                        stattype = "Active";
                    }
                }
                faults_str += string_printf(".%02X(%s)", status, stattype.c_str());
            }
        }
        if (num_faults < 10) {
            break;
        }
        idx += 10;
    }
    if (faults_str == "") {
        cout << "Faults: none" << endl;
    }
    else {
        cout << "Faults: " << faults_str << endl;
    }
}

void getBusType() {
    Byte buffer[64];
    int len = getResponse(0x0D, buffer, 6, true);
    if (len > 0) {
        decodeBusType("CAN-1", buffer[2], buffer[3]);
        decodeBusType("CAN-2", buffer[4], buffer[5]);
    }
}

void decodeBusType(string can, Byte type, Byte speed) {
    string type_str = "Passive";
    if (type & 0x80) {
        type_str = "Active";
    }
    type &= 0x0F;
    if (type == 0x01) {
        type_str += "/11-Bit";
    }
    else if (type == 0x02) {
        type_str += "/29-Bit";
    }
    else {
        type_str += "/Unknown";
    }
    speed &= 0x1F;
    if (speed == 0x01) {
        type_str += "/33.3K";
    }
    else if (speed == 0x02) {
        type_str += "/83.3K";
    }
    else if (speed == 0x03) {
        type_str += "/128K";
    }
    else if (speed == 0x04) {
        type_str += "/250K";
    }
    else {
        type_str += "/Unknown";
    }
    cout << can << ": " << type_str << endl;
}

void getQuickScanData() {
    Byte buffer[128];
    string faults_str = "";
    int idx = 0;
    bool got_faults = false;
    cout << "Quickscan Data:" << endl;
    while (true) {
        buffer[0] = idx;
        vic.sendRequest(app_id, 0x2E, 1, buffer);
        int len = getExpectedResponse(0x2E, buffer, 3, false);
        if (len < 23) {
            break;
        }
        int hw1 = buffer[3];
        int hw2 = buffer[4];
        int hw3 = buffer[5];
        cout << idx << ": hw:" << hw1 << "." << hw2 << "." << hw3;
        int sw1 = buffer[6];
        int sw2 = buffer[7];
        int sw3 = buffer[8];
        cout << " sw:" << sw1 << "." << sw2 << "." << sw3;
        cout << " part:";
        for (int i=9; i < 19; i++) {
            int b = buffer[i];
            printf("%02X", b);
        }
        int ecu_origin = buffer[19];
        cout << " Orig:" << ecu_origin;
        int supp_id = buffer[20];
        cout << " Supp:" << supp_id;
        int variant_id = buffer[21];
        cout << " Var:" << variant_id;
        int diag_ver = buffer[22];
        cout << " Diag:" << diag_ver;
        cout << endl;
        idx++;
    }
}

void ecuLearning() {
    Byte buffer[64];
    int len = getResponse(0x2F, buffer, 2, true);
    if (len > 0) {
        cout << "Reset ECU Learning process" << endl;
    }
}

void getCounts() {
    Byte buffer[64];
    int len = getResponse(0x40, buffer, 18, true);
    if (len > 0) {
        unsigned int rx_bytes = ((unsigned int)buffer[2] << 8) | buffer[3];
        unsigned int rx_errors = ((unsigned int)buffer[4] << 8) | buffer[5];
        unsigned int rx_packets = ((unsigned int)buffer[6] << 8) | buffer[7];
        unsigned int rx_valids = ((unsigned int)buffer[8] << 8) | buffer[9];
        unsigned int tx_bytes = ((unsigned int)buffer[10] << 8) | buffer[11];
        unsigned int tx_packets = ((unsigned int)buffer[12] << 8) | buffer[13];
        unsigned int cana_mesgs = ((unsigned int)buffer[14] << 8) | buffer[15];
        unsigned int canb_mesgs = ((unsigned int)buffer[16] << 8) | buffer[17];
        printf("RB:%04X RE:%04X RP:%04X RV:%04X TB:%04X TP:%04X CA:%04X CB:%04X\n",
               rx_bytes,rx_errors,rx_packets,rx_valids,tx_bytes,tx_packets,cana_mesgs,canb_mesgs);
    }
    else {
        cout << "Invalid frame length" << endl;
    }
}

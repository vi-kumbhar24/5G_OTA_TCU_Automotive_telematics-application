#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "backtick.h"
#include "autonet_files.h"
#include "vic.h"

using namespace std;

void controlCar(string dev_s, string act_s);
int getResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
void getTime();
void setTime();
void getStatus();
int stringToInt(string str);

VIC vic;
int app_id = 1;

int main(int argc, char *argp[]) {
    Byte buffer[256];
    if (argc < 3) {
        cout << "engine on | off" << endl;
        cout << "locks lock | unlock | unlock_driver" << endl;
        cout << "trunk open | close" << endl;
        cout << "rightdoor open | close" << endl;
        cout << "leftdoor open | close" << endl;
        cout << "panic on | off" << endl;
        exit(0);
    }
    vic.openSocket();
    getTime();
    setTime();
    getTime();
    getStatus();
    string arg1 = argp[1];
    string arg2 = argp[2];
    controlCar(arg1, arg2);
    return 0;
}

void controlCar(string dev_s, string act_s) {
    Byte buffer[256];
    Byte dev = 0;
    Byte act = 0;
    if (dev_s == "locks") {
        dev = 1;
        if (act_s == "lock") {
            act = 1;
        }
        else if (act_s == "unlock") {
            act = 2;
        }
        else if (act_s == "unlock_driver") {
            act = 3;
        }
    }
    else if (dev_s == "engine") {
        dev = 2;
        if (act_s == "on") {
            act = 1;
        }
        else if (act_s == "off") {
            act = 2;
        }
    }
    else if (dev_s == "trunk") {
        dev = 3;
        if (act_s == "open") {
            act = 1;
        }
        else if (act_s == "close") {
            act = 2;
        }
    }
    else if (dev_s == "leftdoor") {
        dev = 4;
        if (act_s == "open") {
            act = 1;
        }
        else if (act_s == "close") {
            act = 2;
        }
    }
    else if (dev_s == "rightdoor") {
        dev = 5;
        if (act_s == "open") {
            act = 1;
        }
        else if (act_s == "close") {
            act = 2;
        }
    }
    else if (dev_s == "panic") {
        dev = 6;
        if (act_s == "on") {
            act = 1;
        }
        else if (act_s == "off") {
            act = 2;
        }
    }
    if ((dev != 0) and (act != 0)) {
        buffer[0] = dev;
        buffer[1] = act;
        vic.sendRequest(app_id, 0x21, 2, buffer);
        int len = getExpectedResponse(0x21, buffer, 2, true);
        if (len > 0) {
            sleep(1);
            getStatus();
        }
    }
    else {
        cerr << "Invalid device or action" << endl;
        exit(1);
    }
}

int getResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    vic.sendRequest(app_id, cmnd_id);
    int len = getExpectedResponse(cmnd_id, buffer, expected, exact);
    return len;
}

int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    int len = vic.getResp(buffer,1);
    if (len == 0) {
        cerr << "No response to command:" << cmnd_id << endl;
    }
    else if (len < 2) {
        cerr << "Response too short for command:" << cmnd_id << " len=" << len << endl;
        len = 0;
    }
    else if (buffer[0] != app_id) {
        cerr << "Response has wrong app_id:" << app_id << " got:" << buffer[0] << endl;
        len = 0;
    }
    else if (buffer[1] != cmnd_id) {
        cerr << "Response has wrong cmnd_id" << cmnd_id << " got:" << buffer[1] << endl;
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
    string time_str = backtick("cell_shell gettime");
    int year, mon, mday, hour, min, sec;
    int pos = time_str.find("\n");
    time_str.erase(0,pos);
    sscanf(time_str.c_str(), "%d/%d/%d %d:%d:%d",
           &year, &mon, &mday, &hour, &min, &sec);
    struct tm tm;
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = mday;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    struct timeval tv;
    tv.tv_sec = timegm(&tm);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    vic.setRtc(app_id);
}

void getStatus() {
    Byte buffer[256];
    int len = getResponse(0x20, buffer, 9, false);
    if (len > 0) {
        cout << "Car state:" << endl;
        Byte status = buffer[2];
        Byte locks = buffer[3];
        Byte doors = buffer[4];
        unsigned int ign_time = ((int)buffer[5] << 8) | buffer[6];
        Byte batval = buffer[7];
        if (status & (1 << 0)) {
            cout << "  ignition on for " << ign_time << " min" << endl;
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
        string locks_state = "unlocked";
        if (locks == 0x1F) {
           locks_state = "locked";
        }
        else if (locks == 0x1E) {
           locks_state = "driver_unlocked";
        }
        cout << "  locks " << locks_state << endl;
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
        printf("  batval %.1f\n", batval/10.);
    }
}

void getTirePressures() {
    Byte buffer[64];
    int len = getResponse(0x23, buffer, 9, true);
    if (len > 0) {
        int front_ideal = buffer[2];
        int rear_ideal = buffer[3];
        int left_front = buffer[4];
        int right_front = buffer[5];
        int left_rear = buffer[6];
        int right_rear = buffer[7];
        int spare = buffer[8];
        cout << "Tire pressures:" << endl;
        cout << "  front_ideal: " << front_ideal << "psi" << endl;
        cout << "  rear_ideal: " << rear_ideal << "psi" << endl;
        cout << "  left_front: " << left_front << "psi" << endl;
        cout << "  right_front: " << right_front << "psi" << endl;
        cout << "  left_rear: " << left_rear << "psi" << endl;
        cout << "  right_rear: " << right_rear << "psi" << endl;
        cout << "  spare: " << spare << "psi" << endl;
    }
}

void getOilLife() {
    Byte buffer[64];
    int len = getResponse(0x24, buffer, 3, true);
    if (len > 0) {
        int oil_life = buffer[2];
        cout << "Oil life: " << oil_life << "%" << endl;
    }
}

void getFuelLevel() {
    Byte buffer[64];
    int len = getResponse(0x25, buffer, 3, true);
    if (len > 0) {
        int liters = buffer[2];
        int gals = (int)(liters * 0.2642 + 0.5);
        cout << "Fuel level: " << liters << "L (" << gals << "gal)" << endl;
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
        double speed = speed_vic / 128.;
        int kph = (int)(speed + 0.5);
        int mph = (int)(speed / 1.60934 + 0.5);
        cout << "Speed: " << kph << "kph (" << mph << "mph)" << endl;
    }
}

void getOdometer() {
    Byte buffer[64];
    int len = getResponse(0x28, buffer, 5, true);
    if (len > 0) {
        long int odom = ((long int)buffer[2]<<16) +
                        ((long int)buffer[3]<<8) +
                        buffer[4];
        double km = odom / 10.;
        double mi = km / 1.60934;
        printf("Odometer: %.1fkm (%.1fmi)\n", km, mi);
    }
}

int stringToInt(string str) {
    int val = 0;
    sscanf(str.c_str(), "%d", &val);
    return val;
}

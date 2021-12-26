/***************************************************************************
 * File: checkvic.cpp
 *
 * Routine to check if vic_talker is working
 *
 ***************************************************************************
 * Change log:
 * 2013/05/06 rwp
 *    Initial version
 ***************************************************************************
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "autonet_types.h"
#include "getopts.h"
#include "vic.h"

using namespace std;

void controlCar(string dev_s, string act_s);
int getResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
void setTime();
void getStatus();
void decodeStatus(int len, Byte *buffer);

VIC vic;
int app_id = 15;
bool debug = false;

int main(int argc, char *argp[]) {
    Byte buffer[256];
    Byte notifications[] = {0xF1};
//    vic.openSocket(1,notifications);
    vic.openSocket();
    GetOpts opts(argc, argp, "d");
    debug = opts.exists("d");
    int retval = 1;
    int len = getResponse(0x20, buffer, 9, false);
    if (len > 0) {
        decodeStatus(len, buffer);
        retval = 0;
    }
    return retval;
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

void setTime() {
    vic.setRtc(app_id);
}

void getStatus() {
    Byte buffer[256];
    int len = getResponse(0x20, buffer, 9, false);
    if (len > 0) {
        decodeStatus(len, buffer);
    }
}

void decodeStatus(int len, Byte *buffer) {
    Byte status = buffer[2];
    Byte locks = buffer[3];
    Byte doors = buffer[4];
    unsigned int ign_time = ((int)buffer[5] << 8) | buffer[6];
    Byte batval = buffer[7];
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
    if (vic_status_code & 0x70) {
        vic_status += ",UnknownFailure";
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
    if (status & (1 << 7)) {
        cout << "  CAN bus is active" << endl;
    }
    else {
        cout << "  CAN bus is not active" << endl;
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
    printf("  batval %.1f\n", batval/10.);
    if (len > 9) {
        int more_state = (buffer[9] << 8) | buffer[10];
        int car_capabilities = (buffer[11] << 8) | buffer[12];
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

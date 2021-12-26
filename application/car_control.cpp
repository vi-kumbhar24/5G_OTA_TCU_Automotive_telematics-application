/***************************************************************************
 * car_control.cpp
 * Routine to control a car not using the CAN bus
 *
 * 04/03/29 rwp
 *    Changed to control "demo" vehicle
 * 2015/02/18 rwp
 *    AK-93: Added open/close left/right doors to demo car_control
 *    AK-93: Added returning doors state to demo car_control
 *    AK-93: Added remote start inhibit if doors are open to demo car control
 ***************************************************************************
 */

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <errno.h>
#include <unistd.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "backtick.h"
#include "dateTime.h"
#include "my_features.h"
#include "getopts.h"
#include "split.h"
#include "string_convert.h"
#include "str_system.h"
#include "fileExists.h"
#include "getStringFromFile.h"
#include "string_printf.h"
#include "touchFile.h"
#include "updateFile.h"
using namespace std;

typedef map<string,int> IntAssoc;

#define serial_device "relays"

const char *button2command[6] = {"", "e", "f", "g", "h", "i"};

Assoc command2device;
void init_command2device() {
    command2device["engine"] = "engine";
    command2device["locks"] = "locks";
    command2device["lock"] = "locks";
    command2device["unlock"] = "locks";
    command2device["panic"] = "panic";
    command2device["trunk"] = "trunk";
    command2device["leftdoor"] = "leftdoor";
    command2device["rightdoor"] = "rightdoor";
}

map<string,Strings> states;
void init_states() {
    Strings engine_st;
    engine_st.push_back("off");
    engine_st.push_back("on");
    states["engine"] = engine_st;
    Strings locks_st;
    locks_st.push_back("locked");
    locks_st.push_back("driver_unlocked");
    locks_st.push_back("unlocked");
    states["locks"] = locks_st;
    Strings panic_st;
    panic_st.push_back("off");
    panic_st.push_back("on");
    states["panic"] = panic_st;
    Strings trunk_st;
    trunk_st.push_back("closed");
    trunk_st.push_back("opened");
    states["trunk"] = trunk_st;
    Strings leftdoor_st;
    leftdoor_st.push_back("closed");
    leftdoor_st.push_back("opened");
    states["leftdoor"] = leftdoor_st;
    Strings rightdoor_st;
    rightdoor_st.push_back("closed");
    rightdoor_st.push_back("opened");
    states["rightdoor"] = rightdoor_st;
}

map<string,Assoc> deviceActions;
void init_deviceActions() {
    Assoc engine_act;
    engine_act["on"] = "on";
    engine_act["start"] = "on";
    engine_act["off"] = "off";
    deviceActions["engine"] = engine_act;
    Assoc lock_act;
    lock_act["pushed"] = "locked";
    deviceActions["lock"] = lock_act;
    Assoc unlock_act;
    unlock_act["pushed"] = "unlocked";
    deviceActions["unlock"] = unlock_act;
    Assoc locks_act;
    locks_act["unlock"] = "unlocked";
    locks_act["unlock_driver"] = "driver_unlocked";
    locks_act["lock"] = "locked";
    deviceActions["locks"] = locks_act;
    Assoc trunk_act;
    trunk_act["open"] = "opened";
    trunk_act["close"] = "closed";
    deviceActions["trunk"] = trunk_act;
    Assoc leftdoor_act;
    leftdoor_act["open"] = "opened";
    leftdoor_act["close"] = "closed";
    deviceActions["leftdoor"] = leftdoor_act;
    Assoc rightdoor_act;
    rightdoor_act["open"] = "opened";
    rightdoor_act["close"] = "closed";
    deviceActions["rightdoor"] = rightdoor_act;
    Assoc panic_act;
    panic_act["on"] = "on";
    panic_act["off"] = "off";
    deviceActions["panic"] = panic_act;
}

Assoc carState;
IntAssoc deviceChangeTimes;
IntAssoc deviceResetTimes;
int doors = 0;

void init_carState() {
    carState["locks"] = "locked";
    deviceResetTimes["locks"] = 0;
    deviceChangeTimes["locks"] = 0;
    carState["engine"] = "off";
    deviceResetTimes["engine"] = 180;
    deviceChangeTimes["engine"] = 0;
    carState["leftdoor"] = "closed";
    deviceResetTimes["leftdoor"] = 180;
    deviceChangeTimes["leftdoor"] = 0;
    carState["rightdoor"] = "closed";
    deviceResetTimes["rightdoor"] = 180;
    deviceChangeTimes["rightdoor"] = 0;
    carState["trunk"] = "closed";
    deviceResetTimes["trunk"] = 180;
    deviceChangeTimes["trunk"] = 0;
    carState["panic"] = "off";
    deviceResetTimes["panic"] = 180;
    deviceChangeTimes["panic"] = 0;
    carState["doors"] = "0/15";
    deviceResetTimes["doors"] = 0;
    deviceChangeTimes["doors"] = 0;
}

//#define _POSIX_SOURCE 1         //POSIX compliant source

void doGet(string what);
void printStates();
void readStates();
void fixDoorsLocks();
void writeStates();
void performAction(string device, string new_state);
void doIt(string device, string new_state);;
void pushButton(int button, int duration);
void launch_poller(string device, char *argv0_p);
void closeAndExit(int exitstat);
void openPort(const char *port);
void closePort();
bool lock(const char *dev);
int stringToInt(string str);
void onTerm(int parm);
void usage();

bool debug = true;
int ser_s = -1;		// Serial port descriptor
struct termios oldtio;	// Old port settings for serial port
string lockfile = "";
bool demo = false;

int main(int argc, char *argv[]) {
//    GetOpts opts(argc, argv, "c");
    init_command2device();
    init_states();
    init_deviceActions();
    init_carState();
    if (fileExists(DemoModeFile)) {
        demo = true;
    }
    if (argc != 3) {
        cout << "Error: Invalid arguments" << endl;
        exit;
    }
    readStates();
    string device = argv[1];
    transform(device.begin(), device.end(), device.begin(), ::tolower);
    if (device == "get") {
        string what = argv[2];
        transform(what.begin(), what.end(), what.begin(), ::tolower);
        doGet(what);
    }
    else {
        string action = argv[2];
        transform(action.begin(), action.end(), action.begin(), ::tolower);
        if ( fileExists("/tmp/moving") and
             (getStringFromFile("/tmp/moving") == "1") ) {
            cout << "Error: Cannot execute while vehicle is moving" << endl;
        }
        else {
            if (deviceActions.find(device) != deviceActions.end()) {
                Assoc actions = deviceActions[device];
                if (actions.find(action) != actions.end()) {
                    string new_state = actions[action];
                    device = command2device[device];
                    performAction(device, new_state);
                    launch_poller(device, argv[0]);
                }
                else {
                    cout << "Error: Invalid action" << endl;
                }
            }
            else {
                cout << "Error: Invalid device" << endl;
            }
        }
    }
}

void doGet(string what) {
    if (what == "status") {
        printStates();
    }
    else if (what == "position") {
        string disableloc = backtick(string("getfeature ") + DisableLocationFeature);
        if (disableloc == "") {
            string gps_position = "";
            time_t loc_time = 0;
            if (fileExists(GpsPositionFile)) {
                gps_position = getStringFromFile(GpsPositionFile);
                loc_time = fileTime(GpsPositionFile);
            } else if (fileExists(GpsLastPositionFile)) {
                gps_position = getStringFromFile(GpsLastPositionFile);
                loc_time = fileTime(GpsLastPositionFile);
            }
            if (gps_position != "") {
                string loc_time_str = getDateTime(loc_time);
                cout << "position " << gps_position << "," << loc_time_str << endl;
            }
            else {
                cout << "Error: Position unknown" << endl;
            }
        }
    }
}

void printStates() {
    Assoc::iterator si;
    for (si=carState.begin(); si != carState.end(); si++) {
        string dev = (*si).first;
        string state = (*si).second;
        cout << dev << " " << state << endl;
    }
}

void readStates() {
    time_t now = time(NULL);
    bool changed = false;
    if (fileExists(CarStateFile)) {
        ifstream fin(CarStateFile);
        if (fin.is_open()) {
            while(fin.good() and !fin.eof()) {
                string line;
                getline(fin, line);
                if (!fin.eof()) {
                    Strings parms = split(line, " ");
                    if (parms.size() > 1) {
                        string device = parms[0];
                        string state = parms[1];
                        carState[device] = state;
                        deviceChangeTimes[device] = 0;
                        if (parms.size() > 2) {
                            int changetime = stringToInt(parms[2]);
                            deviceChangeTimes[device] = changetime;
                            if ( (deviceResetTimes[device] > 0) and
                                 ((now - changetime) > deviceResetTimes[device]) ) {
                                string new_state = states[device][0];
                                doIt(device, new_state);
                                if (device == "engine") {
                                    str_system("log_event 'NotifyUser type:remote_start_aborted reason:normal_timeout'");
                                }
                                deviceChangeTimes[device] = 0;
                                changed = true;
                            }
                        }
                    }
                }
            }
            fin.close();
        }
    }
    if ( !fileExists("/tmp/remote_started") and
         (deviceChangeTimes["engine"] > 0) ) {
        deviceChangeTimes["engine"] = 0;
        changed = true;
    }
    if (fileExists("/tmp/engine")) {
        string st = "off";
        if (getStringFromFile("/tmp/engine") == "1") {
            st = "on";
        }
        if (carState["engine"] != st) {
            carState["engine"] = st;
            changed = true;
        }
    }
    if (fileExists("/tmp/doors")) {
        string dl = getStringFromFile("/tmp/doors");
        carState["doors"] = dl;
        unlink("/tmp/doors");
        doors = 0;
        int locks = 0;
        sscanf(dl.c_str(), "%d/%d", &doors, &locks);
        if (doors & 0x04) {
            carState["leftdoor"] = "open";
        }
        else {
            carState["leftdoor"] = "closed";
            deviceChangeTimes["leftdoor"] = 0;
        }
        if (doors & 0x08) {
            carState["rightdoor"] = "open";
        }
        else {
            carState["rightdoor"] = "closed";
            deviceChangeTimes["rightdoor"] = 0;
        }
        if (doors & 0x10) {
            carState["trunk"] = "open";
        }
        else {
            carState["trunk"] = "closed";
            deviceChangeTimes["trunk"] = 0;
        }
        if (locks == 14) {
            carState["locks"] = "driver_unlocked";
        }
        else if (locks == 15) {
            carState["locks"] = "locked";
        }
        else {
            carState["locks"] = "unlocked";
        }
        changed = true;
    }
    if (changed) {
        fixDoorsLocks();
        writeStates();
    }
}

void fixDoorsLocks() {
    doors = 0;
    int locks = 0;
    sscanf(carState["doors"].c_str(), "%d/%d", &doors, &locks);
    string lock_st = carState["locks"];
    if (lock_st == "locked") {
        locks = 15;
    }
    else if (lock_st == "unlocked") {
        locks = 0;
    }
    else if (lock_st == "driver_unlocked") {
        locks = 14;
    }
    if (carState["leftdoor"] == "opened") {
        doors |= 0x04;
    }
    else {
        doors &= ~0x04;
    }
    if (carState["rightdoor"] == "opened") {
        doors |= 0x08;
    }
    else {
        doors &= ~0x08;
    }
    if (carState["trunk"] == "opened") {
        doors |= 0x10;
    }
    else {
        doors &= ~0x10;
    }
    carState["doors"] = string_printf("%d/%d", doors, locks);
}

void writeStates() {
    string tmpfile = CarStateFile;
    tmpfile += ".tmp";
    ofstream outf(tmpfile.c_str());
    if (outf.is_open()) {
       Assoc::iterator si;
       for (si=carState.begin(); si != carState.end(); si++) {
           string dev = (*si).first;
           string state = (*si).second;
           outf << dev << " " << state;
           int changetime = deviceChangeTimes[dev];
           if ( (deviceResetTimes[dev] > 0) and
                (changetime > 0) and
                (state != states[dev][0]) ) {
               outf << " " << changetime;
           }
           outf << endl;
       }
       outf.close();
       rename(tmpfile.c_str(), CarStateFile);
    }
}

void performAction(string device, string new_state) {
    bool do_it = false;
    if (device == "engine") {
        if (carState["engine"] != new_state) {
            if (new_state == "on") {
                doIt("locks", "locked");
                string rs_inhibit = "";
                if (fileExists("/autonet/admin/rs_inhibit")) {
                    rs_inhibit = getStringFromFile("/autonet/admin/rs_inhibit");
                }
                else if (carState["panic"] == "on") {
                    rs_inhibit = "panic_mode";
                }
                else if (carState["trunk"] == "opened") {
                    rs_inhibit = "trunk_liftgate_ajar";
                }
                else if (carState["leftdoor"] == "opened") {
                    rs_inhibit = "left_rear_door_ajar";
                }
                else if (carState["rightdoor"] == "opened") {
                    rs_inhibit = "right_rear_door_ajar";
                }
                else if (doors & 0x01) {
                    rs_inhibit = "driver_door_ajar";
                }
                else if (doors & 0x02) {
                    rs_inhibit = "passenger_door_ajar";
                }
                if (rs_inhibit != "") {
                    if (rs_inhibit == "check_engine_indicator_on") {
                        do_it = true;
                    }
                    else {
                        cout << "Error: Engine on failed reason:" << rs_inhibit << endl;
                    }
                }
                else {
                    do_it = true;
                }
            }
            else {
                if (fileExists("/tmp/remote_started")) {
                    do_it = true;
                }
                else {
                    cout << "Error: Engine off failed" << endl;
                }
            }
            if (do_it) {
                string abort_cause = "";
                if ( fileExists("/tmp/chkeng") and
                     (getStringFromFile("/tmp/chkeng") == "1") ) {
                    abort_cause = "check_engine_indicator_on";
                }
                else if (fileExists(LowFuelFile)) {
                    abort_cause = "low_fuel";
                }
                if (abort_cause != "") {
                    str_system("( sleep 10; rm /tmp/remote_started; echo 0 >/tmp/engine; echo 0 >/tmp/ign; log_event 'NotifyUser type:remote_start_aborted reason:"+abort_cause+"' ) &");
                }
            }
        }
    }
    else if (device == "locks") {
        do_it = true;
    }
    else if (device == "trunk") {
        do_it = true;
    }
    else if (device == "leftdoor") {
        do_it = true;
    }
    else if (device == "rightdoor") {
        do_it = true;
    }
    else if (device == "panic") {
        if (carState["panic"] != new_state) {
            if (new_state == "on") {
                if (fileExists("/tmp/engine") and (getStringFromFile("/tmp/engine") == "1")) {
                    cout << "Error: Cannot turn on panic with engine running" << endl;
                }
                else {
                    do_it = true;
                }
            }
            else {
                do_it = true;
            }
        }
    }
    if (do_it) {
        doIt(device, new_state);
    }
    fixDoorsLocks();
    printStates();
    if (do_it) {
        writeStates();
    }
}

void doIt(string device, string new_state) {
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);
    time_t now = time(NULL);
    bool do_it = true;
    if (!demo) {
        openPort(serial_device);
    }
    if (device == "engine") {
        if (new_state == "on") {
            pushButton(3,1);
            touchFile(RemoteStartedFile);
            updateFile("/tmp/ign", "1");
            updateFile("/tmp/engine", "1");
        }
        else {
            if (fileExists("/tmp/remote_started")) {
                pushButton(3,1);
                unlink(RemoteStartedFile);
                updateFile("/tmp/engine", "0");
                updateFile("/tmp/ign", "0");
            }
            else {
                do_it = false;
            }
        }
    }
    else if (device == "locks") {
        if (new_state == "locked") {
            pushButton(1,1);
        }
        else {
            pushButton(2,1);
        }
    }
    else if (device == "trunk") {
        if (new_state == "opened") {
            pushButton(4,5);
        }
    }
    else if (device == "panic") {
        if (new_state == "on") {
            pushButton(1,5);
        }
        else {
            pushButton(1,1);
        }
    }
    if (do_it) {
        carState[device] = new_state;
        deviceChangeTimes[device] = now;
    }
    if (!demo) {
        closePort();
    }
}

void pushButton(int button, int duration) {
    if (!demo) {
        const char *command = button2command[button];
        write(ser_s, command, 1);
        sleep(duration);
        write(ser_s, "n", 1);
    }
}

void launch_poller(string device, char *argv0_p) {
    if ( (deviceResetTimes[device] > 0) and
         (deviceChangeTimes[device] > 0) and
         (carState[device] != states[device][0]) ) {
        pid_t pid = fork();
        if (pid == 0) {
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            signal(SIGHUP, SIG_DFL);
            signal(SIGUSR1, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            if (argv0_p != NULL) {
                int arg0_len = strlen(argv0_p);
                strncpy(argv0_p, "cc_poller", arg0_len);
            }
            while (true) {
                sleep(1);
                readStates();
                if (deviceChangeTimes[device] == 0) {
                    break;
                }
            }
            exit(0);
        }
    }
}

void closeAndExit(int exitstat) {
    if (!demo) {
        closePort();
    }
    exit(exitstat);
}

void openPort(const char *port) {
    string dev = "/dev/";
    dev += port;
    if (!lock(port)) {
        cout << "Serial device '" << port << "' already in use" << endl;
        closeAndExit(1);
    }
    ser_s = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser_s < 0) {
        perror(serial_device);
        closeAndExit(-1);
    }
    if (tcgetattr(ser_s,&oldtio)) {
        cerr << "Failed to get old port settings" << endl;
    }

    // set new port settings for canonical input processing 
    struct termios newtio;
    newtio.c_cflag = B19200 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VMIN]=1;
    newtio.c_cc[VTIME]=1;
    if (tcsetattr(ser_s,TCSANOW,&newtio)) {
        cerr << "Failed to set new port settings" << endl;
    }
    if (tcflush(ser_s, TCIOFLUSH)) {
        cerr << "Failed to flush serial port" << endl;
    }
    if (fcntl(ser_s, F_SETFL, FNDELAY)) {
        cerr << "Failed to set fndelay on serial port" << endl;
    }
}

void closePort() {
    if (ser_s >= 0) {
        // restore old port settings
        tcflush(ser_s, TCIOFLUSH);
        tcsetattr(ser_s,TCSANOW,&oldtio);
        close(ser_s);        //close the com port
    }
    if (lockfile != "") {
        remove(lockfile.c_str());
    }
}

bool lock(const char *dev) {
    bool locked = false;
    lockfile = "/var/lock/LCK..";
    lockfile += dev;
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0);
    if (fd >= 0) {
        close(fd);
        locked = true;
    }
    return locked;
}

void onTerm(int parm) {
    closeAndExit(0);
}

void usage() {
   cerr << "Usage: powercontroller <command> [args]" << endl;
   cerr << "  commands:" << endl;
   cerr << "    state" << endl;
   cerr << "    version" << endl;
   cerr << "    shutdown <secs> <rebootflag>" << endl;
   cerr << "    setdead <t1> <t2>" << endl;
   cerr << "    batval" << endl;
   cerr << "    ignition" << endl;
   cerr << "    version" << endl;
}

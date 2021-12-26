/*****************************************************************************
 * File: realtimestats.cpp
 * Routine to output realtime stats (used by TruManager dashboard display)
 *****************************************************************************
 * 2013/05/09 rwp
 *    Modified to get WAN device from hardware config file
 *****************************************************************************
 */

#include <iostream>
#include <string>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "backtick.h"
#include "dateTime.h"
#include "fileExists.h"
#include "getopts.h"
#include "getStringFromFile.h"
#include "getParm.h"
#include "getUptime.h"
#include "grepFile.h"
#include "network_utils.h"
#include "readAssoc.h"
#include "readFileArray.h"
#include "split.h"
#include "string_convert.h"
using namespace std;

int report_period = 1;
unsigned long Uptime = 0;
bool alrm_sig = false;

bool wanUp();
void upLoop();
void monWait(int timeout);
bool getWan(bool first);
string eventConnection();
string eventDeltaTime(string cellstate, bool updatetime);
string eventUsage(bool withspeed);
string eventWifiUsers();
string eventGpsPosition();
void getStateInfo();
void getUserConnections();
int getWlan0Users();
void readGpsPosition();
void getCellInfo();
string getCellResponse(const char *cmnd);
double getDoubleUptime();
void on_alrm(int sig);

struct CellInfo {
    string type;
    string device;
    bool connected;
    int socket;
    string talker;
    string state;
    string connection_type;
    string signal;
    unsigned long last_report;
    unsigned long fail_time;

    CellInfo() {
        type = "ppp";
        device = "ppp0";
        connected = false;;
        socket = -1;
        talker = "";
        state = "down";
        connection_type = "";
        signal = "unknown";
        last_report = 0;
        fail_time = 0;
    }
};
CellInfo cellinfo;

struct WanInfo {
    bool up;
    bool exists;
    unsigned long rx_bytes;
    unsigned long rx_frames;
    unsigned long tx_bytes;
    unsigned long tx_frames;
    unsigned long last_rx_bytes;
    unsigned long last_tx_bytes;
    double lasttime;
    double rx_kBps;
    double tx_kBps;
    double max_rx_kBps;
    double max_tx_kBps;
    int64_t sum_rx_bytes;
    int64_t sum_tx_bytes;

    WanInfo() {
        up = false;
        exists = false;;
        rx_bytes = 0;
        rx_frames = 0;
        tx_bytes = 0;
        tx_frames = 0;
        last_rx_bytes = 0;
        last_tx_bytes = 0;
        lasttime = 0.;
        rx_kBps = 0.;
        tx_kBps = 0.;
        max_rx_kBps = 0.;
        max_tx_kBps = 0.;
        sum_rx_bytes = 0;
        sum_tx_bytes = 0;
    }
};
WanInfo waninfo;

class CarState {
  public:
    bool location_disabled;

    CarState() {
        location_disabled = false;
    }
};
CarState car_state;

struct Wlan0Info {
    bool up;
    int users;
    int fake_users;
    string state;

    Wlan0Info() {
        up = false;
        users = 0;
        fake_users = 0;
        state = "down";
    }
};
Wlan0Info wlan0info;

#define GET_POSITION_INTERVAL 60
#define POSITION_REPORT_INTERVAL 300
struct GpsInfo {
    bool present;
    string position;
    string datetime;
    int report_interval;
    int position_interval;
    unsigned long report_time;
    unsigned long position_time;
    off_t geofence_file_time;
    bool report_position;
//    unsigned int report_enabled_until;
//    bool clearing;

    GpsInfo() {
        present = false;
        position = "";
        datetime = "";
        report_interval = 0;
        position_interval = GET_POSITION_INTERVAL;
        report_time = 0;
        position_time = 0;
        geofence_file_time = 0;
        report_position = false;
//        report_enabled_until = 0;
//        clearing = false;
    }
};
GpsInfo gpsinfo;

bool include_gps = false;

int main(int argc, char *argv[]) {
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    umask(S_IWGRP | S_IWOTH);
    GetOpts opts(argc, argv, "d:g");
    if (opts.exists("d")) {
        report_period = opts.getNumber("d");
    }
    include_gps = opts.exists("g");
    signal(SIGALRM, on_alrm);
    Assoc config = readAssoc(HardwareConfFile, " ");
    if (config.find("cell_device") != config.end()) {
        cellinfo.device = config["cell_device"];
    }
    if (config.find("cell_islte") != config.end()) {
        cellinfo.type = "lte";
    }
    while (true) {
        if ( wanUp() and (getDefaultRoute(cellinfo.device) != "") ) {
            cellinfo.state = "up";
            upLoop();
            cellinfo.state = "down";
        }
        else {
            sleep(1);
        }
    }
}

bool wanUp() {
    bool retval = false;
    string line = grepFile(ProcDevFile, cellinfo.device);
    if (line != "") {
        retval = true;
    }
    return retval;
}

void upLoop() {
    Uptime = getUptime();
    unsigned long up_wait = 0;
    unsigned long stats_time = Uptime;
    cellinfo.last_report = Uptime;
    getCellInfo();
    getWan(true);
    while (true) {
        monWait(1);
        getStateInfo();
        if (!getWan(false)) {
            break;
        }
        if ((Uptime - stats_time) >= report_period) {
            bool check_link = false;
            if (waninfo.sum_rx_bytes == 0) {
                check_link = true;
            }
            string event = "Stats";
            event += eventUsage(true);
            event += eventWifiUsers();
            event += eventConnection();
            if (include_gps) {
                event += eventGpsPosition();
            }
            cout << getDateTime() << " " << event << endl;
            stats_time = Uptime;
        }
    }
    getCellInfo();
}

void monWait(int timeout) {
    sleep(timeout);
}

bool getWan(bool first) {
    unsigned long rx_bytes = 0;
    unsigned long rx_frames = 0;
    unsigned long tx_bytes = 0;
    unsigned long tx_frames = 0;
    waninfo.exists = false;
    cellinfo.connected = false;
    bool got_counts = false;
    if (cellinfo.type == "lte") {
        string resp = getCellResponse("isconnected");
        if (resp == "yes") {
            waninfo.exists = true;
            cellinfo.connected = true;
        }
        resp = getCellResponse("getusage");
        if (resp.find("rxbytes") != string::npos) {
            rx_bytes = stringToULong(getParm(resp,"rxbytes:",","));
            tx_bytes = stringToULong(getParm(resp,"txbytes:",","));
            got_counts = true;
        }
    }
    else {
        string line = grepFile(ProcDevFile, cellinfo.device);
        if (line != "") {
            waninfo.exists = true;
            cellinfo.connected = true;
            Strings vals = split(line, " :");
            rx_bytes = stringToULong(vals[1]);
            rx_frames = stringToULong(vals[2]);
            tx_bytes = stringToULong(vals[9]);
            tx_frames = stringToULong(vals[10]);
            got_counts = true;
        }
    }
    if (got_counts) {
        if (first) {
                waninfo.rx_bytes = 0;
                waninfo.tx_bytes = 0;
                waninfo.sum_rx_bytes = 0;
                waninfo.sum_tx_bytes = 0;
                waninfo.max_tx_kBps = 0.;
                waninfo.max_rx_kBps = 0.;
                waninfo.lasttime = getDoubleUptime();
        }
        else {
            unsigned long rx_delta = 0;
            if (rx_bytes < waninfo.last_rx_bytes) {
                rx_delta = 0xFFFFFFFF - waninfo.last_rx_bytes + rx_bytes + 1;
            }
            else {
                rx_delta = rx_bytes - waninfo.last_rx_bytes;
            }
            waninfo.rx_bytes = rx_delta;
            waninfo.sum_rx_bytes += rx_delta;
            unsigned long tx_delta = 0;
            if (tx_bytes < waninfo.last_tx_bytes) {
                tx_delta = 0xFFFFFFFF - waninfo.last_tx_bytes + tx_bytes + 1;
            }
            else {
                tx_delta = tx_bytes - waninfo.last_tx_bytes;
            }
            waninfo.tx_bytes = tx_delta;
            waninfo.sum_tx_bytes += tx_delta;
            double now = getDoubleUptime();
            double deltatime = now - waninfo.lasttime;
            waninfo.lasttime = now;
            waninfo.tx_kBps = tx_delta / deltatime / 1000.;
            waninfo.rx_kBps = rx_delta / deltatime / 1000.;
            waninfo.max_tx_kBps = max(waninfo.tx_kBps, waninfo.max_tx_kBps);
            waninfo.max_rx_kBps = max(waninfo.rx_kBps, waninfo.max_rx_kBps);
        }
        waninfo.last_rx_bytes = rx_bytes;
        waninfo.last_tx_bytes = tx_bytes;
    }
    return waninfo.exists;
}

string eventConnection() {
    string str = "";
    str += " Conn:" + cellinfo.connection_type;
    str += " Sig:" + cellinfo.signal;
    return str;
}

string eventDeltaTime(string cellstate, bool updatetime) {
    string str = "";
    string state = "D";
    unsigned long deltatime = Uptime - cellinfo.last_report;
    if (updatetime) {
        cellinfo.last_report = Uptime;
    }
    if (cellstate == "boot") {
        state = "B";
    }
    else if (cellstate == "up") {
        state = "U";
    }
    str += " " + state + ":" + toString(deltatime);
    return str;
}

string eventUsage(bool withspeed) {
    string str = "";
    char speedbuf[10];
    str += " Up:";
    str += toString(waninfo.sum_tx_bytes);
    if (withspeed) {
        str += "(";
        sprintf(speedbuf, "%.1f", waninfo.max_tx_kBps);
        str += speedbuf;
        str += ")";
    }
    str += " Down:";
    str += toString(waninfo.sum_rx_bytes);
    if (withspeed) {
        str += "(";
        sprintf(speedbuf, "%.1f", waninfo.max_rx_kBps);
        str += speedbuf;
        str += ")";
    }
    waninfo.max_tx_kBps = waninfo.max_rx_kBps = 0.;
    waninfo.sum_tx_bytes = waninfo.sum_rx_bytes = 0;
    return str;
}

string eventWifiUsers() {
    string str = "";
    int wificnt = wlan0info.users;
    str += " wifi:"+toString(wificnt);
    return str;
}

string eventGpsPosition() {
    string str = "";
    if (!car_state.location_disabled) {
        if (fileExists(GpsPositionFile)) {
            readGpsPosition();
        }
        if (gpsinfo.position != "") {
            str += " gps:";
            str += gpsinfo.position;
        }
    }
    gpsinfo.report_time = Uptime;
    return str;
}

void getStateInfo() {
    Uptime = getUptime();
    getUserConnections();
    getCellInfo();
    readGpsPosition();
}

void getUserConnections() {
    wlan0info.up = false;
    wlan0info.state = "down";
    wlan0info.users = 0;
    wlan0info.users = getWlan0Users();
    int wlan0_fake_users = -1;
    if (fileExists(Wlan0UsersFile)) {
        wlan0_fake_users = stringToInt(getStringFromFile(Wlan0UsersFile));
    }
    if (wlan0_fake_users >= 0) {
        wlan0info.users = wlan0_fake_users;
    }
    if (wlan0info.users > 0) {
        wlan0info.up = true;
        wlan0info.state = "up";
    }
}

int getWlan0Users() {
    int users = 0;
    if (fileExists(StationListFile)) {
        Strings lines = readFileArray(StationListFile);
        users = lines.size();
    }
    return users;
}

void readGpsPosition() {
    if (fileExists(GpsPresentFile) and !car_state.location_disabled) {
        if (fileExists(GpsPositionFile)) {
            gpsinfo.position = getStringFromFile(GpsPositionFile);
        }
    }
}

void getCellInfo() {
    string connection_type = "unknown";
    string signal = "unknown";
    string roaming = "";
    string netinfo = getCellResponse("getnetinfo");
//    cerr << "getnetinfo:" << netinfo << endl;
    if (netinfo != "") {
        connection_type = getParm(netinfo, "type:", ",");
        roaming = getParm(netinfo, "roam:", ",");
        if (roaming == "1") {
            roaming == "roaming";
        }
        signal = getParm(netinfo, "signal:", ",");
        if (signal == "") {
            if (connection_type == "1xRTT") {
                signal = getParm(netinfo, "1xrttsig:", ",");
//                cerr << "1xrttsig:" << signal << endl;
            }
            else {
                signal = getParm(netinfo, "evdosig:", ",");
//                cerr << "evdosig:" << signal << endl;
            }
        }
    }
    cellinfo.connection_type = connection_type;
    cellinfo.signal = signal;
//    cerr << "cellinfo.signal:" << cellinfo.signal << endl;
}

string getCellResponse(const char *cmnd) {
    string resp = "";
    string cmndstr = cmnd;
    char buff[256];
    if (cellinfo.socket < 0) {
        if (fileExists(CellTalkerPath)) {
            int sock = -1;
            if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
                cerr << "Could not open cell talker socket" << endl;
            }
            else {
                struct sockaddr_un a;
                memset(&a, 0, sizeof(a));
                a.sun_family = AF_UNIX;
                strcpy(a.sun_path, CellTalkerPath);
                if (connect(sock, (struct sockaddr *)&a, sizeof(a)) < 0) {
                    cerr << "Could not connect to cell talker socket" << endl;
                    close(sock);
                    sock = -1;
                }
                else {
                    cellinfo.fail_time = 0;
                }
            }
            cellinfo.socket = sock;
        }
    }
    if (cellinfo.socket >= 0) {
        int sent = send(cellinfo.socket, cmndstr.c_str(), cmndstr.size(), 0);
        if (sent == 0) {
            close(cellinfo.socket);
            cellinfo.socket = -1;
            cellinfo.fail_time = Uptime;
        }
        else {
            bool done = false;
            while (!done) {
                alarm(1);
                int nread = recv(cellinfo.socket, buff, 255, 0);
                alarm(0);
                if (nread < 0) {
                    if (errno == EINTR) {
                        if (alrm_sig) {
                            alrm_sig = false;
                            string talker = cellinfo.talker;
                            if (talker != "") {
                                break;
                            }
                        }
                    }
                }
                else if (nread == 0) {
                    close(cellinfo.socket);
                    cellinfo.socket = -1;
                    cellinfo.fail_time = Uptime;
                    resp = "";
                    done = true;
                }
                else {
                    buff[nread] = '\0';
                    if (buff[nread-1] == '\n') {
                        nread--;
                        buff[nread] = '\0';
                        done = true;
                    }
                    resp += buff;
                }
            }
        }
    }
    return resp;
}

double getDoubleUptime() {
    string upline = getStringFromFile(ProcUptime);
    double upval = 0;
    if (upline != "") {
        int pos = upline.find(" ");
        upline = upline.substr(0, pos);
        upval = stringToDouble(upline);
    }
    return upval;
}

void on_alrm(int sig) {
    alrm_sig = true;
    signal(SIGALRM, on_alrm);
}

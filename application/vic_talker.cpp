/******************************************************************************
 * File: vic_talker.cpp
 *
 * Daemon to allow multiple applications to communicate with VIC
 *
 ******************************************************************************
 * Change log:
 * 2012/09/12 rwp
 *    Changed format of response to application to include the first byte as
 *    length. This was needed because there is a possiblity of notification
 *    to be appended to another response/notification.
 * 2013/02/01 rwp
 *    Added control file for testing Dow logger
 * 2013/02/27 rwp
 *    Fixed bugs in Dow logging
 * 2013/02/28 rwp
 *    Changed Dow logging to return error code in ack
 *    Fixed double write of Dow log entries
 *    Added rollDowFiles
 * 2013/03/01 rwp
 *    Fixed block_number and number_of_blocks offsets
 *    Fixed log_name if contains trailing nulls
 * 2013/03/04 rwp
 *    Fixed rolling files when log file fills up
 *    Fixed getting higher precision on df command
 * 2013/04/01 rwp
 *    Added ability to send fake error response
 *    Fixed sending errors to apps
 * 2013/05/20 rwp
 *    Added lockup detection
 * 2013/06/20 rwp
 *    HT-38: Stopped logging of connections unles debug is on
 * 2013/07/24 rwp
 *    AUT-43: Added VIC Debug Notfication
 * 2013/07/31 rwp
 *    AUT-55: Fixed condition check for monitor watchdog
 * 2014/01/28 rwp
 *    Disable acking notifications until monitor reads the RTC
 * 2014/01/29 rwp
 *    Change watchdog condition to 1 hour after no need for monitor
 * 2014/08/01 rwp
 *    Dont dump vic_loader packets
 * 2014/09/12 rwp
 *    Changed timeout to pop first(only)
 *    Fixed sending responses to closed app socket
 * 2015/08/06 rwp
 *    AK-122: Enable ssh on ignition reset
 ******************************************************************************
 */

#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "backtick.h"
#include "my_features.h"
#include "fileStats.h"
#include "fileExists.h"
#include "getStringFromFile.h"
#include "getField.h"
#include "getUptime.h"
#include "hiresTime.h"
#include "readAssoc.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
#include "touchFile.h"
#include "updateFile.h"
#include "writeAssoc.h"
#include "dateTime.h"
#include "vic.h"
using namespace std;

#define VicErrorFile "/tmp/vic_error"
#define serial_device "ttymxc2"
#define F_SOF 0xC0
#define F_DLE 0x7D
#define F_EOF 0xFE

//#define _POSIX_SOURCE 1         //POSIX compliant source


class Connection {
  private:
    int _socket;
    int _notifies[16];
    int _num_notifies;
    bool _opened;
    bool _isDownload;

  public:
    Connection() {
        _socket = -1;
        _num_notifies = 0;
        _opened = false;
        _isDownload = false;
    }

    Connection(int sock) {
        _socket = sock;
        _num_notifies = 0;
        _opened = true;
        _isDownload = false;
    }

    void setNotifies(int n, Byte *notifies) {
        for (int i=0; i < n; i++) {
            _notifies[i] = notifies[i];
        }
        _num_notifies = n;
    }

    bool wantNotification(Byte notification) {
        bool does = false;
        for (int i=0; i < _num_notifies; i++) {
            if (notification == _notifies[i]) {
                does = true;
                break;
            }
        }
        return does;
    }

    void close() {
        _socket = -1;
        _opened = false;
    }

    bool isClosed() {
        return !_opened;
    }

    bool isOpen() {
        return _opened;
    }

    void setDownload() {
        _isDownload = true;
    }

    bool isDownload() {
        return _isDownload;
    }
};
typedef map<int,Connection> Connections;
Connections connections;

struct Command {
    int src_sock;
    Byte packet[256];
    int packet_len;

    Command() {
        src_sock = -1;
        packet_len = 0;
    }
};
deque<Command> commands;
unsigned long command_send_time = 0;

class DowLoggerInfo {
public:
    bool enabled;
    int log_type;
    int hw_vers;
    int sw_vers;
    string log_name;
    unsigned long max_size;
    unsigned long file_size;
    string filename;
    unsigned int last_gps;
    int gps_interval;
    int last_seq;
    time_t log_open_time;
//    double last_frame_time;
    FILE *fp;

    DowLoggerInfo() {
        enabled = false;
        log_type = 0;
        log_name = "";
        filename = "";
        max_size = 0;
        file_size = 0;
        last_gps = 0;
        gps_interval = 60;
        last_seq = -1;
        log_open_time = 0;
//        last_frame_time = 0.;
        fp = NULL;
    }
};
DowLoggerInfo dow_logger;

Byte crypt_table[256] = {
0x5A,0xBB,0x60,0xFC,0x71,0xA2,0x47,0x42,0x11,0x48,0x03,0xA1,0x8B,0x40,0xD4,0x36,
0x38,0x44,0x74,0x29,0xF5,0x81,0x8F,0x6F,0xFA,0x13,0x58,0x2A,0xA3,0x06,0x17,0x62,
0x7A,0x5E,0xB4,0x89,0xCD,0x22,0xC3,0x5B,0x5C,0xC9,0xF1,0xD9,0x24,0x26,0xF9,0x4A,
0xF0,0x72,0x88,0xE0,0xB7,0xC0,0x37,0xDB,0x1E,0xAA,0xA6,0xBC,0xE3,0x43,0xF4,0xD2,
0x01,0xB1,0xCE,0x9F,0xC7,0xED,0x86,0x10,0xE9,0xEF,0xFF,0x61,0x18,0x2C,0x2B,0x67,
0xEE,0x19,0xD1,0x3D,0x84,0x65,0x4E,0x05,0x31,0xC5,0x0A,0x55,0x98,0x9D,0xF6,0x4D,
0x49,0xDD,0xAF,0xE1,0x28,0x33,0x12,0x3C,0x9B,0x0E,0xEA,0x21,0x4C,0x14,0x00,0x4F,
0x6E,0x8E,0xA9,0x75,0x25,0x77,0x70,0x5F,0xA8,0x90,0x0B,0x51,0x73,0x96,0x32,0xE2,
0xC2,0x8C,0xF8,0xB3,0xF3,0xBF,0x3B,0x64,0xCA,0x6D,0xE6,0x57,0xD7,0x97,0x45,0x83,
0x59,0xD3,0xA5,0xFE,0x80,0x93,0x1D,0xB9,0xA7,0xD6,0x94,0x08,0xF7,0x92,0x35,0x4B,
0xFD,0xA4,0xB5,0x56,0x7D,0xBD,0x8A,0xAE,0xBA,0x78,0x69,0x16,0x9E,0xD0,0x7B,0xAD,
0x6A,0x52,0x8D,0x99,0x82,0x6C,0x30,0x6B,0x9C,0x1B,0x34,0xE5,0x54,0x2D,0x79,0x23,
0xE8,0xA0,0x50,0x7E,0x0D,0xCC,0xE4,0xD5,0x68,0x1A,0x87,0x1F,0xDF,0x95,0x85,0xF2,
0x7C,0x2E,0x41,0x15,0xC8,0xB6,0x76,0xD8,0x9A,0xB8,0x3E,0xBE,0xAC,0xAB,0xCB,0x27,
0xCF,0x53,0x63,0x91,0x1C,0x09,0xDA,0xFB,0x02,0x3F,0xC4,0x7F,0xEB,0xB2,0xDC,0x3A,
0x66,0xDE,0xC6,0xB0,0x0F,0xE7,0x04,0xEC,0x0C,0x46,0x39,0x20,0x5D,0xC1,0x07,0x2F
};

void processVicFrame(Byte *recv_buf, int frame_len);
bool processAppCommand(Byte *read_buf, int res, int rs);
void sendResponse(int recv_len, Byte *recv_buf);
void sendNextCommand();
void sendNotifications(int cmnd_id, int len, Byte *packet);
void testDowLogger();
void processDowPacket(int len, Byte *packet, bool testing);
int writeDowLogVersion();
int writeDowGpsEntry();
void degToBCD(Byte *p, string degstr);
Byte toBCD(int val);
int writeDowLog(int len, Byte *block);
void createDowLog(time_t timeval);
void closeDowLog();
void rollDowFiles();
double toBytes(string val);
void sendAck(int cmd_id, int datlen, Byte *data, bool sign);
void sendNak(int reason, Byte *recv_buf, int len);
void sendPacket(Byte *packet, int len);
int formatFrame(Byte *frame, Byte *packet, int len);
Byte calc_chksum(Byte *packet, int len);
void checkSystemHealth();
void closeAndExit(int exitstat);
void openPort(string port);
void closePort();
bool lock(string dev);
int listenSocket(const char *name);
void onTerm(int parm);
void onUsr1(int parm);
void onAlrm(int parm);
void checkIgnChange(bool new_state);
bool startEcall(int len, Byte *buffer);
void sendEmergencyInfo(int appid);
string intToString(int i);

bool debug = false;
int ser_s = -1;		// Serial port descriptor
int listen_s = -1;	// Listen socket for command connections
struct termios oldtio;	// Old port settings for serial port
string lockfile = "";
bool simulate_ecall = false;
bool test_dow_log = false;
bool loader_called = false;
bool timeok = false;
bool ign_state = false;
unsigned long ign_change_time = 0;
int ign_change_count = 0;
unsigned long monitor_state_change_time = 0;
bool monitor_state = false;
bool ok_to_ack_notifications = false;

int main(int argc, char *argv[])
{
    int res;
    Byte buf[1024];                       //buffer for where data is put
    Byte read_buf[2048];                  //buffer for where data is put
    Byte recv_buf[2048];                  //buffer for where data is put
    string unsol_line = "";
    Connections::iterator ci;
    Features features;

    if (features.exists(VicDebug)) {
        debug = true;
    }
    if (features.getFeature(VehicleBrand) == "Dow") {
        dow_logger.enabled = true;
        struct sched_param param;
        cerr << "Setting to realtime" << endl;
        param.sched_priority = sched_get_priority_max(SCHED_RR);
        if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
            cerr << "Failed to set realtime" << endl;
        }
    }
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, onUsr1);
    signal(SIGALRM, onAlrm);

    if (fileExists(TestDowLoggerFile)) {
        test_dow_log = true;
    }
    if (fileExists("/user/dowlog") and fileExists("/user/tmp")) {
       rollDowFiles();
       system("mv -f /user/tmp/* /user/dowlog 2>/dev/null");
    }

    cerr << getDateTime() << " vic_talker started" << endl;
    string ser_dev = serial_device;
    if (fileExists("/autonet/admin/vic_port")) {
        string ser1 = getStringFromFile("/autonet/admin/vic_port");
        if (ser1 != "") {
            ser_dev = ser1;
        }
    }
    if (!fileExists("/dev/"+ser_dev)) {
        cerr << "Port " << ser_dev << " does not exist" << endl;
        sleep(60);
        exit(1);
    }
    
    openPort(ser_dev);
    bool frame_to_send = false;

    listen_s = listenSocket(VicTalkerPath);
    if (listen_s < 0) {
        closeAndExit(-1);
    }
   
    // loop while waiting for input. normally we would do something useful here
    bool in_frame = false;
    bool dle = false;
    int recv_len = 0;
    Byte chksum = 0;
    int last_overrun = 0;
    struct serial_icounter_struct counters;
    int ioctl_ret = ioctl(ser_s, TIOCGICOUNT, &counters);
    if (ioctl_ret != -1) {
        last_overrun = counters.overrun;
    }
    bool interbyte_timeout = false;
    while (1) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        if (ser_s >= 0) {
            FD_SET(ser_s, &rd);
            nfds = max(nfds, ser_s);
//            cerr << "add ser_s socket to select" << endl;
        }
        if (listen_s >= 0) {
            FD_SET(listen_s, &rd);
            nfds = max(nfds, listen_s);
//            cerr << "add listen_s socket to select" << endl;
        }
        for (ci=connections.begin(); ci != connections.end(); ci++) {
            int rs = (*ci).first;
            FD_SET(rs, &rd);
            nfds = max(nfds, rs);
//            cerr << "add socket " << rs << " to select" << endl;
        }
        if (frame_to_send) {
            FD_SET(ser_s, &wr);
        }
//        cerr << "+";
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        if (in_frame and !interbyte_timeout) {
            tv.tv_sec = 0;
            tv.tv_usec = 10000;
        }
        int r = select(nfds+1, &rd, &wr, &er, &tv);
//        cerr << "-";
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (r > 0) {
            if ((ser_s >= 0) and FD_ISSET(ser_s, &rd)) {
                alarm(1);
//                if (debug) {
//                    cerr << getDateTime() << " before read" << endl;
//                }
                res = read(ser_s,buf,sizeof(buf)-1);
                alarm(0);
                if (debug) {
                    if (!loader_called) {
                        cerr << hiresDateTime() << " got " << res << " bytes";
                        for (int j=0; j < res; j++) {
                             fprintf(stderr," %02X", buf[j]);
                        }
                        fprintf(stderr,"\n");
                    }
                }
                ioctl_ret = ioctl(ser_s, TIOCGICOUNT, &counters);
                if (ioctl_ret != -1) {
                    if (counters.overrun != last_overrun) {
                        cerr << hiresDateTime() << " Serial overrun:" << counters.overrun << endl;
                        last_overrun = counters.overrun;
                    }
                }
                int i;
                if (res == 0) {
                    closeAndExit(1);
                }
                if (res>0) {
                    int i;
                    for (i=0; i < res; i++) {
                        Byte ch = buf[i];
                        if (ch == F_SOF) {
                            if (in_frame) {
                                cerr << hiresDateTime() << " Lost EOF...tossing " << recv_len << " bytes" << endl;
                            }
                            in_frame = true;
                            recv_len = 0;
                            dle = false;
                            chksum = 0;
                            interbyte_timeout = false;
                        }
                        else if (ch == F_DLE) {
                            dle = true;
                        }
                        else if (ch == F_EOF) {
                            in_frame = false;
                            int frame_len = recv_len;
                            recv_len = 0;
                            if (chksum != 0xFF) {
                                cerr << hiresDateTime() << " Invalid checksum" << endl;
                                sendNak(0, recv_buf, 2);
                            }
                            else {
                                frame_len--; // Remove checksum byte
                                if (debug) {
                                    cerr << hiresDateTime() << " ";
                                    fprintf(stderr,"Rcvd packet(%d):", frame_len);
                                    if (loader_called) {
                                        for (int j=0; j < 3; j++) {
                                            fprintf(stderr," %02X", recv_buf[j]);
                                        }
                                    }
                                    else {
                                        for (int j=0; j < frame_len; j++) {
                                            fprintf(stderr," %02X", recv_buf[j]);
                                        }
                                    }
                                    fprintf(stderr,"\n");
                                }
                                if (fileExists(VicErrorFile)) {
                                    int code = stringToInt(getStringFromFile(VicErrorFile));
                                    recv_buf[3] = recv_buf[0];
                                    recv_buf[4] = recv_buf[1];
                                    recv_buf[0] = 0;
                                    recv_buf[1] = 0xFF;
                                    recv_buf[2] = code;
                                    frame_len = 5;
                                    unlink(VicErrorFile);
                                }
                                processVicFrame(recv_buf, frame_len);
                            }
                        }
                        else if (in_frame) {
                            if (dle) {
                                ch ^= 0x20;
                                dle = false;
                            }
                            recv_buf[recv_len++] = ch;
                            chksum += ch ^ 0xAA;
                        }
                    }
                }  //end if res>0
            }
            if ((listen_s != -1) and FD_ISSET(listen_s, &rd)) {
                struct sockaddr client_addr;
                unsigned int l = sizeof(struct sockaddr);
                memset(&client_addr, 0, l);
                int rs = accept(listen_s, (struct sockaddr *)&client_addr, &l);
                if (rs < 0) {
                    perror ("accept()");
                }
                else {
                    if (debug) {
                        cerr << getDateTime() << " ";
                        cerr << "Got connection: " << rs << endl;
                    }
                    Connection connection(rs);
                    connections[rs] = connection;
                }
            }
            bool check_closes = false;
            for (ci=connections.begin(); ci != connections.end(); ci++) {
                int rs = (*ci).first;
                if (connections[rs].isOpen() and FD_ISSET(rs, &rd)) {
                    alarm(1);
//                    if (debug) {
//                        cerr << getDateTime() << " before read conn" << endl;
//                    }
                    res = read(rs, read_buf, 2048);
                    alarm(0);
                    if (res > 0) {
                        if (debug) {
                            cerr << getDateTime() << " ";
                            fprintf(stderr,"Rcvd mesg from %d:", rs);
                            if (loader_called) {
                                for (int i=0; i < 3; i++) {
                                    fprintf(stderr," %02X", read_buf[i]);
                                }
                            }
                            else {
                                for (int i=0; i < res; i++) {
                                    fprintf(stderr," %02X", read_buf[i]);
                                }
                            }
                            fprintf(stderr,"\n");
                        }
                        check_closes |= processAppCommand(read_buf, res, rs);
                    }
                    else {
                        close(rs);
                        if (connections[rs].isDownload()) {
                            cerr << "Reopening VicTalkerSocket" << endl;
                            listen_s = listenSocket(VicTalkerPath);
                            if (listen_s < 0) {
                                closeAndExit(-1);
                            }
//                            loader_called = false;
                        }
                        if (commands.size() > 0) {
                            Command cmd = commands.front();
                            if (cmd.src_sock == rs) {
                                cmd.src_sock = -1;
                            }
                        }
                        connections[rs].close();
                        check_closes = true;
                    }
                }
            }
            while (check_closes) {
                check_closes = false;
                for (ci=connections.begin(); ci != connections.end(); ci++) {
                    int sock = (*ci).first;
                    if (connections[sock].isClosed()) {
                        if (debug) {
                            cerr << getDateTime() << " Removing connection " << sock << endl;
                        }
                        connections.erase(sock);
                        check_closes = true;
                        break;
                    }
                }
            }
        }
        else {
            if (in_frame and !interbyte_timeout) {
                cerr << hiresDateTime() << " Interbyte timeout" << endl;
                int len = 2;
                if (recv_len < 2) {
                    len = recv_len;
                }
                sendNak(8, recv_buf, len); 
                interbyte_timeout = true;
            }
        }
        if ( (command_send_time > 0) and
             ((time(NULL)-command_send_time) > 5) ) {
             if (commands.size() > 0) {
                 cerr << getDateTime() << " Command timeout... popping command" << endl;
                 commands.pop_front();
                 command_send_time = 0;
                 sendNextCommand();
             }
             else {
                 command_send_time = 0;
             }
        }
        if (test_dow_log) {
            string dt = getDateTime();
            if (dt.substr(0,4) > "2001") {
                testDowLogger();
                test_dow_log = false;
            }
        }
        unsigned long uptime = getUptime();
        bool new_monitor_state = true;
        string keep_on = "";
        if (fileExists(KeepOnReasonFile)) {
            keep_on = getStringFromFile(KeepOnReasonFile);
        }
        string connection = "";
        if (fileExists(ConnectionReasonFile)) {
            connection = getStringFromFile(ConnectionReasonFile);
        }
        if ( !ign_state and (keep_on == "") and (connection == "")) {
            new_monitor_state = false;
        }
        if (new_monitor_state != monitor_state) {
            monitor_state_change_time = uptime;
            monitor_state = new_monitor_state;
        }
        if (((uptime-monitor_state_change_time) > 3600) and !monitor_state) {
           cerr << "Shutting down cpu" << endl;
           system("log_event NotifySupport Monitor failed to shutdown");
           system("shutdown 0 0 &");
           monitor_state_change_time = uptime;
        }
        if ( ((uptime-ign_change_time) > 10) and
             !ign_state and (ign_change_count > 4)) {
           cerr << "Shutting off cpu" << endl;
           if (!fileExists(SshControlFile)) {
               string timestr = toString(time(NULL)+900);
               cerr << "Writing " << timestr << " to " << SshControlFile << endl;
               updateFile(SshControlFile,timestr);
           }
           system("log_event NotifySupport Customer reset unit");
           system("(sleep 5; shutdown 3 2 )&");
           ign_change_count = 0;
        }

    }  //while (1)
    closeAndExit(0);
    return 0;
}  //end of main

void processVicFrame(Byte *recv_buf, int frame_len) {
    int app_id = recv_buf[0];
    int cmd_id = recv_buf[1];
    // The following is just temporary to fix bug in VIC
//    if ((app_id == 0) and (cmd_id == 0x06)) {
//        app_id = 0x20;
//        recv_buf[0] = app_id;
//    }
    if (app_id == 0) {
        if (cmd_id == 0xFF) {
            int err_code = recv_buf[2];
            app_id = recv_buf[3];
            cmd_id = recv_buf[4];
            // Need to send error message to app
            cerr << hiresDateTime() << " Got error" << endl;
            if (commands.size() > 0) {
                Command cmd = commands.front();
                int sock = cmd.src_sock;
                if (sock >= 0) {
                    cerr << getDateTime() << " ";
                    fprintf(stderr,"Sent error to %d(4)\n", cmd.src_sock);
                    Byte error_buf[16];
                    error_buf[0] = 4;
                    error_buf[1] = app_id;
                    error_buf[2] = 0xFF;
                    error_buf[3] = cmd_id;
                    error_buf[4] = err_code;
                    if (send(sock, error_buf, 5, 0) == 0) {
                        close(sock);
                        connections[sock].close();
                    }
                }
                else {
                    cerr << getDateTime() << " Cannot send error, socket closed" << endl;
                }
                commands.pop_front();
                command_send_time = 0;
                sendNextCommand();
            }
        }
        else if ((cmd_id&0xF0) == 0x80) {
            if (!timeok) {
                string dt = getDateTime();
                if (dt.substr(0,4) > "2000") {
                    timeok = true;
                }
            }
            if (timeok and !loader_called) {
                // Dow logger frames
                processDowPacket(frame_len, recv_buf, false);
            }
            else {
                cerr << getDateTime() << " Ignoring DowLogger packet (no time)" << endl;
            }
        }
        else {
            // This is a Notify message
            Byte msg_buf[16];
            Byte resp_buf[64];
            unsigned long uptime = getUptime();
            cerr << getDateTime() << " Got " << string_printf("%02X", cmd_id) << " notification" << endl;
            if (cmd_id == 0xFE) {
                // VIC Debug notifiecaion
                string deb_mesg;
                deb_mesg.assign((char *)(&recv_buf[2]), frame_len-2);
                cerr << getDateTime() << " Debug: " << deb_mesg << endl;
            }
            if (ok_to_ack_notifications or (uptime > 60)) {
                msg_buf[0] = 0x00;
                msg_buf[1] = cmd_id;
                int len = VIC::buildSignedMesg(2, msg_buf, resp_buf);
                sendPacket(resp_buf, len);
                if (cmd_id == 0xF3) {
                    cerr << getDateTime() << " Watchdog notification" << endl;
                    checkSystemHealth();
                }
                else {
                    bool notify = true;
                    if (cmd_id == 0xF0) {
                        notify = startEcall(frame_len, recv_buf);
                    }
                    else if (cmd_id == 0xF1) { /* Ignition Change Notification */
                        checkIgnChange(recv_buf[2]);
                    }
                    if (notify) {
                        sendNotifications(cmd_id, frame_len, recv_buf);
                    }
                }
            }
            else {
                cerr << getDateTime() << " Did not Ack the notification" << endl;
            }
        }
    }
    else if ((app_id == 0x7f) and (cmd_id == 0x01)) {
        // No VIC app, we must load default app
        if (!loader_called) {
            system("vic_loader -l &");
            loader_called = true;
        }
        commands.pop_front();
        command_send_time = 0;
        sendNextCommand();
    }
    else {
        if (cmd_id == 0x20) { /* Get Status Response */
            checkIgnChange(recv_buf[2] & 0x01);
        }
        sendResponse(frame_len, recv_buf);
        sendNextCommand();
    }
}

bool processAppCommand(Byte *read_buf, int res, int rs) {
    bool check_closes = false;
    int app_id = read_buf[0];
    int cmd_id = read_buf[1];
    if ((app_id == 0xFF) and (cmd_id == 0xFF)) {
        connections[rs].setNotifies(res-2, &read_buf[2]);
        Byte resp_buf[16];
        resp_buf[0] = 2;
        resp_buf[1] = read_buf[0];
        resp_buf[2] = read_buf[1];
        if (send(rs, resp_buf, 3, 0) == 0) {
            close(rs);
            connections[rs].close();
            check_closes = true;
        }
        return check_closes;
    }
    if (cmd_id == 0x01) {
        ok_to_ack_notifications = true;
    }
    if ( (cmd_id == 0x2A) and simulate_ecall) {
        sendEmergencyInfo(app_id);
        return false;
    }
    if ((cmd_id == 0x06) and (listen_s != -1)) {
        // load firmware command
        // Set firmware download flag (responses are handled differently)
        // close listen socket (stops other process from trying to use VIC)
        cerr << "Closing listen socket" << endl;
        close(listen_s);
        listen_s = -1;
        cerr << "Removing VicTalkerPath" << endl;
        remove(VicTalkerPath);
        // close all other app sockets
        cerr << "Closing all other connections" << endl;
        Connections::iterator cj;
        for (cj=connections.begin(); cj != connections.end(); cj++) {
             int sock = (*cj).first;
             if (sock != rs) {
                 cerr << "Closing connection:" << sock << endl;
                 close(sock);
                 connections[sock].close();
             }
        }
        check_closes = true;
        // mark this connection for downloading (when this connection closes, then reopen listen socket
        cerr << "Setting this connection as downloading: " << rs << endl;
        connections[rs].setDownload();
        loader_called = true;
        if (dow_logger.log_name != "") {
            closeDowLog();
        }
    }
    Command command;
    command.src_sock = rs;
    command.packet_len = res;
    memcpy(command.packet, read_buf, res);
    commands.push_back(command);
    if (commands.size() == 1) {
        sendPacket(read_buf, res);
        command_send_time = time(NULL);
    }
    return check_closes;
}

void sendResponse(int recv_len, Byte *recv_buf) {
    Byte resp_buf[128];
    if (commands.size() > 0) {
        resp_buf[0] = recv_len;
        for (int i=0; i < recv_len; i++) {
            resp_buf[i+1] = recv_buf[i];
        }
        Command cmd = commands.front();
        int sock = cmd.src_sock;
        if (sock >= 0) {
            if (debug) {
                cerr << getDateTime() << " ";
                fprintf(stderr,"Sent mesg to %d\n", sock);
            }
            if (send(sock, resp_buf, recv_len+1, 0) == 0) {
                close(sock);
                connections[sock].close();
            }
        }
        else {
            cerr << getDateTime() << " Cannot send response, socket closed" << endl;
        }
        commands.pop_front();
        command_send_time = 0;
    }
}

void sendNextCommand() {
    if (commands.size() > 0) {
        Command cmd = commands.front();
        sendPacket(cmd.packet, cmd.packet_len);
        command_send_time = time(NULL);
    }
}

void sendNotifications(int cmnd_id, int len, Byte *packet) {
    Connections::iterator ci;
    Byte resp_buff[128];
    resp_buff[0] = len;
    for (int i=0; i < len; i++) {
        resp_buff[i+1] = packet[i];
    }
    for (ci=connections.begin(); ci != connections.end(); ci++) {
        int rs = (*ci).first;
        if (connections[rs].wantNotification(cmnd_id)) {
            fprintf(stderr,"Sent notification to %d\n", rs);
            if (send(rs, resp_buff, len+1, 0) == 0) {
                close(rs);
                connections[rs].close();
            }
        }
    }
}

void testDowLogger() {
    Byte openpacket[] = {
         0x00, 0x80,	// app_id, cmd_id(open log session)
         0x02,		// log_type
         0x11, 0x22,	// sw_version
         0x33, 0x44,	// hw_version
         0x10, 0x00,	// max_size
         '.', 'd', 't', 'd', 0x00,	// file_name
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00
    };
    processDowPacket(sizeof(openpacket), openpacket, true);
    Byte logpacket[] = {
         0x00, 0x83,	// app_id, cmd_id(unacknowledge log record)
         0x01,		// seq_num
         0x01,		// record_num
         0x01,		// num_blocks
         0x01,		// block_num
         0x00, 0x06,	// block_length
         0x01,		// record_type(CAN1)
         0x00, 0x03,	// record_length
         0xaa, 0xbb, 0xcc
    };
    processDowPacket(sizeof(logpacket), logpacket, true);
    Byte gpspacket[] = {
         0x00, 0x84	// app_id, cmd_id(GPS Log Request)
    };
    processDowPacket(sizeof(gpspacket), gpspacket, true);
    Byte closepacket[] = {
         0x00, 0x81	// app_id, cmd_id(Close log session)
    };
    processDowPacket(sizeof(closepacket), closepacket, true);
}


void processDowPacket(int len, Byte *packet, bool testing) {
    Byte ack_data[16];
    int cmd_id = packet[1];
    if (cmd_id == 0x80) {
        cerr << getDateTime() << " Got DowLogger open log session" << endl;
        // Open log session
        ack_data[0] = 1;
        dow_logger.log_type = packet[2];
        dow_logger.sw_vers = ((int)packet[3] << 8) | packet[3];
        dow_logger.hw_vers = ((int)packet[5] << 8) | packet[6];
        dow_logger.max_size = (((int)packet[7] << 8) | packet[8])* 1024;
        int name_len = len - 9;
        dow_logger.log_name = string((char *)(packet+9), name_len);
        dow_logger.log_name = dow_logger.log_name.c_str();
        dow_logger.log_open_time = time(NULL);
        dow_logger.last_seq = -1;
        if (dow_logger.fp != NULL) {
            closeDowLog();
        }
//        dow_logger.last_frame_time = hiresTime();
        if (!testing) {
//            cerr << getDateTime() << " Sending Ack" << endl;
            sendAck(cmd_id, 1, ack_data, true);
        }
    }
    else if (cmd_id == 0x81) {
        // Close log session
        ack_data[0] = 1;
        if (dow_logger.log_name != "") {
            closeDowLog();
        }
        else {
            ack_data[0] = 0;
        }
//        dow_logger.last_frame_time = 0.;
        if (!testing) {
            sendAck(cmd_id, 1, ack_data, true);
        }
        dow_logger.log_name = "";
    }
    else if (cmd_id == 0x82) {
        // Acked log packet
//        if (dow_logger.last_frame_time > 0.) {
//            double now = hiresTime();
//            if ((now-dow_logger.last_frame_time) > 0.2) {
//               cerr << hiresDateTime() << " Inter frame timeout" << endl;
//            }
//            dow_logger.last_frame_time = now;
//        }
        int seq = packet[2];
        int retval = 1;
        if (seq != dow_logger.last_seq) {
            dow_logger.last_seq = seq;
            retval = writeDowLog(len-2, packet+2);
            touchFile(DowLastLogFile);
        }
        else {
            cerr << hiresDateTime() << " Dropped duplicate logger packet: " << seq << endl;
        }
        ack_data[0] = retval;
        if (!testing) {
            sendAck(cmd_id, 1, ack_data, true);
        }
    }
    else if (cmd_id == 0x83) {
        // Non-Acked log packet
        int seq = packet[2];
        if (seq != dow_logger.last_seq) {
            dow_logger.last_seq = seq;
            writeDowLog(len-2, packet+2);
            touchFile(DowLastLogFile);
        }
        else {
            cerr << hiresDateTime() << " Dropped duplicate logger packet: " << seq << endl;
        }
    }
    else if (cmd_id == 0x84) {
        // Add GPS log entry
        int retval = writeDowGpsEntry();
        ack_data[0] = retval;
        dow_logger.last_gps = time(NULL);
        if (!testing) {
            sendAck(cmd_id, 1, ack_data, true);
        }
    }
}

int writeDowLogVersion() {
    Byte block[128];
    int retval = 0;
    block[0] = 0;	// sequence number
    block[1] = 0x80;	// record number
    block[2] = 1;	// number of blocks
    block[3] = 1;	// block num
    block[4] = 0;	// length(high)
    block[5] = 7;	// length(low)
    block[6] = 0x06;	// record type = Log Version
    block[7] = 0;	// record length(high)
    block[8] = 4;	// record length(low)
    block[9] = dow_logger.sw_vers >> 8;
    block[10] = dow_logger.sw_vers & 0xFF;
    block[11] = dow_logger.hw_vers >> 8;
    block[12] = dow_logger.hw_vers & 0xFF;
    retval = writeDowLog(13, block);
    return retval;
}

int writeDowGpsEntry() {
    Byte block[128];
    int retval = 0;
    block[0] = 0;	// sequence number
    block[1] = 0x80;	// record number
    block[2] = 1;	// number of blocks
    block[3] = 1;	// block num
    block[4] = 0;	// length(high)
    block[5] = 0;	// length(low)
    block[6] = 0x03;	// record type = GPS
    block[7] = 0;	// record length(high)
    block[8] = 0;	// record length(low)
    Byte *p = block+9;
    int l = 0;
    int gps_valid = 0;
    string lat = "0.000000000";
    string lon = "0.000000000";
    if (fileExists(GpsPositionFile)) {
        string latlon = getStringFromFile(GpsPositionFile);
        if (latlon != "") {
            int pos = latlon.find("/");
            lat = latlon.substr(0,pos);
            lon = latlon.substr(pos+1);
            gps_valid = 1;
        }
    }
    *p++ = gps_valid;
    l++;
    time_t time_val;
    time(&time_val);
    tm *ptm = gmtime(&time_val);
    int year = ptm->tm_year+1900;
    int mon = ptm->tm_mon+1;
    *p++ = toBCD(year/100);
    *p++ = toBCD(year%100);
    *p++ = toBCD(mon);
    *p++ = toBCD(ptm->tm_mday);
    *p++ = toBCD(ptm->tm_hour);
    *p++ = toBCD(ptm->tm_min);
    *p++ = toBCD(ptm->tm_sec);
    l += 7;
    degToBCD(p, lat);
    p += 6;
    l += 6;
    degToBCD(p, lon);
    p += 6;
    l += 6;
    int blen = l + 3;
    block[4] = blen >> 8;
    block[5] = blen & 0xFF;
    block[7] = l >> 8;
    block[8] = l & 0xFF;
    dow_logger.last_gps = time(NULL);
    retval = writeDowLog(l+9, block);
    return retval;
}

void degToBCD(Byte *p, string degstr) {
    double d = stringToDouble(degstr);
    int sign = 0;
    if (d < 0) {
        sign = 1;
        d *= -1.;
    }
    int deg = (int)d;
    unsigned long frac = (int)((d-deg)*100000000.);
    *p++ = toBCD(sign*10+deg/100);
    *p++ = toBCD(deg%100);
    *p++ = toBCD(frac/1000000);
    frac %= 1000000;
    *p++ = toBCD(frac/10000);
    frac %= 10000;
    *p++ = toBCD(frac/100);
    frac %= 100;
    *p++ = toBCD(frac);
}

Byte toBCD(int val) {
   Byte bcd = 0;
   bcd = (val/10) << 4;
   bcd |= val % 10;
   return bcd;
}

int writeDowLog(int len, Byte *block) {
    Byte buffer[1024];
    int retval = 2;
    if (dow_logger.fp == NULL) {
        time_t time_val = time(NULL);
//        rollDowFiles();
        if (dow_logger.log_name != "") {
            createDowLog(time_val);
            if (dow_logger.fp != NULL) {
                writeDowLogVersion();
                writeDowGpsEntry();
                dow_logger.last_gps = time(NULL);
            }
        }
        else {
            retval = 2;
        }
    }
    if (dow_logger.fp != NULL) {
        int num_blocks = block[2];
        int block_num = block[3];
        if ((dow_logger.log_type == 2) and (block_num == 1)) {
            unsigned long now = time(NULL);
            if ((now-dow_logger.last_gps) > dow_logger.gps_interval) {
                dow_logger.last_gps = now;
//                cerr << getDateTime() << " writeDowGpsEntry" << endl;
                retval = writeDowGpsEntry();
            }
        }
        int l = formatFrame(buffer, block, len);
        int wl = fwrite(buffer, 1, l, dow_logger.fp);
        if (wl == l) {
            retval = 1;
            dow_logger.file_size += l;
            if ((dow_logger.log_type == 2) and (block_num == num_blocks)) {
                if (dow_logger.file_size >= dow_logger.max_size) {
                    closeDowLog();
                }
            }
        }
        else {
            retval = 2;
        }
    }
    else {
        retval = 0;
    }
    return retval;
}

void createDowLog(time_t timeval) {
    if (dow_logger.log_name != "") {
        rollDowFiles();
        string unitid = getStringFromFile("/etc/hostname");
        tm *ptm = gmtime(&timeval);
        dow_logger.filename = string_printf("%s-%04d%02d%02d%02d%02d%02d%s",
               unitid.c_str(),
               ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday,
               ptm->tm_hour, ptm->tm_min, ptm->tm_sec,
               dow_logger.log_name.c_str());
        string tmp_file = "/user/tmp/"+dow_logger.filename;
        dow_logger.fp = fopen(tmp_file.c_str(), "wb");
        dow_logger.file_size = 0;
    }
}

void closeDowLog() {
    cerr << getDateTime() << " CloseDowLog" << endl;
    if (dow_logger.fp == NULL) {
        if (dow_logger.log_name != "") {
            createDowLog(dow_logger.log_open_time);
        }
    }
    if (dow_logger.fp != NULL) {
        fclose(dow_logger.fp);
        string tmpfile = "/user/tmp/"+dow_logger.filename;
        string logfile = "/user/dowlog/"+dow_logger.filename;
        rename(tmpfile.c_str(), logfile.c_str());
        dow_logger.fp = NULL;
    }
}

void rollDowFiles() {
    int used_percent = 0;
    while (true) {
        string userline = backtick("df -k | grep /user");
        Strings vals = split(userline, " ");
//        cerr << "df:" << userline << endl;
//        cerr << "vals.size():" << vals.size() << endl;
//        cerr << "size:" << vals[1] << endl;
//        cerr << "used:" << vals[2] << endl;
//        cerr << "percent:" << vals[4] << endl;
        double size = toBytes(vals[1]);
        double used = toBytes(vals[2]);
//        cerr << "/user size: " << size << endl;
//        cerr << "/user used: " << used << endl;
        double limit = size*.9 - dow_logger.max_size;
//        cerr << "/user limit: " << limit << endl;
        used_percent = stringToInt(vals[4].substr(0, vals[4].size()-1));
//        cerr << "/user used percent: " << used_percent << endl;
        if (used < limit) {
//            cerr << "Filesystem is not full" << endl;
            break;
        }
        string oldest_file = backtick("ls /user/dowlog | head -1");
        if (oldest_file == "") {
//           cerr << "Filesystem too full - nothing to remove" << endl;
           break;
        }
//        cerr << "Filesystem too full, removing " << oldest_file << endl;
        oldest_file = "/user/dowlog/"+oldest_file;
        unlink(oldest_file.c_str());
    }
}

double toBytes(string val) {
    double retval = 0.;
//    string last = val.substr(val.size()-1);
//    if (last == "G") {
//       retval = stringToDouble(val.substr(0,val.size()-2)) * 1073741824.;
//    }
//    if (last == "M") {
//       retval = stringToDouble(val.substr(0,val.size()-2)) * 1048576.;
//    }
//    else if (last == "K") {
//       retval = stringToDouble(val.substr(0,val.size()-2)) * 1024.;
//    }
//    else {
//       retval = stringToDouble(val);
//    }
    retval = stringToDouble(val) * 1024;
    return retval;
}

void sendAck(int cmd_id, int datlen, Byte *data, bool sign) {
    Byte msg_buf[16];
    Byte resp_buf[64];
    msg_buf[0] = 0x00;
    msg_buf[1] = cmd_id;
    int len = 2;
    for (int i=0; i < datlen; i++) {
        msg_buf[len++] = data[i];
    }
    Byte *buf = msg_buf;
    if (sign) {
        len = VIC::buildSignedMesg(len, msg_buf, resp_buf);
        buf = resp_buf;
    }
    sendPacket(buf, len);
}

void sendNak(int reason, Byte *recv_buf, int len) {
    Byte buf[16];
    buf[0] = 0x00;
    buf[1] = 0xFF;
    buf[2] = reason;
    for (int i=0; i < len; i++) {
        buf[i+3] = recv_buf[i];
    }
    sendPacket(buf, len+3);
}

void sendPacket(Byte *packet, int len) {
    Byte frame[1024];
    int f_len = formatFrame(frame, packet, len);
    if (debug) {
        cerr << hiresDateTime() << " ";
        fprintf(stderr,"Sent packet:");
        if (loader_called) {
            for (int j=0; j < 3; j++) {
                fprintf(stderr," %02X", packet[j]);
            }
        }
        else {
            for (int j=0; j < len; j++) {
                fprintf(stderr," %02X", packet[j]);
            }
        }
        fprintf(stderr,"\n");
    }
    write(ser_s, frame, f_len);
//    if (debug) {
//        cerr << getDateTime() << " After write" << endl;
//    }
}

int formatFrame(Byte *frame, Byte *packet, int len) {
    Byte chksum = calc_chksum(packet, len);
    packet[len] = 0xFF - (chksum^0xAA);
    len++;
    int f_len = 0;
    frame[f_len++] = F_SOF;
    for (int i=0; i < len; i++) {
        Byte b = packet[i];
        if ((b == F_SOF) or (b == F_DLE) or (b == F_EOF)) {
            frame[f_len++] = F_DLE;
            b ^= 0x20;
        }
        frame[f_len++] = b;
    }
    frame[f_len++] = F_EOF;
    return f_len;
}

Byte calc_chksum(Byte *packet, int len) {
    Byte chksum = 0;
    for (int i=0; i < len; i++) {
        chksum += packet[i] ^ 0xAA;
    }
    return chksum;
}

void checkSystemHealth() {
}

void closeAndExit(int exitstat) {
    closePort();
    Connections::iterator ci;
    for (ci=connections.begin(); ci != connections.end(); ci++) {
         int rs = (*ci).first;
         close(rs);
    }
    if (listen_s >= 0) {
        close(listen_s);
    }
    remove(VicTalkerPath);
    exit(exitstat);
}

void openPort(string port) {
    cerr << getDateTime() << " ";
    cerr << "Opening " << port << endl;
    string dev = "/dev/" + port;
    if (fileExists(dev)) {
        if (!lock(port)) {
            cerr << "Serial device '" << port << "' already in use" << endl;
            closeAndExit(1);
        }
        ser_s = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (ser_s < 0) {
            perror(dev.c_str());
            closeAndExit(-1);
        }
//        struct serial_struct serial;
//        ioctl(ser_s, TIOCGSERIAL, &serial);
//        cerr << "xmit_fifo_size:" << serial.xmit_fifo_size << endl;
//        cerr << "recv_fifo_size:" << serial.recv_fifo_size << endl;
        if (tcgetattr(ser_s,&oldtio)) {
            cerr << "Failed to get old port settings" << endl;
        }
    
        // set new port settings for canonical input processing 
        struct termios newtio;
        newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
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
}
    
void closePort() {
    if (ser_s >= 0) {
        // restore old port settings
        tcsetattr(ser_s,TCSANOW,&oldtio);
        tcflush(ser_s, TCIOFLUSH);
        close(ser_s);        //close the com port
    }
    if (lockfile != "") {
        remove(lockfile.c_str());
    }
}

bool lock(string dev) {
    bool locked = false;
    string lockfile_tmp = "/var/lock/LCK.." + dev;
    int fd = open(lockfile_tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0);
    if (fd >= 0) {
        close(fd);
        locked = true;
        lockfile = lockfile_tmp;
    }
    return locked;
}

int listenSocket(const char *name) {
    struct sockaddr_un a;
    int s;
    int yes;
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }
    yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
           (char *)&yes, sizeof(yes)) < 0) {
        perror("setsockopt");
        close(s);
        return -1;
    }
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strcpy(a.sun_path, name);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        perror("bind");
        close(s);
        return -1;
    }
    chmod(name, S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP);
    listen(s, 10);
    return s;
}

void onTerm(int parm) {
    cerr << "Exiting on signal " << parm << endl;
    closeAndExit(0);
}

void onUsr1(int parm) {
    // Simulate crash
    // Send notification
    unsigned int airbags = 0x8000; // Non-specific airbag depoloyed
    unsigned int impacts = 0x0500; // More severe front event, front pretensioner event
    int orientation = 0x00; // Normal
    unsigned int speed = (int)(88.5 * 128);
    int seats_occupied = 0xff; // SNA
    int seatbelts = 0x01; // Driver seatbelt
    Byte buffer[64];
    buffer[0] = 0x00;
    buffer[1] = 0xF0;
    buffer[2] = (Byte)(airbags >> 8);
    buffer[3] = (Byte)(airbags & 0xFF);
    buffer[4] = (Byte)(impacts >> 8);
    buffer[5] = (Byte)(impacts & 0xFF);
    buffer[6] = orientation;
    buffer[7] = (Byte)(speed >> 8);
    buffer[8] = (Byte)(speed & 0xFF);
    buffer[9] = seats_occupied;
    buffer[10] = seatbelts;
    int len = 11;
    if (startEcall(len, buffer)) {
        sendNotifications(0xF0, len, buffer);
        simulate_ecall = true;
    }
    signal(SIGUSR1, onUsr1);
}

void onAlrm(int parm) {
    // Do nothing now, just used to break out of blocking reads
    signal(SIGALRM, onAlrm);
}

void checkIgnChange(bool new_state) {
    if (new_state != ign_state) {
        ign_state = new_state;
        unsigned long uptime = getUptime();
        if ((uptime-ign_change_time) < 5) {
            ign_change_count++;
        }
        else {
            ign_change_count = 0;
        }
        cout << getDateTime() << " Ign change:" << ign_state << " count:" << ign_change_count << endl;
        ign_change_time = uptime;
    }
}

bool startEcall(int len, Byte *buffer) {
    bool started = false;
    time_t last_ecall = fileTime(LastEcallFile);
    if ((time(NULL) - last_ecall) > 12*3600) {
        unsigned int airbags =  ((unsigned int)(buffer[2])<<8) | buffer[3];
        unsigned int impacts = ((unsigned int)(buffer[4])<<8) | buffer[5];
        int orientation = buffer[6];
        unsigned int speed = ((unsigned int)(buffer[7])<<8) | buffer[8];
        int seats_occupied = buffer[9];
        int seatbelts = buffer[10];
        string cmnd = "eCall";
        cmnd += " " + intToString(airbags);
        cmnd += " " + intToString(impacts);
        cmnd += " " + intToString(orientation);
        cmnd += " " + intToString(speed);
        cmnd += " " + intToString(seats_occupied);
        cmnd += " " + intToString(seatbelts);
        cmnd += " >/dev/null 2>&1 </dev/null &";
        system(cmnd.c_str());
        touchFile(LastEcallFile);
        started = true;
    }
    return started;
}

void sendEmergencyInfo(int appid) {
    unsigned int airbags = 0x8000; // Non-specific airbag depoloyed
    unsigned int impacts = 0x0500; // More severe front event, front pretensioner event
    int orientation = 0x00; // Normal
    unsigned int speed = (int)(88.5 * 128);
    int seats_occupied = 0xff; // SNA
    int seatbelts = 0x01; // Driver seatbelt
    Byte buffer[64];
    buffer[0] = appid;
    buffer[1] = 0x2A;
    buffer[2] = (Byte)(airbags >> 8);
    buffer[3] = (Byte)(airbags & 0xFF);
    buffer[4] = (Byte)(impacts >> 8);
    buffer[5] = (Byte)(impacts & 0xFF);
    buffer[6] = orientation;
    buffer[9] = seats_occupied;
    buffer[10] = seatbelts;
    sendResponse(11, buffer);
    sendNextCommand();
}

string intToString(int i) {
    stringstream out;
    out << i;
    return out.str();
}

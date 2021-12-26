/******************************************************************************
 * File: unitcomm.cpp
 *
 * Daemon that communicates with servercomm
 * Also, performs car control commands
 *
 ******************************************************************************
 * Change log:
 * 2012/07/27 rwp
 *    Change "get position" to send location timestamp
 * 2012/08/08 rwp
 *    Changed to remove "-tm" from server name if not nphase network
 *    Changed to user network_utils.h
 * 2012/09/12 rwp
 *    Don't run stored commands if we woke up on CAN
 * 2012/10/16 rwp
 *    Freed packet allocation
 *    Added zombie cleanup
 * 2013/02/01 rwp
 *    Fixed command killing
 * 2013/03/04 rwp
 *    Added appending "*" to swversion if TOC does not match backup TOC
 * 2013/04/03 rwp
 *    Fixed getParm to return pointer to actual parm (HT-24)
 * 2013/05/09 rwp
 *    Modified to get WAN device from hardare config file
 * 2013/05/10 rwp
 *    Fixed using -tm- name for verify
 * 2013/05/17 rwp
 *    Added cell_ping_interval to keep connection alive
 *    Changed nphase pattern to be "10."
 * 2013/06/14 rwp
 *    Changed to use vic.getVersion
 *    CA-151: Changed to use vehicle alarm status if lock status unknown
 * 2013/07/19 rwp
 *    AUT-51: Increased delay for panic control
 * 2013/08/28 rwp
 *    AUT-73: Try to quickly connect to primary server every day
 * 2013/09/04 rwp
 *    AUT-76: Touch RemoteStartedFile
 * 2013/12/12 rwp
 *    AUT-98: Don't kill commands if rebooting
 * 2013/12/15 rwp
 *    AUT-76: Handle VIC status notifications for remote start inhibited
 * 2013/03/31 rwp
 *    AUT-121: Change server name "-ext" to "-pvt" if on nphase network
 * 2013/06/05 rwp
 *    AUT-144: Don't backup_autonet after features saved
 * 2013/11/21 rwp
 *    AK-51: Send engine start inhibited cause if engine is already on
 * 2013/02/04 rwp
 *    AK-75: Don't restart conection process if already in STARTUP
 * 2015/05/11 rwp
 *    AK-111: Set the socket to allow fragmentations (IP_PMTUDISC_DONT)
 * 2015/05/27 rwp
 *    Change nphase network for "verizon" regardless of position/case
 * 2016/06/12 rwp
 *    Change max open retry count to 4
 * 2018/04/30 rwp
 *    Modified to work with vehicle abstraction
 ******************************************************************************
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include "readconfig.h"
#include "tail.h"
#include "unitcomm.h"
#include "autonet_files.h"
#include "autonet_types.h"
#include "dateTime.h"
#include "backtick.h"
#include "eventLog.h"
#include "fileStats.h"
#include "fileExists.h"
#include "my_features.h"
#include "getParm.h"
#include "getPids.h"
#include "getStringFromFile.h"
#include "getToken.h"
#include "grepFile.h"
#include "logError.h"
#include "network_utils.h"
#include "readBinaryFile.h"
#include "readAssoc.h"
#include "remote_start_reasons.h"
#include "split.h"
#include "ssl_keys.h"
#include "string_printf.h"
#include "string_convert.h"
#include "str_system.h"
#include "touchFile.h"
#include "updateFile.h"
#include "va_helper.h"
#include "writeAssoc.h"
#include "writeStringToFile.h"
#include "writeBinaryFile.h"

using namespace std;

typedef int Socket;
Config config(ConfigFile);
vector<int> childs;

int app_id = 2;
bool nphase_network = true;

#define CloseDelay 5

#define COMM_VERSION 1

#define OPEN_REQ      1
#define OPEN          2
#define OPEN_ACK      3
#define DATA          4
#define ACK           5
#define CLOSE         6
#define CLOSE_ACK     7
#define COMMAND       8
#define COMMAND_ACK   9
#define REDIRECT     10
#define RESTART      11
#define PING         12
#define PING_ACK     13
#define CONFIG       14
#define CONFIG_ACK   15

#define Hostname                1
#define Event                   2
#define Seq                     3
#define Coldstart               4
#define Command                 5
#define CommandId               6
#define CommandData             7
#define CommandKill             8
#define Version                 9
#define Pubkey                 10
#define RedirectServer         11
#define Disable                12
#define Sig                    13
#define CommandUser            14
#define RateLimit              15
#define CommandSummary         16
#define Geofences              17
#define PositionReportInterval 18
#define NewCommandData         19
#define CommVersion            20
#define Features               21
/*
bool wanUp();
bool defaultRoute();
void handleConnections();
void closeAndExit(int exitstat);
string getSwVersion();
int findConnection(string ip, int port);
int findCommand(string ip, int port, int cmndid);
string getIpAddress(string name);
Socket readerSocket(const char *name);
void killAllCommands();
void killCommandProcs(int pid);
void on_hup(int sig);
void on_int(int sig);
void on_usr1(int sig);
void on_child(int sig);
*/
string parmtypes[] = {
    "", "Hostname", "Event", "Seq", "Coldstart", "Command", "CommandId", "CommandData", "CommandKill",
    "Version", "Pubkey", "RedirectServer", "Disable", "Sig", "CommandUser", "RateLimit", "CommandSummary",
    "Geofences", "PositionReportInterval", "NewCommandData", "CommVersion", "Features"
};

string frametypes[] = {
    "" , "OpenReq", "Open", "OpenAck", "Data", "Ack", "Close", "CloseAck",
    "Command", "CommandAck", "Redirect", "Restart", "Ping", "PingAck",
    "Config", "ConfigAck"
};

string unitid;

class CarState {
  public:
    int locks;
    int raw_locks;
    bool engine;
    bool ignition;
    int doors;
    int raw_doors;
    bool panic;
    bool emergency;
    bool moving;
    string abort_reason;

    CarState() {
        locks = 0;
        raw_locks = 0;
        engine = false;
        ignition = false;
        doors = 0;
        panic = false;
        emergency = false;
        moving = false;
        abort_reason = "";
    }

    void decodeStatus(string resp) {
        locks = 0;
        raw_locks = 0;
        engine = false;
        ignition = false;
        doors = 0;
        panic = false;
        emergency = false;
        moving = false;
        string locks_str = getParm(resp, "locks:", ",");
        string doors_str = getParm(resp, "doors:", ",");
        string trunk_str = getParm(resp, "trunk:", ",");
        string engine_str = getParm(resp, "engine:", ",");
        string ignition_str = getParm(resp, "ignition:", ",");
        string speed_str = getParm(resp, "speed:", ",");
        string panic_str = getParm(resp, "panic:", ",");
        abort_reason = getParm(resp, "abort_reason:", ",");
        ignition = ignition_str == "on";
        engine = (engine_str != "") and (engine_str != "off");
        panic = panic_str == "on";
        float kph = 0;
        kph = stringToDouble(speed_str);
        moving = kph > 8.;
        if (doors_str != "") {
            for (int i=0; i < 4; i++) {
                if (doors_str[i*2] == 'o') {
                    doors |= (1 << i);
                }
            }
        }
        if (locks_str != "") {
            for (int i=0; i < 4; i++) {
                if (locks_str[i*2] == 'l') {
                    raw_locks |= (1 << i);
                }
            }
        }
        if (trunk_str == "o") {
            doors != 0x10;
        }
        if (raw_locks == 0x0F) {
            // All locked
            locks = 1;
        }
        else if (raw_locks == 0x0E) {
            // Driver unlocked
            locks = 3;
        }
        else {
            // Something else is unlocked
            locks = 2;
        }
    }

    string getState(string dev) {
        string state = "unknown";
        if (dev == "engine") {
            state = "off";
            if (engine) {
                state = "on";
            }
        }
        else if (dev == "locks") {
            state = "unknown";
            if (locks == 1) {
                state = "locked";
            }
            else if (locks == 2) {
                state = "unlocked";
            }
            else if (locks == 3) {
                state = "driver_unlocked";
            }
        }
        else if (dev == "trunk") {
            state = "unknown";
            if (doors != 0xFF) {
                state = "closed";
                if (doors & 0x10) {
                    state = "opened";
                }
            }
        }
        else if (dev == "leftdoor") {
            state = "unknown";
            if (doors != 0xFF) {
                state = "closed";
                if (doors & 0x04) {
                    state = "opened";
                }
            }
        }
        else if (dev == "rightdoor") {
            state = "unknown";
            if (doors != 0xFF) {
                state = "closed";
                if (doors & 0x08) {
                    state = "opened";
                }
            }
        }
        else if (dev == "panic") {
            state = "off";
            if (panic) {
                state = "on";
            }
        }
        else if (dev == "doors") {
            state = toString(doors) + "/" + toString(raw_locks);
        }
           
        return state;
    }

    string outputState(string dev) {
        return dev + " " + getState(dev);
    }
};

class CommandInfo {
  private:
    char *_argv0_p;
    string va_status_str = "ignition,engine,locks,doors,trunk,panic,speed,abort_reason";
    VaHelper va;

    void _car_control(string command) {
        CarState carState;
        _argv0_p = NULL;
        Strings args = split(command, " ");
        string dev = args[1];
        string act = args[2];
        bool queued_command = false;
        if (args.size() > 3) {
            if (args[3] == "queued") {
                queued_command = true;
            }
        }
        va.createSocket("cc", false);
        string status_str = va.sendQuery(va_status_str);
        carState.decodeStatus(status_str);
        if (dev == "get") {
            _performGet(act, carState);
        }
        else {
            if ((dev == "engine") and (act == "start")) {
                act = "on";
            }
            if (carState.moving) {
                cout << "Error: Cannot execute while vehicle is moving" << endl;
            }
            else if (queued_command and fileExists(WokeOnCanFile)) {
                cout << "Error: Queued command not executed" << endl;
            }
            else if ( (dev == "engine") and
                      ((act == "on") or (act == "start")) and
                      (carState.engine) ) {
                cout << "Error: Engine on failed reason:ignition_in_run_start" << endl;
            }
            else {
//                if (lock_before_start and
//                    (dev == "engine") and (act == "on")) {
//                     _performAction("locks", "lock", carState);
//                }
                _performAction(dev, act, carState);
            }
        }
        cout << "<<EOF>>" << endl;
    }

    void _performGet(string act, CarState carState) {
        if (act == "position") {
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
            else {
                cout << "Error: Location Service disabled" << endl;
            }
        }
        else if (act == "status") {
            _sendStatus(carState);
        }
        updateFile(CarControlCommandFile, "get "+act);
    }

    void _performAction(string dev_s, string act_s, CarState carState) {
        int dev_wait = 1;
        bool error = false;
        string expected_state = "";
        if (dev_s == "locks") {
            if (act_s == "lock") {
                expected_state = "locked";
            }
            else if (act_s == "unlock") {
                expected_state = "unlocked";
            }
            else if (act_s == "unlock_driver") {
                expected_state = "driver_unlocked";
            }
            else {
                error = true;
            }
        }
        else if (dev_s == "engine") {
            if (act_s == "on") {
                if (carState.engine) {
                    act_s = "";
                }
                else {
                    dev_wait = 20;
                }
                expected_state = "on";
            }
            else if (act_s == "off") {
                if (!carState.engine) {
                    act_s = "";
                }
                else {
                    dev_wait = 3;
                }
                expected_state = "off";
            }
            else {
                error = true;
            }
        }
        else if (dev_s == "trunk") {
            if (act_s == "open") {
                dev_wait = 4;
                expected_state = "opened";
            }
            else if (act_s == "close") {
                dev_wait = 20;
                expected_state = "closed";
            }
            else {
                error = true;
            }
        }
        else if (dev_s == "leftdoor") {
            if (act_s == "open") {
                dev_wait = 4;
                expected_state = "opened";
            }
            else if (act_s == "close") {
                dev_wait = 10;
                expected_state = "closed";
            }
            else {
                error = true;
            }
        }
        else if (dev_s == "rightdoor") {
            if (act_s == "open") {
                dev_wait = 4;
                expected_state = "opened";
            }
            else if (act_s == "close") {
                dev_wait = 10;
                expected_state = "closed";
            }
            else {
                error = true;
            }
        }
        else if (dev_s == "panic") {
            dev_wait = 5;
            if (act_s == "on") {
                if (carState.engine) {
                    cout << "Error: Cannot turn on panic with engine running" << endl;
                    act_s = "";
                }
                else {
                    expected_state = "on";
                }
            }
            else if (act_s == "off") {
                expected_state = "off";
            }
            else {
                error = true;
            }
        }
        else {
            error = true;
        }
        if (error) {
            cout << "Error: Invalid device or action" << endl;
        }
        else {
            updateFile(CarControlCommandFile, dev_s+" "+act_s);
            string state = "unknown";
            if (act_s != "") {
                string ve_command = dev_s + " " + act_s;
                va.vah_send(ve_command);
                for (int i=0; i < dev_wait; i++) {
                    sleep(1);
                    string resp = va.sendQuery(va_status_str);
                    carState.decodeStatus(resp);
                    state = carState.getState(dev_s);
                    if (state == expected_state) {
                        break;
                    }
                    if ( (dev_s == "engine") and (act_s == "on") and
                         (carState.abort_reason != "") ) {
                        break;
                    }
                }
                if (dev_s == "engine") {
                    if (act_s == "off") {
                        if (state == "off") {
                            unlink(RemoteStartedFile);
                        }
                    }
                    else {
                        if (state == "on") {
                            touchFile(RemoteStartedFile);
                        }
                    }
                }
            }
            state = carState.getState(dev_s);
            if (state != expected_state) {
                if ((dev_s == "engine") and (act_s == "on")) {
                    if (carState.abort_reason != "") {
                        cout << "Error: Engine on failed reason:" << carState.abort_reason << endl;
                    }
                    else {
                        cout << "Error: Engine on failed" << endl;
                    }
                }
            }
            _sendStatus(carState);
        }
    }

    void _sendStatus(CarState carState) {
        cout << carState.outputState("engine") << endl;
        cout << carState.outputState("locks") << endl;
        cout << carState.outputState("trunk") << endl;
        cout << carState.outputState("leftdoor") << endl;
        cout << carState.outputState("rightdoor") << endl;
        cout << carState.outputState("panic") << endl;
        cout << carState.outputState("doors") << endl;
    }

  public:
    string command;
    int id;
    string server_ip;
    int server_port;
    int pid;
    unsigned long exited_time;
    bool closed;
    Tail tail;
    string cmndfile;
    string outfile;
    bool killing;
    string user;
    string summary;

    CommandInfo(int cmndid, string cmnd, string ip, int port) {
        command = cmnd;
        id = cmndid;
        server_ip = ip;
        server_port = port;
        pid = 0;
        exited_time = 0;
        closed = false;
        cmndfile = "";
        outfile = "";
        killing = false;
        user = "";
        summary = "";
    }

    void setArgv0(char *argv0_p) {
        _argv0_p = argv0_p;
    }

    void execute() {
        string basefile = CommandsDir;
        basefile += "cmnd-" + server_ip;
        basefile += "." + toString(server_port);
        basefile += "-" + toString(id);
        cmndfile = basefile + ".sh";
        outfile = basefile + ".out";
        writeStringToFile(outfile, "");
        string cmndstr = cmndfile;
        logError("executing '"+cmndstr+"'");
        logError("command:"+command);
        pid_t child_pid = fork();
        if (child_pid < 0) {
            logError("Fork failed, child_pid="+toString(child_pid));
        }
        else if (child_pid) {
            pid = child_pid;
            childs.push_back(pid);
        }
        else {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTERM, SIG_DFL);
            signal(SIGHUP, SIG_DFL);
            signal(SIGUSR1, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);
            if (_argv0_p != NULL) {
                int arg0_len = strlen(_argv0_p);
                strncpy(_argv0_p, "unitcomm_run_cmnd", arg0_len);
            }
            int mypid = getpid();
            string cmndstr = "";
            cmndstr += "#!/bin/sh\n";
            cmndstr += "#PID=" + toString(mypid) + "\n";
            cmndstr += command + "\n";
            cmndstr += "echo '<<EOF>>'";
            writeStringToFile(cmndfile, cmndstr);
            int xmode = S_IRWXU | S_IRGRP | S_IROTH;
            chmod(cmndfile.c_str(), xmode);
            int type = O_CREAT | O_WRONLY | O_TRUNC;
            int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
            int fd = open(outfile.c_str(), type, mode);
            if (fd >= 0) {
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
                close(0);
                EventLog log;
                string event = command;
                if (summary != "") {
                    event = summary;
                }
                size_t pos;
                while ((pos=event.find_first_of("\n'\"")) != string::npos) {
                     event.erase(pos,1);
                }
                string c_user = "none";
                if (user != "") {
                    c_user = user;
                }
                event = "Exec user:" + c_user + " " + event;
                log.append(event);
                log.closeLog();
                if (!fileExists(DemoModeFile) and (strncmp(command.c_str(), "car_control ", 12) == 0)) {
                    _car_control(command);
                }
                else {
                    execl(cmndfile.c_str(), cmndfile.c_str(), NULL);
                }
            }
            exit(1);
        }
        tail.open(outfile);
    }

    bool getLine(string &line) {
        return tail.read(line);
    }

    void closeCommand() {
        tail.close();
        unlink(cmndfile.c_str());
        unlink(outfile.c_str());
        closed = true;
//        int stat = 0;
//        waitpid(pid, &stat, 0);
    }

    bool match(string ip, int port, int cmndid) {
        bool retval = false;
        if ( (server_ip == ip) and
             (server_port == port) and
             (id == cmndid) ) {
            retval = true;
        }
        return retval;
    }
};
vector<CommandInfo> commands;

class Packet {
  private:
    unsigned char *_buffer;
    int _len;

  public:
    Packet() {
        _len = 0;
        _buffer = new unsigned char[32000];
    }

    ~Packet() {
        delete _buffer;
    }

    int length() {
        return _len;
    }

    unsigned char * data() {
        return _buffer;
    }

    void addByte(int val) {
        _buffer[_len++] = val & 0xFF;
    }

    void addWord(int val) {
        addByte(val>>8);
        addByte(val);
    }

    void addLong(long val) {
        addWord(val>>16);
        addWord(val);
    }

    void addString(string str) {
        const char *chars = str.c_str();
        for (int i=0; i < str.size(); i++) {
            addByte(*chars++);
        }
    }

    void hexDump() {
        cout << "Buffer len: " << _len << endl;
        for (int i=0; i < _len; i++) {
            cout << setw(2) << setfill('0') << hex << (int)_buffer[i] << " ";
        }
        cout << dec << endl;
    }
};

class FrameParm {
  private:
    int _type;
    int _numval;
    string _strval;

  public:
    FrameParm(int type) {
        _type = type;
    }

    FrameParm(int type, int val) {
        _type = type;
        _numval = val;
    }

    FrameParm(int type, string str) {
        _type = type;
        _strval = str;
    }

    int type() {
        return _type;
    }

    int getnum() {
        return _numval;
    }

    string getstr() {
        return _strval;
    }
};

class Frame {
  private:
    vector<FrameParm> _parms;
    int _type;
    unsigned char *_packet;
    int _verify_len;

    int _getword(unsigned char *p) {
        int val = 0;
        val = (*p++) << 8;
        val |= *p++;
        return val;
    }

    int _getlong(unsigned char *p) {
        int val = 0;
        val = _getword(p) << 16;
        val |= _getword(p+2);
        return val;
    }

  public:
    Frame() {
    }

    Frame(int type) {
        _type = type;
    }

    Frame(int pktlen, unsigned char *buffer) {
        _packet = buffer;
        _verify_len = 0;
        int packet_len = pktlen;
        unsigned char *p = buffer;
        _type = *p++;
        pktlen--;
        bool error = false;
        while (pktlen >= 4) {
            int parmtype = _getword(p);
            p += 2;
            pktlen -= 2;
            int parmlen = _getword(p);
            p +=2;
            pktlen -= 2;
            if (pktlen < parmlen) {
                error = true;
                break;
            }
            string str;
            int val;
//            cout << parmtype << " " << parmlen << " ";
//            for (int i=0; i < parmlen; i++) {
//                cout << setw(2) << setfill('0') << hex << (int)p[i];
//            }
//            cout << endl;
            switch (parmtype) {
              case Hostname:
              case Event:
              case Version:
              case Command:
              case CommandUser:
              case CommandSummary:
              case Pubkey:
              case RedirectServer:
              case RateLimit:
              case Geofences:
              case NewCommandData:
              case Features:
                str = (char *)p;
                str = str.substr(0, parmlen);
                addParm(parmtype, str);
//                cout << "Adding: " << parmtypes[parmtype] << "=" << str << endl;
                break;
              case Sig:
                _verify_len = packet_len - (pktlen + 4);
                str.assign((char *)p, parmlen);
                addParm(parmtype, str);
//                cout << "Adding: " << parmtypes[parmtype] << "=<binary data>" << endl;
                break;
              case Seq:
                val = _getlong(p);
                addParm(parmtype, val);
//                cout << "Adding: " << parmtypes[parmtype] << "=" << val << endl;
                break;
              case CommandId:
              case PositionReportInterval:
              case CommVersion:
                val = _getword(p);
                addParm(parmtype, val);
//                cout << "Adding: " << parmtypes[parmtype] << "=" << val << endl;
                break;
              case Coldstart:
              case Disable:
              case CommandKill:
                addParm(parmtype);
//                cout << "Adding: " << parmtypes[parmtype] << endl;
                break;
              default:
                logError("Invalid parmaeter type: " + parmtype);
                break;
            }
            p += parmlen;
            pktlen -= parmlen;
        }
        if (pktlen > 0) {
            error = true;
            logError("Packet too short");
        }
    }

    bool verify(string server) {
        bool verified = false;
        if (getParm(Sig)) {
            writeBinaryFile("/tmp/recv_frame.dat", (char *)_packet, _verify_len);
            FrameParm *sigparm = getParm(Sig);
            string sig = sigparm->getstr();
            writeBinaryFile("/tmp/recv_frame.sig", sig);
            string serverkeyfile = "/tmp/" + server + ".pubkey";
            string openssl_cmd = "openssl dgst -sha1 -verify ";
            openssl_cmd += serverkeyfile;
            openssl_cmd += " -signature /tmp/recv_frame.sig /tmp/recv_frame.dat";
            openssl_cmd += " >/dev/null 2>&1";
            int stat = system(openssl_cmd.c_str());
            if (stat == 0) {
                verified = true;
            }
            else {
                logError("Packet not verified");
            }
            unlink("/tmp/recv_frame.sig");
            unlink("/tmp/recv_frame.dat");
        }
        return verified;
    }

    void addParm(FrameParm parm) {
        _parms.push_back(parm);
    }

    void addParm(int type) {
        addParm(FrameParm(type));
    }

    void addParm(int type, int val) {
        addParm(FrameParm(type, val));
    }

    void addParm(int type, string val) {
        addParm(FrameParm(type, val));
    }

    void replaceParm(FrameParm parm) {
        int type = parm.type();
        for (int i=0; i < _parms.size(); i++) {
            FrameParm parm1 = _parms.at(i);
            if (parm1.type() == type) {
                _parms[i] = parm;
                break;
            }
        }
    }

    void replaceParm(int type) {
        replaceParm(FrameParm(type));
    }

    void replaceParm(int type, int val) {
        replaceParm(FrameParm(type, val));
    }

    void replaceParm(int type, string val) {
        replaceParm(FrameParm(type, val));
    }

    FrameParm *getParm(int type, int cnt) {
        FrameParm *parm_p = NULL;
        int n = 0;
        for (int i=0; i < _parms.size(); i++) {
            FrameParm *p = &(_parms[i]);
            if (p->type() == type) {
                if (n == cnt) {
                    parm_p = p;
                    break;
                }
                n++;
            }
        }
        return parm_p;
    }

    FrameParm *getParm(int type) {
        FrameParm *parm_p = getParm(type, 0);
        return parm_p;
    }

    int type() {
       return _type;
    }

    Packet buildPacket() {
        Packet packet;
        string str;
        bool signit = false;
        packet.addByte(_type);
        for (int i=0; i < _parms.size(); i++) {
            FrameParm parm = _parms.at(i);
            int type = parm.type();
            if (type != Sig) {
                packet.addWord(type);
            }
            switch (type) {
              case Hostname:
              case Event:
              case Version:
              case Command:
              case CommandUser:
              case CommandSummary:
              case Pubkey:
              case RedirectServer:
              case RateLimit:
              case Geofences:
              case NewCommandData:
              case Features:
                str = parm.getstr();
                packet.addWord(str.size());
                packet.addString(str);
                break;
              case Sig:
                signit = true;
                break;
              case Seq:
                packet.addWord(4);
                packet.addLong(parm.getnum());
                break;
              case CommandId:
              case PositionReportInterval:
              case CommVersion:
                packet.addWord(2);
                packet.addWord(parm.getnum());
                break;
              case Coldstart:
              case Disable:
              case CommandKill:
                packet.addWord(0);
                break;
            }
        }
        if (signit) {
            writeBinaryFile("/tmp/send_frame.dat", (char *)packet.data(), packet.length());
            string openssl_cmd = "openssl dgst -sha1 -sign ";
            openssl_cmd += PrivateKeyFile;
            openssl_cmd += " -out /tmp/send_frame.sig /tmp/send_frame.dat";
            openssl_cmd += " >/dev/null 2>&1";
            int stat = system(openssl_cmd.c_str());
            if (stat == 0) {
                string sig = readBinaryFile("/tmp/send_frame.sig");
                packet.addWord(Sig);
                packet.addWord(sig.size());
                packet.addString(sig);
            }
            unlink("/tmp/send_frame.sig");
            unlink("/tmp/send_frame.dat");
        }
//        _parms.clear();
        return packet;
    }

    void dumpFrame() {
        for (int i=0; i < _parms.size(); i++) {
            FrameParm parm = _parms[i];
        }
    }
};

typedef int Socket;

#define STARTUP 1
#define OPENING 2
#define UP      3
#define CLOSING 4
#define DOWN    5

#define EventDataTimeout 2
#define CommandDataTimeout 2

class Connection {
  private:
    Socket _socket;
    string _servers[2];
    string _server_name;
    string _verify_server;
    int _num_servers;
    int _server_idx;
    string _dstip;
    int _dstport;
    struct sockaddr_in _to;
    int _state;
    bool _coldstart;
    unsigned long _recvseq;
    unsigned long _sendseq;
    bool _disable;
    string _swversion;
    string _toswversion;
    string _public_key;
    string _ratelimit;
    string _geofences;
    int _positionReportInterval;
    time_t _timerval;
    int _resend_counter;
    Frame _last_frame;
    bool _outstanding_packets;
    bool _reopenConnection;
    string _event;
    Socket _events_socket;
    bool _closed;
    bool _last_data_was_event;
    char *_argv0_p;
    int _ping_interval;
    bool _first_server;

    void _getServerIdx() {
        _server_idx = _readServerIdx();
    }

    int _readServerIdx() {
        int idx = 0;
        if (fileExists(ServerIdxFile)) {
            string serveridx_str = getStringFromFile(ServerIdxFile);
            sscanf(serveridx_str.c_str(), "%d", &idx);
            logError("read server_idx: " + serveridx_str);
        }
        return idx;
    }

    int _incServerIdx() {
        _server_idx++;
        if (_server_idx >= _num_servers) {
            _server_idx = 0;
        }
    }

    void _updateServerIdx() {
        if (_server_idx != _readServerIdx()) {
            if (_server_idx == 0) {
                remove(ServerIdxFile);
            }
            else {
                char intbuf[5];
                sprintf(intbuf, "%d", _server_idx);
                updateFile(ServerIdxFile, intbuf);
            }
        }
    }

    void _startupProcessFrame(Frame frame) {
        int type = frame.type();
        switch (type) {
          case OPEN:
            if (_processOpen(frame)) {
                //logError("Got open packet");
                _sendOpenAck();
                _state = UP;
                _timerval = 0;
                if (_ping_interval != 0) {
                    _timerval = time(NULL) + _ping_interval;
                }
//                if (frame.getParm(Disable)) {
//                    if (!fileExists(DisableFile)) {
//                        system("autonet_router disable");
//                    }
//                }
//                else {
//                    if (fileExists(DisableFile)) {
//                        system("autonet_router enable");
//                    }
//                }
                FrameParm *parm = frame.getParm(Features);
                if (parm != NULL) {
                    _saveFeatures(parm->getstr());
                }
                parm = frame.getParm(Version);
                if (parm != NULL) {
                    string to_sw_version = parm->getstr();
                    if (to_sw_version != _swversion) {
                        string upgrade_cmd = "upgrade ";
                        upgrade_cmd += to_sw_version;
                        upgrade_cmd += " &";
                        system(upgrade_cmd.c_str());
                    }
                }
                updateFile(CommStateFile, "Connected");
                if (_server_idx == 0) {
                    unlink(SecondaryConnectedFile);
                }
                else {
                    if (!fileExists(SecondaryConnectedFile)) {
                        touchFile(SecondaryConnectedFile);
                    }
                }
                _first_server = false;
                _updateServerIdx();
                if (_outstanding_packets) {
                    _last_frame.replaceParm(Seq, _sendseq);
                    //logError("Sending outstanding packet");
                    _send(_last_frame);
                    _timerval = time(NULL) + 2;
                    _resend_counter = 0;
                }
                else if (_event != "") {
                    //logError("Sending queued event");
                    _sendEvent(_event);
                    _event = "";
                }
            }
            break;
          case REDIRECT:
            if (_processRedirect(frame)) {
                _setServerName();
                _setServerIp();
                if (_dstip != "") {
                    _recvseq = random();
                    _sendOpenReq();
                }
                _resend_counter = 0;
                _timerval = time(NULL) + 2;
            }
            break;
        }
    }

    void _startupTimeout() {
//        int max_retrys = 5;
        int max_retrys = 4;
        if (_first_server) {
            max_retrys = 2;
        }
        if (_resend_counter < max_retrys) {
            if (_dstip == "") {
                _setServerIp();
            }
            if (_dstip != "") {
                logError("Sending OpenReq");
                _recvseq = random();
                _sendOpenReq();
            }
            _resend_counter++;
            int backoff = 2 << _resend_counter;
            _timerval = time(NULL) + backoff;
        }
        else {
            if (_first_server) {
                _getServerIdx();
            }
            else {
                _incServerIdx();
            }
            _setServerName();
            _setServerIp();
            if (_dstip != "") {
                _recvseq = random();
                _sendOpenReq();
            }
            _resend_counter = 0;
            _timerval = time(NULL) + 2;
            _first_server = false;
        }
    }

    void _openingProcessFrame(Frame frame) {
        int type = frame.type();
        switch (type) {
          case OPEN_REQ:
            if (_processOpenReq(frame)) {
                _sendOpen();
            }
            break;
          case OPEN_ACK:
            //cerr << "Got OpenAck" << endl;
            if (_processOpenAck(frame)) {
               _state = UP;
            }
            break;
        }
    }

    void _openingTimeout() {
    }

    void _upProcessFrame(Frame frame) {
        int type = frame.type();
        switch (type) {
          case ACK:
            if (_processAck(frame)) {
                _sendseq = _incSeq(_sendseq);
                _outstanding_packets = false;
                _timerval = 0;
                if (_last_data_was_event) {
                    //logError("Got event ack");
                    send(_events_socket, "\n", 1, 0);
                    _last_data_was_event = false;
                }
                if (_event != "") {
                    //logError("Sending queued event");
                    _sendEvent(_event);
                    _event = "";
                }
                else {
                    if (_ping_interval != 0) {
                        _timerval = time(NULL) + _ping_interval;
                    }
                }
            }
            break;
          case COMMAND:
            if (_processCommand(frame)) {
               // Nothing to do, all is done in _processCommand
            }
            break;
          case COMMAND_ACK:
            break;
          case RESTART:
            if (_processRestart(frame)) {
                _restartConnection();
            }
            break;
          case CLOSE:
            if (_processClose(frame)) {
                _sendCloseAck();
                _state = DOWN;
            }
          case PING:
            _sendPingAck();
            break;
          case CONFIG:
            break;
          case CONFIG_ACK:
            break;
        }
    }

    void _restartConnection() {
        if (_state != STARTUP) {
            _recvseq = random();
            _sendOpenReq();
            _state = STARTUP;
            _resend_counter = 0;
            _timerval = time(NULL) + 2;
        }
    }

    void _upTimeout() {
        if (_outstanding_packets) {
           if (_resend_counter < 5) {
//               _last_frame.addParm(Seq, _sendseq);
               _send(_last_frame);
               _resend_counter++;
               int backoff = 2 << _resend_counter;
               _timerval = time(NULL) + backoff;
           }
           else {
               if (_reopenConnection) {
                   logError("Sending data failed...reopening connection");
                   updateFile(CommStateFile, "Connecting");
                   _restartConnection();
               }
               else {
                   logError("Sending data failed...closing connection");
                   _closeConnection();
               }
           }
        }
        else if (_ping_interval != 0) {
            _sendPing();
            _timerval = time(NULL) + _ping_interval;
        }
    }

    void _closingProcessFrame(Frame frame) {
        int type = frame.type();
        switch (type) {
          case CLOSE_ACK:
            _closed = true;
            _state = DOWN;
            _timerval = 0;
            break;
        }
    }

    void _closingTimeout() {
        if (_resend_counter < 2) {
            _sendClose();
            _timerval = time(NULL) + 5;
            _resend_counter++;
        }
        else {
            _state = DOWN;
            _timerval = 0;
            _closed = true;
        }
    }

    void _downProcessFrame(Frame frame) {
        int type = frame.type();
        switch (type) {
        }
    }

    void _downTimeout() {
    }

    bool _processOpen(Frame frame) {
        bool ok = false;
        if (!frame.verify(_verify_server)) return false;
        FrameParm *parm = frame.getParm(Seq);
        if (parm) {
            _sendseq = _incSeq(parm->getnum());
            ok = true;
        }
        return ok;
    }

    bool _processOpenReq(Frame frame) {
        bool ok = false;
        if (!frame.verify(_verify_server)) return false;
        FrameParm *parm = frame.getParm(Seq);
        if (parm) {
            _sendseq = _incSeq(parm->getnum());
            _recvseq = random();
            ok = true;
        }
        return ok;
    }

    bool _processOpenAck(Frame frame) {
        bool ok = false;
//        if (!frame.verify(_verify_server)) return false;
        ok = true;
        return ok;
    }

    void _sendOpenReq() {
        Frame frame(OPEN_REQ);
        if (_coldstart) {
            frame.addParm(Coldstart);
        }
        frame.addParm(Hostname,unitid);
        frame.addParm(Seq,_recvseq);
        frame.addParm(CommVersion,COMM_VERSION);
        if (_swversion != "") {
            frame.addParm(Version, _swversion);
        }
        if (_public_key != "") {
            frame.addParm(Pubkey, _public_key);
        }
        frame.addParm(Sig, "");
        _send(frame);
    }

    void _sendOpen() {
        Frame frame(OPEN);
        if (_coldstart) {
            frame.addParm(Coldstart);
        }
        frame.addParm(Hostname,unitid);
        frame.addParm(Seq, _recvseq);
//        if (_disable) {
//            frame.addParm(Disable);
//        }
//        if (_toswversion != "") {
//            frame.addParm(Version, _toswversion);
//        }
//        if (_ratelimit != "") {
//            frame.addParm(RateLimit, _ratelimit);
//        }
//        if (_geofences != "") {
//            frame.addParm(Geofences, _geofences);
//        }
//        frame.addParm(PositionReportInterval, _positionReportInterval);
        frame.addParm(Sig, "");
        //cerr << "Sending Open" << endl;
        _send(frame);
    }

    void _sendOpenAck() {
        Frame frame(OPEN_ACK);
        if (_coldstart) {
            frame.addParm(Coldstart);
            if (_swversion != "") {
                 frame.addParm(Version, _swversion);
            }
        }
        _send(frame);
        _timerval = 0;
        _state = UP;
    }

    bool _processRedirect(Frame &frame) {
        bool ok = false;
        if (!frame.verify(_verify_server)) return false;
        FrameParm *parm = frame.getParm(RedirectServer, 0);
        if (parm) {
            _servers[0] = parm->getstr();
            _num_servers = 1;
            parm = frame.getParm(RedirectServer, 1);
            if (parm) {
                _servers[1] = parm->getstr();
                _num_servers = 2;
            }
            else {
                _servers[1] = "";
            }
            string servers = _servers[0];
            if (_num_servers > 1) {
                servers += " " + _servers[1];
            }
//            config.updateConfig("unit.servers", servers);
            config.updateConfig("unit.v1-servers", servers);
            _server_idx = 0;
            _updateServerIdx();
            ok = true;
        }
        return ok;
    }

    bool _processClose(Frame frame) {
        bool ok = false;
        if (frame.verify(_verify_server)) {
            ok = true;
        }
        return ok;
    }

    void _sendCloseAck() {
        Frame frame(CLOSE_ACK);
        _send(frame);
    }

    void _sendClose() {
        Frame frame(CLOSE);
        frame.addParm(Sig, "");
        _send(frame);
    }

    void _sendEvent(string event) {
        Frame frame(DATA);
        frame.addParm(Event, event);
        frame.addParm(Seq, _sendseq);
        _send(frame);
//        _timerval = time(NULL) + 2;
        _timerval = time(NULL) + EventDataTimeout;
        _resend_counter = 0;
        _last_frame = frame;
        _outstanding_packets = true;
        _last_data_was_event = true;
    }

    void _sendCommandData(int cmndid, string line) {
        Frame frame(DATA);
        frame.addParm(CommandId, cmndid);
        frame.addParm(NewCommandData, line);
        frame.addParm(Seq, _sendseq);
        _send(frame);
//        _timerval = time(NULL) + 2;
        _timerval = time(NULL) + CommandDataTimeout;
        _resend_counter = 0;
        _last_frame = frame;
        _outstanding_packets = true;
    }

    bool _processAck(Frame frame) {
        bool ok = false;
        FrameParm *parm = frame.getParm(Seq);
        if (parm) {
            int recvseq = parm->getnum();
            if (recvseq == _incSeq(_sendseq)) {
                ok = true;
            }
        }
        return ok;
    }

    bool _processCommand(Frame frame) {
        bool ok = false;
        if (!frame.verify(_verify_server)) return false;
        bool killing = false;
        FrameParm *parm = frame.getParm(CommandId);
        if (!parm) return false;
        int id = parm->getnum();
        int idx = findCommand(_dstip, _dstport, id);
        parm = frame.getParm(CommandKill);
        if (parm) {
            killing = true;
        }
        if (killing) {
            if (idx >= 0) {
                _sendCommandAck(id, killing);
                killCommandProcs(commands[idx].pid);
                remove(commands[idx].outfile.c_str());
                remove(commands[idx].cmndfile.c_str());
                commands.erase(commands.begin()+idx);
                ok = true;
            }
        }
        else {
            if (idx >= 0) {
                _sendCommandAck(id, killing);
                ok = true;
            }
            else {
                parm = frame.getParm(Command);
                if (!parm) return false;
                string command = parm->getstr();
                CommandInfo cmndinfo(id, command, _dstip, _dstport);
                parm = frame.getParm(CommandUser);
                if (parm) {
                    cmndinfo.user = parm->getstr();
                }
                parm = frame.getParm(CommandSummary);
                if (parm) {
                    cmndinfo.summary = parm->getstr();
                }
                if (_argv0_p != NULL) {
                    cmndinfo.setArgv0(_argv0_p);
                }
                cmndinfo.execute();
                commands.push_back(cmndinfo);
                _sendCommandAck(id, killing);
                ok = true;
            }
        }
        return ok;
    }

    void _sendCommandAck(int id, bool killing) {
        Frame frame(COMMAND_ACK);
        frame.addParm(CommandId, id);
        if (killing) {
            frame.addParm(CommandKill);
        }
        _send(frame);
    }

    bool _processRestart(Frame &frame) {
        bool ok = true;
        if (!frame.verify(_verify_server)) return false;
        return ok;
    }

    void _sendPing() {
        Frame frame(PING);
        _send(frame);
    }

    void _sendPingAck() {
        Frame frame(PING_ACK);
        _send(frame);
    }

    void _send(Frame frame) {
        Packet packet = frame.buildPacket();
        //logError("Sending packet to "+_dstip+":"+toString(_dstport));
        //packet.hexDump();
        sendto(_socket, packet.data(), packet.length(), 0, (struct sockaddr *)&_to, sizeof(_to));
    }

    int _incSeq(int seq) {
        seq++;
        if (seq > 65535) {
            seq = 0;
        }
        return seq;
    }

    void _closeConnection() {
        logError("_closeConnection");
        _closed = true;
    }

    void _saveFeatures(string features_str) {
        Assoc features;
        Strings strs = split(features_str, "[]");
        for (int i=0; i < strs.size(); i++) {
            string str = strs[i];
            int pos = str.find("=");
            if (pos != string::npos) {
                string var = str.substr(0,pos);
                string val = str.substr(pos+1);
                features[var] = val;
            }
        }
        Assoc old_features = readAssoc(FeaturesFile, "=");
        bool diff = false;
        Assoc::iterator it;
        for (it=features.begin(); it != features.end(); it++) {
            string var = (*it).first;
            if (old_features.find(var) == old_features.end()) {
                diff = true;
                break;
            }
            if (old_features[var] != features[var]) {
                diff = true;
                break;
            }
        }
        for (it=old_features.begin(); it != old_features.end(); it++) {
            string var = (*it).first;
            if (features.find(var) == features.end()) {
                diff = true;
                break;
            }
        }
        if (diff) {
            string tmpfile = FeaturesFile;
            tmpfile += ".tmp";
            writeAssoc(tmpfile, features, "=");
            rename(tmpfile.c_str(), FeaturesFile);
//            system("backup_autonet");
        }
    }

    void _setServerName() {
        _server_name = _servers[_server_idx];
        _verify_server = _server_name;
        if (!nphase_network) {
            int pos = _server_name.find("-tm-");
            if (pos != string::npos) {
               _server_name.erase(pos,3);
            }
        }
        else {
            int pos = _server_name.find("-ext");
            if (pos != string::npos) {
               _server_name.replace(pos,4,"-pvt");
            }
        }
        createServerKeyFile(_verify_server);
    }

    void _setServerIp() {
        _dstip = getIpAddress(_server_name);
        if (_dstip != "") {
            logError("ServerIP: " + _dstip);
            _to.sin_family = AF_INET;
            _to.sin_addr.s_addr = inet_addr(_dstip.c_str());
            _to.sin_port = htons(_dstport);
        }
        else {
            logError("Could not resolve: "+_server_name);
        }
    }

  public:
    Connection(Socket sock) {
        _server_name = "";
        _verify_server = "";
        _dstip = "";
        _dstport = 0;
        _num_servers = 0;
        _socket = sock;
        _coldstart = true;
        _disable = true;
        _recvseq = 0;
        _sendseq = 0;
        _swversion = "";
        _toswversion = "";
        _public_key = "";
        _ratelimit = "";
        _geofences = "";
        _positionReportInterval = 0;
        _state = DOWN;
        _outstanding_packets = false;
        _reopenConnection = false;
        _event = "";
        _events_socket = -1;
        _closed = false;
        _last_data_was_event = false;
        _argv0_p = NULL;
        _ping_interval = 0;
        _first_server = false;
    }

    void setSwVersion(string swversion) {
        _swversion = swversion;
    }

    void setEventsSocket(Socket events_socket) {
        _events_socket = events_socket;
    }

    void setKeys(const char *keyfile) {
        _public_key = cleanKey(keyfile);
    }

    void setArgv0(char *argv0_p) {
        _argv0_p = argv0_p;
    }

    void setPingInterval(int val) {
        _ping_interval = val;
    }

    bool match(string ip, int port) {
         bool retval = false;
         if ((_dstip == ip) and (_dstport == port)) {
             retval = true;
         }
         return retval;
    }

    void openConnection(string servers, int port) {
        char servers_copy[servers.size()+1];
        strcpy(servers_copy, servers.c_str());
        char *p = strchr(servers_copy, ' ');
        _servers[0] = "";
        _servers[1] = "";
        _num_servers = 1;
        if (p) {
            *p = '\0';
            p++;
            _servers[1] = p;
            _num_servers = 2;
        }
        _servers[0] = servers_copy;
        if (_first_server) {
            _server_idx = 0;
        }
        else {
            _getServerIdx();
        }
        _dstport = port;
        _setServerName();
        _setServerIp();
        if (_dstip != "") {
            _recvseq = random();
            //logError("Sending OpenReq");
            _sendOpenReq();
        }
        _closed = false;
        _state = STARTUP;
        _reopenConnection = true;
        _resend_counter = 0;
        _timerval = time(NULL) + 2;
    }

    bool connect(string ip, int port, Frame frame) {
        bool ok = false;
        _verify_server = ip;
        if (createServerKeyFile(_verify_server)) {
            if (_processOpenReq(frame)) {
                logError("Got connection: " + ip);
                _dstport = port;
                _dstip = ip;
                _verify_server = ip;
                _to.sin_family = AF_INET;
                _to.sin_addr.s_addr = inet_addr(_dstip.c_str());
                _to.sin_port = htons(_dstport);
                _sendOpen();
                _closed = false;
                _state = OPENING;
                ok = true;
            }
        }
        return ok;
    }

    void closeConnection() {
        logError("closeConnection");
        _sendClose();
        _state = CLOSING;
        _resend_counter = 0;
        _timerval = time(NULL) + 5;
    }

    void clearConnection() {
        _state = DOWN;
        _closed = true;
        _timerval = 0;
    }

    void restartConnection() {
        _restartConnection();
    }

    bool isUp() {
        return(_state == UP);
    }

    bool closed() {
        return _closed;
    }

    bool closing() {
        return _state == CLOSING;
    }

    bool outstandingPackets() {
        return(_outstanding_packets);
    }

    void processFrame(Frame frame) {
        //logError("Got:"+toString(frame.type())+" State:"+toString(_state));
        switch (_state) {
          case STARTUP:
            _startupProcessFrame(frame);
            break;
          case OPENING:
            _openingProcessFrame(frame);
            break;
          case UP:
            _upProcessFrame(frame);
            break;
          case CLOSING:
            _closingProcessFrame(frame);
            break;
          case DOWN:
            _downProcessFrame(frame);
            break;
        }
    }

    void sendEvent(string event) {
        if (isUp() && !_outstanding_packets) {
            //logError("Sending event now");
            _sendEvent(event);
        }
        else {
            //logError("Queueing event to be sent later");
            _event = event;
        }
    }

    bool sendCommandData(int cmndid, string line) {
        bool retval = false;
        if (isUp() && !_outstanding_packets) {
            _sendCommandData(cmndid, line);
            retval = true;
        }
        return retval;
    }

    void processTimeout() {
        if ((_timerval > 0) && (time(NULL) > _timerval)) {
            _timerval = 0;
            switch (_state) {
              case STARTUP:
                _startupTimeout();
                break;
              case OPENING:
                _openingTimeout();
                break;
              case UP:
                _upTimeout();
                break;
              case CLOSING:
                _closingTimeout();
                break;
              case DOWN:
                _downTimeout();
                break;
            }
        }
    }

    void sendPingAck(string ip, int port) {
        _dstip = ip;
        _dstport = port;
        _to.sin_family = AF_INET;
        _to.sin_addr.s_addr = inet_addr(_dstip.c_str());
        _to.sin_port = htons(_dstport);
        _sendPingAck();
    }

    void setFirstServer() {
        _getServerIdx();
        if (_server_idx != 0) {
            if ( !fileExists(SecondaryConnectedFile) or
                 ((time(NULL)-fileTime(SecondaryConnectedFile)) > 14400) ) {
                unlink(SecondaryConnectedFile);
                logError("Setting first_server");
                _first_server = true;
            }
        }
    }
};

vector<Connection> connections;
bool close_comm_requested = false;
time_t close_time = 0;
bool restart_connection = false;
Socket listen_sock = -1;
Socket events_sock = -1;
char *argv0;
//rcg string celldevice = "ppp0";
string celldevice = "brtrunk";
int ping_interval = 0;

int main(int argc, char *argv[]) {
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    umask(S_IWGRP | S_IWOTH);
    signal(SIGINT, &on_int);
    signal(SIGQUIT, &on_int);
    signal(SIGTERM, &on_int);
    signal(SIGHUP, &on_hup);
    signal(SIGUSR1, &on_usr1);
    signal(SIGCHLD, &on_child);
//    signal(SIGCHLD, SIG_IGN);
    argv0 = argv[0];
    int localport = 5152;
    generateKeys();
    if (config.exists("unit.listen-port")) {
        localport = config.getInt("unit.listen-port");
    }
    listen_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (listen_sock < 0) {
        logError("Could not create socket");
        exit(0);
    }
    struct linger linger_opt;
    linger_opt.l_onoff = 0;
    linger_opt.l_linger = 0;
    setsockopt(listen_sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
    int sock_flag = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &sock_flag, sizeof(sock_flag)); 
    int val = IP_PMTUDISC_DONT;
    setsockopt(listen_sock, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(localport);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        logError("Could not bind to port");
        exit(0);
    }
    updateFile(CommStateFile, "NoWAN");
    Connection server(listen_sock);
    string swversion = getSwVersion();
    server.setSwVersion(swversion);
    server.setKeys(PublicKeyFile);
    server.setArgv0(argv0);
    server.setFirstServer();
    connections.push_back(server);
    while (true) {
        Assoc hdw_config = readAssoc(HardwareConfFile, " ");
        if (hdw_config.find("cell_device") != hdw_config.end()) {
            celldevice = hdw_config["cell_device"];
        }
        if (hdw_config.find("cell_ping_interval") != hdw_config.end()) {
            ping_interval = stringToInt(hdw_config["cell_ping_interval"]);
        }
        if ( wanUp() and (getDefaultRoute(celldevice) != "") and
             fileExists(CommNeededFile)) {
            logError("WAN up");
            string ip = getInterfaceIpAddress(celldevice);
            string provider = backtick("./data/modem_client getoperator");
            if ( (strcasestr(provider.c_str(),"verizon") != NULL) and
                 (strncmp(ip.c_str(), "10.", 3) == 0) ) {
                nphase_network = true;
            }
            else {
                nphase_network = false;
            }
            unitid = getStringFromFile(UnitIdFile);
            if (config.exists("unit.unitid")) {
                unitid = config.getString("unit.unitid");
            }
            cerr << "Unitid: " << unitid << endl;
            updateFile(CommStateFile, "Connecting");
            handleConnections();
            logError("Lost WAN");
            updateFile(CommStateFile, "NoWAN");
        }
        else {
            sleep(1);
        }
    }
}

bool wanUp() {
    bool retval = false;
    string line = grepFile(ProcDevFile, celldevice);
    if (line != "") {
        retval = true;
    }
    return retval;
}

void handleConnections() {
    char event[32000];
    int serverport = 5151;
    if (config.exists("unit.server-port")) {
        serverport = config.getInt("unit.server-port");
    }
    string servers = "127.0.0.1";
    if (config.exists("unit.v1-servers")) {
        servers = config.getString("unit.v1-servers");
    }
    else if (config.exists("unit.servers")) {
        servers = config.getString("unit.servers");
    }
    connections[0].openConnection(servers, serverport);
    if (ping_interval != 0) {
        connections[0].setPingInterval(ping_interval);
    }
    struct sockaddr_in from;
    unsigned char buffer[32000];
    socklen_t fromlen = sizeof(from);
    bool comm_needed = true;
    while (comm_needed) {
        if (events_sock < 0) {
            events_sock = readerSocket(ReaderPath);
            if (events_sock >= 0) {
                 connections[0].setEventsSocket(events_sock);
            }
        }
        int nfds = 0;
        fd_set read_set;
        FD_ZERO(&read_set);
        if (listen_sock >= 0) {
            FD_SET(listen_sock, &read_set);
            nfds = max(nfds,listen_sock);
        }
        if (events_sock >= 0) {
            FD_SET(events_sock, &read_set);
            nfds = max(nfds,events_sock);
        }
        struct timeval select_time;
        select_time.tv_sec = 1;
        select_time.tv_usec = 0;
        int ready_cnt = select(nfds+1, &read_set, NULL, NULL, &select_time);
        if (!wanUp()) {
            break;
        }
        if (ready_cnt > 0) {
            if (FD_ISSET(listen_sock, &read_set)) {
                int len = recvfrom(listen_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from, &fromlen);
                Frame frame(len, buffer);
                string fromip;
                fromip = inet_ntoa(from.sin_addr);
                int fromport = ntohs(from.sin_port);
                //cout << "Received packet from " << fromip << ":" << fromport << endl;
                int conn_idx = findConnection(fromip, fromport);
                if (conn_idx >= 0) {
                    //cout << "packet connidx:" << conn_idx << endl;
                    connections[conn_idx].processFrame(frame);
                    if (connections[conn_idx].closed() and (conn_idx != 0)) {
                        //logError("Closing/removing connection: "+toString(conn_idx));
                        connections.erase(connections.begin()+conn_idx);
                    }
                }
                else {
                    Connection connection(listen_sock);
                    switch (frame.type()) {
                      case OPEN_REQ:
                        //cerr << "Got OpenReq" << endl;
                        if (connection.connect(fromip, fromport, frame)) {
                            connection.setArgv0(argv0);
                            //cerr << "number of connections:" << connections.size() << endl;
                            connections.push_back(connection);
                        }
                        break;
                      case PING:
                        connection.sendPingAck(fromip, fromport);
                        break;
                    }
                }
            }
            if (FD_ISSET(events_sock, &read_set)) {
                int nread = recv(events_sock, event, sizeof(event), 0);
                if (nread > 0) {
                    event[nread] = '\0';
                    if ((nread > 1) and (event[nread-1] == '\n')) {
                        nread--;
                        event[nread] = '\0';
                    }
                    //logError("Got event");
                    connections[0].sendEvent(event);
                }
                else {
                    close(events_sock);
                    events_sock = -1;
                }
            }
        }
        else {
            int i;
            for (i=0; i < connections.size(); i++) {
                connections[i].processTimeout();
                if (connections[i].closed()) {
                    if (i != 0) {
                        //logError("Connection closed:"+toString(i));
                        connections.erase(connections.begin()+i);
                        i--;
                    }
                }
            }
        }
        if (close_comm_requested) {
            if ((time(NULL)-close_time) >= CloseDelay) {
                logError("Closing....close_com_requested-delay");
                updateFile(CommStateFile, "Closed");
//                closeAndExit(0);
                close_comm_requested = false;
            }
            if (connections[0].outstandingPackets()) {
//                logError("Still events to send");
                close_time = time(NULL);
            }
            else if (connections[0].closed()) {
                logError("Closing....close_com_requested-closed");
                updateFile(CommStateFile, "Closed");
//                closeAndExit(0);
                close_comm_requested = false;
            }
            else if ( !connections[0].closing() and
                      ((time(NULL)-close_time) > 2) ) {
                logError("Setting closeConnection");
                connections[0].closeConnection();
            }
        }
        if (restart_connection) {
            connections[0].restartConnection();
            string swversion = getSwVersion();
            connections[0].setSwVersion(swversion);
            restart_connection = false;
        }
        for (int cmnd_idx=0; cmnd_idx < commands.size(); cmnd_idx++) {
            string dstip = commands[cmnd_idx].server_ip;
            int dstport = commands[cmnd_idx].server_port;
            int comm_idx = findConnection(dstip, dstport);
            if (comm_idx >= 0) {
                if (connections[comm_idx].isUp()) {
                    if (!connections[comm_idx].outstandingPackets()) {
                        string line;
                        if (commands[cmnd_idx].getLine(line)) {
                            //cerr << "Command data: (" << dstip << ":" << dstport <<") " << line << endl;
                            connections[comm_idx].sendCommandData(commands[cmnd_idx].id, line);
                            if (line.find("<<EOF>>") != string::npos) {
                                commands[cmnd_idx].closeCommand();
                                commands.erase(commands.begin()+cmnd_idx);
                                cmnd_idx--;
                            }
                        }
                    }
                }
                else {
                }
            }
            else {
            }
            for (int i=childs.size()-1; i >= 0; i--) {
                int pid = childs[i];
                if (pid > 0) {
                    int status = 0;
                    pid_t child = waitpid(pid,&status,WNOHANG);
                    if (child == pid) {
                        childs.erase(childs.begin()+i);
                        logError("Background removing pid: "+toString(pid)+" left:"+toString((int)childs.size()));
                    }
                }
            }
//            if (commands[cmnd_idx].closed and (commands[cmnd_idx].pid == 0) ) {
//                commands.erase(commands.begin()+cmnd_idx);
//                cmnd_idx--;
//            }
//            else if ( (commands[cmnd_idx].pid == 0) and
//                      ((time(NULL)-commands[cmnd_idx].exited_time) > 60) ) {
//                if (connections[comm_idx].isUp()) {
//                    if (connections[comm_idx].outstandingPackets()) {
//                        commands[cmnd_idx].exited_time = time(NULL);
//                    }
//                    else {
//                        connections[comm_idx].sendCommandData(commands[cmnd_idx].id, "<<Failed: process terminated>>");
//                        commands[cmnd_idx].closeCommand();
//                    }
//                }
//                else {
//                    commands[cmnd_idx].closeCommand();
//                }
//            }
        }
//        string eventline;
//        if (server.isUp() && !server.outstandingPackets() && eventfile.read(eventline)) {
//            server.sendEvent(eventline);
//        }
        if (!fileExists(CommNeededFile)) {
            comm_needed = false;
        }
    }
    // If we are rebooting don't kill the commands because it might
    // be a reboot sent from TruManager
    if (!fileExists(RebootReasonFile)) {
        killAllCommands();
    }
    commands.clear();
    for (int idx=connections.size()-1; idx > 0; idx--) {
        connections.erase(connections.begin()+idx);
    }
    connections[0].clearConnection();
//    closeAndExit(0);
}

void closeAndExit(int exitstat) {
//    logError("closeAndExit");
    killAllCommands();
    unlink(CommStateFile);
    exit(exitstat);
}

string getSwVersion() {
    string swversion = getStringFromFile(SwVersionFile);
    if (strncmp(swversion.c_str(), "autonet", 7) == 0) {
        swversion = swversion.substr(7);
    }
    if (fileExists(BackupTOCFile)) {
        int retval = str_system(string("cmp -s ")+BackupTOCFile+" "+TOCFile);
        if (retval != 0) {
            swversion += "*";
        }
    }
    int pos;
    if ((pos=swversion.find(" ")) != string::npos) {
        swversion.erase(pos);
    }
    if (fileExists(ContentVersionFile)) {
        string contentversion = getStringFromFile(ContentVersionFile);
        if (contentversion != "") {
            swversion += "-";
            swversion += contentversion;
        }
    }
    logError("SwVersion: " + swversion);
    return swversion;
}

string getIpAddress(string name) {
    string command = "resolvedns " + name;
    string resp = backtick(command);
    if (resp != "") {
        resp = getToken(resp, 0);
    }
    return resp;
}

int findConnection(string ip, int port) {
    int idx = -1;
    int i;
    for (i=0; i < connections.size(); i++) {
        if (connections[i].match(ip, port)) {
            idx = i;
            break;
        }
    }
    return idx;
}

int findCommand(string ip, int port, int id) {
    int idx = -1;
    int i;
    for (i=0; i < commands.size(); i++) {
        if (commands[i].match(ip, port, id)) {
            idx = i;
            break;
        }
    }
    return idx;
}

Socket readerSocket(const char *name) {
    Socket sock = -1;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        logError("Could not open event logger socket");
    }
    else {
        struct sockaddr_un a;
        memset(&a, 0, sizeof(a));
        a.sun_family = AF_UNIX;
        strcpy(a.sun_path, name);
        if (connect(sock, (struct sockaddr *)&a, sizeof(a)) < 0) {
            logError("Could not connect to event reader socket");
            close(sock);
            sock = -1;
        }
    }
    return sock;
}

void killAllCommands() {
    GetPids pids;
    for (int cmnd_idx=0; cmnd_idx < commands.size(); cmnd_idx++) {
        int pid = commands[cmnd_idx].pid;
        if (pid > 0) {
            pids.killChildren(pid, SIGTERM);
        }
    }
}

void killCommandProcs(int pid) {
    if (pid <= 0) {
        logError("Attempt to kill bad child, PID="+toString(pid));
    }
    else {
//        kill(pid, SIGTERM);
//    int stat = 0;
//    waitpid(pid, &stat, 0);
        GetPids pids;
        pids.listChildren(pid);
        pids.killChildren(pid, SIGTERM);
    }
}

void on_hup(int sig) {
   logError("Got HUP signal");
   if (close_comm_requested) {
//       closeAndExit(0);
   }
   else {
       close_comm_requested = true;
       close_time = time(NULL);
   }
   signal(SIGHUP, on_hup);
}

void on_int(int sig) {
    logError("Exiting on signal: "+toString(sig));
    closeAndExit(sig);
}

void on_usr1(int sig) {
    restart_connection = 1;
    signal(SIGUSR1, on_usr1);
}

void on_child(int sig) {
    int stat = 0;
    int status = 0;
//    logError("Got SIGCHLD");
//    for (int i=0; i < commands.size(); i++) {
    for (int i=childs.size()-1; i >= 0; i--) {
//        int pid = commands[i].pid;
        int pid = childs[i];
        if (pid > 0) {
            pid_t child = waitpid(pid,&status,WNOHANG);
            if (child == pid) {
                childs.erase(childs.begin()+i);
                logError("Removing pid: "+toString(pid)+" left:"+toString((int)childs.size()));
//                commands[i].pid = 0;
//                commands[i].exited_time = time(NULL);
            }
        }
    }
//    wait(&stat);
    signal(SIGCHLD, on_child);
}

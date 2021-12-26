/******************************************************************************
 * File tw_talker.cpp
 *
 * Daemon to send/recieve commands/responses from Quectel cell module
 *
 ******************************************************************************
 * Change log:
 ******************************************************************************
 */

#include <map>
#include <cstring>
#include <string>
#include <iostream>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "autonet_files.h"
#include "backtick.h"
#include "csv.h"
#include "dateTime.h"
#include "my_features.h"
#include "getField.h"
#include "getParm.h"
#include "getStringFromFile.h"
#include "getUptime.h"
#include "readAssoc.h"
#include "readconfig.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
#include "touchFile.h"
#include "updateFile.h"
#include "writeAssoc.h"
#include "str_system.h"
#include "trim.h"

using namespace std;
string serial_device = "celldiag";


// Ron's Change that was not inclueded
#define diag_device "smd8"
#define modem_device "cellmodem"
#define qualcomm_device "qualcomm"
//#define _POSIX_SOURCE 1         //POSIX compliant source

typedef map<int,bool> Readers;

void config_diag_port();
void config_modem_port();
void readHardwareInfo();
void updateHardwareInfo();
void closeAndExit(int exitstat);
void openPort(string port);
void closePort();
bool lock(string dev);
string processCommand(const char *command);
string help();
string getprofile();
string cdma_getprofile();
string lte_getprofile();
string getstatus();
string getmodel();
string getModel();
string gettemp();
string gettime();
time_t getunixtime();
string make_gmt_time(string who, string timestr);
time_t make_unix_time(string timestr, int &tzmin);
string getposition();
string getnetinfo();
string getusage();
string cdma_getnetinfo();
string lte_getnetinfo();
string reset();
string reset1();
string reboot();
bool resetIt();
void setDefaults();
void setPdpContext();
string activate();
string data_connect();
string data_disconnect();
string isconnected();
string isviable();
string lowpower();
string resetgps();
void clearGps();
void startGps();
string stopgps();
string checksms();
string sendsms(string cmndstr);
bool sendSms(string number, string mesg, bool enc_hex);
string formatGsmPdu(string number, string mesg, int &length);
string listsms();
string listsms_text();
string listsms_pdu();
string readsms(string cmndstr);
string readsms_text(string idx);
string readsms_pdu(string idx);
string decode_sms(string pdu);
string unpack8to7(string instr, int len);
string unhex(string msg);
string hexit(string msg);
string deletesms(string cmndstr);
string clearsms();
string enable_extended_cmds();
string ecall_setformatid(string cmndstr);
string ecall_setmessageid(string cmndstr);
string ecall_setcontrol(string cmndstr);
string ecall_setvin(string cmndstr);
string ecall_setstorage(string cmndstr);
string ecall_settimestamp(string cmndstr);
string ecall_setlocation(string cmndstr);
string ecall_setdirection(string cmndstr);
string ecall_setcfg(string cmndstr);
string ecall(string cmndstr);

string loadprl(string prl_file);

void killConnection();
void resumeConnection();
string getResponse(string command);
string getResponse(string command, int timeout);
string getResponse(string command, string resp, int timeout, string busy_resp="");
string getRestOfLine();
bool isUnsolicitedEvent(string line);
void unsolicitedEvent(string unsol_line);
bool beginsWith(string str, string match);
void stringToUpper(string &s);
int listenSocket(const char *name);
void onTerm(int parm);
void onAlrm(int parm);
string NetRoute();
string DataCreate();
string DataConfig();
string DataStart();

bool debug = false;
bool log_at = false;
int ser_s = -1;		// Serial port descriptor
int listen_s = -1;	// Listen socket for command connections
struct termios oldtio;	// Old port settings for serial port
Readers readers;
string lockfile = "";
string model = "";
bool lte_model = true;
bool evdo_model = false;
bool activation_required = false;
bool has_gps = false;
bool simok = false;
unsigned char mesg_reference = 0;
string version = "";
Features features;
string last_opened_port = "";
string iccid = "";
Assoc hdw_info;
bool convoluted = true;
unsigned long start_gps_time = 0;
bool gps_turned_off = false;
string mdn = "";
bool include_callback = false;
bool text_mode = true;
Config config;


// Add by Baoshu & Kyle
typedef enum 
{
    DATACONNECT_NoCreate = 0,
    DATACONNECT_None = 1,
    DATACONNECT_Config = 2,
    DATACONNECT_Connecting  = 3,
    DATACONNECT_Connected = 4,
    DATACONNECT_Disconnected   = 5,
    DATACONNECT_Unknown = 6,
    DATACONNECT_Panic = 7,
}DATACONNECT_TYPE;

DATACONNECT_TYPE qt_talker_datacon_state = DATACONNECT_NoCreate;

DATACONNECT_TYPE data_state();

string unsol_events[] = {
    "#GPS_STATUS",
    ""
};
#define GPS_OFF_TIME 5
#define GPS_ON_TIME 20

int main(int argc, char *argv[])
{
    int res;
    char buf[255];                       //buffer for where data is put
    char read_buf[255];                       //buffer for where data is put
    string unsol_line = "";
    Readers::iterator ri;

    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, onAlrm);
    if (features.exists("CellDebug")) {
        log_at = true;
    }

    readHardwareInfo();
	openPort(diag_device);
    string resp = getResponse("AT",1); // Throw away old unsolicited responses
    if (resp.find("OK") == string::npos) {
        cout << "No Response on diag port" << endl;
        bool ok = false;
        for (int i=0; i < 5; i++) {
            resp = getResponse("AT",5);
            if (resp.find("OK") != string::npos) {
                ok = true;
                break;
            }
        }
        if (!ok) {
//            resetIt();
        }
    }

    listen_s = listenSocket(CellTalkerPath);
    if (listen_s < 0) {
        closeAndExit(-1);
    }

    model = getModel();
    if (model == "") {
        cerr << "Get model failed" << endl;
        model == "unknown";
    }
    else if (beginsWith(model,"AG35")) {
        lte_model = true;
        has_gps = true;
//        enable_extended_cmds();
    }
    else {
        cerr << "Unknown model number" << endl;
    }
    if (lte_model) {
        string iccid_resp = getResponse("AT+ICCID",1);
//        if (beginsWith(iccid_resp,"+CCID:")) {
        if (iccid_resp.find("CCID:") != string::npos) {
            iccid = getField(iccid_resp,": ",1);
            string cpin = getResponse("AT+CPIN?",1);
            if (cpin.find("READY") != string::npos) {
                simok = true;
            }
        }
    }
    setDefaults();
    updateHardwareInfo();
    if (has_gps) {
        startGps();
    }

    // loop while waiting for input. normally we would do something useful here
    while (1) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser_s, &rd);
        nfds = max(nfds, ser_s);
        FD_SET(listen_s, &rd);
        nfds = max(nfds, listen_s);
        for (ri=readers.begin(); ri != readers.end(); ri++) {
            int rs = (*ri).first;
            bool opened = (*ri).second;
            if (opened) {
                FD_SET(rs, &rd);
                nfds = max(nfds, rs);
            }
        }
        struct timeval *tvp = NULL;
        struct timeval tv;
        int r = select(nfds+1, &rd, &wr, &er, tvp);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("select()");
            exit(1);
        }
        if (FD_ISSET(ser_s, &rd)) {
            alarm(1);
            res = read(ser_s,buf,255);
            alarm(0);
            int i;
            if (res == 0) {
                closeAndExit(1);
            }
            if (res>0) {
                int i;
                for (i=0; i < res; i++) {
                    char ch = buf[i];
                    if (ch == '\r') {
                        ch = '\n';
                    }
                    if (ch == '\n') {
                        if  (unsol_line != "") {
                            unsolicitedEvent(unsol_line);
                            unsol_line = "";
                        }
                    }
                    else {
                        unsol_line += ch;
                    }
                }
            }  //end if res>0
        }
        if (FD_ISSET(listen_s, &rd)) {
            struct sockaddr client_addr;
            unsigned int l = sizeof(struct sockaddr);
            memset(&client_addr, 0, l);
            int rs = accept(listen_s, (struct sockaddr *)&client_addr, &l);
            if (rs < 0) {
                perror ("accept()");
            }
            else {
                readers[rs] = true;
            }
        }
        for (ri=readers.begin(); ri != readers.end(); ri++) {
            int rs = (*ri).first;
            bool set = (*ri).second;
            if (set and FD_ISSET(rs, &rd)) {
                alarm(1);
                res = read(rs, read_buf, 255);
                alarm(0);
                if (res > 0) {
                    read_buf[res] = '\0';
                    while ( (read_buf[res-1] == '\n') or
                         (read_buf[res-1] == '\r') ) {
                        read_buf[--res] = '\0';
                    }
                    if (strncasecmp(read_buf, "exit", 4) == 0) {
                        close(rs);
//                        readers.erase(rs);
                        readers[rs] = false;
                    }
                    else {
                        if (unsol_line != "") {
                            unsol_line += getRestOfLine();
                            if (debug) {
                                string datetime = getDateTime();
                                cout << datetime << " " << unsol_line << endl;
                            }
                            unsol_line = "";
                        }
                        resp = processCommand(read_buf);
                        resp += "\n";
                        write(rs, resp.c_str(), resp.size());
                    }
                }
                else {
                    close(rs);
//                    readers.erase(rs);
                    readers[rs] = false;
                }
            }
        }
        bool closed = true;
        while (closed) {
            closed = false;
            for (ri=readers.begin(); ri != readers.end(); ri++) {
                int rs = (*ri).first;
                bool set = (*ri).second;
                if (!set) {
                    readers.erase(rs);
                    closed = true;
                    break;
                }
            }
        }
        if (start_gps_time != 0) {
            if ((getUptime()-start_gps_time) > GPS_ON_TIME) {
                (void)getResponse("AT$GPSNMUN=2,1,1,1,1,1,1",1); // Enable NMEA Sentences
                (void)getResponse("AT$GPSP=1",1); // Start GPS
                cout << getDateTime() << " GPS turned on" << endl;
                start_gps_time = 0;
            }
            else if ((getUptime()-start_gps_time) > GPS_OFF_TIME) {
                if (!gps_turned_off) {
                    (void)getResponse("AT$GPSP=0",1); // Start GPS
                    cout << getDateTime() << " GPS turned off" << endl;
                    gps_turned_off = true;
                }
            }
        }
    }  //while (1)
    closeAndExit(0);
    return 0;
}  //end of main


// Ron's Change that was not inclueded
void config_diag_port() {
    cout << "Closing diag port" << endl;
    closePort();
    cout << "Opening modem port" << endl;
    openPort(modem_device);
    string resp = getResponse("AT");
    cout << "Response to AT: " << resp << endl;
    resp = getResponse("AT#DIAGCFG=1");
    cout << "Response to AT#DIAGCFG=1: " << resp << endl;
    closePort();
    cout << "Waiting 5 seconds for module to reset" << endl;
    sleep(5);
    cout << "Reopening diag port" << endl;
    openPort(diag_device);
    resp = getResponse("AT");
    cout << "Response to AT: " << resp << endl;
    setDefaults();
}

void config_modem_port() {
    string resp = getResponse("AT#DIAGCFG=0");
    cout << "Response to AT#DIAGCFG=0: " << resp << endl;
    cout << "Closing diag port" << endl;
    closePort();
    cout << "Waiting 5 seconds for module to reset" << endl;
    sleep(5);
    cout << "Opening modem port" << endl;
    openPort(modem_device);
    resp = getResponse("AT");
    cout << "Response to AT: " << resp << endl;
    setDefaults();
}

void readHardwareInfo() {
    hdw_info = readAssoc(HardwareConfFile, " ");
}

void updateHardwareInfo() {
    hdw_info["cell_model"] = model;
    hdw_info["cell_type"] = model;
    hdw_info["cell_vendor"] = "Sierra";
    if (activation_required) {
        hdw_info["activation_required"] = "yes";
    }
    if (lte_model) {
        //hdw_info["cell_islteppp"] = "yes";
        hdw_info["cell_iccid"] = iccid;
    }
    if (has_gps) {
        hdw_info["cell_gps"] = "true";
    }
    string tmpfile = HardwareConfFile;
    tmpfile += ".tmp";
    writeAssoc(tmpfile, hdw_info, " ");
    rename(tmpfile.c_str(), HardwareConfFile);
}

void closeAndExit(int exitstat) {
    closePort();
    Readers::iterator ri;
    for (ri=readers.begin(); ri != readers.end(); ri++) {
         int rs = (*ri).first;
         bool opened = (*ri).second;
         if (opened) {
             close(rs);
             readers.erase(rs);
         }
    }
    if (listen_s > 0) {
        close(listen_s);
    }
    remove(CellTalkerPath);
    exit(exitstat);
}

void openPort(string port) {
    last_opened_port = port;
    string dev = "/dev/";
    dev += port;
    if (!lock(port)) {
        cout << "Serial device '" << port << "' already in use" << endl;
        closeAndExit(1);
    }
    ser_s = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser_s < 0) {
		perror(diag_device);
        remove(lockfile.c_str());
        closeAndExit(-1);
    }
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

void closePort() {
    if (ser_s >= 0) {
        // restore old port settings
        tcsetattr(ser_s,TCSANOW,&oldtio);
        tcflush(ser_s, TCIOFLUSH);
        close(ser_s);        //close the com port
        if (lockfile != "") {
            remove(lockfile.c_str());
        }
    }
}

bool lock(string dev) {
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

string processCommand(const char *command) {
    string cmndstr = command;
    string resp = "";
    if (strncasecmp(command, "at", 2) == 0) {
        resp = getResponse(cmndstr, 1);
        int pos;
        while ((pos=resp.find("\n")) != string::npos) {
            resp.replace(pos,1,";");
        }
    }
    else if (cmndstr == "") {
        resp = "Ready";
    }
    else if (cmndstr == "help") {
        resp = help();
    }
    else if (cmndstr == "getprofile") {
        resp = getprofile();
    }
    else if (cmndstr == "getstatus") {
        resp = getstatus();
    }
    else if (cmndstr == "getmodel") {
        resp = getmodel();
    }
    else if (cmndstr == "gettemp") {
        resp = gettemp();
    }
    else if (cmndstr == "gettime") {
        resp = gettime();
    }
    else if (cmndstr == "getposition") {
        resp = getposition();
    }
    else if (cmndstr == "getnetinfo") {
        resp = getnetinfo();
    }
    else if (cmndstr == "getusage") {
        resp = getusage();
    }
    else if (cmndstr == "reboot") {
        resp = reboot();
    }
    else if (cmndstr == "reset") {
        resp = reset();
    }
    else if (cmndstr == "reset1") {
        resp = reset1();
    }
    else if (cmndstr == "activate") {
        resp = activate();
    }
    else if (cmndstr == "connect") {
        resp = data_connect();
    }
    else if (cmndstr == "disconnect") {
        resp = data_disconnect();
    }
    else if (cmndstr == "isconnected") {
        resp = isconnected();
    }
    else if (cmndstr == "isviable") {
        resp = isviable();
    }
    else if (cmndstr == "checksms") {
        resp = checksms();
    }
    else if (cmndstr == "listsms") {
        resp = listsms();
    }
    else if (beginsWith(cmndstr, "readsms")) {
        resp = readsms(cmndstr);
    }
    else if (beginsWith(cmndstr, "deletesms")) {
        resp = deletesms(cmndstr);
    }
    else if (beginsWith(cmndstr, "sendsms ")) {
        resp = sendsms(cmndstr);
    }
    else if (cmndstr == "clearsms") {
        resp = clearsms();
    }
    else if (cmndstr == "lowpower") {
        resp = lowpower();
    }
	
	else if (cmndstr == "resetgps") {
        resp = resetgps();
    }
    else if (cmndstr == "stopgps") {
        resp = stopgps();
    }
    else if (beginsWith(cmndstr, "enable_extended_cmds")) {
        resp = enable_extended_cmds();
    }
    else if (beginsWith(cmndstr, "ecall_setformatid")) {
        resp = ecall_setformatid(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_setmessageid")) {
        resp = ecall_setmessageid(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_setcontrol")) {
        resp = ecall_setcontrol(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_setvin")) {
        resp = ecall_setvin(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_setstorage")) {
        resp = ecall_setstorage(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_settimestamp")) {
        resp = ecall_settimestamp(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_setlocation")) {
        resp = ecall_setlocation(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_setdirection")) {
        resp = ecall_setdirection(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall_setcfg")) {
        resp = ecall_setcfg(cmndstr);
    }
    else if (beginsWith(cmndstr, "ecall")) {
        resp = ecall(cmndstr);
    }
	else if (beginsWith(cmndstr, "loadprl ")) {
        string prl_file = cmndstr.substr(8);
        resp = loadprl(prl_file);
    }
	else {
        resp = "Invalid command";
    }
    return resp;
}

string help() {
    string outstr = "at*,activate,checksms,connect,disconnect,isconnected,isviable,listsms,readsms,deletesms,clearsms,sendsms,getnetinfo,enable_extended_cmds,ecall_setformatid,ecall_setmessageid,ecall_setcontrol,ecall_setvin,ecall_setstorage,ecall_settimestamp,ecall_setlocation,ecall_setdirection,ecall,getmodel,getposition,getprofile,getstatus,gettemp,gettime,getusage,lowpower,help,reboot,reset,reset1";
    string gps_commands = ",stopgps,cleargps,resetgps";
    if (has_gps) {
        outstr += gps_commands;
    }
    return outstr;
}

string getprofile() {
    string outstr = "";
    if (lte_model) {
        outstr = lte_getprofile();
    }
    else {
        outstr = cdma_getprofile();
    }
    return outstr;
}

string cdma_getprofile() {
    string outstr = "";
    string cgsn = getResponse("AT+CGSN");
    if (cgsn.find("ERROR") == string::npos) {
        outstr += ",imei:" + getField(cgsn,":,",0);
    }
    string cimi = getResponse("AT+CIMI");
    if (cimi.find("ERROR") == string::npos) {
        outstr += ",imsi:" + getField(cimi,":",0);
    }
    string omeid = getResponse("AT#MEIDESN?");
    if (beginsWith(omeid,"#MEIDESN:")) {
        outstr += ",esn:" + getField(omeid,": ,",1);
    }
    string mdn_resp = getResponse("AT+CNUM");
    if (mdn_resp != "ERROR") {
        mdn = mdn_resp;
        outstr += ",mdn:" + mdn;
    }
    string msid = getResponse("AT$MSID?");
    if (beginsWith(msid,"$MSID:")) {
        string min = getField(msid, ": ,", 1);
        min = min.substr(min.size()-10);
        outstr += ",min:" + min;
    }
    string prl_s = getResponse("AT$PRL?");
    if (beginsWith(prl_s, "$PRL:")) {
        string prl = getField(prl_s, ": ,", 1);
        outstr += ",prl:" + prl;
    }
    string provider = "verizon";
    string cops = getResponse("AT+COPS?");
    if (beginsWith(cops, "+COPS")) {
        provider = getField(cops, ":,", 3);
        if (provider[0] == '"') {
            provider = provider.substr(1, provider.size()-2);
        }
    }
    outstr += ",provider:" + provider;
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string lte_getprofile() {
    string outstr = "";
    if (iccid == "") {
        outstr += ",sim:NotInserted";
    }
    else {
        outstr += ",iccid:" + iccid;
        string cpin = getResponse("AT+CPIN?");
        if (cpin.find("SIM not inserted") != string::npos) {
            outstr += ",sim:NotInserted";
        }
        else if (cpin.find("SIM PIN") != string::npos) {
            outstr += ",sim:PinRequired";
        }
        else if (cpin.find("SIM PUK") != string::npos) {
            outstr += ",sim:PukRequired";
        }
        else if (cpin.find("READY") != string::npos) {
            outstr += ",sim:Ready";
        }
        string cgsn = getResponse("AT+CGSN");
        if (cgsn.find("ERROR") == string::npos) {
            outstr += ",imei:" + getField(cgsn,":,",0);
        }
        string cimi = getResponse("AT+CIMI");
        if (cimi.find("ERROR") == string::npos) {
            outstr += ",imsi:" + getField(cimi,":",0);
        }
	string mdn_resp = getResponse("AT+CNUM"); 
        if (beginsWith(mdn_resp, "+CNUM:")) {       
	    string MDN_num = getField(mdn_resp,":,",2);
	    if (MDN_num.substr(0,1) == "\"") {
                MDN_num = MDN_num.substr(1,MDN_num.size()-2);
            }
            outstr += ",mdn:" + MDN_num;
            mdn = MDN_num;
        }
        string cops = getResponse("AT+COPS?");
        if (beginsWith(cops, "+COPS:")) {
            string prov = getField(cops,":,",3);
            if (prov.substr(0,1) == "\"") {
                prov = prov.substr(1,prov.size()-2);
            }
            outstr += ",provider:"+prov;
        }
        outstr += ",prl:-1";
    }
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string getstatus() {
    string outstr = "";
    string creg = getResponse("AT+CREG?");
    string registered = "1";
    if (beginsWith(creg,"+CREG:")) {
        string stat = getField(creg,": ,",2);
        if (stat == "0") {
            registered = "0";
        }
        else if (stat == "1") {
        }
        else if (stat == "2") {
        }
        else if (stat == "3") {
        }
        else if (stat == "4") {
        }
        else if (stat == "5") {
        }
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    outstr += ",registered:" + registered;
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string getmodel() {
    string outstr = "";
    string man = getResponse("AT+GMI");
    if (man.find("ERROR") != string::npos) {
        outstr = "ERROR";
        return outstr;
    }
    string model = getResponse("AT+GMM");
    if (model.find("ERROR") != string::npos) {
        outstr = "ERROR";
        return outstr;
    }
    if (!beginsWith(man,"\"")) {
        man = "\"" + man + "\"";
    }
    if (!beginsWith(model,"\"")) {
        model = "\"" + model + "\"";
    }
    outstr = man + "," + model;
    string version = getResponse("AT+GMR");
    if (version != "ERROR") {
        outstr += "," + version;
    }
    return outstr;
}

string getModel() {
    string model = getResponse("AT+GMM");
    if (model.find("ERROR") != string::npos) {
        model = "";
    }
    return model;
}

string gettemp() {
    string qtemp = getResponse("AT#QTEMP?");
    string retstr = "ERROR";
    if (beginsWith(qtemp, "#QTEMP:")) {
        string val = getField(qtemp, ": ,", 1);
        if (val == "0") {
            retstr = "tempok";
        }
        else {
            retstr = "overtemp";
        }
    }
    return retstr;
}

string gettime() {
    string gmttime = "unknown";
    string timestr = getResponse("AT+CCLK?");
    if (beginsWith(timestr, "+CCLK:")) {
        gmttime = make_gmt_time("gettime", timestr);
    }
    return gmttime;
} 

time_t getunixtime() {
    time_t t = 0;
    string timestr = getResponse("AT+OCCLK?");
    if (beginsWith(timestr, "+OCCLK:")) {
        int tzmin;
        t = make_unix_time(timestr, tzmin);
    }
    else {
        cerr << "Failed getunixtime" << endl;
    }
    return t;
}

string make_gmt_time(string who, string timestr) {
    string outstr = "";
    int tzmin;
    time_t time_l = make_unix_time(timestr, tzmin);
    time_t t = time(NULL);
    double diff = abs(time_l - t);
    if (diff > 5) {
        string datetime = getDateTime();
        cout << datetime << " " << who << " " << timestr << " Diff=" << diff << endl;
    }
    struct tm tm;
    gmtime_r(&time_l, &tm);
    outstr = string_printf("%04d/%02d/%02d %02d:%02d:%02d %d",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, tzmin);
    return outstr;
}

time_t make_unix_time(string timestr, int &tz_min) {
    string datetime = getField(timestr, " ", 1);
    int yr, mon, day, hr, min, sec;
    int tzadj;
    int n = sscanf(datetime.c_str(), "\"%d/%d/%d,%2d:%2d:%2d%d\"",
           &yr, &mon, &day, &hr, &min, &sec, &tzadj);
    time_t time_l = 0;
    if (n != 7) {
        cout << getDateTime() << " Invalid time: " << datetime << endl;
    }
    else {
        struct tm tm;
        tm.tm_year = yr + 100;
        tm.tm_mon = mon - 1;
        tm.tm_mday = day;
        tm.tm_hour = hr;
        tm.tm_min = min;
        tm.tm_sec = sec;
        time_l = timegm(&tm);
        //time_l -= tzadj * 15 * 60;
        tz_min = tzadj * 15;
    }
    return time_l;
}

string getposition() {
    string position = "";
    string cellpos = getResponse("AT$CELLPOS?");
    if (beginsWith(cellpos, "$CELLPOS:")) {
        string lat_s = getField(cellpos, ": ,", 1);
        string lon_s = getField(cellpos, ": ,", 2);
        position = lat_s + "/" + lon_s;
    }
    return position;
}

string getnetinfo() {
    string outstr = "";
    if (lte_model) {
        outstr = lte_getnetinfo();
    }
    else {
        outstr = cdma_getnetinfo();
    }
    return outstr;
}

string getusage() {
    static unsigned long rxbytes = 0;
    rxbytes++;
    string outstr = "";
    if (lte_model) {
        outstr = string("rxbytes:") + toString(rxbytes) + ",txbytes:10";
        //outstr = lte_getusage();
    }
    else {
        //outstr = cdma_getusage();
    }
    return outstr;
}

string cdma_getnetinfo() {
    string outstr = "";
    string type = "1xRTT";
    string roaming = "0";
    string registered = "1";
    string creg = getResponse("AT+CREG?", 5);
    if (beginsWith(creg,"+CREG:")) {
        string stat = getField(creg,": ,",2);
        if (stat == "0") {
            registered = "0";
        }
        else if (stat == "1") {
        }
        else if (stat == "2") {
        }
        else if (stat == "3") {
        }
        else if (stat == "4") {
        }
        else if (stat == "5") {
            roaming = "1";
        }
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    char percent_str[5];
    string csq = getResponse("AT+CSQ", 5);
    if (csq.find("ERROR") == string::npos) {
        string rssi;
        if (beginsWith(csq, "+CSQ:")) {
            rssi = getField(csq,":, ",1);
        }
        else {
            rssi = getField(csq,", ",0);
        }
        int rssi_v;
        sscanf(rssi.c_str(), "%d", &rssi_v);
        if (rssi_v == 99) {
            rssi_v = 0;
        }
        if (rssi_v > 32) {
            rssi_v = 32;
        }
        int percent = (int)(rssi_v / 32.0 * 100. + 0.5);
        sprintf(percent_str, "%d", percent);
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    if (evdo_model) {
        string hdrps = getResponse("AT#HDRPS?",5);
        if (beginsWith(hdrps, "#HDRPS:")) {
            string dbmstr = getField(hdrps,":, ",17);
            string evdorev = getField(hdrps,":, ",9);
            if (evdorev == "1") {
                type = "EVDO-0";
            }
            else if (evdorev == "2") {
                type = "EVDO-A";
            }
            else if (evdorev == "3") {
                type = "EVDO-B";
            }
            outstr += ",1xrttsig:";
            outstr += percent_str;
            float dbm;
            sscanf(dbmstr.c_str(), "%f", &dbm);
            int percent = (int)((dbm-(-113.))/(-51-(-113))*100.+.5);
            if (percent > 100) {
                percent = 100;
            }
            else if (percent < 0) {
                percent = 0;
            }
            char p_str[10];
            sprintf(p_str, "%d", percent);
            outstr += ",evdosig:";
            outstr += p_str;
        }
        else {
           outstr = "ERROR";
        }
    }
    else {
        outstr += ",signal:";
        outstr += percent_str;
    }
    outstr = "type:" + type + ",roam:" + roaming + outstr;
    outstr += ",registered:" + registered;
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string lte_getnetinfo() {
    string outstr = "";
    string type = "lte";
    string roaming = "0";
    string creg = getResponse("AT+CREG?");
    if (beginsWith(creg,"+CREG:")) {
        string stat = getField(creg,": ,",2);
        if (stat == "0") {
        }
        else if (stat == "1") {
        }
        else if (stat == "2") {
        }
        else if (stat == "3") {
        }
        else if (stat == "4") {
        }
        else if (stat == "5") {
            roaming = "1";
        }
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    string csq = getResponse("AT+CSQ");
    if (csq.find("ERROR") == string::npos) {
        string rssi;
        if (beginsWith(csq, "+CSQ:")) {
            rssi = getField(csq,":, ",1);
        }
        else {
            rssi = getField(csq,", ",0);
        }
        int rssi_v;
        sscanf(rssi.c_str(), "%d", &rssi_v);
        if (rssi_v == 99) {
            rssi_v = 0;
        }
        if (rssi_v > 32) {
            rssi_v = 32;
        }
        int percent = (int)(rssi_v / 32.0 * 100. + 0.5);
        char percent_str[5];
        sprintf(percent_str, "%d", percent);
        outstr += ",signal:";
        outstr += percent_str;
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    outstr = "type:" + type + ",roam:" + roaming + outstr;
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string reset() {
    string outstr = "Failed";
    killConnection();
    if (resetIt()) {
        outstr = "OK";
    }
    resumeConnection();
    return outstr;
}

string reset1() {
    string outstr = "Failed";
    killConnection();
    (void)getResponse("AT+CFUN=1");
    (void)getResponse("AT&W");
    if (resetIt()) {
        outstr = "OK";
    }
    resumeConnection();
    return outstr;
}

string reboot() {
    string outstr = "Failed";
    killConnection();
    if (resetIt()) {
        outstr = "OK";
    }
    resumeConnection();
    return outstr;
}

bool resetIt() {
    bool res = false;
    string resp = getResponse("AT$RESET", 5);
    if (resp == "OK") {
        res = true;
        closePort();
        cout << "Waiting 10 seconds for module to reset" << endl;
        sleep(10);
        openPort(last_opened_port);
        setDefaults();
        if (has_gps) {
            startGps();
        }
    }
    return res;
}

void setDefaults() {
    string resp;
    // Set SMS format to Text mode
    resp = getResponse("AT+CMGF=1");
    // Set Character Set to "GSM"
    resp = getResponse("AT+CSCS=\"GSM\"");
    // Set Message Storage to "SIM"
    resp = getResponse("AT+CPMS=\"SM\",\"SM\",\"SM\"");
    // Set SMS Routing
    resp = getResponse("AT+CNMI=2,1");
    // Set Phone Book = Local, so we can read MDM
    resp = getResponse("AT$QCPBMPREF=1");
	// Set GPS output port to Linux /dev/smd7
    resp = getResponse("AT+QGPSCFG=\"outport\",\"linuxsmd\"");
    cout << "set linuxsmd result: " << resp << endl;
    if (resp != "OK") {
        resp = getResponse("AT+QGPSCFG=\"outport\",\"usbnmea\"");
        cout << "set usbnmea result: " << resp << endl;
    }
    resp = getResponse("AT+QGPSCFG=\"fixfreq\",1");
/*
    resp = getResponse("AT#NITZ=7,0");
    // Set the RI line to pulse with SMS and save the config for power up
    resp = getResponse("AT#E2SMSRI?");
    if (beginsWith(resp,"#E2SMSRI:")) {
        string oldval = getField(resp, ": ", 1);
        if (oldval != "1000") {
            cout << "Setting RI to pulse with SMS" << endl;
            resp = getResponse("AT#E2SMSRI=1000");
            resp = getResponse("AT&W");
            resp = getResponse("AT&P");
        }
    }
*/
//    resp = getResponse("AT+CFUN?");
//    if (beginsWith(resp,"+CFUN:")) {
//        string oldval = getField(resp, ": ", 1);
////        if (oldval != "0") {
//        if (oldval != "5") {
//            cout << "Setting power up mode in power save mode" << endl;
////            resp = getResponse("AT+CFUN=0");
//            resp = getResponse("AT+CFUN=5");
//            resp = getResponse("AT&W");
//            resp = getResponse("AT&P");
//        }
//    }
    resp = getResponse("AT+CFUN=1");
    if (lte_model) {
        setPdpContext();
    }
}

void setPdpContext() {
//    string resp;
//    string default_apn = "VZWINTERNET";
//    string cgdcont = getResponse("AT+CGDCONT?");
//    if (cgdcont.find("+CGDCONT: 1") == string::npos) {
//        resp = getResponse("AT+CGDCONT=1,\"IPV4V6\",\"VZWIMS\"");
//    }
//    if (cgdcont.find("+CGDCONT: 2") == string::npos) {
//        resp = getResponse("AT+CGDCONT=2,\"IPV4V6\",\"VZWADMIN\"");
//    }
//    if (cgdcont.find("+CGDCONT: 3") == string::npos) {
//        resp = getResponse("AT+CGDCONT=3,\"IPV4V6\",\""+default_apn+"\"");
//    }
//    if (cgdcont.find("+CGDCONT: 4") == string::npos) {
//        resp = getResponse("AT+CGDCONT=4,\"IPV4V6\",\"VZWAPP\"");
//    }
//    if (cgdcont.find("+CGDCONT: 5") == string::npos) {
//        resp = getResponse("AT+CGDCONT=5,\"IPV4V6\",\"VZW800\"");
//    }
}

string activate() {
/*
    string outstr = "Failed";
    killConnection();
    string resp = getResponse("ATD*22899;", 3);
    if (resp == "OK") {
        time_t start_time = time(NULL);
        bool mdn_set = false;
        bool prl_set = false;
        bool commit = false;
        while ((time(NULL)-start_time) < 60) {
            resp = getResponse("","\n",60,"activating");
            if (resp != "") {
                cout << getDateTime() << " Activate: " << resp << endl;
                if (resp.find("#OTASP: 0") != string::npos) {
                }
//                else if (resp.find("#OTASP: PRL_DOWNLOADED") != string::npos) {
//                    prl_set = true;
//                }
//                else if (resp.find("#OTASP: MDN_DOWNLOADED") != string::npos) {
//                    mdn_set = true;
//                }
                else if (resp.find("#OTASP: 1") != string::npos) {
                    commit = true;
                }
                else if (resp.find("OTASP: 2") != string::npos) {
                    break;
                }
            }
        }
//        if (mdn_set and commit) {
        if (commit) {
            outstr = "OK";
        }
    }
    resumeConnection();
    return outstr;
*/
    return "OK";
}

string data_connect() {
    string outstr = "Failed";
    string resp;
    int retry = 0; //Amount of times we have tried to set up connecttion
    if (isconnected() == "Connected"){
    	outstr = "Already Connected";
    	return outstr;
    }else{
    	while ((retry <= 3)){
	       switch(data_state()){
		    // Actions to Create connection
	    	case DATACONNECT_NoCreate:
	    		outstr = DataCreate();
                if(outstr == "FAIL"){
                    return "1:Failed to create";
                }
	    		break;

   			// Actions to Configure connection
	    	case DATACONNECT_None:
	    		outstr = DataConfig();
	    		if (outstr == "FAIL"){
                    return "2:Failed to config";
                }
                break;

    		//Actions to Start Connection
	    	case DATACONNECT_Config:
   				outstr = DataStart();
   				retry++;
                if (outstr == "FAIL"){
                    return "3: Failed to connect";
                }
                break;
                
    		//Actions to Start Connection
	    	case DATACONNECT_Disconnected:
	    		outstr = DataStart();
	    		retry++;
                if (outstr == "FAIL"){
                    return "Failed to connect";
                }
                break;

            //Actions to add route rules after set up the data connection
            case DATACONNECT_Connected:        		
                	outstr="Connected";
                	return outstr;
                
                
	    	case DATACONNECT_Connecting:
	    		outstr = "Connecting";
                retry++;
                if (outstr == "FAIL"){
                    return "4: Failed to connect";
                }
	    		break;
	    	
	    	default:
	    		outstr = "Unknown State \nPlease Check Attena or Sim and reboot the device";
                return outstr;
	    		break;
	    }
		if((outstr.find("FAIL") != string::npos) ||  (outstr.find("ERROR") != string::npos)){
			retry++;
		}else{
			retry = 0;
		}
     }
     if((retry >= 3)){
        outstr = "Failed to Connect";
     }
 
    return outstr;
}	
    }
    

string DataCreate(){
	string outstr = "FAIL";
	string resp = getResponse("AT+CEREG?");
	
	if (!(resp.find(",1") || resp.find(",5"))) {
		outstr="FAIL: Not registered,please retry later";
	}

    else if(data_state() == DATACONNECT_NoCreate){
	resp = getResponse("at+qdatacfg=\"create\",110,4");
	if (beginsWith(resp, "OK")) {
		outstr = "PASS";  
	}else if (resp.find("ERROR") != string::npos) {
		// Error
		outstr = "ERROR";
		}
	}else{
		// Failed
		outstr = "FAIL";
	    return outstr;
	 }
	return outstr;
}

string DataConfig(){
	string outstr = "FAIL";
	string resp = getResponse("at+qdatacfg=\"config\",110,1,10");
    
    if (beginsWith(resp, "OK")) {
    	outstr = "PASS";
    }else{
   		outstr = "FAIL";
		return outstr;
    }
    return outstr;
}

string DataStart(){
	string outstr = "FAIL";
	string resp = getResponse("at+qdatacfg=\"start\",110");

    if (beginsWith(resp, "OK")) {
		outstr ="PASS";
		NetRoute();
    }else{
    	outstr = "FAIL";
    }
	return outstr;
}

string NetRoute(){
	string outstr= "FAIL";
    string natlist = backtick("route |grep rmnet_data0 | grep default");
    if (natlist.empty()){
	    system("ip ro add  default dev rmnet_data0");
    }
    natlist = backtick("iptables -L -t nat -v | grep rmnet_data0 |grep MASQUERADE");
    if (natlist.empty()){
        system("iptables -t nat -A POSTROUTING -o rmnet_data0 -j MASQUERADE");
        system("iptables -t filter -F");
   	    system("iptables -t nat -L -n -v"); //check the nat rules.
    }
   		
	outstr = "PASS";
	return outstr;
}

string data_disconnect() {
    string outstr = "FAIL";
    string resp;
    if(data_state() == DATACONNECT_Connected){
    resp = getResponse("at+qdatacfg=\"stop\",110");
        if (beginsWith(resp, "OK")) {
            outstr = "Disconnected";
        }else{
            outstr ="Failed to disconnect";
        }
    }else{
        outstr = "No connect to end";
    }
    return outstr;
}

DATACONNECT_TYPE data_state(){ 
    string resp =  getResponse("at+qdatacfg=\"getlist\"");
    DATACONNECT_TYPE state = DATACONNECT_NoCreate;

    if (resp.find("110") != string::npos){
        resp =  getResponse("at+qdatacfg=\"getstat\",110");
        if (resp.find("none") != string::npos){
            state = DATACONNECT_None;
        }else if (resp.find("configured") != string::npos){
            state = DATACONNECT_Config;
        }else if (resp.find("connecting") != string::npos){
            state = DATACONNECT_Connecting;
        }else if (resp.find("diconnected") != string::npos){
            state = DATACONNECT_Disconnected;
        }else if (resp.find("connected") != string::npos){
            state = DATACONNECT_Connected;
        }   
    }else if(resp.find("OK") != string::npos){
        state = DATACONNECT_NoCreate;
    }else if (resp.empty()){
            state = DATACONNECT_Panic;
    }else{
        state = DATACONNECT_Unknown;
    }
    
    return state;
}
    

string isconnected() {
    string outstr = "Not Connected";
    qt_talker_datacon_state = data_state();
    if(qt_talker_datacon_state == DATACONNECT_Connected){
    	outstr = "Connected";
    }
    
    return outstr;
}

string isviable() {
    string outstr = "no";
    //string dns = str_system("cell_shell \"at+cgcontrdp=1\" | awk -F, 'NR==2 {print $6}'");
    string dns0 = getResponse("at+cgcontrdp=1", 4);
    string dns = backtick("echo \""+dns0+"\" | awk -F, 'NR==1 {print $6}'");

    if (!dns.empty()) {
        //nslookup  www.google.com 172.26.38.1 | awk 'NR==5 {print $3}'
        string nslookup = backtick("nslookup  www.google.com "+dns+" | awk 'NR==5 {print $3}'");
        if (!nslookup.empty()) {
           outstr = "yes";
        }
    }
    return outstr;
}

string lowpower() {
    string resp = "";
//    resp = getResponse("AT+CFUN=0");
    resp = getResponse("AT+CFUN=5");
    if (resp != "OK") {
        resp = "ERROR";
    }
    return resp;
}

string resetgps() {

      return "OK";
}

string cleargps() {
    return "OK";
}

void startGps() {
//    string gpsautostart = getResponse("AT+QGPSCFG=\"autogps\"");
//    string function = getParm(gpsautostart, "\"autogps\",");
//    if (function == "0") {
//        (void)getResponse("AT+QGPSCFG=\"autogps\",1");
        (void)getResponse("AT+QGPS=1");
//    }
}

string stopgps() {
    string resp = "";
    resp = getResponse("AT+QGPSEND",3);
    if (resp != "OK") {
        resp = "ERROR";
    }
    return resp;
}

string checksms() {
    string outstr = "No";
    string resp = getResponse("AT+CMGL=\"ALL\"");
    if (beginsWith(resp, "+CMGL:")) {
        outstr = "Yes";
    }
    else if (resp.find("ERROR") != string::npos) {
        // Error
        resp = getResponse("AT+CMGF=1");
    }
    return outstr;
}

string sendsms(string cmndstr) {
    string resp = "";
    Strings args = csvSplit(cmndstr, " ");
    args.erase(args.begin()); // Remove "sendsms"
    bool enc_hex = false;
    while (args.size() > 2) {
        if (args[0] == "-h") {
            enc_hex = true;
            args.erase(args.begin());
        }
        else {
            break;
        }
    }
    if (args.size() != 2) {
        resp = "You must specify number and message";
    }
    else if (args[0].find_first_not_of("+0123456789") != string::npos) {
        resp = "Invalid phone number";
    }
    else {
        string number = args[0];
        string mesg = csvUnquote(args[1]);
        if (sendSms(number, mesg, enc_hex)) {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

bool sendSms(string number, string mesg, bool enc_hex) {
    bool success = false;
    string resp;
    if (enc_hex) {
        resp = getResponse("AT+CMGF=0");
//        resp = getResponse("AT+CMGS=\""+number+"\",0",">", 1);
//        if (resp.find(">") != string::npos) {
//            resp = getResponse(mesg + "\x1a", 5);
//            if (resp.find("+CMGS:") != string::npos) {
//                success = true;
//            }
//            else {
//                cout << "sms response:" << resp << endl;
//            }
//        }
        int pdu_length = 0;
        string pdu = formatGsmPdu(number, mesg, pdu_length);
        //cout << "PDU:" << pdu << endl;
        resp = getResponse("AT+CMGS="+toString(pdu_length), ">", 1);
        if (resp.find(">") != string::npos) {
            resp = getResponse(pdu + "\x1a", 5);
            if (resp.find("+CMGS:") != string::npos) {
                success = true;
            }
            else {
                cout << "sms response:" << resp << endl;
            }
        }
        resp = getResponse("AT+CMGF=1");
    }
    else {
        resp = getResponse("AT+CMGS=\""+number+"\"",">", 1);
        if (resp.find(">") != string::npos) {
            resp = getResponse(mesg + "\x1a", 5);
            if (resp.find("+CMGS:") != string::npos) {
                success = true;
            }
            else {
                cout << "sms response:" << resp << endl;
            }
        }
    }
    return success;
}

string formatGsmPdu(string number, string mesg, int &length) {
    string pdu = "";
    length = 0;
    string number_plan = "80";
    if (beginsWith(number, "+")) {
        number_plan = "91";
        number.erase(0,1);
    }
    if ((number.size()%2) == 1) {
        number += "F";
    }
    pdu += string_printf("%02X", number.size()/2+1);
    pdu += number_plan;
    string swapped_number = "";
    for (int i=0; i < number.size(); i+=2) {
        swapped_number += number.at(i+1);
        swapped_number += number.at(i);
    }
    pdu += swapped_number;
    //length += number.size()/2+2;
    if (include_callback) {
        string cb_number = mdn;
        string cb_number_plan = "80";
        if (beginsWith(cb_number, "+")) {
            cb_number_plan = "91";
            cb_number.erase(0,1);
        }
        if ((cb_number.size()%2) == 1) {
            cb_number += "F";
        }
        pdu += string_printf("%02X", cb_number.size()/2+1);
        length += 1;
        pdu += cb_number_plan;
        length += 1;
        swapped_number = "";
        for (int i=0; i < cb_number.size(); i+=2) {
            swapped_number += cb_number.at(i+1);
            swapped_number += cb_number.at(i);
        }
        pdu += swapped_number;
        length += swapped_number.size()/2;
    }
    else {
        pdu += "00"; // Length of callback number
        length += 1;
    }
    pdu += "10020000";
    length += 4;
    pdu += string_printf("%02X", mesg.size()/2);
    length += 1;
    pdu += mesg;
    length += mesg.size()/2;
    return pdu;
}

string listsms() {
    string outstr = "";
    if (text_mode) {
       outstr = listsms_text();
    }
    else {
       outstr = listsms_pdu();
    }
    return outstr;
}

string listsms_text() {
    string outstr = "";
    string resp = getResponse("AT+CMGL=\"ALL\"");
    Strings lines = split(resp, "\n");
    for (int i=0; i < lines.size(); i++) {
        string line = lines[i];
        if (beginsWith(line, "+CMGL: ")) {
            line.erase(0, 7);
            Strings flds = csvSplit(line, ",");
            string idx = flds[0];
            string number = flds[2];
            string msg = "";
            for (int j=i+1; j < lines.size(); j++) {
                string ln = lines[j];
                if (beginsWith(ln, "+CMGL: ")) {
                    break;
                }
                if (msg != "") {
                    msg += "\n";
                }
                msg += ln;
            }
            if (outstr != "") {
                outstr += ";";
            }
            msg = hexit(msg);
            outstr += idx+":"+number+" "+msg;
        }
    }
    if (outstr == "") {
        outstr = "none";
    }
    return outstr;
}

string listsms_pdu() {
    string outstr = "";
    string resp = getResponse("AT+CMGF=0");
    resp = getResponse("AT+CMGL=4");
    Strings lines = split(resp, "\n");
    for (int i=0; i < lines.size(); i++) {
        string line = lines[i];
        if (beginsWith(line, "+CMGL: ")) {
            line.erase(0, 7);
            Strings flds = csvSplit(line, ",");
            string idx = flds[0];
            string number = flds[2];
            string msg = "";
            for (int j=i+1; j < lines.size(); j++) {
                string ln = lines[j];
                if (beginsWith(ln, "+CMGL: ")) {
                    break;
                }
                if (msg != "") {
                    msg += "\n";
                }
                msg += ln;
            }
            if (outstr != "") {
                outstr += ";";
            }
            outstr += idx+":"+decode_sms(msg);;
        }
    }
    if (outstr == "") {
        outstr = "none";
    }
    resp = getResponse("AT+CMGF=1");
    return outstr;
}

string readsms(string cmndstr) {
    string outstr = "none";
    cmndstr.erase(0, 8);        // Remove "readsms "
    string idx = cmndstr;
    if (text_mode) {
        outstr = readsms_text(idx);
    }
    else {
        outstr = readsms_pdu(idx);
    }
    return outstr;
}

string readsms_text(string idx) {
    string outstr = "none";
    string resp = getResponse("AT+CMGR="+idx);
    if (beginsWith(resp, "+CMGR: ")) {
        Strings lines = split(resp, "\n");
        string line = lines[0];
        line.erase(0, 7);
        Strings flds = csvSplit(line, ",");
        string number = flds[1];
        string msg = "";
        for (int i=1; i < lines.size(); i++) {
            string ln = lines[i];
            if (msg != "") {
                msg += "\n";
            }
            msg += ln;
        }
        msg = hexit(msg);
        outstr = idx+":"+number+" "+msg;
    }
    return outstr;
}

string readsms_pdu(string idx) {
    string outstr = "none";
    string resp = getResponse("AT+CMGF=0");
    resp = getResponse("AT+CMGR="+idx);
    if (beginsWith(resp, "+CMGR: ")) {
        Strings lines = split(resp, "\n");
        string line = lines[0];
        line.erase(0, 7);
        Strings flds = csvSplit(line, ",");
        string number = flds[1];
        string msg = "";
        for (int i=1; i < lines.size(); i++) {
            string ln = lines[i];
            if (msg != "") {
                msg += "\n";
            }
            msg += ln;
        }
        outstr = decode_sms(msg);
    }
    resp = getResponse("AT+CMGF=1");
    return outstr;
}

string decode_sms(string pdu) {
    int len;
    sscanf(pdu.c_str(), "%02x", &len);
    int idx = 2;
    string number_type = pdu.substr(idx, 2);
    string number = pdu.substr(idx+2, (len-1)*2);
    idx += len*2;
    string swapped_number = "";
    for (int i=0; i < number.size(); i+=2) {
        swapped_number += number.at(i+1);
        swapped_number += number.at(i);
    }
    if (swapped_number.at(swapped_number.size()-1) == 'F') {
        swapped_number.erase(swapped_number.size()-1);
    }
    number = "";
    if (number_type == "91") {
       number = "+";
    }
    number += swapped_number;
    number = csvQuote(number);
    string datetime = pdu.substr(idx, 12);
    idx += 12;
    string format = pdu.substr(idx+6, 2);
    idx += 8;
    string msglen_str = pdu.substr(idx, 2);
    int msglen;
    sscanf(msglen_str.c_str(), "%02x", &msglen);
    idx += 2;
    string msg = pdu.substr(idx);
    //cout << "format:" << format << " len:" << msglen << " msg:" << msg << endl;
    if (format == "02") {
        msg = unpack8to7(msg, msglen);
        msg = csvQuote(msg);
    }
    else {
        msg = unhex(msg);
    }
    return number+" "+msg;
}

string enable_extended_cmds()
{
    string resp = "";
    resp = getResponse("AT!ENTERCND=\"A710\"", 2);
    if (resp == "OK") {
        resp = "OK";
    }
    else {
        resp = "Failed";
    }
    return resp;
}

string ecall_setformatid(string cmndstr) {
    string resp = "";
    //cmndstr.erase(cmndstr.begin(), cmndstr.begin()+1); // remove "ecall_setformatid"
    Strings args = csvSplit(cmndstr, " ");
    if (args.size() != 2) {
        resp = "You must specify 1 arg";
    }
    else {
        string id = args[1];
    
        resp = getResponse("AT!MECALLMSDBLK=1,"+id, 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_setmessageid(string cmndstr) {
    string resp = "";
    //cmndstr.erase(cmndstr.begin(), cmndstr.begin()+17); // remove "ecall_setmessageid"
    Strings args = csvSplit(cmndstr, " ");
    if (args.size() != 2) {
        resp = "You must specify 1 arg";
    }
    else {
        string id = args[1];
    
        resp = getResponse("AT!MECALLMSDBLK=2,"+id, 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_setcontrol(string cmndstr) {
    string resp = "";
    cmndstr.erase(cmndstr.begin(), cmndstr.begin()+16); // remove "ecall_setcontrol"
    cmndstr=trim(cmndstr);
    Strings args = csvSplit(cmndstr, ",");
    if (args.size() != 4) {
        resp = "You must specify 4 args";
    }
    else {
        string autoActivation = args[0];
        string testCall = args[1];
        string positionTrusted = args[2];
        string vehicleType = args[3];

        int val =0;
        if (!autoActivation.compare("1")) val += 1;
        if (!testCall.compare("1")) val += 2;
        if (!positionTrusted.compare("1")) val += 4;
        if (!vehicleType.compare("1")) val += 8;
    
        //resp = getResponse("AT!MECALLMSDBLK=3,"+cmndstr, 2);
        resp = getResponse("AT!MECALLMSDBLK=3,\""+toString(val)+"\"", 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_setvin(string cmndstr) {
    string resp = "";
    //cmndstr.erase(cmndstr.begin(), cmndstr.begin()+13); // remove "ecall_setvin"
    Strings args = csvSplit(cmndstr, " ");
    if (args.size() != 2) {
        resp = "You must specify 1 arg";
    }
    else {
        string vin = args[1];
    
        resp = getResponse("AT!MECALLMSDBLK=4,"+vin, 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_setstorage(string cmndstr) {
    string resp = "";
    cmndstr.erase(cmndstr.begin(), cmndstr.begin()+16); // remove "ecall_setstorage"
    cmndstr=trim(cmndstr);
    Strings args = csvSplit(cmndstr, ",");
    if (args.size() != 6) {
        resp = "You must specify 6 args";
    }
    else {
        string gasTankPresent = args[0];
        string dieselTankPresent = args[1];
        string compressedNaturalGas = args[2];
        string liquidPropaneGas = args[3];
        string electricEnergyStorage = args[4];
        string hydrogenStorage = args[5];
    
        int val =0;
        if (!gasTankPresent.compare("1")) val += 1;
        if (!dieselTankPresent.compare("1")) val += 2;
        if (!compressedNaturalGas.compare("1")) val += 4;
        if (!liquidPropaneGas.compare("1")) val += 8;
        if (!electricEnergyStorage.compare("1")) val += 16;
        if (!hydrogenStorage.compare("1")) val += 32;
    
        //resp = getResponse("AT!MECALLMSDBLK=5,"+cmndstr, 2);
        resp = getResponse("AT!MECALLMSDBLK=5,\""+toString(val)+"\"", 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_settimestamp(string cmndstr) {
    string resp = "";
    //cmndstr.erase(cmndstr.begin(), cmndstr.begin()+15); // remove "ecall_timestamp"
    Strings args = csvSplit(cmndstr, " ");
    if (args.size() != 2) {
        resp = "You must specify 1 arg";
    }
    else {
        string timestamp = args[1];
    
        resp = getResponse("AT!MECALLMSDBLK=6,\""+timestamp+"\"", 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_setlocation(string cmndstr) {
    string resp = "";
    cmndstr.erase(cmndstr.begin(), cmndstr.begin()+17); // remove "ecall_setlocation"
    cmndstr=trim(cmndstr);
    Strings args = csvSplit(cmndstr, ",");
    if (args.size() != 2) {
        resp = "You must specify 2 args";
    }
    else {
        string latitude = args[0];
        string longitude = args[1];

        double lat_temp=atof(latitude.c_str());
        double lon_temp=atof(longitude.c_str());
        int lat_milliarcseconds = int((lat_temp * 3600000) + .5);
        int lon_milliarcseconds = int((lon_temp * 3600000) + .5);

        resp = getResponse("AT!MECALLMSDBLK=7,\"" + toString(lat_milliarcseconds) + ":" +toString(lon_milliarcseconds)+"\"", 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_setdirection(string cmndstr) {
    string resp = "";
    //cmndstr.erase(cmndstr.begin(), cmndstr.begin()+18); // remove "ecall_setdirection"
    Strings args = csvSplit(cmndstr, " ");
    if (args.size() != 2) {
        resp = "You must specify 1 args";
    }
    else {
        string direction = args[1];

        double dDirection =atof(direction.c_str());
        //int iDirection = int((dDirection / 2 ) + .5);
        int iDirection = int((dDirection / 2 ));

        resp = getResponse("AT!MECALLMSDBLK=8,\"" + toString(iDirection) +"\"", 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall_setcfg(string cmndstr) {
    string resp = "";
    cmndstr.erase(cmndstr.begin(), cmndstr.begin()+12); // remove "ecall_setcfg"
    cmndstr=trim(cmndstr);
    Strings args = csvSplit(cmndstr, ",");
    if (args.size() != 2) {
        resp = "You must specify 2 args";
    }
    else {
        string number = args[0];
        string redials = args[1];

        resp = getResponse("AT!MECALLCFG=0,0,1,\"" + number + "\",0," +redials+ ",5,8,1", 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string ecall(string cmndstr) {
    string resp = "";
    cmndstr.erase(cmndstr.begin(), cmndstr.begin()+5); // remove "ecall"
    cmndstr=trim(cmndstr);
    Strings args = csvSplit(cmndstr, ",");
    if ((args.size() == 0) || (args.size() > 2)) {
        resp = "You must specify 1 or 2 args";
    }
    else {
        //string startStop = args[0];
        //string typeCall = args[1];

        resp = getResponse("AT!MECALL=" + cmndstr, 2);
        if (resp == "OK") {
            resp = "OK";
        }
        else {
            resp = "Failed";
        }
    }
    return resp;
}

string unpack8to7(string instr, int len) {
    string outstr = "";
    int idx = 0;
    int bit = 1;
    int remain = 0;
    int bytes[140];
    for (int i=0; i < instr.size()/2; i++) {
        string hex = instr.substr(i*2, 2);
        int byte;
        sscanf(hex.c_str(), "%x", &byte);
        bytes[i] = byte;
        //printf("%02X ", byte);
    }
    //printf("\n");
    int numchrs = len * 8 / 7;
    //cout << "numchrs:" << numchrs << endl;
    for (int i=0; i < numchrs; i++) {
        //printf("remain:%02X bit:%d idx:%d\n", remain, bit, idx);
        char ch;
        if (bit == 8) {
            ch = remain;
            bit = 1;
            remain = 0;
        }
        else {
            ch = (bytes[idx] >> bit) | remain;
            remain = (bytes[idx] << (7 - bit)) & 0x7F;
            idx += 1;
            bit += 1;
        }
        if (ch != 0) {
            //printf("val:%02x\n", ch);
            //cout << idx << ": '" << ch << "'" << endl;
            outstr += ch;
        }
    }
    return outstr;
}

string unhex(string msg) {
    string retstr = msg;
    if ( ((msg.size()%2)==0) and
         (msg.find_first_not_of("01234567890ABCDEF") == string::npos) ) {
        string str = "";
        bool asc = true;
        bool two_byte = true;
        for (int i=0; i < msg.size(); i+=2) {
            char byte_str[3];
            strcpy(byte_str, msg.substr(i,2).c_str());
            int byte_val = 0;
            sscanf(byte_str, "%x", &byte_val);
            if ( two_byte and ((i%4)==0) ) {
                if (byte_val == 0x00) {
                    continue;
                }
                else if (i == 0) {
                    two_byte = false;
                }
                else {
                    asc = false;
                    break;
                }
            }
            if (byte_val < 0x20) {
                if (byte_val != 0x0A) {
                    asc = false;
                    break;
                }
            }
            else if (byte_val > 0x7E) {
                asc = false;
                break;
            }
            str += (char)byte_val;
        }
        if ( asc and two_byte and ((msg.size()%4) != 0) ) {
            asc = false;
        }
        if (asc) {
           retstr = csvQuote(str);
        }
    }
    return retstr;
}

string hexit(string msg) {
    string outstr = csvQuote(msg);
    bool text = true;
    for (int j=0; j < msg.size(); j++) {
        char ch = msg.at(j);
        if (!isprint(ch) and (ch != '\n')) {
            text = false;
            break;
        }
    }
    if (!text) {
        outstr = "";
        for (int j=0; j < msg.size(); j++) {
            char ch = msg.at(j);
            outstr += string_printf("%02X", ch);
        }
    }
    return outstr;
}

string deletesms(string cmndstr) {
    string outstr = "Error";
    cmndstr.erase(0, 10);       // Remove "deletesms "
    string idx = cmndstr;
    string resp = getResponse("AT+CMGD="+idx);
    if (resp.find("OK") != string::npos) {
        outstr = "OK";
    }
    return outstr;
}

string clearsms() {
    string outstr = "OK";
    string resp = getResponse("AT+CMGL=\"ALL\"");
    Strings lines = split(resp, "\n");
    for (int i=0; i < lines.size(); i++) {
        string line = lines[i];
        if (beginsWith(line, "+CMGL: ")) {
            string msg = getField(line, ": ,", 1);
            string cmnd = "AT+CMGD=" + msg;
            string resp2 = getResponse(cmnd);
        }
    }
    return outstr;
}

string loadprl(string prl_file) {
    string resp = "ERROR";
    killConnection();
    string output = backtick("ow_prl_loader -f "+prl_file+" -d /dev/"+qualcomm_device);
    if (beginsWith(output, "Success")) {
        cout << getDateTime() << " PRL loaded" << endl;
        if (resetIt()) {
            resp = "OK";
        }
    }
    resumeConnection();
    return resp;
};
void killConnection() {
    cout << getDateTime() << " Disabling connections" << endl;
    touchFile(DontConnectFile);
    touchFile(NoCellCommandsFile);
    system("pkill pppd");
    sleep(1);
}

void resumeConnection() {
    cout << getDateTime() << " Re-enabling connections" << endl;
    unlink(NoCellCommandsFile);
    unlink(DontConnectFile);
}

string getResponse(string command) {
    return getResponse(command, "OK\n", 1, "");
}

string getResponse(string command, int timeout) {
    return getResponse(command, "OK\n", timeout, "");
}

string getResponse(string command, string ans, int timeout, string busy_resp) {
    bool done = false;
    string outstr = "";
    string cmndstr = command;
    string expresp = command;
    Readers::iterator ri;
    if (expresp.length() >= 2) {
        expresp.erase(0,2);	// Get rid of "AT"
        if ((expresp.length() > 0) and (!isalpha(expresp.at(0)))) {
            expresp.erase(0,1);
        }
        size_t endpos = expresp.find_first_of("=?");
        if (endpos != string::npos) {
            expresp.erase(endpos);
        }
    }
    stringToUpper(expresp);
    cmndstr += "\r";
    time_t start_time = time(NULL);
    if (log_at) {
        cout << getDateTime() << " Sending: " << cmndstr << endl;
    }
    alarm(60);
    write(ser_s, cmndstr.c_str(), cmndstr.size());
    char last_ch = '\n';
    string line = "";
    while (!done) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser_s, &rd);
        nfds = max(nfds, ser_s);
        if (busy_resp != "") {
            FD_SET(listen_s, &rd);
            nfds = max(nfds, listen_s);
            for (ri=readers.begin(); ri != readers.end(); ri++) {
                int rs = (*ri).first;
                bool opened = (*ri).second;
                if (opened) {
                    FD_SET(rs, &rd);
                    nfds = max(nfds, rs);
                }
            }
        }
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (r > 0) {
            if (FD_ISSET(ser_s, &rd)) {
                char buf[255];
                int res = read(ser_s,buf,255);
                int i;
                if (res>0) {
                    for (i=0; i < res; i++) {
                        char ch = buf[i];
                        if (ch == '\r') {
                            ch = '\n';
                        }
                        if ( (ch == '\n') and (ch == last_ch) ) {
                            continue;
                        }
                        last_ch = ch;
                        line += ch;
                        string tmpout = outstr + line;
                        if (tmpout.find(ans) != string::npos) {
                            done = true;
                        }
                        if (ch == '\n') {
                            if ( ( (expresp.length() == 0 ) or
                                   (line.find(expresp) == string::npos) ) and
                                 isUnsolicitedEvent(line) ) {
                                unsolicitedEvent(line);
                            }
                            else {
                                outstr += line;
                            }
                            line = "";
                        }
                    }
                }  //end if res>0
            }
            if (busy_resp != "") {
                if (FD_ISSET(listen_s, &rd)) {
                    struct sockaddr client_addr;
                    unsigned int l = sizeof(struct sockaddr);
                    memset(&client_addr, 0, l);
                    int rs = accept(listen_s, (struct sockaddr *)&client_addr, &l);
                    if (rs < 0) {
                        perror ("accept()");
                    }
                    else {
                        readers[rs] = true;
                    }
                }
                for (ri=readers.begin(); ri != readers.end(); ri++) {
                    int rs = (*ri).first;
                    bool set = (*ri).second;
                    if (set and FD_ISSET(rs, &rd)) {
                        alarm(1);
                        char read_buf[255];
                        int res = read(rs, read_buf, 255);
                        alarm(0);
                        if (res > 0) {
                            read_buf[res] = '\0';
                            while ( (read_buf[res-1] == '\n') or
                                 (read_buf[res-1] == '\r') ) {
                                read_buf[--res] = '\0';
                            }
                            if (strncasecmp(read_buf, "exit", 4) == 0) {
                                close(rs);
                                readers[rs] = false;
                            }
                            else {
                                string resp = "busy:"+busy_resp;
                                resp += "\n";
                                write(rs, resp.c_str(), resp.size());
                            }
                        }
                        else {
                            close(rs);
                            readers[rs] = false;
                        }
                    }
                }
                bool closed = true;
                while (closed) {
                    closed = false;
                    for (ri=readers.begin(); ri != readers.end(); ri++) {
                        int rs = (*ri).first;
                        bool set = (*ri).second;
                        if (!set) {
                            readers.erase(rs);
                            closed = true;
                            break;
                        }
                    }
                }
            }
        }
        else {
            if ((time(NULL) - start_time) >= timeout) {
                outstr += line;
                line = "";
                if (debug) {
                    cerr << getDateTime() << " Timeout:" << command << endl;
                    while ((outstr.size() > 0) and (outstr.at(0) == '\n')) {
                         outstr.erase(0,1);
                    }
                    while ((outstr.size() > 0) and (outstr.at(outstr.size()-1) == '\n')) {
                        outstr.erase(outstr.size()-1);
                    }
                    size_t pos;
                    while ((pos=outstr.find("\n\n")) != string::npos) {
                        outstr.erase(pos,1);
                    }
                    cerr << outstr << endl;
                }
                done = true;
            }
        }
    }  //while (!done)
    outstr += line;
    alarm(0);
    while ((outstr.size() > 0) and (outstr.at(0) == '\n')) {
         outstr.erase(0,1);
    }
    while ((outstr.size() > 0) and (outstr.at(outstr.size()-1) == '\n')) {
        outstr.erase(outstr.size()-1);
    }
    size_t pos;
    while ((pos=outstr.find("\n\n")) != string::npos) {
        outstr.erase(pos,1);
    }
    cmndstr = command + "\n";
    if (strncmp(outstr.c_str(),cmndstr.c_str(),cmndstr.size()) == 0) {
        outstr = outstr.substr(cmndstr.size());
    }
//    if (outstr.find("ERROR") != string::npos) {
//        cerr << command << endl;
//    }
    if ((pos=outstr.find("\nOK")) != string::npos) {
        outstr.erase(pos);
    }
//    while ((pos=outstr.find("\n")) != string::npos) {
//        outstr.replace(pos,1,";");
//    }
    if (log_at) {
        cout << getDateTime() << " Output: " << outstr << endl;
    }
    return outstr;
}

string getRestOfLine() {
    bool done = false;
    string outstr = "";
    alarm(60);
    while (!done) {
        fd_set rd;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_SET(ser_s, &rd);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        nfds = max(nfds, ser_s);
        int r = select(nfds+1, &rd, NULL, NULL, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (r > 0) {
            if (FD_ISSET(ser_s, &rd)) {
                char buf[255];
                int res = read(ser_s,buf,255);
                int i;
                if (res>0) {
                    for (i=0; i < res; i++) {
                        char ch = buf[i];
                        if (ch == '\r') {
                            ch = '\n';
                        }
                        if (ch == '\n') {
                            done = true;
                            break;
                        }
                        outstr += ch;
                    }
                }  //end if res>0
            }
        }
        else {
            done = 1;
        }
    }  //while (!done)
    alarm(0);
    return outstr;
}

bool isUnsolicitedEvent(string line) {
    bool retval = false;
    for (string *sp=unsol_events; *sp != ""; sp++) {
        if (beginsWith(line,*sp)) {
            retval = true;
            break;
        }
    }
    return retval;
}

void unsolicitedEvent(string unsol_line) {
    if (unsol_line[unsol_line.length()-1] == '\n') {
        unsol_line.erase(unsol_line.length()-1);
    }
    if (unsol_line.find("_OBSTIME:") != string::npos) {
        if (debug) {
            string gmttime = make_gmt_time("unsol", unsol_line);
        }
    }
    else {
        string datetime = getDateTime();
        cout << datetime << " " << unsol_line << endl;
    }
}

bool beginsWith(string str, string match) {
    return (strncmp(str.c_str(), match.c_str(), match.size()) == 0);
}

void stringToUpper(string &s) {
    for (int l=0; l < s.length(); l++) {
       s[l] = toupper(s[l]);
    }
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
    closeAndExit(0);
}

void onAlrm(int parm) {
    // Do nothing for now, just used to break out of blocking reads
    signal(SIGALRM, onAlrm);
}

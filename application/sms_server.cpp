/******************************************************************************
 * File sms_server.cpp
 *
 * Daemon to send/recieve SMS commands/responses from Option Wireless cell module
 *
 ******************************************************************************
 * Change log:
 * 2013/01/08 rwp
 *    Initial version
 ******************************************************************************
 */

#include <map>
#include <cstring>
#include <string>
#include <iostream>
#include <vector>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "autonet_files.h"
#include "backtick.h"
#include "csv.h"
#include "my_features.h"
#include "getField.h"
#include "getParm.h"
#include "getUptime.h"
#include "readAssoc.h"
#include "writeAssoc.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
#include "touchFile.h"
#include "dateTime.h"
#include "readAssoc.h"
#include "writeAssoc.h"
using namespace std;

#define serial_device "celldiag"
#define SmsListenPort 5055

class ReaderInfo {
  public:
    bool opened;
    bool unsol_listener;
    string number;
    int socket;

    ReaderInfo() {
        opened = true;
        unsol_listener = false;
        number = "";
        socket = -1;
    }
};

class SmsInfo {
  public:
    string index;
    string number;
    string data;

    SmsInfo() {
        index = "";
        number = "";
        data = "";
    }
};

typedef map<int,ReaderInfo> Readers;
typedef vector<SmsInfo> SmsList;

void selectTech(string tech);
void saveGsmConfig(Assoc gsm_config);
void updateHardwareInfo();
void closeAndExit(int exitstat);
void openPort(const char *port);
void closePort();
bool lock(const char *dev);
string processCommand(int rs, const char *command);
string processSend(int rs, string cmndstr);
bool sendSms(string number, string mesg, bool enc_hex);
string formatGsmPdu(string number, string mesg);
string processRecv(int rs, string cmndstr);
void processSms(SmsList list);
string help();
string getModel();
string make_gmt_time(string who, string obstime);
string checksms();
bool hasSms();
string clearsms();
string listsms();
SmsList getSmsList();
string readsms(string cmndstr);
string unhex(string msg);
string deletesms(string cmndstr);
string del_sms(string idx);
string getResponse(string command);
string getResponse(string command, int timeout);
string getResponse(string command, string resp, int timeout);
string getRestOfLine();
bool beginsWith(string str, string match);
int tcpListenSocket(int listen_port);
void onTerm(int parm);
void onAlrm(int parm);

bool debug = true;
int ser_s = -1;		// Serial port descriptor
int listen_s = -1;	// Listen socket for command connections
struct termios oldtio;	// Old port settings for serial port
Readers readers;
string lockfile = "";
string ow_model = "";
bool gsm_model = false;
bool activation_required = false;
unsigned char mesg_reference = 0;

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

    Features features;
    bool gsmEnabled = features.exists(GsmEnabled);

    listen_s = tcpListenSocket(SmsListenPort);
    if (listen_s < 0) {
        closeAndExit(-1);
    }
   
    openPort(serial_device);

    (void)getResponse("AT",5); // Throw away old unsolicited responses
    ow_model = getModel();
    if (ow_model == "") {
        cerr << "Get model failed" << endl;
        ow_model == "unknown";
    }
    else if (ow_model == "GTM609") {
        gsm_model = false;
        string oimgact = getResponse("AT_OIMGACT?",1);
        string tech = getParm(oimgact, "Tech: ", "\n");
        cerr << "Current tech:" << tech << endl;
        if (gsmEnabled) {
            if (tech != "UMTS") {
                string oimglst = getResponse("AT_OIMGLST?",1);
                if (oimglst.find("Tech: UMTS") != string::npos) {
                    cerr << "Setting to use UMTS" << endl;
                    selectTech("UMTS");
                    gsm_model = true;
                }
            }
            else {
                gsm_model = true;
            }
        }
        else {
            if (tech != "CDMA") {
                string oimglst = getResponse("AT_OIMGLST?",1);
                if (oimglst.find("Tech: CDMA") != string::npos) {
                    cerr << "Setting to use CDMA" << endl;
                    selectTech("CDMA");
                    activation_required = true;
                }
                else {
                    gsm_model = true;
                }
            }
            else {
                activation_required = true;
            }
        }
    }
    else if (ow_model == "GTM601") {
        gsm_model = true;
    }
    else {
        cerr << "Unknown model number" << endl;
    }
    if (gsm_model) {
        Assoc gsm_config = readAssoc(GsmConfFile, ":");
        string iccid = getResponse("AT+ICCID", 1);
        if (beginsWith(iccid,"ICCID:")) {
            iccid = getField(iccid,": ",1);
            if ( (gsm_config.find("iccid") == gsm_config.end()) or
                 (iccid != gsm_config["iccid"]) ) {
                gsm_config.clear();
                gsm_config["iccid"] = iccid;
                saveGsmConfig(gsm_config);
            }
            string cpin = getResponse("AT+CPIN?",1);
            if (cpin.find("SIM PIN") != string::npos) {
                if (gsm_config.find("pin") != gsm_config.end()) {
                    string pin = gsm_config["pin"];
                    cpin = getResponse("AT+CPIN="+pin, 1);
                    if (cpin.find("OK") == string::npos) {
                        gsm_config.erase("pin");
                        saveGsmConfig(gsm_config);
                    }
                }
            }
        }
        else {
            iccid = "";
        }
        getResponse("AT+CMGF=1");
    }
    else {
        getResponse("AT$QCMGF=1");
    }

    updateHardwareInfo();
    unsigned long last_sms_check = getUptime();

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
            bool opened = readers[rs].opened;
            if (opened) {
                FD_SET(rs, &rd);
                nfds = max(nfds, rs);
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
            if (res < 0) {
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
                            if (debug) {
                                if (unsol_line.find("_OBSTIME:") != string::npos) {
                                    string gmttime = make_gmt_time("unsol", unsol_line);
                                }
                                else {
                                    string datetime = getDateTime();
                                    cerr << datetime << " " << unsol_line << endl;
                                }
                            }
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
                ReaderInfo ri;
                ri.socket = rs;
                readers[rs] = ri;
            }
        }
        for (ri=readers.begin(); ri != readers.end(); ri++) {
            int rs = (*ri).first;
            bool opened = readers[rs].opened;
            if (opened and FD_ISSET(rs, &rd)) {
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
                        readers[rs].opened = false;
                    }
                    else {
                        if (unsol_line != "") {
                            unsol_line += getRestOfLine();
                            if (debug) {
                                string datetime = getDateTime();
                                cerr << datetime << " " << unsol_line << endl;
                            }
                            unsol_line = "";
                        }
                        string resp = processCommand(rs, read_buf);
                        resp += "\n";
                        write(rs, resp.c_str(), resp.size());
                    }
                }
                else {
                    close(rs);
                    readers[rs].opened = false;
                }
            }
        }
        bool closed = true;
        while (closed) {
            closed = false;
            for (ri=readers.begin(); ri != readers.end(); ri++) {
                int rs = (*ri).first;
                bool opened = readers[rs].opened;
                if (!opened) {
                    readers.erase(rs);
                    closed = true;
                    break;
                }
            }
        }
        unsigned long now = getUptime();
        if ((now-last_sms_check) >= 5) {
            SmsList list = getSmsList();
            if (list.size() > 0) {
                processSms(list);
            }
            last_sms_check = now;
        }

    }  //while (1)
    closeAndExit(0);
    return 0;
}  //end of main

void selectTech(string tech) {
    (void)getResponse("AT_OIMGACT="+tech, 1);
}

void saveGsmConfig(Assoc gsm_config) {
    string tmp_conf = GsmConfFile + string(".tmp");
    writeAssoc(tmp_conf, gsm_config, ":");
    rename(tmp_conf.c_str(), GsmConfFile);
}

void updateHardwareInfo() {
    Assoc info = readAssoc(HardwareConfFile, " ");
    info["cell_model"] = ow_model;
    info["cell_type"] = ow_model;
    info["cell_vendor"] = "Option";
    if (gsm_model) {
        info["cell_isgsm"] = "yes";
    }
    else {
        Assoc::iterator it = info.find("cell_isgsm");
        if (it != info.end()) {
           info.erase(it);
        }
    }
    if (activation_required) {
        info["activation_required"] = "yes";
    }
    string tmpfile = HardwareConfFile;
    tmpfile += ".tmp";
    writeAssoc(tmpfile, info, " ");
    rename(tmpfile.c_str(), HardwareConfFile);
}

void closeAndExit(int exitstat) {
    closePort();
    while (readers.size() > 0) {
         int rs = (*readers.begin()).first;
//    Readers::iterator ri;
//    for (ri=readers.begin(); ri != readers.end(); ri++) {
//        int rs = (*ri).first;
//         bool opened = (*ri).second;
         if (readers[rs].opened) {
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

string processCommand(int rs, const char *command) {
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
    else if (cmndstr == "checksms") {
        resp = checksms();
    }
    else if (cmndstr == "clearsms") {
        resp = clearsms();
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
    else if (beginsWith(cmndstr, "send ")) {
        resp = processSend(rs, cmndstr);
    }
    else if (beginsWith(cmndstr, "recv ")) {
        resp = processRecv(rs, cmndstr);
    }
    else {
        resp = "Invalid command";
    }
    return resp;
}

string processSend(int rs, string cmndstr) {
    string resp = "";
    Strings args = csvSplit(cmndstr, " ");
    args.erase(args.begin()); // Remove "send"
    bool req_resp = false;
    bool enc_hex = false;
    while (args.size() > 2) {
        if (args[0] == "-r") {
            req_resp = true;
            args.erase(args.begin());
        }
        else if (args[0] == "-h") {
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
            if (req_resp) {
                readers[rs].number = number;
            }
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
    if (gsm_model) {
        if (enc_hex) {
            resp = getResponse("AT+CMGF=0");
            string pdu = formatGsmPdu(number, mesg);
//            cout << "PDU:" << pdu << endl;
            resp = getResponse("AT+CMGS="+toString(pdu.size()/2), ">", 1);
            if (resp.find(">") != string::npos) {
                resp = getResponse(pdu + "\r\x1a", 5);
                if (resp.find("+CMGS:") != string::npos) {
                    success = true;
                }
                else {
//                    cout << "sms response:" << resp << endl;
                }
            }
            resp = getResponse("AT+CMGF=1");
        }
        else {
            resp = getResponse("AT+CMGS=\""+number+"\"", ">", 1);
            if (resp.find(">") != string::npos) {
                resp = getResponse(mesg + "\x1a", 5);
                if (resp.find("+CMGS:") != string::npos) {
                    success = true;
                }
                else {
//                    cout << "sms response:" << resp << endl;
                }
            }
        }
    }
    else {
        if (enc_hex) {
            resp = getResponse("AT$QCMGF=0");
            resp = getResponse("AT$QCMGS=\""+number+"\",0",">", 1);
            if (resp.find(">") != string::npos) {
                resp = getResponse(mesg + "\x1a", 5);
                if (resp.find("$QCMGS:") != string::npos) {
                    success = true;
                }
                else {
//                    cout << "sms response:" << resp << endl;
                }
            }
            resp = getResponse("AT$QCMGF=1");
        }
        else {
            resp = getResponse("AT$QCMGS=\""+number+"\"",">", 1);
            if (resp.find(">") != string::npos) {
                resp = getResponse(mesg + "\x1a", 5);
                if (resp.find("$QCMGS:") != string::npos) {
                    success = true;
                }
                else {
//                    cout << "sms response:" << resp << endl;
                }
            }
        }
    }
    return success;
}

string formatGsmPdu(string number, string mesg) {
    string pdu = "";
    pdu += "00";	// Length of SMSC (use stored SMSC)
    pdu += "01";	// First octed (SMS-SUBMIT, no validity period)
    pdu += string_printf("%02X", mesg_reference);
    string number_plan = "A1";
    if (beginsWith(number, "+")) {
        number_plan = "91";
        number.erase(0,1);
    }
    pdu += string_printf("%02X", number.size());
    pdu += number_plan;
    if ((number.size()%2) == 1) {
        number += "F";
    }
    string swapped_number = "";
    for (int i=0; i < number.size(); i+=2) {
        swapped_number += number.at(i+1);
        swapped_number += number.at(i);
    }
    pdu += swapped_number;
//    pdu += "00";	// PID (no telematic interworking)
    pdu += "20";	// PID (telematic interworking, no class)
    pdu += "04";	// DCS (8bit, no class)
			// VP (no validy period)
    pdu += string_printf("%02X", mesg.size()/2);
    pdu += mesg;
    return pdu;
}
// lenght 24
// 00 01 00 0B 91 9191473191F5 20 04 0A 30313233343536373839
// 0001000B919191473191F520040A30313233343536373839
// 0001000B919191473191F520040A30313233343536373839
// 0001000B919191066769F820040A30313233343536373839
//  1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4
// 0001000B919191653184F920040A30313233343536373839
// 00 01 00 0B 91 9191653184F9 20 04 0A 30313233343536373839
// 000100            0B919191653184F92004              0A30313233343536373839
// Recvd
// 07914140279568F0040B919191653184F920043110907101840A0A30313233343536373839
// 07914140279568F0 04  0B919191653184F9 20  04  3110907101840A 0A 30313233343536373839
// SMSC-number      1st Orig-number      PID DCS Timestamp
//     14047259860x 

string processRecv(int rs, string cmndstr) {
    string resp = "OK";
    Strings args = csvSplit(cmndstr, " ");
    args.erase(args.begin()); // Remove "recv"
    bool unsol_reader = false;
    bool enc_hex = false;
    while (args.size() > 0) {
        if (args[0] == "-u") {
            unsol_reader = true;
            args.erase(args.begin());
        }
        else if (args[0] == "-h") {
            enc_hex = true;
            args.erase(args.begin());
        }
        else {
            break;
        }
    }
    if (unsol_reader) {
        readers[rs].unsol_listener = true;
    }
    else if (args.size() > 0) {
        string number = args[0];
        readers[rs].number = number;
    }
    return resp;
}

void processSms(SmsList list) {
    while (list.size() > 0) {
        string idx = list[0].index;
        string number = list[0].number;
        string data = list[0].data;
        bool processed = false;
        Readers::iterator ri;
        for (ri=readers.begin(); ri != readers.end(); ri++) {
            int rs = (*ri).first;
            if (readers[rs].number == number) {
                string resp = "mesg "+number+" "+csvQuote(data)+"\n";
                write(rs, resp.c_str(), resp.size());
                processed = true;
            }
        }
        if (!processed) {
            for (ri=readers.begin(); ri != readers.end(); ri++) {
                int rs = (*ri).first;
                if (readers[rs].unsol_listener) {
                    string resp = "mesg "+number+" "+csvQuote(data)+"\n";
                    write(rs, resp.c_str(), resp.size());
                }
            }
        }
        del_sms(idx);
        list.erase(list.begin());
    }
}

string help() {
    string outstr = "at*,help,checksms,clearsms,listsms,readsms,deletesms,send,recv";
    return outstr;
}

string getModel() {
    string model = getResponse("AT+GMM");
    if (model.find("ERROR") != string::npos) {
        model = "";
    }
    return model;
}

string make_gmt_time(string who, string obstime) {
    string datetime = getDateTime();
    string outstr = "";
    string low_s = getField(obstime, ": ,", 1);
    string hi_s = getField(obstime, ": ,", 2);
    string leaps_s = getField(obstime, ": ,", 3);
    outstr = hi_s.substr(2) + low_s.substr(2);
    unsigned long low_b = 0;
    unsigned long hi_b = 0;
    int leaps_b = 0;
    sscanf(low_s.c_str(), "0x%lx", &low_b);
    sscanf(hi_s.c_str(), "0x%lx", &hi_b);
    sscanf(leaps_s.c_str(), "%d", &leaps_b);
    if (leaps_b == 0) {
        cerr << datetime << " Invalid leapseconds...setting it to 15" << endl;
        leaps_b = 15;
    }
//    uint64_t time64 = low_b + (((uint64_t)hi_b)<<32);
    u_int64_t time64 = low_b + (((u_int64_t)hi_b)<<32);
//    cerr << "Time in decimal: " << time64 << endl;
    double time_d = (double)time64 * 0.08;
//    fprintf(stderr, "Time in seconds, since Jan 6, 1980: %.2f\n", time_d);
    time_d += 315964800;
    time_d -= leaps_b;
    time_t t = time(NULL);
    double diff = time_d - t;
    if (diff > 5) {
        cerr << datetime << " " << who << " " << obstime << " Diff=" << diff << endl;
    }
//    fprintf(stderr, "Time in seconds, since Jan 1, 1970: %.2f\n", time_d);
//    fprintf(stderr, "Linux time (Jan 1, 1970): %ld = %s\n", t, l_time.c_str());
//    fprintf(stderr, "Diff time: %.2fn", diff);
    struct tm tm;
    int decisec = (int)((time_d - (int)time_d) * 100);
    time_t time_i = (time_t)time_d;
    gmtime_r(&time_i, &tm);
    char timebuf[32];
    sprintf(timebuf, "%04d/%02d/%02d %02d:%02d:%02d.%02d",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, decisec);
    outstr = timebuf;
    return outstr;
}

string checksms() {
    string outstr = "No";
    if ( hasSms() ) {
        outstr = "Yes";
    }
    return outstr;
}

bool hasSms() {
    bool retval = false;
    if (gsm_model) {
        string resp = getResponse("AT+CMGL=\"ALL\"");
        if (beginsWith(resp, "+CMGL:")) {
            retval = true;
        }
    }
    else {
        string resp = getResponse("AT$QCMGL=\"ALL\"");
        if (beginsWith(resp, "$QCMGL:")) {
            retval = true;
        }
    }
    return retval;
}

string clearsms() {
    string outstr = "OK";
    SmsList list = getSmsList();
    for (int i=0; i < list.size(); i++) {
        string idx = list[i].index;
        string cmnd = "AT+CMGD=" + idx;
        string resp2 = getResponse(cmnd);
    }
    return outstr;
}

string listsms() {
    string outstr = "";
    SmsList list = getSmsList();
    for (int i=0; i < list.size(); i++) {
        if (outstr != "") {
            outstr += ";";
        }
        string idx = list[i].index;
        string number = list[i].number;
        string data = list[i].data;
        data = csvQuote(data);
        outstr += idx+":"+number+" "+data;
    }
    if (outstr == "") {
        outstr = "none";
    }
    return outstr;
}

SmsList getSmsList() {
    SmsList list;
    if (gsm_model) {
        string resp = getResponse("AT+CMGL=\"ALL\"");
        Strings lines = split(resp, "\n");
        for (int i=0; i < lines.size(); i++) {
            string line = lines[i];
            if (beginsWith(line, "+CMGL: ")) {
                line.erase(0, 7);
                Strings flds = csvSplit(line, ",");
                string idx = flds[0];
                string number = csvUnquote(flds[2]);
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
                msg = unhex(msg);
                SmsInfo smsinfo;
                smsinfo.index = idx;
                smsinfo.number = number;
                smsinfo.data = msg;
                list.push_back(smsinfo);
            }
        }
    }
    else {
        string resp = getResponse("AT$QCMGL=\"ALL\"");
        Strings lines = split(resp, "\n");
        for (int i=0; i < lines.size(); i++) {
            string line = lines[i];
            if (beginsWith(line, "$QCMGL: ")) {
                line.erase(0, 8);
                Strings flds = csvSplit(line, ",");
                string idx = flds[0];
                string number = flds[2];
                string msg = "";
                for (int j=i+1; j < lines.size(); j++) {
                    string ln = lines[j];
                    if (beginsWith(ln, "$QCMGL: ")) {
                        break;
                    }
                    if (msg != "") {
                        msg += "\n";
                    }
                    msg += ln;
                }
                msg = unhex(msg);
                SmsInfo smsinfo;
                smsinfo.index = idx;
                smsinfo.number = number;
                smsinfo.data = msg;
                list.push_back(smsinfo);
            }
        }
    }
    return list;
}

string readsms(string cmndstr) {
    string outstr = "none";
    cmndstr.erase(0, 8);	// Remove "readsms "
    string idx = cmndstr;
    if (gsm_model) {
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
            msg = unhex(msg);
            msg = csvQuote(msg);
            outstr = idx+":"+number+" "+msg;
        }
    }
    else {
        string resp = getResponse("AT$QCMGR="+idx);
        if (beginsWith(resp, "$QCMGR: ")) {
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
            msg = unhex(msg);
            msg = csvQuote(msg);
            outstr = idx+":"+number+" "+msg;
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
           retstr = str;
        }
    }
    return retstr;
}

string deletesms(string cmndstr) {
    string outstr = "Error";
    cmndstr.erase(0, 10);	// Remove "deletesms "
    string idx = cmndstr;
    outstr = del_sms(idx);
    return outstr;
}

string del_sms(string idx) {
    string outstr = "Error";
    if (gsm_model) {
        string resp = getResponse("AT+CMGD="+idx);
        if (resp.find("ERROR") == string::npos) {
            outstr = "OK";
        }
    }
    else {
        string resp = getResponse("AT$QCMGD="+idx);
        if (resp.find("ERROR") == string::npos) {
            outstr = "OK";
        }
    }
    return outstr;
}

string getResponse(string command) {
    return getResponse(command, "OK\n", 1);
}

string getResponse(string command, int timeout) {
    return getResponse(command, "OK\n", timeout);
}

string getResponse(string command, string ans, int timeout) {
    bool done = false;
    string outstr = "";
//    cout << "getResponse:" << command << endl;
    string cmndstr = command;
    cmndstr += "\r";
    time_t start_time = time(NULL);
    alarm(60);
    write(ser_s, cmndstr.c_str(), cmndstr.size());
    string line = "";
    char lastchar = '\n';
    while (!done) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser_s, &rd);
        nfds = max(nfds, ser_s);
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
                        if ((ch == '\n') and (lastchar == '\n')) {
                            continue;
                        }
                        lastchar = ch;
                        if (ch == '\n') {
//                            cout << line << endl;
                            line = "";
                        }
                        else {
                            line += ch;
                        }
                        outstr += ch;
                        if (outstr.find(ans) != string::npos) {
                            done = true;
                        }
                    }
                }  //end if res>0
            }
        }
        else {
            if ((time(NULL) - start_time) >= timeout) {
                if (debug) {
                    cerr << "Timeout:" << command << endl;
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

bool beginsWith(string str, string match) {
    return (strncmp(str.c_str(), match.c_str(), match.size()) == 0);
}

int tcpListenSocket(int listen_port) {
    struct sockaddr_in a;
    int s;
    int yes;
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = INADDR_ANY;
    a.sin_port = htons(listen_port);
    if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        perror("bind");
        close(s);
        return -1;
    }
    listen(s, 100);
    return s;
}

void onTerm(int parm) {
    cerr << "Got term:" << parm << endl;
    closeAndExit(0);
}

void onAlrm(int parm) {
    // Do nothing for now, just used to break out of blocking reads
    signal(SIGALRM, onAlrm);
}

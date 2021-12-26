/******************************************************************************
 * File qw_talker.cpp
 *
 * Daemon to send/recieve commands/responses from Quanta Computers cell module
 *
 ******************************************************************************
 * Change log:
 * 2013/05/08 rwp
 *    Initial version (copied from ow_talker.cpp)
 * 2014/05/22 rwp
 *    Removed debug print
 *    Removed setting cell_device to eth1
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
#include "readAssoc.h"
#include "split.h"
#include "string_printf.h"
#include "touchFile.h"
#include "writeAssoc.h"
using namespace std;

#define serial_device "celldiag"

//#define _POSIX_SOURCE 1         //POSIX compliant source

typedef map<int,bool> Readers;

void updateHardwareInfo();
void closeAndExit(int exitstat);
void openPort(const char *port);
void closePort();
bool lock(const char *dev);
string processCommand(string cmndstr);
string help();
string getprofile();
string getstatus();
string getmodel();
string getModel();
string gettime();
string make_gmt_time(string who, string obstime);
string getnetinfo();
string getusage();
string reset();
string reboot();
string activate();
string data_connect();
string data_disconnect();
string isconnected();
string checksms();
string sendsms(string cmndstr);
bool sendSms(string number, string mesg, bool enc_hex);
string formatGsmPdu(string number, string mesg);
string listsms();
string readsms(string cmndstr);
void dumpStr(string str);
string unhex(string msg);
string deletesms(string cmndstr);
string clearsms();
void killConnection();
void resumeConnection();
string getResponse(string command);
string getResponse(string command, int timeout);
string getResponse(string command, string resp, int timeout);
string getRestOfLine();
bool beginsWith(string str, string match);
int listenSocket(const char *name);
void onTerm(int parm);
void onAlrm(int parm);

bool debug = true;
int ser_s = -1;		// Serial port descriptor
int listen_s = -1;	// Listen socket for command connections
struct termios oldtio;	// Old port settings for serial port
Readers readers;
string lockfile = "";
string model = "";
bool gsm_model = false;
bool lte_model = false;
bool activation_required = false;
unsigned char mesg_reference = 0;

int main(int argc, char *argv[])
{
    int res;
    char buf[255];                       //buffer for where data is put
    char read_buf[255];                       //buffer for where data is put
    string unsol_line = "";
    string resp;
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
   
    openPort(serial_device);

    (void)getResponse("AT",5); // Throw away old unsolicited responses
    while (true) {
        resp = getResponse("AT",5);
        if (resp == "OK") {
            break;
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
    else if (model == "ALT3100") {
        gsm_model = false;
        lte_model = true;
        activation_required = false;
    }
    else {
        cerr << "Unknown model number" << endl;
    }
//    string iccid = getResponse("AT+ICCID", 1);
//    if (beginsWith(iccid,"ICCID:")) {
//        iccid = getField(iccid,": ",1);
//        if ( (gsm_config.find("iccid") == gsm_config.end()) or
//             (iccid != gsm_config["iccid"]) ) {
//        }
//    string cpin = getResponse("AT+CPIN?",1);
    getResponse("AT+CMGF=1");
    getResponse("AT+CMEE=2",5);
    getResponse("AT%CEER=1",5);
//    getResponse("AT%EXE=stop-logs-to-host.sh",10);
    resp = getResponse("AT%GETCFG=\"USIM_SIMULATOR\"");
    if (resp == "1") {
        getResponse("AT%SETCFG=\"USIM_SIMULATOR\",\"0\"");
    }

    updateHardwareInfo();

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
                cout << getDateTime() << " Nothing received from module...closing" << endl;
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
                                string datetime = getDateTime();
                                cerr << datetime << " " << unsol_line << endl;
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
                                cerr << datetime << " " << unsol_line << endl;
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

    }  //while (1)
    cout << getDateTime() << " Closing" << endl;
    closeAndExit(0);
    return 0;
}  //end of main


void updateHardwareInfo() {
    Assoc info = readAssoc(HardwareConfFile, " ");
    if (model != "") {
        info["cell_model"] = model;
        info["cell_type"] = model;
    }
    info["cell_vendor"] = "Quanta";
    info["cell_islte"] = "yes";
//    info["cell_device"] = "eth1";
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

string processCommand(string cmndstr) {
    string resp = "";
    if (strncasecmp(cmndstr.c_str(), "at", 2) == 0) {
        int at_timeout = 10;
        int pos = cmndstr.find(" to=");
        if (pos != string::npos) {
            at_timeout = stringToInt(cmndstr.substr(pos+4));
            cmndstr.erase(pos);
        }
        resp = getResponse(cmndstr, at_timeout);
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
    else if (cmndstr == "gettime") {
        resp = gettime();
    }
    else if (cmndstr == "getnetinfo") {
        resp = getnetinfo();
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
    else if (cmndstr == "getusage") {
        resp = getusage();
    }
    else if (cmndstr == "reboot") {
        resp = reboot();
    }
    else if (cmndstr == "reset") {
        resp = reset();
    }
    else if (cmndstr == "activate") {
        resp = activate();
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
    else {
        resp = "Invalid command";
    }
    return resp;
}

string help() {
    string outstr = "at*,activate,checksms,listsms,readsms,deletesms,clearsms,sendsms,getnetinfo,getmodel,getprofile,getstatus,getusage,connect,disconnect,isconnected,gettime,help,reboot,reset";
    return outstr;
}

string getprofile() {
    string outstr = "";
    string cgsn = getResponse("AT+CGSN");
    if (cgsn.find("ERROR") == string::npos) {
        outstr += ",imei:" + cgsn;
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    bool simok = false;
    bool simin = false;
    string cpin = getResponse("AT+CPIN?");
//    if (cpin.find("SIM not inserted") != string::npos) {
    if (cpin.find("ERROR") != string::npos) {
        outstr += ",sim:NotInserted";
    }
    else if (cpin.find("SIM PIN") != string::npos) {
        outstr += ",sim:PinRequired";
        simin = true;
    }
    else if (cpin.find("SIM PUK") != string::npos) {
        outstr += ",sim:PukRequired";
        simin = true;
    }
    else if (cpin.find("READY") != string::npos) {
        outstr += ",sim:Ready";
        simin = true;
        simok = true;
    }
    if (simin) {
        string crsm = getResponse("AT+CRSM=176,12258,0,0,10");
        if (beginsWith(crsm,"+CRSM:")) {
            string iccid = getField(crsm,":,", 3);
            string swapped_number = "";
            for (int i=0; i < iccid.size(); i+=2) {
                swapped_number += iccid.at(i+1);
                swapped_number += iccid.at(i);
            }
            iccid = swapped_number;
            outstr += ",iccid:" + iccid;
        }
//        string iccid = getResponse("AT+ICCID");
//        if (beginsWith(iccid,"ICCID:")) {
//            outstr += ",iccid:" + getField(iccid,": ",1);
//        }
//        else {
//            outstr = "ERROR";
//            return outstr;
//        }
    }
    if (simok) {
        string cnum = getResponse("AT+CNUM");
        if (beginsWith(cnum,"+CNUM")) {
            cnum = cnum.substr(6);
            Strings vals = csvSplit(cnum, ",");
            outstr += ",mdn:" + csvUnquote(vals[1]);
        }
//        else {
//            outstr = "ERROR";
//            return outstr;
//        }
        string cimi = getResponse("AT+CIMI");
        if (cimi.find("ERROR") == string::npos) {
            outstr += ",imsi:" + cimi;
        }
//        else {
//            outstr = "ERROR";
//            return outstr;
//        }
//        string osimop = getResponse("AT_OSIMOP");
//        if (beginsWith(osimop, "_OSIMOP:")) {
//            string mcn = getField(osimop,":,",3);
//            mcn = mcn.substr(1,mcn.size()-2);
//            outstr += ",mcn:" + mcn;
//        }
//        else {
//            outstr = "ERROR";
//            return outstr;
//        }
        string cops = getResponse("AT+COPS?");
        if (beginsWith(cops, "+COPS:")) {
            string prov = getField(cops,":,",3);
            if (prov.substr(0,1) == "\"") {
                prov = prov.substr(1,prov.size()-2);
            }
            outstr += ",provider:"+prov;
        }
    }
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string getstatus() {
    string outstr = "";
    string creg = getResponse("AT+CREG?");
    string activated = "1";
    if (beginsWith(creg,"+CREG:")) {
        string stat = getField(creg,": ,",2);
        if (stat == "0") {
            activated = "0";
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
    outstr += ",activated:" + activated;
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    // state:Idle|Connected|Dormant|Disconnecting
    // at+cpas?
    // locked:0|1
    // at????
    // radio:0|1
    // at+cfun?
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
    return outstr;
}

string getModel() {
    string model = getResponse("AT+GMM");
    if (model.find("ERROR") != string::npos) {
        model = "";
    }
    return model;
}

string gettime() {
    string gmttime = "unknown";
    if (!gsm_model) {
        string cclk = getResponse("AT+CCLK?");
        if (beginsWith(cclk, "+CCLK:")) {
            cclk.erase(0,7);
            gmttime = make_gmt_time("gettime", cclk);
        }
    }
    return gmttime;
} 

string make_gmt_time(string who, string timestr) {
// +CCLK: 13/05/08,13:32:18-16
//    string datetime = getDateTime();
    string outstr = "";
    struct tm tm;
    int tz_adjust;
    sscanf(timestr.c_str(),"%d/%d/%d,%d:%d:%d%d",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &tz_adjust);
    tm.tm_year = tm.tm_year + 2000 - 1900;
    tm.tm_mon = tm.tm_mon - 1;
//    time_t gmttime = mktime(&tm);
//    gmttime -= tz_adjust * 15 * 60;
//    struct tm *ptm = gmtime(&gmttime);
    char timebuf[32];
    sprintf(timebuf, "Bad:%04d/%02d/%02d %02d:%02d:%02d",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    outstr = timebuf;
    return outstr;
}

string getnetinfo() {
    string outstr = "";
    string type = "None";
    string roaming = "0";
    bool registered = true;
    string creg = getResponse("AT+CREG?");
    if (beginsWith(creg,"+CREG:")) {
        string stat = getField(creg,": ,",2);
        if (stat == "0") {
        }
        else if (stat == "1") {
            registered = true;
        }
        else if (stat == "2") {
        }
        else if (stat == "3") {
        }
        else if (stat == "4") {
        }
        else if (stat == "5") {
            registered = true;
            roaming = "1";
        }
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    if (!registered) {
        outstr = "type:None,signal:0";
    }
    else {
        type = "LTE";
// AT%CSQ   %CSQ: 15,99,27
        string csq = getResponse("AT+CSQ");
        if (beginsWith(csq,"+CSQ:")) {
            string rssi = getField(csq,":, ",1);
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
    }
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string getusage() {
    string outstr = "None";
    string resp = getResponse("AT%COUNT=\"PDM\"");
    if (beginsWith(resp, "PDM Stats:")) {
        string rx_bytes = getParm(resp, "RX total number of bytes: ", "\n");
        string tx_bytes = getParm(resp, "TX total number of bytes: ", "\n");
        outstr = "rxbytes:"+rx_bytes+",txbytes:"+tx_bytes;
    }
    return outstr;
}

string reset() {
    string outstr = "Failed";
    killConnection();
    string resp = getResponse("AT+CFUN=0", 5);
    if (resp == "OK") {
        sleep(1);
        resp = getResponse("AT+CFUN=1");
        if (resp == "OK") {
            outstr = "OK";
        }
    }
    resumeConnection();
    return outstr;
}

string reboot() {
    string outstr = "Failed";
//    string resp = getResponse("AT+REBOOT");
//    if (resp == "OK") {
//        closePort();
//        sleep(5);
//        system("/etc/init.d/cellular-modules start");
//        openPort(serial_device);
//        outstr = "OK";
//    }
    outstr = "Not Implemented";
    return outstr;
}

string activate() {
    string outstr = "Failed";
    outstr = "Not Implemented";
    return outstr;
}

string data_connect() {
    string outstr = "Failed";
    string resp = getResponse("AT%DPDNACT=1", 60);
    if (resp == "OK") {
        outstr = "OK";
    }
    else {
        resp = getResponse("AT%CEER?");
        if (beginsWith(resp, "%CEER:")) {
            resp.erase(0,6);
            cout << getDateTime() << " Connection fail error: " << resp << endl;
            outstr = "Failed:" + resp;
        }
    }
    return outstr;
}

string data_disconnect() {
    string outstr = "Failed";
    string resp = getResponse("AT%DPDNACT=0", 10);
    if (resp == "OK") {
        outstr = "OK";
    }
    return outstr;
}

string isconnected() {
    string outstr = "no";
    string resp = getResponse("AT%DPDNACT?",5);
    if (resp == "%DPDNACT: 1") {
        outstr = "yes";
    }
    else {
        resp = getResponse("AT%CEER?");
        cerr << getDateTime() << " Connection closed: " << resp << endl;
    }
    return outstr;
}

string checksms() {
    string outstr = "No";
    string resp = getResponse("AT+CMGL=\"ALL\"");
    if (resp.substr(0,2) == " \n") {
        resp.erase(0,2);
    }
    if (beginsWith(resp, "+CMGL:")) {
        outstr = "Yes";
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
        string pdu = formatGsmPdu(number, mesg);
        cout << "PDU:" << pdu << endl;
        resp = getResponse("AT+CMGS="+toString(pdu.size()/2), ">", 1);
        if (resp.find(">") != string::npos) {
            resp = getResponse(pdu + "\r\x1a", 5);
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
        resp = getResponse("AT+CMGS=\""+number+"\"", ">", 1);
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

string formatGsmPdu(string number, string mesg) {
    string pdu = "";
    pdu += "00";        // Length of SMSC (use stored SMSC)
    pdu += "01";        // First octed (SMS-SUBMIT, no validity period)
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
//    pdu += "00";      // PID (no telematic interworking)
    pdu += "20";        // PID (telematic interworking, no class)
    pdu += "04";        // DCS (8bit, no class)
                        // VP (no validy period)
    pdu += string_printf("%02X", mesg.size()/2);
    pdu += mesg;
    return pdu;
}

string listsms() {
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
            msg = unhex(msg);
            msg = csvQuote(msg);
            outstr += idx+":"+number+" "+msg;
        }
    }
    if (outstr == "") {
        outstr = "none";
    }
    return outstr;
}

string readsms(string cmndstr) {
    string outstr = "none";
    cmndstr.erase(0, 8);        // Remove "readsms "
    string idx = cmndstr;
    string resp = getResponse("AT+CMGR="+idx);
    if (resp.substr(0,2) == " \n") {
        resp.erase(0,2);
    }
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
    return outstr;
}

void dumpStr(string str) {
    cout << "len:" << str.size() << endl;
    for (int i=0; i < str.size(); i++) {
        char ch = str.at(i);
        printf("%02X", ch);
    }
    printf("\n");
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

void killConnection() {
    cerr << getDateTime() << " Disabling connections" << endl;
    touchFile(DontConnectFile);
    touchFile(NoCellCommandsFile);
    system("pkill pppd");
    sleep(1);
}

void resumeConnection() {
    cerr << getDateTime() << " Re-enabling connections" << endl;
    unlink(NoCellCommandsFile);
    unlink(DontConnectFile);
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
    string cmndstr = command;
    cmndstr += "\n";
    string datetime = getDateTime();
//    cout << datetime << " Command: " << command << endl;
    time_t start_time = time(NULL);
    bool timeout_occured = false;
    alarm(60);
    write(ser_s, cmndstr.c_str(), cmndstr.size());
    char last_ch = '\n';
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
                        if ( (ch == '\n') and (ch == last_ch) ) {
                            continue;
                        }
                        last_ch = ch;
                        outstr += ch;
//                        if (ch == '\n') {
                            if (outstr.find(ans) != string::npos) {
                                done = true;
                            }
                            else if (outstr.find("ERROR\n") != string::npos) {
                                done = true;
                            }
//                        }
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
                timeout_occured = true;
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
    if (timeout_occured and (outstr == "")) {
        outstr = "TIMEOUT";
    }
    datetime = getDateTime();
//    cout << datetime << " Response: " << outstr << endl;
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

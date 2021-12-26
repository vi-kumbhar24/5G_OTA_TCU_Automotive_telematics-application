/******************************************************************************
 * File ow_talker.cpp
 *
 * Daemon to send/recieve commands/responses from Option Wireless cell module
 *
 ******************************************************************************
 * Change log:
 * 2012/08/09 rwp
 *    Added getting operator selection in getprofile_gsm
 * 2012/08/21 rwp
 *    Fixed GSM getprofile to work with no SIM or wrong PIN
 *    Added checking of ICCID for GSM
 *    Added return PIN indictation on GSM getprofile
 * 2012/08/22 rwp
 *    Changed GSM getprofile to fetch ICCID if SIM is present
 * 2012/12/07 rwp
 *    Delete all GSM info on SIM change
 * 2012/12/13 rwp
 *    Added resetgps command
 * 2012/12/28 rwp
 *    Fixed MDN code for GSM
 * 2013/01/03 rwp
 *    Fixed resetgps command
 * 2013/01/14 rwp
 *    Added SMS commands
 * 2013/06/05 rwp
 *    Don't reset cell after loadprl
 *    GSM getprofile returns prl:-1
 * 2013/07/01 rwp
 *    AUT-41: Turn on A-GPS if non-gsm and version >= 2.2.46.0
 * 2013/10/24 rwp
 *    Start GPS different based on GpsInitMode feature
 * 2013/11/08 rwp
 *    AUT-91: Dont start aGPS in less than 15 minutes
 * 2014/03/24 rwp
 *    Added sending version with getmodel response
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
#include "readAssoc.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
#include "touchFile.h"
#include "updateFile.h"
#include "writeAssoc.h"
using namespace std;

#define serial_device "celldiag"

//#define _POSIX_SOURCE 1         //POSIX compliant source

typedef map<int,bool> Readers;

void selectTech(string tech);
void saveGsmConfig(Assoc gsm_config);
void updateHardwareInfo();
void closeAndExit(int exitstat);
void openPort(const char *port);
void closePort();
bool lock(const char *dev);
string processCommand(const char *command);
string help();
string getprofile();
string getprofile_gsm();
string getprofile_cdma();
string getstatus();
string getmodel();
string getModel();
string gettemp();
string gettime();
double getunixtime();
string make_gmt_time(string who, string obstime);
double make_unix_time(string obstime);
string getposition();
string getnetinfo();
string getnetinfo_gsm();
string getnetinfo_cdma();
string reset();
string reboot();
string activate();
string resetgps();
void clearGps();
void startGps();
void setGpsStandalone();
string checksms();
string sendsms(string cmndstr);
bool sendSms(string number, string mesg, bool enc_hex);
string formatGsmPdu(string number, string mesg);
string listsms();
string readsms(string cmndstr);
string unhex(string msg);
string deletesms(string cmndstr);
string clearsms();
string loadprl(string prl_file);
void killConnection();
void resumeConnection();
string getResponse(string command);
string getResponse(string command, int timeout);
string getResponse(string command, string resp, int timeout);
string getRestOfLine();
bool isUnsolicitedEvent(string line);
void unsolicitedEvent(string unsol_line);
bool beginsWith(string str, string match);
void stringToUpper(string &s);
int listenSocket(const char *name);
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
bool resetting_gps = false;
time_t reset_gps_time = 0;
string version = "";
bool set_gps_standalone = false;
Features features;

string unsol_events[] = {
    "_OBSTIME:",
    "_OGPSEVT:",
    ""
};

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

    bool gsmEnabled = features.exists(GsmEnabled);

    listen_s = listenSocket(CellTalkerPath);
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
        cout << getDateTime() << " Current tech:" << tech << endl;
        if (gsmEnabled) {
            if (tech != "UMTS") {
                string oimglst = getResponse("AT_OIMGLST?",1);
                if (oimglst.find("Tech: UMTS") != string::npos) {
                    cout << "Setting to use UMTS" << endl;
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
                    cout << "Setting to use CDMA" << endl;
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
    string oid = getResponse("AT_OID", 1);
    version = getParm(oid, "FWV: ", "\n ");
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
            getResponse("AT+CMGF=1");
        }
        else {
            iccid = "";
            getResponse("AT$QCMGF=1");
        }
    }

    updateHardwareInfo();
    startGps();

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
        if (resetting_gps) {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            tvp = &tv;
        }
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
                        string resp = processCommand(read_buf);
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
        if (resetting_gps) {
            clearGps();
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
    else if (cmndstr == "reboot") {
        resp = reboot();
    }
    else if (cmndstr == "reset") {
        resp = reset();
    }
    else if (cmndstr == "resetgps") {
        resp = resetgps();
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
    string outstr = "at*,activate,checksms,listsms,readsms,deletesms,clearsms,sendsms,getnetinfo,getmodel,getposition,getprofile,getstatus,gettemp,gettime,help,loadprl,reboot,reset,resetgps";
    return outstr;
}

string getprofile() {
    string outstr = "";
    if (gsm_model) {
        outstr = getprofile_gsm();
    }
    else {
        outstr = getprofile_cdma();
    }
    return outstr;
}

string getprofile_gsm() {
    string outstr = "";
    string cgsn = getResponse("AT+CGSN");
    if (cgsn.find(",") != string::npos) {
        outstr += ",imei:" + getField(cgsn,":,",0);
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    bool simok = false;
    bool simin = false;
    string cpin = getResponse("AT+CPIN?");
    if (cpin.find("SIM not inserted") != string::npos) {
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
        string iccid = getResponse("AT+ICCID");
        if (beginsWith(iccid,"ICCID:")) {
            outstr += ",iccid:" + getField(iccid,": ",1);
        }
//        else {
//            outstr = "ERROR";
//            return outstr;
//        }
    }
    if (simok) {
        string cnum = getResponse("AT+CNUM");
        if (beginsWith(cnum,"+CNUM: ")) {
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
            outstr += ",imsi:" + getField(cimi,":",0);
        }
//        else {
//            outstr = "ERROR";
//            return outstr;
//        }
        string osimop = getResponse("AT_OSIMOP");
        if (beginsWith(osimop, "_OSIMOP:")) {
            string mcn = getField(osimop,":,",3);
            mcn = mcn.substr(1,mcn.size()-2);
            outstr += ",mcn:" + mcn;
        }
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
        outstr += ",prl:-1";
    }
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string getprofile_cdma() {
    string outstr = "";
    string cgsn = getResponse("AT+CGSN");
    if (cgsn.find(",") != string::npos) {
        outstr += ",imei:" + getField(cgsn,":,",0);
    }
    string cimi = getResponse("AT+CIMI");
    if (cimi.find("ERROR") == string::npos) {
        outstr += ",imsi:" + getField(cimi,":",0);
    }
    string omeid = getResponse("AT_OMEID");
    if (beginsWith(omeid,"_OMEID")) {
        outstr += ",esn:" + getField(omeid,": ,",1);
    }
    string mdn = getResponse("AT+MDN?");
    if (mdn != "ERROR") {
        outstr += ",mdn:" + mdn;
    }
    string oimsi = getResponse("AT_OIMSI?");
    if (beginsWith(oimsi,"_OIMSI:")) {
        oimsi = getField(oimsi, ": ,", 1);
        string min = oimsi.substr(oimsi.size()-10);
        outstr += ",min:" + min;
    }
    string oprlv = getResponse("AT_OPRLV?");
    if (beginsWith(oprlv, "_OPRLV:")) {
        string prl = getField(oprlv, ": ,", 1);
        outstr += ",prl:" + prl;
    }
    outstr += ",provider:verizon";
    if (outstr == "") {
        outstr = "ERROR";
    }
    else if (beginsWith(outstr,",")) {
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
    outstr += "," + version;
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
    string opatemp = getResponse("AT_OPATEMP?");
    string retstr = "ERROR";
    if (beginsWith(opatemp, "_OPATEMP:")) {
        Strings vals = split(opatemp, ": ,");
        retstr = "temp:" + vals[vals.size()-1];
        if (vals.size() < 5) {
            retstr += ",overtemp:" + vals[2];
        }
        else {
            retstr += ",data_overtemp:" + vals[2];
            retstr += ",voice_overtemp:" + vals[3];
        }
    }
    return retstr;
}

string gettime() {
    string gmttime = "unknown";
    if (!gsm_model) {
        string obstime = getResponse("AT_OBSTIME?");
        if (beginsWith(obstime, "_OBSTIME:")) {
            gmttime = make_gmt_time("gettime", obstime);
        }
    }
    return gmttime;
} 

double getunixtime() {
    double t = 0;
    if (!gsm_model) {
        string obstime = getResponse("AT_OBSTIME?");
        if (beginsWith(obstime, "_OBSTIME:")) {
            t = make_unix_time(obstime);
        }
        else {
            cerr << "Failed getunixtime" << endl;
        }
    }
    return t;
}

string make_gmt_time(string who, string obstime) {
    string outstr = "";
    double time_d = make_unix_time(obstime);
    time_t t = time(NULL);
    double diff = abs(time_d - t);
    if (diff > 5) {
        string datetime = getDateTime();
        cout << datetime << " " << who << " " << obstime << " Diff=" << diff << endl;
    }
//    fprintf(stderr, "Time in seconds, since Jan 1, 1970: %.2f\n", time_d);
//    fprintf(stderr, "Linux time (Jan 1, 1970): %ld = %s\n", t, l_time.c_str());
//    fprintf(stderr, "Diff time: %.2fn", diff);
    struct tm tm;
    int decisec = (int)((time_d - (int)time_d) * 100);
    time_t time_i = (time_t)time_d;
    gmtime_r(&time_i, &tm);
    outstr = string_printf("%04d/%02d/%02d %02d:%02d:%02d.%02d",
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, decisec);
    return outstr;
}

double make_unix_time(string obstime) {
    string low_s = getField(obstime, ": ,", 1);
    string hi_s = getField(obstime, ": ,", 2);
    string leaps_s = getField(obstime, ": ,", 3);
    unsigned long low_b = 0;
    unsigned long hi_b = 0;
    int leaps_b = 0;
    sscanf(low_s.c_str(), "0x%lx", &low_b);
    sscanf(hi_s.c_str(), "0x%lx", &hi_b);
    sscanf(leaps_s.c_str(), "%d", &leaps_b);
    if (leaps_b == 0) {
        string datetime = getDateTime();
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
    return time_d;
}

string getposition() {
    string position = "";
    string obsi = getResponse("AT_OBSI?");
    if (beginsWith(obsi, "_OBSI:")) {
        string lat_s = getField(obsi, ": ,", 2);
        string lon_s = getField(obsi, ": ,", 3);
        double lat = 0;
        double lon = 0;
        sscanf(lat_s.c_str(), "%lf", &lat);
        sscanf(lon_s.c_str(), "%lf", &lon);
        lat /= 14400.;
        lon /= 14400.;
        char buffer[32];
        sprintf(buffer, "%.5f/%.5f", lat, lon);
        position = buffer;
    }
    return position;
}

string getnetinfo() {
    string outstr = "";
    if (gsm_model) {
        outstr = getnetinfo_gsm();
    }
    else {
        outstr = getnetinfo_cdma();
    }
    return outstr;
}

string getnetinfo_gsm() {
    string outstr = "";
    string type = "None";
    string roaming = "0";
    bool registered = false;
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
        string owcti = getResponse("AT_OWCTI?");
        if (beginsWith(owcti,"_OWCTI:")) {
            string ctype = getField(owcti,": ,",1);
            if (ctype == "0") {
    //            type = "Unknown";
            }
            else if (ctype == "1") {
                type = "WCDMA";
            }
            else if (ctype == "2") {
                type = "HSDPA";
            }
            else if (ctype == "3") {
                type = "HSUPA";
            }
            else if (ctype == "4") {
                type = "HSPA";
            }
        }
        else {
            outstr = "ERROR";
            return outstr;
        }
        if (type == "None") {
            string octi = getResponse("AT_OCTI?");
            if (beginsWith(octi,"_OCTI:")) {
                string ctype = getField(octi,": ,",2);
                if (ctype == "0") {
                    type = "Unknown";
                }
                else if (ctype == "1") {
                    type = "GSM";
                }
                else if (ctype == "2") {
                    type = "GPRS";
                }
                else if (ctype == "3") {
                    type = "EDGE";
                }
            }
            else {
                outstr = "ERROR";
                return outstr;
            }
        }
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

string getnetinfo_cdma() {
    string outstr = "";
    string type = "None";
    string roaming = "0";
//    string ossys = getResponse("AT_OSSYS?");
//    if (beginsWith(ossys,"_OSSYS:")) {
//        string mode = getField(ossys,": ,",2);
//        if (mode == "4") {
//            type = "1xRTT";
//        }
//        else if ((mode == "5") or (mode == "6")) {
//            type = "EVDO";
//        }
//    }
//    else {
//        outstr = "ERROR";
//        return outstr;
//    }
    string ocdmarev = getResponse("AT_OCDMAREV?");
    if (beginsWith(ocdmarev,"_OCDMAREV:")) {
        string rev = getField(ocdmarev,": ,",2);
        if (rev == "0x1") {
            type = "1xRTT";
        }
        else if (rev == "0x2") {
            type = "EVDO-0";
        }
        else if (rev == "0x4") {
            type = "EVDO-A";
        }
        else if (rev == "0x8") {
            type = "EVDO-B";
        }
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    if (type == "1xRTT") {
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
    }
    else {
        string cgreg = getResponse("AT+CGREG?");
        if (beginsWith(cgreg,"+CGREG:")) {
            string stat = getField(cgreg,": ,",2);
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
        outstr += ",1xrttsig:";
        outstr += percent_str;
    }
    else {
        outstr = "ERROR";
        return outstr;
    }
    string hdrcsq = getResponse("AT^HDRCSQ");
    if (beginsWith(hdrcsq, "^HDRCSQ:")) {
        string hdrssi = getField(hdrcsq,":, ",1);
//        int hdrssi_v;
//        sscanf(hdrssi.c_str(), "%d", &hdrssi_v);
        outstr += ",evdosig:";
        outstr += hdrssi;
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
    killConnection();
    string resp = getResponse("AT+CDV=*22899", 3);
    if (resp == "OK") {
        time_t start_time = time(NULL);
        bool mdn_set = false;
        bool prl_set = false;
        bool commit = false;
        while ((time(NULL)-start_time) < 60) {
            resp = getResponse("","\n",60);
            if (resp != "") {
                cout << getDateTime() << " Activate: " << resp << endl;
                if (resp.find("OTASP: SPL_UNLOCKED") != string::npos) {
                }
                else if (resp.find("OTASP: PRL_DOWNLOADED") != string::npos) {
                    prl_set = true;
                }
                else if (resp.find("OTASP: MDN_DOWNLOADED") != string::npos) {
                    mdn_set = true;
                }
                else if (resp.find("OTASP: COMMIT_SUCCESSFUL") != string::npos) {
                    commit = true;
                }
                else if (resp.find("OTASP: END") != string::npos) {
                    break;
                }
            }
        }
        if (mdn_set and commit) {
            outstr = "OK";
        }
    }
    resumeConnection();
    return outstr;
}

string resetgps() {
    string resp = "";
    resp = getResponse("AT_OGPS=0");
    resetting_gps = true;
    reset_gps_time = time(NULL);
    return resp;
}

void clearGps() {
    if ((time(NULL)-reset_gps_time) >= 5) {
//        string resp = getResponse("AT_OGPSCLEAR=\"ALL\",1", "\n", 1);
        string resp = getResponse("AT_OGPSCLEAR=\"ALL\",1", "xx", 1);
        if (resp == "OK") {
            cout << "GPS cleared" << endl;
            startGps();
            resetting_gps = false;
        }
        reset_gps_time = time(NULL);
    }
}

void startGps() {
    string gpsinitmode = features.getFeature(GpsInitModeFeature);
    (void)getResponse("AT_OGPSEVT=1",1); // Turn on GPS events
    bool use_agps = false;
    if (!gsm_model) {
        if (version >= "2.2.46.0") {
            use_agps = true;	// May try to use aGPS
        }
    }
    if (use_agps) {
        string mode = gpsinitmode;
        if (mode == "") {
            mode = "3";		// Default behavior
        }
        use_agps = false;	// Use standalone as default
        set_gps_standalone = false;
        if (mode == "1") {
            // Use standalone
        }
        else if (mode == "2") {
            use_agps = true;
            set_gps_standalone = false;
        }
        else if (mode == "3") {
            double last_agps = 0.0;
            if (fileExists(LastAgpsTimeFile)) {
                last_agps = stringToDouble(getStringFromFile(LastAgpsTimeFile));
            }
            double now = getunixtime();
            if ((now - last_agps) > 900) {
                use_agps = true;
                set_gps_standalone = true;
            }
            else {
                cout << getDateTime() << " Too soon to start aGPS" << endl;
            }
        }
    }
    if (use_agps) {
        cout << getDateTime() << " Starting aGPS MS Based" << endl;
        (void)getResponse("AT_OGPSMODE=2,1",1); // Start GPSOneXtra
    }
    else {
        cout << getDateTime() << " Starting GPSOneXtra" << endl;
        (void)getResponse("AT_OGPSMODE=1,1",1); // Start GPSOneXtra
    }
    (void)getResponse("AT_OGPS=2",1); // Start GPS
}

void setGpsStandalone() {
    cout << getDateTime() << " Starting GPSOneXtra" << endl;
    (void)getResponse("AT_OGPSMODE=1,1",1); // Set to GPS standalone
    set_gps_standalone = false;
}

string checksms() {
    string outstr = "No";
    if (gsm_model) {
        string resp = getResponse("AT+CMGL=\"ALL\"");
        if (beginsWith(resp, "+CMGL:")) {
            outstr = "Yes";
        }
    }
    else {
        string resp = getResponse("AT$QCMGL=\"ALL\"");
        if (beginsWith(resp, "$QCMGL:")) {
            outstr = "Yes";
        }
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
    if (gsm_model) {
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
                    cout << "sms response:" << resp << endl;
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
                    cout << "sms response:" << resp << endl;
                }
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
    if (gsm_model) {
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
                if (outstr != "") {
                    outstr += ";";
                }
                msg = unhex(msg);
                msg = csvQuote(msg);
                outstr += idx+":"+number+" "+msg;
            }
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
    cmndstr.erase(0, 10);       // Remove "deletesms "
    string idx = cmndstr;
    if (gsm_model) {
        string resp = getResponse("AT+CMGD="+idx);
        if (resp.find("OK") != string::npos) {
            outstr = "OK";
        }
    }
    else {
        string resp = getResponse("AT$QCMGD="+idx);
        if (resp.find("OK") != string::npos) {
            outstr = "OK";
        }
    }
    return outstr;
}

string clearsms() {
    string outstr = "OK";
    if (gsm_model) {
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
    }
    else {
        string resp = getResponse("AT$QCMGL=\"ALL\"");
        Strings lines = split(resp, "\n");
        for (int i=0; i < lines.size(); i++) {
            string line = lines[i];
            if (beginsWith(line, "$QCMGL: ")) {
                string msg = getField(line, ": ,", 1);
                string cmnd = "AT$QCMGD=" + msg;
                string resp2 = getResponse(cmnd);
            }
        }
    }
    return outstr;
}

string loadprl(string prl_file) {
    string resp = "ERROR";
    string output = backtick("ow_prl_loader -f "+prl_file);
    if (beginsWith(output, "Success")) {
        cout << getDateTime() << " PRL loaded" << endl;
//        reset();
        resp = "OK";
    }
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
    return getResponse(command, "OK\n", 1);
}

string getResponse(string command, int timeout) {
    return getResponse(command, "OK\n", timeout);
}

string getResponse(string command, string ans, int timeout) {
    bool done = false;
    string outstr = "";
    string cmndstr = command;
    string expresp = command;
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
        }
        else {
            if ((time(NULL) - start_time) >= timeout) {
                outstr += line;
                line = "";
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
        if (beginsWith(unsol_line, "_OGPSEVT: ")) {
            string res = getParm(unsol_line, "_OGPSEVT: ");
            if (set_gps_standalone) {
                if ( (res == "0") or
                     (res == "1") ) {
                    double now = getunixtime();
                    updateFile(LastAgpsTimeFile, string_printf("%0.2f",now));
                }
                else if ( (res == "2") or
                     (res == "3") or
                     (res == "4") ) {
                     setGpsStandalone();
                 }
            }
        }
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

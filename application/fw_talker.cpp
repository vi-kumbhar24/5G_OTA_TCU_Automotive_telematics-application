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
#include <sys/stat.h>
#include "autonet_files.h"
#include "getField.h"
#include "dateTime.h"
using namespace std;

#define serial_device "celldiag"

//#define _POSIX_SOURCE 1         //POSIX compliant source

typedef map<int,bool> Readers;

void closeAndExit(int exitstat);
void openPort(const char *port);
void closePort();
bool lock(const char *dev);
string processCommand(const char *command);
string getprofile();
string getmodel();
string gettime();
string getnetinfo();
string reset();
string reboot();
string getResponse(string command);
string getResponse(string command, int timeout);
string getResponse(string command, string resp, int timeout);
string getRestOfLine();
bool beginsWith(string str, string match);
int listenSocket(const char *name);
void onTerm(int parm);

bool debug = true;
int ser_s = -1;		// Serial port descriptor
int listen_s = -1;	// Listen socket for command connections
struct termios oldtio;	// Old port settings for serial port
Readers readers;
string lockfile = "";

int main(int argc, char *argv[])
{
    int res;
    char buf[255];                       //buffer for where data is put
    char read_buf[255];                       //buffer for where data is put
    string unsol_line = "";
    Readers::iterator ri;

    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);

    listen_s = listenSocket(CellTalkerPath);
    if (listen_s < 0) {
        closeAndExit(-1);
    }
   
    openPort(serial_device);

    (void)getResponse("AT", 5); // Throw away old unsolicited responses
    (void)getResponse("AT+CIEV=0"); // Stop unsolicited CIEV responses
    (void)getResponse("AT^HRSSILVL=0"); // Stop unsolicited HHSILVL responses

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
        int r = select(nfds+1, &rd, &wr, &er, NULL);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (FD_ISSET(ser_s, &rd)) {
            res = read(ser_s,buf,255);
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
                res = read(rs, read_buf, 255);
                if (res > 0) {
                    read_buf[res] = '\0';
                    while ( (read_buf[res-1] == '\n') or
                         (read_buf[res-1] == '\r') ) {
                        read_buf[--res] = '\0';
                    }
                    if (strncasecmp(read_buf, "exit", 4) == 0) {
                        close(rs);
                        readers.erase(rs);
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
                        string resp = processCommand(read_buf);
                        resp += "\n";
                        write(rs, resp.c_str(), resp.size());
                    }
                }
                else {
                    close(rs);
                    readers.erase(rs);
                }
            }
        }

    }  //while (1)
    closeAndExit(0);
    return 0;
}  //end of main

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
        remove(CellTalkerPath);
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
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL);
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
    }
    else if (cmndstr == "") {
        resp = "Ready";
    }
    else if (cmndstr == "getprofile") {
        resp = getprofile();
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
    else if (cmndstr == "reset") {
        resp = reset();
    }
    else if (cmndstr == "reboot") {
        resp = reboot();
    }
    else {
        resp = "Invalid command";
    }
    return resp;
}

string getprofile() {
    string outstr = "";
    string vgmuid = getResponse("AT+VGMUID?");
    if (beginsWith(vgmuid,"+VGMUID:")) {
        outstr += ",esn:" + getField(vgmuid,":,",5);
    }
    string vmdn = getResponse("AT+VMDN?");
    if (beginsWith(vmdn,"+VMDN:")) {
        outstr += ",mdn:" + getField(vmdn,":",1);
    }
    string vmin = getResponse("AT+VMIN?");
    if (beginsWith(vmin,"+VMIN:")) {
        outstr += ",min:" + getField(vmin,":",1);
    }
    string vprlid = getResponse("AT+VPRLID?");
    if (beginsWith(vprlid,"+VPRLID:")) {
        outstr += ",prl:" + getField(vprlid,":",1);
    }
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string getmodel() {
    string outstr = "";
    string man = getResponse("AT+GMI");
    if (beginsWith(man,"+GMI: ")) {
        man = man.substr(6);
    }
    string model = getResponse("AT+GMM");
    if (beginsWith(model,"+GMM: ")) {
        model = model.substr(6);
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

string gettime() {
    string outstr = "";
    string datetime = getResponse("AT+CCLK?");
    if (beginsWith(datetime,"+CCLK:")) {
        datetime = datetime.substr(6);
        if (beginsWith(datetime,"\"")) {
            datetime = datetime.substr(1);
            datetime.erase(datetime.size()-1);
        }
        int year, month, day, hour, min, sec, off;
        sscanf(datetime.c_str(), "%d/%d/%d,%d:%d:%d %d",
               &year, &month, &day, &hour, &min, &sec, &off);
        struct tm tm;
        tm.tm_year = year + 2000 - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        time_t gmt = timegm(&tm);
        gmt -= off * 3600;
        struct tm *ptm = gmtime(&gmt);
        char dtstr[20];
        sprintf(dtstr, "%04d/%02d/%02d %02d:%02d:%02d",
                ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday,
                ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
        outstr = dtstr;
        if (off == 0) {
            outstr = "localtime: " + outstr;
        }
    }
    return outstr;
}

string getnetinfo() {
    string outstr = "";
    string type = "None";
    string sysinfo = getResponse("AT^SYSINFO");
    if (beginsWith(sysinfo,"^SYSINFO:")) {
        string roaming = getField(sysinfo,":,",3);
        string service = getField(sysinfo,":,",4);
        if (service == "0") {
            type = "None";
        }
        else if (service == "2") {
            type = "1xRTT";
        }
        else if ((service == "4") or (service == "8")) {
            type = "EVDO";
        }
        else {
            type = "Unknown";
        }
        outstr += ",roam:" + roaming;
    }
    string csq = getResponse("AT+CSQ?");
    if (beginsWith(csq,"+CSQ:")) {
        string sig = getField(csq,":,",1);
        if (sig == "99") {
            sig = "0";
        }
        outstr += ",1xrttsig:" + sig;
    }
    string evcsq1 = getResponse("AT+EVCSQ1");
    if (beginsWith(evcsq1,"+EVCSQ1:")) {
        string dbmstr = getField(evcsq1,":,",1);
        float dbm;
        sscanf(dbmstr.c_str(), "%f", &dbm);
        int percent = (int)((dbm-(-125.))/(-75-(-125))*100.+.5);
        char p_str[10];
        sprintf(p_str, "%d", percent);
        outstr += ",evdosig:";
        outstr += p_str;
    }
    if (type == "EVDO") {
        string evcsq2 = getResponse("AT+EVCSQ2");
        if (beginsWith(evcsq2,"+EVCSQ2:Rev")) {
            string evdo_rev = getField(evcsq2,":,",1);
            evdo_rev = evdo_rev.substr(3);
            type += "-" + evdo_rev;
        }
    }
    outstr = "type:" + type + outstr;
    if (beginsWith(outstr,",")) {
        outstr = outstr.substr(1);
    }
    return outstr;
}

string reset() {
    string outstr = "Failed";
    string resp = getResponse("AT+CPOF");
    if (resp == "OK") {
        sleep(1);
        resp = getResponse("AT+CPON");
        if (resp == "OK") {
            outstr = "OK";
        }
    }
    return outstr;
}

string reboot() {
    string outstr = "Failed";
    string resp = getResponse("AT+REBOOT");
    if (resp == "OK") {
        closePort();
        sleep(5);
        system("/etc/init.d/cellular-modules start");
        openPort(serial_device);
        resp = "OK";
    }
    return outstr;
}

string getResponse(string command) {
    return getResponse(command, "OK", 1);
}

string getResponse(string command, int timeout) {
    return getResponse(command, "OK", timeout);
}

string getResponse(string command, string ans, int timeout) {
    bool done = false;
    string outstr = "";
    string cmndstr = command;
    cmndstr += "\r";
    time_t start_time = time(NULL);
    write(ser_s, cmndstr.c_str(), cmndstr.size());
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
                        outstr += ch;
                        if (ch == '\n') {
                            if (outstr.find(ans) != string::npos) {
                                done = true;
                            }
                        }
                    }
                }  //end if res>0
            }
        }
        else {
            if ((time(NULL) - start_time) >= timeout) {
                if (debug) {
                    cerr << "Timeout" << endl;
                    cerr << outstr << endl;
                }
                done = true;
            }
        }
    }  //while (!done)
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
    if ((pos=outstr.find("\nOK")) != string::npos) {
        outstr.erase(pos);
    }
    return outstr;
}

string getRestOfLine() {
    bool done = false;
    string outstr = "";
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

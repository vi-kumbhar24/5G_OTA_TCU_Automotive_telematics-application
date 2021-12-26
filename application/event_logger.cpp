/***************************************************************************
 * File: event_logger.cpp
 *
 * Routine to process events from multiple sources and provide the evnets to
 * a single reader
 ***************************************************************************
 * 2015/02/18 rwp
 *    AK-96: Truncate saved events to 300
 ***************************************************************************
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tail.h"
#include "autonet_files.h"
#include "fileExists.h"
#include "readFileArray.h"
#include "writeStringToFile.h"
#include "getStringFromFile.h"
#include "updateFile.h"
using namespace std;

void sendEvent();
void writeEvents();
void setEventCount();
void closeAndExit(int exitstat);
int listenSocket(const char *name);
int dgramSocket(const char *name);
void onTerm(int parm);
void onUsr1(int parm);
string intToString(int i);
int stringToInt(string str);

bool debug = true;
int listen_s = -1;	// Listen socket for reader connections
int logger_s = -1;	// Events socket
int reader_s = -1;	// Event reader socket
bool readerReady = false;
Strings events;

int main(int argc, char *argv[])
{
    char buff[32000];
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGHUP, onTerm);
    signal(SIGUSR1, onUsr1);
    signal(SIGPIPE, SIG_IGN);

    if (fileExists(EventLogFile)) {
        events = readFileArray(EventLogFile);
        unlink(EventLogFile);
    }
    setEventCount();

    listen_s = listenSocket(ReaderPath);
    if (listen_s < 0) {
        closeAndExit(-1);
    }
    logger_s = dgramSocket(LoggerPath);
    if (logger_s < 0) {
        closeAndExit(-1);
    }
   
    // loop while waiting for input. normally we would do something useful here
    while (1) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(listen_s, &rd);
        nfds = max(nfds, listen_s);
        FD_SET(logger_s, &rd);
        nfds = max(nfds, logger_s);
        if (reader_s >= 0) {
            FD_SET(reader_s, &rd);
            nfds = max(nfds, reader_s);
        }
        int r = select(nfds+1, &rd, &wr, &er, NULL);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("select()");
            closeAndExit(1);
        }
        if (FD_ISSET(logger_s, &rd)) {
            int res = recv(logger_s, buff, sizeof(buff)-1, 0);
            if (res > 0) {
                buff[res] = '\0';
                while ( (res > 0) and
                        ( (buff[res-1] == '\r') or
                          (buff[res-1] == '\n') ) ) {
                    buff[--res] = '\0';
                }
                cerr << "Got event: " << buff << endl;
                events.push_back(buff);
                setEventCount();
                if (readerReady) {
                    sendEvent();
                }
            }
        }
        if (FD_ISSET(listen_s, &rd)) {
            struct sockaddr client_addr;
            unsigned int l = sizeof(struct sockaddr);
            memset(&client_addr, 0, l);
            int s = accept(listen_s, (struct sockaddr *)&client_addr, &l);
            if (s < 0) {
                perror ("accept()");
            }
            else if (reader_s < 0) {
                cerr << "Got connection" << endl;
                reader_s = s;
                readerReady = true;
                sendEvent();
            }
            else {
                cout << "Another reader is already connected" << endl;
                close(s);
            }
        }
        if ((reader_s >= 0) and FD_ISSET(reader_s, &rd)) {
            int res = recv(reader_s, buff, sizeof(buff), 0);
            if (res > 0) {
                buff[res] = '\0';
                cerr << "Got Line:" << buff << endl;
                if (strncasecmp(buff, "exit", 4) == 0) {
                    close(reader_s);
                    reader_s = -1;
                }
                else {
                    if (events.size() > 0) {
                        cerr << "removing event from list" << endl;
                        events.erase(events.begin());
                        setEventCount();
                        sendEvent();
                    }
                    else {
                        cerr << "Ack for event not sent" << endl;
                    }
                }
            }
            else {
                close(reader_s);
                reader_s = -1;
            }
        }
    }
    closeAndExit(0);
    return 0;
}

void sendEvent() {
    cerr << "sendEvent" << endl;
    if (events.size() > 0) {
        string event = events[0];
        cerr << "event sent to reader:" << event << endl;
        event += "\n";
        send(reader_s, event.c_str(), event.size(), 0);
        readerReady = false;
    }
    else {
        readerReady = true;
    }
}

void writeEvents() {
    while (events.size() > 300) {
        events.erase(events.begin());
    }
    if (events.size() > 0) {
        ofstream out(EventLogFile);
        if (out.is_open()) {
            for (int i=0; i < events.size(); i++) {
                out << events[i] << endl;
            }
            out.close();
        }
    }
    unlink(EventCountFile);
}

void setEventCount() {
    if (events.size() > 0) {
        int num = events.size();
        cerr << "writing count:" << num << endl;
        updateFile(EventCountFile, intToString(num));
    }
    else {
        unlink(EventCountFile);
    }
}

void flushEvents() {
    if (readerReady) {
        events.clear();
    }
    else {
        events.erase(events.begin()+1, events.end());
    }
    setEventCount();
}

void closeAndExit(int exitstat) {
    writeEvents();
    if (reader_s >= 0) {
        close(reader_s);
    }
    if (listen_s >= 0) {
        close(listen_s);
        remove(ReaderPath);
    }
    if (logger_s >= 0) {
        close(logger_s);
        remove(LoggerPath);
    }
    exit(exitstat);
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

int dgramSocket(const char *name) {
    struct sockaddr_un a;
    int s;
    int yes;
    if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
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
    chmod(name, S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP);
    return s;
}

void onTerm(int parm) {
    closeAndExit(0);
}

void onUsr1(int parm) {
    flushEvents();
    signal(SIGUSR1, onUsr1);
}

string intToString(int i) {
    stringstream out;
    out << i;
    return out.str();
}

int stringToInt(string str) {
    istringstream ist(str);
    int i;
    ist >> i;
    return i;
}

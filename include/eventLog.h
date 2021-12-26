#ifndef EVENTLOG_H
#define EVENTLOG_H

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "autonet_files.h"
#include "dateTime.h"
#include "appendToFile.h"
using namespace std;

class EventLog {
  private:
    int _socket;

    void _append(string event) {
        bool sent = false;
        while (!sent) {
            for (int i=0; (_socket < 0) and (i < 2); i++) {
                if ((_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
                    cerr << "Could not open event logger socket" << endl;
                }
                else {
                    struct sockaddr_un a;
                    memset(&a, 0, sizeof(a));
                    a.sun_family = AF_UNIX;
                    strcpy(a.sun_path, LoggerPath);
                    if (connect(_socket, (struct sockaddr *)&a, sizeof(a)) < 0) {
                        cerr << "Could not connect to event logger socket" << endl;
                        close(_socket);
                        _socket = -1;
                    }
                }
                if ((_socket < 0) and (i > 1)) {
                    sleep(1);
                }
            }
            if (_socket < 0) {
                cerr << "Writing event to event file" << endl;
                appendToFile(EventLogFile, event);
                sent = true;
                break;
            }
            else {
                int bytes_sent = send(_socket, event.c_str(), event.size(), 0);
                if (bytes_sent > 0) {
                    sent = true;
                    break;
                }
                else {
                    close(_socket);
                    _socket = -1;
                }
            }
        }
    }

  public:
    EventLog() {
        _socket = -1;
    }

    void append(string datetime, string event) {
        event = datetime + " " + event;
        _append(event);
    }

    void append(string event) {
        string datetime = getDateTime();
        append(datetime, event);
    }

    void append(time_t timeval, string event) {
        string datetime = getDateTime(timeval);
        append(datetime, event);
    }

    void appendWithTimestamp(string event) {
        _append(event);
    }

    void signalMonitor() {
        system("pkill -USR1 monitor");
    }

    void closeLog() {
        if (_socket != -1) {
            close(_socket);
            _socket = -1;
        }
    }
};


#endif

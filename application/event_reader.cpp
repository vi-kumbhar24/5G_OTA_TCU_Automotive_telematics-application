#include <iostream>
#include <string>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "autonet_files.h"
#include "logError.h"
using namespace std;

typedef int Socket;

Socket readerSocket(const char *name);

main() {
    struct timeval select_time;
    char event[255];
    Socket events_sock = readerSocket(ReaderPath);
    if (events_sock < 0) {
        exit(0);
    }
    bool gotevent = false;
    while (true) {
        int nfds = 0;
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(events_sock, &read_set);
        nfds = max(nfds,events_sock);
        select_time.tv_sec = 1;
        select_time.tv_usec = 0;
        int ready_cnt = select(nfds+1, &read_set, NULL, NULL, &select_time);
        if (ready_cnt) {
            if (FD_ISSET(events_sock, &read_set)) {
                int nread = recv(events_sock, event, 255, 0);
                event[nread] = '\0';
                if ((nread > 1) and (event[nread-1] == '\n')) {
                    nread--;
                    event[nread] = '\0';
                }
                cout << "Event: " << event << endl;
                gotevent = true;
            }
        }
        if (gotevent) {
            string ans;
            cerr << "waiting to ack event";
            getline(cin, ans);
            send(events_sock, "\n", 1, 0);
            gotevent = false;
        }
    }
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
            perror("Could not connect to event reader socket");
            close(sock);
            sock = -1;
        }
    }
    return sock;
}

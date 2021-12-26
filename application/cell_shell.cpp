#include <iostream>
#include <string>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include "autonet_files.h"
#include "getopts.h"
#include "logError.h"
using namespace std;

typedef int Socket;

void sendCommand(string cmnd);
Socket readerSocket(const char *name);
Socket cell_talker;

main(int argc, char **argp) {
    GetOpts opts(argc, argp, "lt:");
    bool loopit = opts.exists("l");
    int waittime = opts.getNumber("t");
    cell_talker = readerSocket(CellTalkerPath);
    if (cell_talker < 0) {
        exit(0);
    }
    if (argc > 1) {
        while (true) {
            for (int i=1; i < argc; i++) {
                string cmnd = argp[i];
                cout << cmnd << ":" << endl;
                sendCommand(cmnd);
                if ((loopit or (i < (argc-1))) and (waittime > 0)) {
                    sleep(waittime);
                }
            }
            if (!loopit) {
                break;
            }
        }
    }
    else {
        while (true) {
            cout << "cell> ";
            string cmnd;
            getline(cin, cmnd);
            if (cmnd == "exit") {
                break;
            }
            sendCommand(cmnd);
        }
    }
    close(cell_talker);
}

void sendCommand(string cmnd) {
    char buff[256];
    cmnd += "\n";
    send(cell_talker, cmnd.c_str(), cmnd.size(), 0);
    bool done = false;
    string result = "";
    while (!done) {
        int nread = recv(cell_talker, buff, 255, 0);
        buff[nread] = '\0';
        if ((nread > 1) and (buff[nread-1] == '\n')) {
            done = true;
            nread--;
            buff[nread] = '\0';
        }
        result += buff;
    }
    if (result.find(";") != string::npos) {
        int pos;
        while ((pos=result.find(";")) != string::npos) {
            result.replace(pos,1,"\n");
        }
    }
    else if ((strncasecmp(cmnd.c_str(), "AT", 2) != 0) and
             (result.find(",") != string::npos)) {
        int pos;
        while ((pos=result.find(",")) != string::npos) {
            result.replace(pos,1,"\n");
        }
    }
    cout << result << endl;
}

Socket readerSocket(const char *name) {
    Socket sock = -1;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        logError("Could not open cell talker socket");
    }
    else {
        struct sockaddr_un a;
        memset(&a, 0, sizeof(a));
        a.sun_family = AF_UNIX;
        strcpy(a.sun_path, name);
        if (connect(sock, (struct sockaddr *)&a, sizeof(a)) < 0) {
            perror("Could not connect to cell talker socket");
            close(sock);
            sock = -1;
        }
    }
    return sock;
}

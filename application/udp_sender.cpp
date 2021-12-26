#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "autonet_types.h"
#include "backtick.h"
#include "getToken.h"
#include "fileStats.h"
#include "fileExists.h"
using namespace std;

string getIpAddress(string name);

bool debug = false;

main(int argc, char *argv[]) {
    int serverport = 5555;
    int min = 1;
    int max = 64;
    int timeout = 1;
    GetOpts opts(argc, argv, "m:x:p:t:");
    if (opts.exists("m")) {
        min = opts.getNumber("m");
    }
    if (opts.exists("x")) {
        max = opts.getNumber("x");
    }
    if (opts.exists("p")) {
        serverport = opts.getNumber("p");
    }
    if (opts.exists("t")) {
        timeout = opts.getNumber("t");
    }
    if (argc < 2) {
        cout << "usage: udp_sender [options] host" << endl;
        cout << "options:" << endl;
        cout << "-m min     Minimum packet length" << endl;
        cout << "-x max     Maximum packet length" << endl;
        cout << "-p port    Port number" << endl;
        cout << "-t timeout Response timeout" << endl;
        exit(1);
    }
    string server_ip = getIpAddress(argv[1]);
    if (server_ip == "") {
        cerr << "Could not resolve " << server_ip << endl;
        exit(1);
    }
    int sock = -1;
    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        cerr << "Could not open socket" << endl;
        exit(1);
    }
    struct linger linger_opt;
    linger_opt.l_onoff = 0;
    linger_opt.l_linger = 0;
    setsockopt(sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
    int sock_flag = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sock_flag, sizeof(sock_flag));
    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = inet_addr(server_ip.c_str());
    to.sin_port = htons(serverport);
    struct sockaddr_in from;
    socklen_t fromlen;
    char frame[32000];
    char resp[64];
    while (true) {
        int n;
        if (debug) cerr << ".";
        sendto(sock, frame, n, 0, (struct sockaddr *)&to, sizeof(to));
        int nfds = 0;
        fd_set read_set;
        FD_ZERO(&read_set);
        if (sock >= 0) {
            FD_SET(sock, &read_set);
            nfds = max(nfds,sock);
        }
        struct timeval select_time;
        select_time.tv_sec = timeout;
        select_time.tv_usec = 0;
        int ready_cnt = select(nfds+1, &read_set, NULL, NULL, &select_time);
        if (ready_cnt > 0) {
            if (FD_ISSET(sock, &read_set)) {

                n = recvfrom(sock, frame, sizeof(frame), 0, (struct sockaddr *)&from, &fromlen);
                if (n <= 0) {
                    break;
                }
            }
        }
    }
}

string getIpAddress(string name) {
    string ip = "";
    if (name.find_first_not_of("0123456789.") == string::npos) {
        ip = name;
    }
    else {
        string resp = backtick("resolvedns "+name);
        if (resp != "") {
            ip = getToken(resp, 0);
        }
    }
    return ip;
}

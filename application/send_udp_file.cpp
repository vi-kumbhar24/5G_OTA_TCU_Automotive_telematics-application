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
    if (argc < 5) {
        cout << "usage: send_udp_storm host port file size" << endl;
        exit(1);
    }
    string server_ip = getIpAddress(argv[1]);
    if (server_ip == "") {
        cerr << "Could not resolve " << server_ip << endl;
        exit(1);
    }
    int serverport = 0;
    if (sscanf(argv[2], "%d", &serverport) != 1) {
        cerr << "Invalid port " << argv[2] << endl;
        exit(1);
    }
    char * file = argv[3];
    if (!fileExists(file)) {
        cerr << "Input file does not exist" << endl;
        exit(1);
    }
    int filesize = fileSize(file);
    int framesize = 0;
    if (sscanf(argv[4], "%d", &framesize) != 1) {
        cerr << "Invalid size " << argv[2] << endl;
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
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(serverport);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        cerr << "Could not bind to port" << endl;
        exit(0);
    }
    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = inet_addr(server_ip.c_str());
    to.sin_port = htons(serverport);
    struct sockaddr_in from;
    socklen_t fromlen;
    char frame[32000];
    ifstream fin(file);
    if (fin.is_open()) {
        sprintf(frame, "%d\n", filesize);
        sendto(sock, frame, strlen(frame), 0, (struct sockaddr *)&to, sizeof(to));
        int n;
        while (fin.good()) {
            fin.read(frame, framesize);
            n = fin.gcount();
            if (n == 0) break;
            if (debug) cerr << ".";
            sendto(sock, frame, n, 0, (struct sockaddr *)&to, sizeof(to));
            n = recvfrom(sock, frame, sizeof(frame), 0, (struct sockaddr *)&from, &fromlen);
            if (n <= 0) {
                break;
            }
        }
        fin.close();
    }
    else {
        cerr << "Failed to open: " << file << endl;
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

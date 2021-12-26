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
#include "string_convert.h"
using namespace std;

string getIpAddress(string name);

bool debug = false;

main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "usage: recv_udp_storm port file" << endl;
        exit(1);
    }
    int serverport = 0;
    if (sscanf(argv[1], "%d", &serverport) != 1) {
        cerr << "Invalid port " << argv[2] << endl;
        exit(1);
    }
    char * file = argv[2];
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
    char frame[32000];
    char ack[] = "\n";
    struct sockaddr_in from;
    socklen_t fromlen;
    ofstream fout(file);
    int n = recvfrom(sock, frame, sizeof(frame), 0, (struct sockaddr *)&from, &fromlen);
    frame[n] = '\0';
    int filesize = stringToInt(frame);
    if (fout.is_open()) {
        while (filesize > 0) {
            n = recvfrom(sock, frame, sizeof(frame), 0, (struct sockaddr *)&from, &fromlen);
            if (n <= 0) {
                break;
            }
            string fromip;
            fout.write(frame, n);
            if (debug) cerr << ".";
            sendto(sock, ack, strlen(ack), 0, (struct sockaddr *)&from, fromlen);
            filesize -= n;
        }
        fout.close();
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

#include <iostream>
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
using namespace std;

string getIpAddress(string name);

main(int argc, char *argv[]) {
    if (argc < 3) {
        cout << "usage: send_udp_storm host port" << endl;
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
    string server_ip = getIpAddress(argv[1]);
    if (server_ip == "") {
        cerr << "Could not resolve " << server_ip << endl;
        exit(1);
    }
    int server_port = 0;
    if (sscanf(argv[2], "%d", &server_port) != 1) {
        cerr << "Invalid port " << argv[2] << endl;
        exit(1);
    }
    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = inet_addr(server_ip.c_str());
    to.sin_port = htons(server_port);
    char frame[1025];
    for (int i=0; i < 1023; i++) {
        frame[i] = 'A';
    }
    frame[1023] = '\n';
    frame[1024] = '\0';
    while (true) {
        sendto(sock, frame, 1024, 0, (struct sockaddr *)&to, sizeof(to));
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

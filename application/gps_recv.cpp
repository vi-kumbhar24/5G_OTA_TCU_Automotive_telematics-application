#include <iostream>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "autonet_types.h"
#include "getopts.h"
#include "splitFields.h"
#include "updateFile.h"
using namespace std;

#define localport 5555

int main(int argc, char *argp[]) {
    GetOpts opts(argc, argp, "n");
    bool no_response = opts.exists("n");
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        cerr << "Could not create socked" << endl;
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
    addr.sin_port = htons(localport);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        cerr << "Could not bind to port" << endl;
        exit(1);
    }
    struct sockaddr_in from;
    unsigned char buffer[32000];
    socklen_t fromlen = sizeof(from);
    while (true) {
        int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from, &fromlen);
        string fromip;
        fromip = inet_ntoa(from.sin_addr);
        int fromport = ntohs(from.sin_port);
//        cout << "Recv:" << fromip << ":" << fromport << endl;
        string packet;
        packet.assign((char*)buffer, len);
//        cout << "Got:" << packet;
        Strings fields = splitFields(packet, ",");
        string unitid = fields[0];
        string tstamp = fields[1];
        string lat = fields[2];
        string lon = fields[3];
        string mph = fields[4];
        string dir = fields[5];
        string resp = unitid + "," + tstamp + "\n";
        if (!no_response) {
//            cout << "Sending:" << resp;
            sendto(sock, resp.c_str(), resp.size(), 0, (struct sockaddr *)&from, sizeof(from));
        }
        string pos = lat + "/" + lon;
        updateFile("/tmp/gps_ref_position", pos);
    }
}

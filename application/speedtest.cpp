/**********************************************************************
 * speedtest.cpp
 *
 * Routine to test the speed of a connection
 **********************************************************************
 * 2014/11/18 rwp
 *    Added the -s option to specify the speedtest server
 **********************************************************************
 */
#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "autonet_files.h"
#include "backtick.h"
#include "getopts.h"
#include "hiresTime.h"
#include "readconfig.h"
using namespace std;

#define SEND_PORT 5005
#define RECV_PORT 5006
#define SEND_CNT 100
#define SpeedTestServer "unit.speedtest-server"

int main(int argc, char *argv[]) {
    char buffer[1500];
    GetOpts opts(argc, argv, "cs:");
    Config config(ConfigFile);
    bool calc_size = opts.exists("c");
    string server_name = "speedtest.autonetmobile.net";
    if (opts.exists("s")) {
        server_name = opts.get("s");
    }
    else if (config.exists(SpeedTestServer)) {
        server_name = config.getString(SpeedTestServer);
    }
    string serverip = backtick("resolvedns "+server_name);
    cout << serverip << endl;
    int recv_s = socket(AF_INET, SOCK_STREAM, 0);
    if (recv_s < 0) {
        cerr << "Could not open recv socket" << endl;
        exit(1);
    }
    struct hostent *server;
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_aton(serverip.c_str(), &server_addr.sin_addr);
    server_addr.sin_port = htons(SEND_PORT);
    if (connect(recv_s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Could not connect to send server" << endl;
        exit(1);
    }
    int cnt = 0;
    double start_time = hiresTime();
    while (true) {
        int len = recv(recv_s, buffer, sizeof(buffer), 0);
        if (len == 0) {
            break;
        }
        cnt += len;
    }
    double elapsed = hiresTime() - start_time;
    close(recv_s);
    double kbps = cnt / elapsed / 1000;
    char speed_buf[15];
    sprintf(speed_buf, "%.1f", kbps);
    cout << "Download test complete. Received " << cnt << " bytes. Speed: " << speed_buf << "Kb/s" << endl;

    int recv_cnt = cnt;
    char send_buffer[1001];
    char *p = send_buffer;
    for (int i=0; i < 100; i++) {
        strcpy(p, "0123456789");
        p += 10;
    }
    int send_size = strlen(send_buffer);
    int send_s = socket(AF_INET, SOCK_STREAM, 0);
    if (send_s < 0) {
        cerr << "Could not open send socket" << endl;
        exit(1);
    }
    server_addr.sin_port = htons(RECV_PORT);
    if (connect(send_s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Could not connect to send server" << endl;
        exit(1);
    }
    start_time = hiresTime();
    int send_cnt = SEND_CNT;
    if (calc_size) {
       send_cnt = recv_cnt / 1000;
    }
    cnt = 0;
    for (int i=0; i < send_cnt; i++) {
        int n = send(send_s, send_buffer, send_size, 0);
        cnt += send_size;
    }
    int n = recv(send_s, buffer, sizeof(buffer), 0);
    elapsed = hiresTime() - start_time;
    close(send_s);
    kbps = cnt / elapsed / 1000;
    sprintf(speed_buf, "%.1f", kbps);
    cout << "Upload test complete. Sent " << cnt << " bytes. Speed: " << speed_buf << " Kb/s" << endl;
}

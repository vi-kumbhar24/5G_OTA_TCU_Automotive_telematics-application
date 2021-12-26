#include <iostream>
#include <fstream>
#include <map>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include "autonet_types.h"
#include "getopts.h"
#include "hiresTime.h"
#include "split.h"
#include "string_convert.h"
using namespace std;

bool debug = false;
int num_cans = 0;
bool one_can = false;

class CAN_MSG {
public:
    int id;
    int len;
    unsigned char data[64];
    bool l29;
    int bus;
    int cycle_time;
    Strings messages;
    int message_counter;
    double tx_time;

    CAN_MSG() {
        id = 0;
        len = 0;
        l29 = false;
        memset(data, 0x00, 64);
        bus = 0;
        cycle_time = 0;
        message_counter = 0;
        tx_time = 0.;
    }

    CAN_MSG(string id_str, int l, int b, int ct) {
        if (one_can) {
            b = 0;
        }
        l29 = false;
        if (id_str.length() > 3) {
            l29 = true;
        }
        len = l;
        id = strtoul(id_str.c_str(), NULL, 16);
        memset(data, 0x00, 64);
        bus = b;
        cycle_time = ct;
        if ((b+1) > num_cans) {
            num_cans = b + 1;
        }
    }

    CAN_MSG(string msgs[], int ct) {
        for (int i=0; msgs[i] != ""; i++) {
            messages.push_back(msgs[i]);
        }
        cycle_time = ct;
        message_counter = 0;
    }
};

typedef map<string,CAN_MSG> CanMessages;

CanMessages can_msg;
int can_socks[4];
bool can_state[4];
Strings msg_ids;
bool single = false;

void process_signals(string frame);
void proc_signal(string sig, string val);
void start_can(int b);
void stop_can(int b);
double next_xmit();
void send_messages();
void send_msg(string id, bool sng=true);
int canOpen(string ifname, bool filter);
void canClose();

main(int argc, char *argv[]) {
    Strings can_ifs;
    GetOpts opts(argc, argv, "1dhp:s");
    one_can = opts.exists("1");
    debug = opts.exists("d");
    single = opts.exists("s");
    string portstr = opts.get("p");
    if (opts.exists("h")) {
        cout << "usage: cansim [-hs] [-p port] <can_if> . . . " << endl;
        cout << "where:" << endl;
        cout << "  -1       : Sends all messages on a single CAN interface" << endl;
        cout << "  -d       : Turns on debug mode" << endl;
        cout << "  -h       : Displays this help" << endl;
        cout << "  -p port  : Specified the listen port for signals (default 50011)" << endl;
        cout << "  -s       : Only outputs single messages, instead of repetative" << endl;
        cout << "  can_if   : CAN interfaces to used" << endl;
        exit(1);
    }
    for (int ix=1; ix < argc; ix++) {
        can_ifs.push_back(argv[ix]);
    }
    srand(time(NULL));
    can_msg["334"] = CAN_MSG("334", 8, 0, 500);
    can_msg["112"] = CAN_MSG("112", 2, 0, 10);
    can_msg["2A0"] = CAN_MSG("2A0", 8, 0, 50);
    can_msg["11C"] = CAN_MSG("11C", 8, 0, 10);
    can_msg["3D2"] = CAN_MSG("3D2", 4, 1, 500);
    can_msg["310"] = CAN_MSG("310", 7, 1, 500);
    can_msg["1D0"] = CAN_MSG("1D0", 8, 1, 500);
    can_msg["304"] = CAN_MSG("304", 8, 1, 500);
    string vin_msgs[] = {"3E0_lo", "3E0_mid", "3E0_hi", ""};
    can_msg["3E0"] = CAN_MSG(vin_msgs, 50);
    can_msg["3E0_lo"] = CAN_MSG("3E0", 8, 1, 0);
    can_msg["3E0_mid"] = CAN_MSG("3E0", 8, 1, 0);
    can_msg["3E0_hi"] = CAN_MSG("3E0", 8, 1, 0);
    can_msg["170"] = CAN_MSG("170", 8, 1, 10);
    for (CanMessages::iterator it= can_msg.begin(); it != can_msg.end(); it++) {
        msg_ids.push_back(it->first);
    }
    int serverport = 50011;
    if (portstr != "") {
        if (sscanf(portstr.c_str(), "%d", &serverport) != 1) {
            cerr << "Invalid port " << argv[2] << endl;
            exit(1);
        }
    }
    if (num_cans > can_ifs.size()) {
        cerr << "Not enough CAN interfaces specified. Requires: " << num_cans << endl;
        exit(1);
    }
    for (int i=0; i < can_ifs.size(); i++) {
        string dev = can_ifs[i];
        can_socks[i] = canOpen(dev, false);
        if (can_socks[i] < 0) {
            cerr << "Unable to open " << dev << endl;
            exit(1);
        } 
        can_state[i] = false;
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
    char frame[32000];
    struct sockaddr_in from;
    socklen_t fromlen;
    while (true) {
        double nt = next_xmit();
        int nfds = 0;
        fd_set rd, wr, er;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(sock, &rd);
        nfds = max(nfds, sock);
        struct timeval tv;
        if (nt == 0.) {
            tv.tv_sec = 1;
            tv.tv_usec = 0;
        }
        else {
            double now = hiresTime();
            double delay = nt - now;
            if (delay < 0.) {
                delay = 0.000010;
            }
            tv.tv_sec = (long)delay;
            tv.tv_usec = (long)((delay - tv.tv_sec) * 1000000.);
        }
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if (r > 0) {
            if (FD_ISSET(sock, &rd)) {
                int n = recvfrom(sock, frame, sizeof(frame)-1, 0, (struct sockaddr *)&from, &fromlen);
                if (n < 0) {
                    break;
                }
                frame[n] = '\0';
                if (frame[n-1] == '\n') {
                    frame[--n] = '\0';
                }
                process_signals(frame);
            }
        }
        send_messages();
    }
}

void process_signals(string frame) {
    Strings signals = split(frame, ",");
    for (int i=0; i < signals.size(); i++) {
        string signal = signals[i];
        string sig = signal;
        string val = "";
        int pos = signal.find(':');
        if (pos != string::npos) {
            sig = signal.substr(0,pos);
            val = signal.substr(pos+1);
        }
        if (debug) {
            cout << "name:" << sig << " val:" << val << endl;
        }
        proc_signal(sig, val);
    }
}

void proc_signal(string sig, string val) {
    if (sig == "can_a") {
        if (val == "active") {
            start_can(0);
        }
        else {
            stop_can(0);
        }
    }
    else if (sig == "can_b") {
        if (val == "active") {
            start_can(1);
        }
        else {
            stop_can(1);
        }
    }
    else if (sig == "ignition") {
        unsigned char *data = can_msg["112"].data;
        data[0] &= 0xF8;
        if (val == "on") {
            data[0] != 0x4;
        }
        else {
            data[0] != 0x1;
        }
        send_msg("112");
    }
    else if (sig == "engine") {
        /* No information for RPM */
        unsigned char *data = can_msg["2A0"].data;
        data[5] &= 0xE3;
        if (val == "off") {
            data[5] |= 0x0 << 2;
        }
        else {
            data[5] |= 0x3 << 2;
        }
        send_msg("2A0");
    }
    else if (sig == "speed") {
        int speed = int(stringToDouble(val)*10.+0.5);
        unsigned char *data = can_msg["11C"].data;
        data[4] = (speed >> 8) & 0xFF;
        data[5] = speed & 0xFF;
        send_msg("11C");
    }
    else if (sig == "odometer") {
        int odom = int(stringToDouble(val)*10.+0.5);
        unsigned char *data = can_msg["3D2"].data;
        data[0] = (odom >> 16) & 0xFF;
        data[1] = (odom >> 8) & 0xFF;
        data[2] = odom & 0xFF ;
        send_msg("3D2");
    }
    else if (sig == "fuel") {
        int fuel = stringToInt(val);
        fuel *= 2;
        unsigned char *data = can_msg["310"].data;
        data[0] = (can_msg["310"].data[0] & 0xFE) | ((fuel >> 8) & 0x01);
        data[1] = fuel & 0xFF;
        send_msg("310");
    }
    else if (sig == "transmission") {
        unsigned char *data = can_msg["170"].data;
        data[0] &= 0xE3;
        if (val == "p") {
            data[0] |= 0 << 2;
        }
        else if (val == "r") {
            data[0] |= 1 << 2;
        }
        else if (val == "n") {
            data[0] |= 2 << 2;
        }
        else if (val == "sna") {
            data[0] |= 7 << 2;
        }
        else {
            data[0] |= 3 << 2;
        }
        send_msg("170");
    }
    else if (sig == "vin") {
        can_msg["3E0_lo"].data[0] = 0x0;
        memcpy(&can_msg["3E0_lo"].data[1], &val.c_str()[0], 7);
        can_msg["3E0_mid"].data[0] = 0x1;
        memcpy(&can_msg["3E0_mid"].data[1], &val.c_str()[7], 7);
        can_msg["3E0_hi"].data[0] = 0x2;
        memcpy(&can_msg["3E0_hi"].data[1], &val.c_str()[14], 3);
        send_msg("3E0_lo");
        send_msg("3E0_mid");
        send_msg("3E0_hi");
    }
    else if (sig == "doors") {
        Strings vals = split(val, "/");
        unsigned char *data = can_msg["334"].data;
        data[2] &= 0xE1;
        if (vals[0] == "o") {
           data[2] |= 0x02;
        }
        if (vals[1] == "o") {
           data[2] |= 0x04;
        }
        if (vals[2] == "o") {
           data[2] |= 0x08;
        }
        if (vals[3] == "o") {
           data[2] |= 0x10;
        }
        send_msg("334");
    }
    else if (sig == "trunk") {
        unsigned char *data = can_msg["334"].data;
        data[2] &= 0xBF;
        if (val == "o") {
           data[2] |= 0x40;
        }
        send_msg("334");
    }
    else if (sig == "locks") {
        unsigned char *data = can_msg["334"].data;
        data[0] &= 0x9F;
        if (val == "l/l/l/l") {
            data[0] |= 0x0 << 5;
        }
        else if (val == "u/l/l/l") {
            data[0] |= 0x1 << 5;
        }
        else if (val == "sna/sna/sna/sna") {
            data[0] |= 0x3 << 5;
        }
        else {
            data[0] |= 0x2 << 5;
        }
        send_msg("334");
    }
    else if (sig == "seats") {
        Strings vals = split(val, "/");
        unsigned char *data = can_msg["1D0"].data;
        data[3] &= 0xF3;
        if (vals[1] == "o") {
           data[3] |= 0x04;
        }
        else if (vals[1] == "sna") {
           data[3] |= 0x0C;
        }
        send_msg("1D0");
    }
    else if (sig == "seatbelts") {
        Strings vals = split(val, "/");
        unsigned char *data = can_msg["1D0"].data;
        data[2] &= 0xF0;
        if (vals[0] == "u") {
           data[2] |= 0x01;
        }
        else if (vals[0] == "sna") {
           data[2] |= 0x03;
        }
        if (vals[1] == "u") {
           data[2] |= 0x04;
        }
        else if (vals[1] == "sna") {
           data[2] |= 0x0C;
        }
        send_msg("1D0");
    }
    else if (sig == "tire_pressures") {
        Strings vals = split(val, "/");
        unsigned char *data = can_msg["304"].data;
        if (vals[0] == "sna") {
            data[3] = 0xFF;
        }
        else {
            data[3] = stringToInt(vals[0]);
        }
        if (vals[1] == "sna") {
            data[4] = 0xFF;
        }
        else {
            data[4] = stringToInt(vals[1]);
        }
        if (vals[2] == "sna") {
            data[5] = 0xFF;
        }
        else {
            data[5] = stringToInt(vals[2]);
        }
        if (vals[3] == "sna") {
            data[6] = 0xFF;
        }
        else {
            data[6] = stringToInt(vals[3]);
        }
        if (vals.size() > 4) {
            if (vals[4] == "sna") {
                data[7] = 0xFF;
            }
            else {
                data[7] = stringToInt(vals[4]);
            }
        }
        send_msg("304");
    }
    else if (sig == "oil_life") {
    }
    else if (sig == "low_fuel_indicator") {
        unsigned char *data = can_msg["310"].data;
        data[0] &= 0xBF;
        if (val == "on") {
            data[0] |= 0x40;
        }
        send_msg("310");
    }
    else if (sig == "check_engine_indicator") {
        unsigned char *data = can_msg["2A0"].data;
        data[0] &= 0xFE;
        if (val == "yes") {
            data[0] |= 0x01;
        }
        send_msg("2A0");
    }
    else if (sig == "faults") {
    }
    else if (sig == "ecus") {
    }
}

void start_can(int b) {
    if (!can_state[b]) {
        for (int i=0; i < msg_ids.size(); i++) {
            string id = msg_ids[i];
            CAN_MSG msg = can_msg[id];
            if (msg.bus != b) {
                continue;
            }
            if (msg.cycle_time == 0) {
                continue;
            }
            double r = double(rand()) / double(RAND_MAX);
            double delay = msg.cycle_time * r / 1000.;
            can_msg[id].tx_time = hiresTime() + delay;
        }
        can_state[b] = true;
    }
}

void stop_can(int b) {
    can_state[b] = false;
}

double next_xmit() {
    double nt = 0.;
    for (int i=0; i < msg_ids.size(); i++) {
        string id = msg_ids[i];
        CAN_MSG msg = can_msg[id];
        if (!can_state[msg.bus]) {
            continue;
        }
        if (msg.cycle_time == 0) {
            continue;
        }
        if ((nt == 0.) or (msg.tx_time < nt)) {
            nt = msg.tx_time;
        }
    }
    return nt;
}

void send_messages() {
    double nt = 0.;
    for (int i=0; i < msg_ids.size(); i++) {
        string id = msg_ids[i];
        CAN_MSG msg = can_msg[id];
        if (!can_state[msg.bus]) {
            continue;
        }
        if (msg.cycle_time == 0) {
            continue;
        }
        double now = hiresTime();
        if ((msg.tx_time - now) < 0.0001) {
            can_msg[id].tx_time = now + (double)msg.cycle_time / 1000.;
            if (msg.messages.size() > 0) {
                string nxt_msg = msg.messages[msg.message_counter];
                can_msg[id].message_counter = (msg.message_counter + 1) % msg.messages.size();
                send_msg(nxt_msg, false);
            }
            else {
                send_msg(id, false);
            }
        }
    }
}

void send_msg(string id_str, bool sng) {
    if (single == sng) {
        struct can_frame frame;
        CAN_MSG msg = can_msg[id_str]; 
        int id = msg.id;
        int bus = msg.bus;
        if (debug) {
            if (msg.l29) {
                printf("bus:%d id: %08X\n", bus, id);
            }
            else {
                printf("bus:%d id: %03X\n", bus, id);
            }
        }
        int len = msg.len;
        if (debug) {
            printf("Data:");
            for (int i=0; i < len; i++) {
                printf(" %02X", msg.data[i]);
            }
            printf("\n");
        }
        frame.can_id  = id;
        frame.can_dlc = len;
        memcpy(frame.data, msg.data, len);
        int sock = can_socks[bus];
        int nbytes = write(sock, &frame, sizeof(struct can_frame));
        if (nbytes < 0) {
            fprintf(stderr, "Write sock failed: %s\n", strerror(errno));
        }
    }
}

int canOpen(string ifname, bool filter) {
    int recv_own_msgs;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct timeval tv;
    int sock;

    if ((sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        fprintf(stderr, "Error opening socket\n");
        return -1;
    }

    if (filter) {
        //struct can_filter rfilter[4];
        struct can_filter rfilter[1];
        //rfilter[0].can_id   = 0x80001234;   /* Exactly this EFF frame */
        //rfilter[0].can_mask = CAN_EFF_MASK; /* 0x1FFFFFFFU all bits valid */
        //rfilter[1].can_id   = 0x123;        /* Exactly this SFF frame */
        //rfilter[1].can_mask = CAN_SFF_MASK; /* 0x7FFU all bits valid */
        //rfilter[2].can_id   = 0x200 | CAN_INV_FILTER; /* all, but 0x200-0x2FF */
        //rfilter[2].can_mask = 0xF00;        /* (CAN_INV_FILTER set in can_id) */
        //rfilter[3].can_id   = 0;            /* don't care */
        //rfilter[3].can_mask = 0;            /* ALL frames will pass this filter */
        rfilter[0].can_id   = 0x7E0;
        rfilter[0].can_mask = 0xFF0;          /* 7E0-7EF */
        setsockopt(sock, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter[0], sizeof(rfilter));
    }

    strcpy(ifr.ifr_name, ifname.c_str());
    if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
        fprintf(stderr, "Error getting socket ifindex\n");
        return -2;
    }

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error in socket bind\n");
        return -3;
    }

    //recv_own_msgs = 0; /* 0 = disabled (default), 1 = enabled */
    //setsockopt(sock, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
    //                &recv_own_msgs, sizeof(recv_own_msgs));

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0) {
        fprintf(stderr, "Error setting socket timeout\n");
        return -4;
    }
    //setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

    return sock;
}


void canClose(int can_bus) {
    close(can_socks[can_bus]);
}

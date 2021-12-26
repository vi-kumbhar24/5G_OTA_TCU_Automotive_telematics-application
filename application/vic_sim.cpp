#include <iostream>
#include <fstream>
#include <errno.h>
#include <fcntl.h>
#include <openssl/hmac.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include "autonet_types.h"
#include "cobs.h"
#include "crc32.h"
#include "dateTime.h"
#include "getopts.h"
#include "fileExists.h"
#include "hiresTime.h"
#include "readconfig.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
using namespace std;

bool use_config_manager = true;
//vic.VeRemoteAddr
string ve_remote_addr = "192.168.225.42:5020";
string ve_remote_ip;
int ve_remote_port;
//vic.VeBindPort
int ve_port = 50010;
//vic.VicSimAddr
string vicsim_addr = "0.0.0.0:50011";
//vic.VicVaAddr
string vicva_addr = "0.0.0.0:50012";
Config config;
string SerialPort;
string SerialSpeed;
static uint8_t key[16] = {0x71,0xEB,0x12,0xE4,0xBC,0xB4,0xE8,0xBE,0xC1,0x26,0xC5,0x45,0xF6,0x5F,0xCC,0x92};
#define NONCE_NOTIFICATION (0xEF)
#define CRC_ERROR (0x00)
#define AUTH_ERROR (0x01)
#define NONCE_ERROR (0x02)
#define TOO_LONG_ERROR (0x03)
#define FORMAT_ERROR (0x04)
#define TYPE_ERROR (0x05)
#define COMMAND_ERROR (0x06)
#define PARAM_ERROR (0x07)

bool debug = false;

void sendSignals(char *signals);
void sendInfoFrame();
void sendFrame();
void processFrame(int n);
void processError();
string errorString(uint8_t err_type);
void sendNonceNotify();
void signFrame();
bool verifyFrame();
void addCrc32(void);
bool checkCrc32(void);
void process_signals(string frame);
void proc_signal(string sig, string val);
void handleRequest();
void processResponse(uint8_t type);
void sendError(int error_type);
void sendRemoteCommands(uint8_t device, uint8_t action);
void openPort(string port, string speed);
int baudString(string baud);
void closePort();
bool lock(string port);
void closeAndExit(int exitstat);
void closeAll();
void onTerm(int parm);
string hexdump(uint8_t *packet, int size);

uint8_t frame[1024];
char signals[1024];
uint8_t cobs_frame[1030];
int frame_len;
uint8_t can_status = 0xFF;
bool do_nonce = false;
uint16_t send_nonce = 1;
uint16_t recv_nonce = 1;
uint16_t new_nonce;
int ve_sock = -1;
int vic_sock;
struct sockaddr_in to_vicva;
int serial_sock = -1;
bool serial_port = false;
struct termios oldtio;  // Old port settings for serial port
string lockfile = "";
bool response_pending = false;
uint8_t sent_type = 0x00;
int watchdog_timer = 5;

main(int argc, char *argv[]) {
    GetOpts opts(argc, argv, "dhns:");
    debug = opts.exists("d");
    do_nonce = opts.exists("n");
    if (opts.exists("h")) {
        cout << "usage: vic_sim [-dhs] msgs . . . " << endl;
        cout << "where:" << endl;
        cout << "  -d                : Turns on debug mode" << endl;
        cout << "  -h                : Displays this help" << endl;
        cout << "  -s <port>:<speed> : Use serial port/speed" << endl;
        cout << "  msgs     : Messages to send" << endl;
        exit(1);
    }
    if (opts.exists("s")) {
        SerialPort = opts.get("s");
        int pos = SerialPort.find(":");
        if (pos != string::npos) {
            SerialSpeed = SerialPort.substr(pos+1);
            SerialPort = SerialPort.substr(0,pos);
        }
        use_config_manager = false;
    }
    if (use_config_manager) {
        char config_key[] = "vic.*";
        config.readConfigManager(config_key);
        if (config.exists("vic.VicSimAddr")) {
            vicsim_addr = config.getString("vic.VicSimAddr");
        }
        if (config.exists("vic.VicVaAddr")) {
            vicva_addr = config.getString("vic.VicVaAddr");
        }
        if (config.exists("vic.VeBindPort")) {
            ve_port = config.getInt("vic.VeBindPort");
        }
        if (config.exists("vic.VeRemoteAddr")) {
            ve_remote_addr = config.getString("vic.VeRemoteAddr");
        }
    }
    int pos;
    vic_sock = -1;
    struct linger linger_opt;
    linger_opt.l_onoff = 0;
    linger_opt.l_linger = 0;
    int sock_flag = 1;
    struct sockaddr_in addr;
    uint8_t recv_buff[1030];
    if (SerialPort != "") {
        openPort(SerialPort, SerialSpeed);
        serial_port = true;
        ve_remote_addr = "0.0.0.0:5020";
    }
    else {
        pos = vicsim_addr.find(":");
        string server_ip = vicsim_addr.substr(0, pos);
        int server_port = stringToInt(vicsim_addr.substr(pos+1));
        //cout << "server address:" << server_ip << " " << server_port << endl;
        pos = vicva_addr.find(":");
        string vicva_ip = vicva_addr.substr(0, pos);
        int vicva_port = stringToInt(vicva_addr.substr(pos+1));
        //cout << "vicva address:" << vicva_ip << " " << vicva_port << endl;
        to_vicva.sin_family = AF_INET;
        to_vicva.sin_port = htons(vicva_port);
        to_vicva.sin_addr.s_addr = inet_addr(vicva_ip.c_str());
        vic_sock = socket(PF_INET, SOCK_DGRAM, 0);
        if (vic_sock < 0) {
            cerr << "Could not open socket" << endl;
            exit(1);
        }
        setsockopt(vic_sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
        int sock_flag = 1;
        setsockopt(vic_sock, SOL_SOCKET, SO_REUSEADDR, &sock_flag, sizeof(sock_flag));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server_port);
        addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
        if (bind(vic_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            cerr << "Could not bind to port" << endl;
            exit(0);
        }
    }
    pos = ve_remote_addr.find(":");
    ve_remote_ip = ve_remote_addr.substr(0, pos);
    ve_remote_port = stringToInt(ve_remote_addr.substr(pos+1));
    ve_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (ve_sock < 0) {
        cerr << "Could not open socket" << endl;
        exit(1);
    }
    setsockopt(ve_sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
    setsockopt(ve_sock, SOL_SOCKET, SO_REUSEADDR, &sock_flag, sizeof(sock_flag));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ve_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(ve_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        cerr << "Could not bind to port" << endl;
        exit(0);
    }
    struct sockaddr_in from;
    socklen_t fromlen;
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    srand(time(NULL));
    if (argc > 1) {
        for (int i=1; i < argc; i++) {
            frame_len = 0;
            char msg[100];
            strcpy(msg, argv[i]);
            int l = strlen(msg);
            msg[l++] = '\n';
            msg[l] = '\0';
            if (serial_port) {
                cout << "sending: " << msg;
                int n = write(serial_sock, msg, l);
                if (n < 0) {
                    perror("serial_sock");
                }
            }
            else {
                char *msg = argv[i];
                process_signals(msg);
                if (debug) {
                    cout << "Sending:" << hexdump(frame, frame_len) << endl;
                }
                size_t cl = StuffData(frame, frame_len, cobs_frame);
                //cout << "Sending:" << hexdump(cobs_frame, cl) << endl;
                int len = sendto(vic_sock, cobs_frame, cl, 0, (struct sockaddr *)&to_vicva, sizeof(to_vicva));
            }
            sleep(1);
        }
    }
    else {
        response_pending = false;
        int fl = 0;
        bool in_frame = false;
        bool interbyte_timeout = false;
        if (do_nonce) {
            sendNonceNotify();
            do_nonce = false;
        }
        while (true) {
            fd_set rd, wr, er;
            int nfds = 0;
            FD_ZERO(&rd);
            FD_ZERO(&wr);
            FD_ZERO(&er);
            if (!response_pending and (ve_sock >= 0)) {
                FD_SET(ve_sock, &rd);
                nfds = max(nfds, ve_sock);
            }
            if (vic_sock >= 0) {
                FD_SET(vic_sock, &rd);
                nfds = max(nfds, vic_sock);
            }
            if (serial_sock >= 0) {
                FD_SET(serial_sock, &rd);
                nfds = max(nfds, serial_sock);
            }
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            int r = select(nfds+1, &rd, &wr, &er, &tv);
            if ((r == -1) && (errno == EINTR)) {
                continue;
            }
            if (r < 0) {
                perror ("select()");
                exit(1);
            }
            if (r > 0) {
                if ((ve_sock >= 0) and FD_ISSET(ve_sock, &rd)) {
                    fromlen = sizeof(from);
                    int n = recvfrom(ve_sock, signals, sizeof(signals)-1, 0, (struct sockaddr *)&from, &fromlen);
                    if (n < 0) {
                        perror("recvfrom");
                        break;
                    }
                    signals[n] = '\0';
                    if (signals[n-1] == '\n') {
                        signals[n-1] = '\0';
                        n--;
                    }
                    //cout << "ve:" << signals << endl;
                    sendSignals(signals);
                }
                if ((vic_sock >= 0) and FD_ISSET(vic_sock, &rd)) {
                    fromlen = sizeof(from);
                    int n = recvfrom(vic_sock, cobs_frame, sizeof(cobs_frame)-1, 0, (struct sockaddr *)&from, &fromlen);
                    if (n < 0) {
                        break;
                    }
                    processFrame(n);
                }
                if ((serial_sock >= 0) and FD_ISSET(serial_sock, &rd)) {
                    int res = read(serial_sock, recv_buff, sizeof(recv_buff)-1);
                    if (res == 0) {
                        closeAndExit(1);
                    }
                    if (res > 0) {
                        for (int i=0; i < res; i++) {
                             uint8_t b = recv_buff[i];
                             if (b == 0x00) {
                                 if (in_frame) {
                                     processFrame(fl);
                                     in_frame = false;
                                     fl = 0;
                                 }
                                 else {
                                     in_frame = true;
                                     fl = 0;
                                     interbyte_timeout = false;
                                 }
                             }
                             else if (in_frame) {
                                 cobs_frame[fl++] = b;
                             }
                        }
                    }
                }
            }
            else {
                if (in_frame and !interbyte_timeout) {
                    cerr << "Interbyte timeout" << endl;
                    interbyte_timeout = true;
                }
            }
        }
    }
    closeAndExit(0);
}

void sendSignals(char *signals) {
    //cout << signals << endl;
    frame_len = 0;
    frame[frame_len++] = 0xF1;
    frame_len = 3;
    process_signals(signals);
    if (frame_len > 3) {
        sendInfoFrame();
    }
}

void sendInfoFrame() {
    frame[1] = send_nonce >> 8;
    frame[2] = send_nonce & 0xFF;
    if (debug) {
        cout << "sending nonce:" << send_nonce << endl;
    }
    sendFrame();
    sent_type = frame[0];
    response_pending = true;
}

void sendFrame() {
    addCrc32();
    if (debug) {
        cout << "Sending:" << hexdump(frame, frame_len) << endl;
    }
    if (serial_port) {
        cobs_frame[0] = 0x00;
        size_t cl = StuffData(frame, frame_len, cobs_frame+1);
        cl++;
        cobs_frame[cl++] = 0x00;
        //cout << "COBS:" << hexdump(cobs_frame, cl) << endl;
        int len = write(serial_sock, cobs_frame, cl);
    }
    else {
        size_t cl = StuffData(frame, frame_len, cobs_frame);
        //cout << "COBS:" << hexdump(cobs_frame, cl) << endl;
        int len = sendto(vic_sock, cobs_frame, cl, 0, (struct sockaddr *)&to_vicva, sizeof(to_vicva));
    }
}

void processFrame(int n) {
    frame_len = UnStuffData(cobs_frame, n, frame);
    if (debug) {
        cout << "got:" << hexdump(frame, frame_len) << endl;
    }
    if (checkCrc32()) {
        uint8_t type = frame[0];
        uint16_t nonce = ((uint16_t)frame[1] << 8) | frame[2];
        if (type == 0xFF) {  // Error frame
            processError();
        }
        else if (response_pending and (type == sent_type) and (nonce == send_nonce)) {
            send_nonce++;
            response_pending = false;
            if (frame_len > 3) {
                processResponse(type);
            }
        }
        else {
            if (nonce == recv_nonce) {
                recv_nonce++;
                handleRequest();
            }
            else if (type == NONCE_NOTIFICATION) {
                handleRequest();  // Handle Nonce regardless
            }
            else {
                cout << "Received Nonce error" << endl;
                sendError(NONCE_ERROR);
            }
        }
    }
    else {
        cout << "CRC Error" << endl;
        sendError(CRC_ERROR);
    }
}

// processError: processes Error packet received
// The global buffer "frame" contains the frame
// The global variable "frame_len" is the length of the frame
void processError() {
    uint16_t nonce = ((uint16_t)frame[1] << 8) | frame[2];
    uint8_t err_type = frame[3];
    uint16_t exp_nonce = ((uint16_t)frame[4] << 8) | frame[5];
    string error_str = errorString(err_type);
    cout << "Error Frame:" << endl;
    cout << "Error type:" << error_str << endl;
    cout << "Nonce:" << nonce << endl;
    cout << "RecvNonce:" << exp_nonce << endl;
    cout << "Errored frame:" << hexdump(frame+6, frame_len-6) << endl;
}

// errorString: Converts from error type to string
// @err_typ is the error type
string errorString(uint8_t err_type) {
    string error_str = "unknown";
    if (err_type == CRC_ERROR) {
        error_str = "CRC_ERROR";
    }
    else if (err_type == AUTH_ERROR) {
        error_str = "AUTH_ERROR";
    }
    else if (err_type == NONCE_ERROR) {
        error_str = "NONCE_ERROR";
    }
    else if (err_type == TOO_LONG_ERROR) {
        error_str = "TOO_LONG_ERROR";
    }
    else if (err_type == FORMAT_ERROR) {
        error_str = "FORMAT_ERROR";
    }
    else if (err_type == TYPE_ERROR) {
        error_str = "TYPE_ERROR";
    }
    else if (err_type == PARAM_ERROR) {
        error_str = "PARAM_ERROR";
    }
    return error_str;
}

// sendNonceNotify: Send Nonce Notification packet
void sendNonceNotify() {
    frame_len = 0;
    frame[frame_len++] = NONCE_NOTIFICATION;
    frame[frame_len++] = send_nonce >> 8;
    frame[frame_len++] = send_nonce & 0xFF;
    new_nonce = rand();
    if (debug) {
        cout << "Calculated random new_nonce:" << new_nonce << endl;
    }
    frame[frame_len++] = new_nonce >> 8;
    frame[frame_len++] = new_nonce & 0xFF;
    signFrame();
    sendFrame();
    sent_type = frame[0];
    response_pending = true;
}


// signFrame: add signature to the frame
// Global "frame" buffer contains the frame
// Global variable "frame_len" contains the frame length
void signFrame() {
    uint8_t *digest;
    unsigned int dg_len;
    digest = HMAC(EVP_sha1(), key, sizeof(key), frame, frame_len, frame+frame_len, &dg_len);
    frame_len += dg_len;
    //cout << "signed frame:" << hexdump(frame, frame_len) << endl;;
}

// verifyFrame: verifies the signature included *some* frames
// Global "frame" buffer contains the frame
// Global variable "frame_len" contains the frame length
bool verifyFrame() {
    bool retval = false;
    uint8_t dg_buff[64];
    uint8_t *digest;
    int sig_len = EVP_MD_size(EVP_sha1());
    //cout << "sig_len:" << sig_len << endl;
    unsigned int dg_len;
    //cout << "verifying signature" << endl;
    digest = HMAC(EVP_sha1(), key, sizeof(key), frame, frame_len-sig_len, dg_buff, &dg_len);
    if (memcmp(frame+frame_len-sig_len, dg_buff, sig_len) == 0) {
        retval = true;
    }
    else {
        cout << "Frame verification failed" << endl;
    }
    frame_len -= dg_len;
    return retval;
}

void addCrc32(void) {
    uint32_t crc = crc32(frame, frame_len);
    frame[frame_len++] = (uint8_t)(crc >> 24);
    frame[frame_len++] = (uint8_t)((crc >> 16) & 0xFF);
    frame[frame_len++] = (uint8_t)((crc >> 8) & 0xFF);
    frame[frame_len++] = (uint8_t)(crc & 0xFF);
}

bool checkCrc32(void) {
    frame_len -= 4;
    uint32_t crc = crc32(frame, frame_len);
    uint32_t r_crc = ((uint32_t)frame[frame_len] << 24) |
                     ((uint32_t)frame[frame_len+1] << 16) |
                     ((uint32_t)frame[frame_len+2] << 8) |
                     (uint32_t)frame[frame_len+3];
    return (crc == r_crc);
}

void process_signals(string signals) {
    Strings sigs = split(signals, ",");
    for (int i=0; i < sigs.size(); i++) {
        string signal = sigs[i];
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
        int pos = 0;
        frame[frame_len++] = 3;
        frame[frame_len++] = 1;
        uint8_t v = 3;
        if (val == "active") {
            v = 1;
        }
        else if (val == "off") {
            v = 0;
        }
        can_status &= ~(0x03 << pos);
        can_status |= v << pos;
        frame[frame_len++] = can_status;
    }
    else if (sig == "can_b") {
        int pos = 2;
        frame[frame_len++] = 3;
        frame[frame_len++] = 1;
        uint8_t v = 3;
        if (val == "active") {
            v = 1;
        }
        else if (val == "off") {
            v = 0;
        }
        can_status &= ~(0x03 << pos);
        can_status |= v << pos;
        frame[frame_len++] = can_status;
    }
    else if (sig == "ignition") {
        frame[frame_len++] = 1;
        frame[frame_len++] = 1;
        uint8_t v = 0;
        if (val == "on") {
            v = 1;
        }
        else if (val == "sna") {
            v = 255;
        }
        frame[frame_len++] = v;
    }
    else if (sig == "engine") {
        frame[frame_len++] = 2;
        frame[frame_len++] = 2;
        uint16_t v = 0;
        if (val == "sna") {
            v = 65535;
        }
        else if (val != "off") {
            v = stringToInt(val);
        }
        frame[frame_len++] = v >> 8;
        frame[frame_len++] = v & 0xFF;
    }
    else if (sig == "speed") {
        frame[frame_len++] = 7;
        frame[frame_len++] = 2;
        int speed = 65535;
        if (val != "sna") {
            speed = int(stringToDouble(val)*10.+0.5);
        }
        frame[frame_len++] = speed >> 8;
        frame[frame_len++] = speed & 0xff;
    }
    else if (sig == "odometer") {
        frame[frame_len++] = 8;
        frame[frame_len++] = 4;
        uint32_t odom = 4294967295;
        if (val != "sna") {
            odom = uint32_t(stringToDouble(val)*10.+0.5);
        }
        frame[frame_len++] = odom >> 24;
        frame[frame_len++] = (odom >> 16) & 0xff;
        frame[frame_len++] = (odom >> 8) & 0xff;
        frame[frame_len++] = odom & 0xff;
    }
    else if (sig == "fuel") {
        frame[frame_len++] = 9;
        frame[frame_len++] = 1;
        int fuel = 255;
        if (val != "sna") {
            fuel = stringToInt(val);
        }
        frame[frame_len++] = fuel;
    }
    else if (sig == "panic") {
        frame[frame_len++] = 12;
        frame[frame_len++] = 1;
        uint8_t v = 0;
        if (val == "on") {
            v = 1;
        }
        else if (val == "sna") {
            v = 255;
        }
        frame[frame_len++] = v;
    }
    else if (sig == "transmission") {
        frame[frame_len++] = 22;
        frame[frame_len++] = 1;
        uint8_t v = val[0];
        if (val == "sna") {
            v = 255;
        }
        frame[frame_len++] = v;
    }
    else if (sig == "vin") {
        frame[frame_len++] = 48;
        if (val == "sna") {
            val = "";
        }
        int l = val.length();
        frame[frame_len++] = l;
        memcpy(frame+frame_len, val.c_str(), l);
        frame_len += l;
    }
    else if (sig == "doors") {
        frame[frame_len++] = 4;
        frame[frame_len++] = 1;
        Strings vals = split(val, "/");
        uint8_t doors = 0;
        for (int i=0; i < 4; i++) {
            int v = 0;
            if (vals[i] == "o") {
                v = 1;
            }
            else if (vals[i] == "sna") {
                v = 3;
            }
            doors |= v << (i*2);
        }
        frame[frame_len++] = doors;
    }
    else if (sig == "trunk") {
        frame[frame_len++] = 6;
        frame[frame_len++] = 1;
        int v = 0;
        if (val == "o") {
            v = 1;
        }
        else if (val == "sna") {
            v = 255;
        }
        frame[frame_len++] = v;
    }
    else if (sig == "locks") {
        frame[frame_len++] = 5;
        frame[frame_len++] = 1;
        Strings vals = split(val, "/");
        uint8_t locks = 0;
        for (int i=0; i < 4; i++) {
            int v = 0;
            if (vals[i] == "l") {
                v = 1;
            }
            else if (vals[i] == "sna") {
                v = 3;
            }
            locks |= v << (i*2);
        }
        frame[frame_len++] = locks;
    }
    else if (sig == "seats") {
        frame[frame_len++] = 18;
        frame[frame_len++] = 2;
        Strings vals = split(val, "/");
        uint16_t seats = 0;
        for (int i=0; i < 5; i++) {
            int v = 0;
            if (vals[i] == "o") {
                v = 1;
            }
            else if (vals[i] == "sna") {
                v = 3;
            }
            seats |= v << (i*2);
        }
        frame[frame_len++] = seats >> 8;
        frame[frame_len++] = seats & 0xFF;
    }
    else if (sig == "seatbelts") {
        frame[frame_len++] = 19;
        frame[frame_len++] = 2;
        Strings vals = split(val, "/");
        uint16_t belts = 0;
        for (int i=0; i < 5; i++) {
            int v = 0;
            if (vals[i] == "f") {
                v = 1;
            }
            else if (vals[i] == "sna") {
                v = 3;
            }
            belts |= v << (i*2);
        }
        frame[frame_len++] = belts >> 8;
        frame[frame_len++] = belts & 0xFF;
    }
    else if (sig == "tire_pressures") {
        uint8_t vs[5];
        Strings vals = split(val, "/");
        int l = vals.size();
        frame[frame_len++] = 11;
        frame[frame_len++] = l;
        for (int i=0; i < l; i++) {
            vs[i] = 0xFF;
            if (vals[i] != "sna") {
                vs[i] = stringToInt(vals[i]);
            }
        }
        memcpy(frame+frame_len, vs, l);
        frame_len += l;
    }
    else if (sig == "oil_life") {
        frame[frame_len++] = 13;
        frame[frame_len++] = 1;
        uint8_t v = 255;
        if (val != "sna") {
            v = stringToInt(val);
        }
        frame[frame_len++] = v;
    }
    else if (sig == "low_fuel_indicator") {
        frame[frame_len++] = 10;
        frame[frame_len++] = 1;
        uint8_t v = 255;
        if (val == "on") {
            v = 1;
        }
        else if (val == "off") {
            v = 0;
        }
        frame[frame_len++] = v;
    }
    else if (sig == "check_engine_indicator") {
        frame[frame_len++] = 16;
        frame[frame_len++] = 1;
        uint8_t v = 255;
        if (val == "yes") {
            v = 1;
        }
        else if (val == "no") {
            v = 0;
        }
        frame[frame_len++] = v;
    }
    else if (sig == "sunroof") {
        frame[frame_len++] = 28;
        frame[frame_len++] = 1;
        uint8_t v = 255;
        if (val != "sna") {
            v = stringToInt(val);
        }
        frame[frame_len++] = v;
    }
    else if (sig == "faults") {
    }
    else if (sig == "ecus") {
    }
    else if (sig == "ev_charge_state") {
        frame[frame_len++] = 49;
        frame[frame_len++] = 2;
        uint16_t bc = 65535;
        if (val != "sna") {
            bc = int(stringToDouble(val)*100.+0.5);
        }
        frame[frame_len++] = bc >> 8;
        frame[frame_len++] = bc & 0xff;
    }
    else if (sig == "ev_discharge_rate") {
        frame[frame_len++] = 50;
        frame[frame_len++] = 2;
        uint16_t dr = 65535;
        if (val != "sna") {
            dr = int(stringToDouble(val)*100.+0.5);
        }
        frame[frame_len++] = dr >> 8;
        frame[frame_len++] = dr & 0xff;
    }
    else if (sig == "v2x_steering_wheel_angle") {
        frame[frame_len++] = 51;
        frame[frame_len++] = 2;
        uint16_t swa = 65535;
        if (val != "sna") {
            swa = stringToInt(val) + 540;
        }
        frame[frame_len++] = swa >> 8;
        frame[frame_len++] = swa & 0xff;
    }
    else {
        cout << "Unknown signal: " << sig << endl;
    }
}

// handleRequest: handles requests from VA
// Global "frame" buffer contains the request
// It formats the response in the global "frame" buffer
// Global variable frame_len contains the length of input frame
// Global variable frame_len is update to length of the response
// frame
void handleRequest() {
    if (verifyFrame()) {
        uint8_t type = frame[0];
        uint16_t nonce = ((uint16_t)frame[1] << 8) | frame[2];
        if (debug) {
            cout << "received nonce:" << nonce << endl;
        }
        if (type == 0x01) { // Get RTC
            struct tm tm;
            time_t now;
            time(&now);
            (void)gmtime_r(&now, &tm);
            tm.tm_year += 1900;
            tm.tm_mon++;
            frame_len = 3;
            frame[frame_len++] = tm.tm_year % 100;
            frame[frame_len++] = tm.tm_mon;
            frame[frame_len++] = tm.tm_mday;
            frame[frame_len++] = tm.tm_hour;
            frame[frame_len++] = tm.tm_min;
            frame[frame_len++] = tm.tm_sec;
        }
        else if (type == 0x07) { // Set watchdog timer
            watchdog_timer = frame[3];
            cout << "Setting watchdog = " << watchdog_timer << endl;
            frame_len = 3;
        }
        else if (type == 0x0A) { // Get VIC version
            string build = "GWM";
            int build_len = build.length();
            frame_len = 3;
            // Add firmware version
            frame[frame_len++] = 0x01;
            frame[frame_len++] = 0x00;
            frame[frame_len++] = 0x01;
            // Add build version
            frame[frame_len++] = build_len;
            memcpy(frame+frame_len, build.c_str(), build_len);
            frame_len += build_len;
            // Add bootloader version
            frame[frame_len++] = 0x01;
            frame[frame_len++] = 0x00;
            // Add requested version
            frame[frame_len++] = build_len;
            memcpy(frame+frame_len, build.c_str(), build_len);
            frame_len += build_len;
        }
        else if (type == 0x0D) { // Get VIC status
            frame_len = 3;
            uint8_t status = 0x00;
            if ((can_status & 0x03) == 1) {
                status |= 0x20;
            }
            if (((can_status >> 2) & 0x03) == 1) {
                status |= 0x40;
            }
            frame[frame_len++] = status;
        }
        else if (type == 0x21) { // Remote car control command
            uint8_t device = frame[3];
            uint8_t action = frame[4];
            sendRemoteCommands(device, action);
            frame_len = 3;
        }
        else if (type == NONCE_NOTIFICATION) {
            frame_len = 3;
            send_nonce = ((uint16_t)frame[3] << 8) | frame[4];
            recv_nonce = rand();
            if (debug) {
                cout << "Calculated new recv_nonce:" << recv_nonce << endl;
            }
            frame[frame_len++] = recv_nonce >> 8;
            frame[frame_len++] = recv_nonce & 0xFF;
            signFrame();
        }
        if (debug) {
            cout << "response:" << hexdump(frame, frame_len) << endl;
        }
        if (frame_len > 0) {
            sendFrame();
        }
    }
    else {
        sendError(AUTH_ERROR);
    }
}

// processResponse: processes responses to frames sent
// The actual frame already stored in the global "frame" buffer
void processResponse(uint8_t type) {
    if (type == NONCE_NOTIFICATION) {
        if (verifyFrame()) {
            send_nonce = ((uint16_t)frame[3] << 8) | frame[4];
            recv_nonce = new_nonce;
            if (debug) {
                cout << "Setting send_nonce:" << send_nonce << endl;
                cout << "Setting recv_nonce:" << recv_nonce << endl;
            }
        }
        else {
            sendError(AUTH_ERROR);
        }
    }
}

// sendError: sends Error response frame
// @error_type is the type of error
// The global buffer "frame" contains the frame that had the error
// The global variable "frame_len" is the length of the frame with error
void sendError(int error_type) {
    uint8_t errored_frame[3];
    int l = 3;
    if (frame_len < 3) {
        l = frame_len;
    }
    memcpy(errored_frame, frame, l);
    frame_len = 0;
    frame[frame_len++] = 0xFF;
    frame[frame_len++] = send_nonce >> 8;
    frame[frame_len++] = send_nonce & 0xFF;
    frame[frame_len++] = error_type;
    frame[frame_len++] = recv_nonce >> 8;
    frame[frame_len++] = recv_nonce & 0xFF;
    memcpy(frame+frame_len, errored_frame, l);
    frame_len += l;
    sendFrame();
}

// sendRemoteCommands: creates remote command string and send
// it to the Vehicle Emulator
// @device: the device to control
// @action: the action to perform
// Global variables ve_remote_ip and ve_remote_port are the IP
// address an port to send the command string
// Global variable ve_sock is the socket to use
void sendRemoteCommands(uint8_t device, uint8_t action) {
    string command;
    switch (device) {
      case 0x01:
        command = "locks ";
        if (action == 0x01) {
            command += "lock";
        }
        else if (action == 0x02) {
            command += "unlock";
        }
        else if (action == 0x03) {
            command += "unlock_driver";
        }
	break;
      case 0x02:
        command = "engine ";
        if (action == 0x01) {
            command += "on";
        }
        else if (action == 0x02) {
            command += "off";
        }
        break;
      case 0x03:
        command = "trunk ";
        if (action == 0x01) {
            command += "open";
        }
        else if (action == 0x02) {
            command += "close";
        }
        break;
      case 0x04:
        command = "leftdoor ";
        if (action == 0x01) {
            command += "open";
        }
        else if (action == 0x02) {
            command += "close";
        }
        break;
      case 0x05:
        command = "rightdoor ";
        if (action == 0x01) {
            command += "open";
        }
        else if (action == 0x02) {
            command += "close";
        }
        break;
      case 0x06:
        command = "panic ";
        if (action == 0x01) {
            command += "on";
        }
        else if (action == 0x02) {
            command += "off";
        }
        break;
      case 0x0C:
        command = "sunroof ";
        command += toString(action);
        break;
    }
    command += "\n";
    if (debug) {
        cout << "sending: " << command << endl;
    }
    struct sockaddr_in to;
    to.sin_family = AF_INET;
    to.sin_port = htons(ve_remote_port);
    to.sin_addr.s_addr = inet_addr(ve_remote_ip.c_str());
    int n = sendto(ve_sock, command.c_str(), command.length(), 0, (struct sockaddr *)&to, sizeof(to));
    if (n <= 0) {
        cerr << "Failed to send" << endl;
    }
}

// openPort: routing to open a serial port
// It will creates a lock file to stop multiple programs from accessing
// the serial port
// @port is the name of the port to open. "/dev/" is prepended it
// @speed is the baudrate to use
// Global variable serial_sock is set
// Global structure old tio is set to restore port settings on exit
void openPort(string port, string speed) {
    cerr << getDateTime() << " ";
    cerr << "Opening " << port << endl;
    string dev = "/dev/" + port;
    if (fileExists(dev)) {
        if (!lock(port)) {
            cerr << "Serial device '" << port << "' already in use" << endl;
            closeAndExit(1);
        }
        serial_sock = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
        if (serial_sock < 0) {
            perror(dev.c_str());
            closeAndExit(-1);
        }
//        struct serial_struct serial;
//        ioctl(serial_sock, TIOCGSERIAL, &serial);
//        cerr << "xmit_fifo_size:" << serial.xmit_fifo_size << endl;
//        cerr << "recv_fifo_size:" << serial.recv_fifo_size << endl;
        if (tcgetattr(serial_sock,&oldtio)) {
            cerr << "Failed to get old port settings" << endl;
        }

        // set new port settings for canonical input processing 
        struct termios newtio;
        int baud = baudString(speed);
        newtio.c_cflag = baud | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;
        newtio.c_lflag = 0;
        newtio.c_cc[VMIN]=1;
        newtio.c_cc[VTIME]=1;
        if (tcsetattr(serial_sock,TCSANOW,&newtio)) {
            cerr << "Failed to set new port settings" << endl;
        }
        if (tcflush(serial_sock, TCIOFLUSH)) {
            cerr << "Failed to flush serial port" << endl;
        }
        if (fcntl(serial_sock, F_SETFL, FNDELAY)) {
            cerr << "Failed to set fndelay on serial port" << endl;
        }
    }
}

// baudString: converts baudrate from a string to value used in tty config
// @baud is the baudrate string
// returns value to use for setting the tty config
int baudString(string baud) {
    int val = B4800;
    if (baud == "2400") {
        val = B2400;
    }
    else if (baud == "4800") {
        val = B4800;
    }
    else if (baud == "9600") {
        val = B9600;
    }
    else if (baud == "19200") {
        val = B19200;
    }
    else if (baud == "38400") {
        val = B38400;
    }
    else if (baud == "57600") {
        val = B57600;
    }
    else if (baud == "115200") {
        val = B115200;
    }
    else if (baud == "230400") {
        val = B230400;
    }
    else if (baud == "460800") {
        val = B460800;
    }
    else if (baud == "921600") {
        val = B921600;
    }
    else {
        cout << "Invalid serial port speed: " << baud << endl;
    }
    return val;
}

// closePort: closes the open serial port
// The global variable serial_sock is the socket to close
// The globar structure oldtio is used to restore the port to its
// original state
// It removes the lock file
void closePort() {
    if (serial_sock >= 0) {
        // restore old port settings
        tcsetattr(serial_sock,TCSANOW,&oldtio);
        tcflush(serial_sock, TCIOFLUSH);
        close(serial_sock);        //close the com port
        if (lockfile != "") {
            remove(lockfile.c_str());
        }
    }
}

// lock: creates a lock file for the serial port
// @port is the serial port
bool lock(string port) {
    bool locked = false;
    lockfile = "/var/lock/LCK..";
    lockfile += port;
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0);
    if (fd >= 0) {
        close(fd);
        locked = true;
    }
    return locked;
}

// closeAndExit: closes the serial port and exits
// @exitstat is the exit status
void closeAndExit(int exitstat) {
    closeAll();
    exit(exitstat);
}

// closeAll: closes all ports and cleans up
void closeAll() {
    closePort();
}

// onTerm: routine to perform on "termination" signals
void onTerm(int parm) {
    closeAndExit(0);
}

// hexdump: dumps the given buffer in hex to a string
// Used for debugging purposes
// @packet: pointer to data to dump
// @size: number of bytes to dump
// Returns a string of the hex data
string hexdump(uint8_t *packet, int size) {
    string outstr = "\n";
    int j = 0;
    for (int i=0; i < size; i++) {
        outstr += string_printf("%02X", packet[i]);
        if ((i+1) != size) {
            if (j == 15) {
                outstr += "\n";
                j = 0;
            }
            else {
                outstr += " ";
                j++;
            }
        }
    }
    return(outstr);
}

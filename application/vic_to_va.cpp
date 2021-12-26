#define VA2

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
#include "autonet_types.h"
#include "cobs.h"
#include "crc32.h"
#include "dateTime.h"
#include "fileExists.h"
#include "getopts.h"
#include "hiresTime.h"
#ifdef VA2
#include "lpVa.h"
#endif
#include "readconfig.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
#include "va_helper.h"
using namespace std;

bool debug = false;

#ifdef VA2
void va2_command_callback(char type, const char *key, const char *value);
#endif
void processFrame(uint8_t *buffer, int n);
void processResponse(uint8_t type);
void sendToVa(string str);
void processIncoming(uint8_t type, uint16_t nonce);
void processError();
string errorString(uint8_t err_type);
void sendAck(uint8_t type, uint16_t nonce);
void sendError(int error_type);
void sendNonceNotify();
void sendCommand(string cmnd);
void signFrame();
bool verifyFrame();
void sendFrame();
void buildCommand(string command);
void addCrc32(void);
bool checkCrc32(void);
void process_states(uint8_t *frame, int len);
string process_state(uint8_t type, int l, uint8_t *p);
void openPort(string port, string speed);
int baudString(string baud);
void closePort();
bool lock(string port);
void closeAndExit(int exitstat);
void closeAll();
void onTerm(int parm);
string hexdump(uint8_t *packet, int size);

bool serial_port = false;
uint8_t frame[1024];
uint8_t cobs_frame[1030];
int frame_len;
bool do_nonce = false;
uint16_t send_nonce = 1;
uint16_t recv_nonce = 1;
uint16_t new_nonce;
int serial_sock = -1;
struct termios oldtio;  // Old port settings for serial port
string lockfile = "";
int vic_sock = -1;
Config config;
struct sockaddr_in vic_to;
//vic.VicSimAddr
string vicsim_addr = "0.0.0.0:50011";
//Vic.VicVaAddr
string vasim_addr = "0.0.0.0:50012";
bool response_pending = false;
uint8_t sent_type = 0x00;
VaHelper va;
VaHelper va_commands;
string SerialPort;
string SerialSpeed;
static char key[16] = {0x71,0xEB,0x12,0xE4,0xBC,0xB4,0xE8,0xBE,0xC1,0x26,0xC5,0x45,0xF6,0x5F,0xCC,0x92};
#define NONCE_NOTIFICATION (0xEF)
#define CRC_ERROR (0x00)
#define AUTH_ERROR (0x01)
#define NONCE_ERROR (0x02)
#define TOO_LONG_ERROR (0x03)
#define FORMAT_ERROR (0x04)
#define TYPE_ERROR (0x05)
#define COMMAND_ERROR (0x06)
#define PARAM_ERROR (0x07)
#define TIMEOUT_ERROR (0x08)

main(int argc, char *argv[]) {
    GetOpts opts(argc, argv, "dhnp:");
    debug = opts.exists("d");
    do_nonce = opts.exists("n");
    string portstr = opts.get("p");
    if (opts.exists("h")) {
        cout << "usage: vic_to_va [-dhn] [-p portstr] msgs . . . " << endl;
        cout << "where:" << endl;
        cout << "  -d            : Turns on debug mode" << endl;
        cout << "  -h            : Displays this help" << endl;
        cout << "  -n            : Sends Nonce negotiation" << endl;
        cout << "  -p port:speed : Specifies serial port and speed" << endl;
        cout << "  msgs          : Messages to send" << endl;
        exit(1);
    }
    char config_key[] = "vic.*";
    config.readConfigManager(config_key);
    if (config.exists("vic.VicSimAddr")) {
        vicsim_addr = config.getString("vic.VicSimAddr");
    }
    if (config.exists("vic.VicVaAddr")) {
        vasim_addr = config.getString("vic.VicVaAddr");
    }
    if (config.exists("vic.SerialPort")) {
        SerialPort = config.getString("vic.SerialPort");
    }
    if (config.exists("vic.SerialSpeed")) {
        SerialSpeed = config.getString("vic.SerialSpeed");
    }
    int pos;
    if (portstr != "") {
        SerialPort = portstr;
        pos = SerialPort.find(":");
        if (pos != string::npos) {
            SerialSpeed = SerialPort.substr(pos+1);
            SerialPort = SerialPort.substr(0, pos);
        }
    }
    if (SerialPort != "") {
        openPort(SerialPort, SerialSpeed);
        serial_port = true;
    }
    else {
        pos = vasim_addr.find(":");
        string server_ip = vasim_addr.substr(0, pos);
        int server_port = stringToInt(vasim_addr.substr(pos+1));
        //cout << "server address:" << server_ip << " " << server_port << endl;
        pos = vicsim_addr.find(":");
        string vic_ip = vicsim_addr.substr(0, pos);
        int vic_port = stringToInt(vicsim_addr.substr(pos+1));
        //cout << "vic address:" << vic_ip << " " << vic_port << endl;
        vic_sock = socket(PF_INET, SOCK_DGRAM, 0);
        if (vic_sock < 0) {
            cerr << "Could not open socket" << endl;
            exit(1);
        }
        struct linger linger_opt;
        linger_opt.l_onoff = 0;
        linger_opt.l_linger = 0;
        setsockopt(vic_sock, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
        int sock_flag = 1;
        setsockopt(vic_sock, SOL_SOCKET, SO_REUSEADDR, &sock_flag, sizeof(sock_flag));
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server_port);
        addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
        if (bind(vic_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            cerr << "Could not bind to port" << endl;
            exit(0);
        }
        vic_to.sin_family = AF_INET;
        vic_to.sin_port = htons(vic_port);
        vic_to.sin_addr.s_addr = inet_addr(vic_ip.c_str());
    }
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    srand(time(NULL));
#ifdef VA2
    if (lp_va_initFromConfig("vic") != lp_va_ok) {
        cerr << "Failed lp_va_initFromConfig" << endl;
        exit(1);
    }
    if (lp_va_setCallback(va2_command_callback) != lp_va_ok) {
        cerr << "Failed lp_va_setCallback" << endl;
        exit(1);
    }
#else
    va.createSocket("ve", false);
    va_commands.createSocket("ve", true);
    string resp = va.sendQuery("ignition",5,1);
    if (resp == "") {
        cerr << "VA not responding" << endl;
    }
    else {
        cout << "Talking to VA" << endl;
    }
#endif
    uint8_t buffer[1030];
    uint8_t recv_buff[1030];
    struct sockaddr_in from;
    socklen_t fromlen;
    int argi = 1;
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
        //if (!response_pending and (ve_sock >= 0)) {
        //    FD_SET(ve_sock, &rd);
        //    nfds = max(nfds, ve_sock);
        //}
        if (vic_sock >= 0) {
            FD_SET(vic_sock, &rd);
            nfds = max(nfds, vic_sock);
        }
        if (serial_sock >= 0) {
            FD_SET(serial_sock, &rd);
            nfds = max(nfds, serial_sock);
        }
#ifndef VA2
        int cmnds_sock = va_commands.getSocket();
        if (cmnds_sock >= 0) {
            FD_SET(cmnds_sock, &rd);
            nfds = max(nfds, cmnds_sock);
        }
#endif
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
            if ((vic_sock >= 0) and FD_ISSET(vic_sock, &rd)) {
                fromlen = sizeof(from);
                int n = recvfrom(vic_sock, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&from, &fromlen);
                if (n < 0) {
                    perror("recvfrom");
                    break;
                }
                processFrame(buffer, n);
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
                                 processFrame(buffer, fl);
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
                             buffer[fl++] = b;
                         }
                    }
                }
            }
#ifndef VA2
            if ((cmnds_sock >= 0) and FD_ISSET(cmnds_sock, &rd)) {
                string resp = va_commands.vah_recv();
                if (debug) {
                    cout << "Got command: " << resp << endl;
                }
                sendCommand(resp);
            }
#endif
        }
        else { // res == 0
            if (in_frame and !interbyte_timeout) {
                cerr << "Interbyte timeout" << endl;
                sendError(TIMEOUT_ERROR);
                interbyte_timeout = true;
            }
            if (argi < argc) {
                sendCommand(argv[argi]);
                if (debug) {
                    cout << "arg: " << argv[argi] << endl;
                }
                argi++;
            }
        }
    }
    cerr << "exiting" << endl;
    closeAndExit(0);
}

#ifdef VA2
void va2_command_callback(char type, const char *key, const char *value) {
    if (type == LP_VA_CBCOMMAND) {
        string cmd = string(key) + " " + value;
        sendCommand(cmd);
    }
}
#endif

// processFrame: processes a frame received from SerialPort or UDP.
// The start-of-frame and end-of-frame ('\0') for the serial port need
// need to be remove prior to passing this routine
// Parameters
// This routine unstuffs the COBS and stores the resultant frame in
// the global "frame" buffer
// It also calculates and checks the CRC32
// It also checks if the frame is an acknowlgment/response to the previously
// sent frame
// @buffer is pointer to the buffer containing the frame
// @n is the number of bytes in the frame
void processFrame(uint8_t *buffer, int n) {
    frame_len = UnStuffData(buffer, n, frame);
    if (debug) {
        cout << "got:" << hexdump(frame, n) << endl;
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
                processIncoming(type, nonce);
            }
            else if (type == NONCE_NOTIFICATION) {
                processIncoming(type, nonce);  // Handle Nonce regardless
            }
            else {
                cout << "Received Nonce error" << endl;
                sendError(NONCE_ERROR);
            }
        }
    }
    else {
        cout << "CRC32 failure" << endl;
        uint8_t type = frame[0];
        if (type != 0xFF) {
            sendError(CRC_ERROR);
        }
    }
}

// processResponse: processes responses to frames sent
// The actual frame already stored in the global "frame" buffer
void processResponse(uint8_t type) {
    if (type == 0x01) {    // RTC
        struct tm tm;
        int year = frame[3] + 2000;
        int mon = frame[4];
        int day = frame[5];
        int hour = frame[6];
        int min = frame[7];
        int sec = frame[8];
        string datetime = string_printf("%04d-%02d-%02d/%02d:%02d:%02d", year, mon, day, hour, min, sec);
        cout << "RTC: " << datetime << endl;
        tm.tm_year = year - 1900;
        tm.tm_mon = mon - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        time_t rtc = mktime(&tm);
        string rtc_sig = string_printf("rtc:%ld", rtc);
        time_t now = time(NULL);
        cout << "System time:" << now << endl;
        sendToVa(rtc_sig);
    }
    else if (type == 0x0A) {
        char build[16];
        char req_build[16];
        int i = 3;
        int sw_maj = frame[i++];
        int sw_min = frame[i++];
        int sw_step = frame[i++];
        int bld_len = frame[i++];
        memcpy(build, frame+i, bld_len);
        build[bld_len] = '\0';
        i += bld_len;
        int bl_maj = frame[i++];
        int bl_min = frame[i++];
        int req_len = frame[i++];
        memcpy(req_build, frame+i, req_len);
        req_build[req_len] = '\0';
        printf("VIC build:%s version:%d.%d.%d\n",build,sw_maj,sw_min,sw_step);
        printf("VIC bootloader version:%d.%d\n", bl_maj, bl_min);
        printf("VIC requested build:%s\n", req_build);
        string vicversion = string_printf("vicversion:%s-%d.%d.%d/%d.%d/%s",
                            build,sw_maj,sw_min,sw_step,bl_maj,bl_min,req_build);
        sendToVa(vicversion);
    }
    else if (type == 0x0D) {
        int status = frame[3];
        printf("VIC status:%02X\n", status);
        string vicstatus = string_printf("vicstatus:%d",status);
        sendToVa(vicstatus);
    }
    else if (type == NONCE_NOTIFICATION) {
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

// sendToVa: sends states string to VA
// @str is string containing states to send
void sendToVa(string str) {
    if (debug) {
        cout << str << endl;
    }
#ifdef VA2
    Strings sigs = split(str, ",");
    for (int i=0; i < sigs.size(); i++) {
        string sig = sigs[i];
        int err = lp_va_notify(sig.c_str());
        if (err != lp_va_ok) {
            cerr << "Failed lp_va_notify(" << err << "):" << sig << endl;
        }
    }
//    if (lp_va_notify(str.c_str()) != lp_va_ok) {
//        cerr << "Failed lp_va_notify" << endl;
//        closeAndExit(1);
//    }
#else
    va.vah_send(str);
#endif
}

// processIncoming: processes an incoming frame (Notifications)
// Currently only supports NotifyStateChanges
// The actual frame already stored in the global "frame" buffer
// It will send an Acknowlegment frame
// @type: type of the frame
// @nonce: nonce that was received in the frame
void processIncoming(uint8_t type, uint16_t nonce) {
    if (debug) {
        cout << "received nonce:" << nonce << endl;
    }
    if (type == NONCE_NOTIFICATION) {
        if (verifyFrame()) {
            send_nonce = ((uint16_t)frame[3] << 8) | frame[4];
            frame_len = 0;
            frame[frame_len++] = type;
            frame[frame_len++] = (uint8_t)(nonce >> 8);
            frame[frame_len++] = (uint8_t)(nonce & 0xFF);
            recv_nonce = rand();
            frame[frame_len++] = recv_nonce >> 8;
            frame[frame_len++] = recv_nonce & 0xFF;
            signFrame();
            sendFrame();
            if (debug) {
                cout << "Calc random recv_nonce:" << recv_nonce << endl;
            }
        }
        else {
            sendError(AUTH_ERROR);
        }
    }
    else {
        if (type == 0xF1) {
            process_states(frame+3, frame_len-3);
        }
        sendAck(type, nonce);
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

// sendAck: sends Acknowledgement frame
// @type: type of frame received
// @nonce: nonce that was frame that is being acknowleged
void sendAck(uint8_t type, uint16_t nonce) {
    frame[0] = type;
    frame[1] = (uint8_t)(nonce >> 8);
    frame[2] = (uint8_t)(nonce & 0xFF);
    frame_len = 3;
    sendFrame();
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

// sendCommand: sends a command/request frame
// It formats the frame in the global "frame" buffer
// It invokes sendFrame to send the frame
// @cmnd command string to be sent
void sendCommand(string cmnd) {
    frame_len = 0;
    buildCommand(cmnd);
    if (frame_len > 0) {
        frame[1] = send_nonce >> 8;
        frame[2] = send_nonce & 0xFF;
        if (debug) {
            cout << "sending nonce:" << send_nonce << endl;
        }
        signFrame();
        sendFrame();
        sent_type = frame[0];
        response_pending = true;
    }
}

// signFrame: add signature to the frame
// Global "frame" buffer contains the frame
// Global variable "frame_len" contains the frame length
void signFrame() {
    uint8_t *digest;
    unsigned int dg_len;
    digest = HMAC(EVP_sha1(), key, sizeof(key), frame, frame_len, frame+frame_len, &dg_len);
    frame_len += dg_len;
    //if (debug) {
    //    cout << "signed frame:" << hexdump(frame, frame_len) << endl;;
    //}
}

// verifyFrame: verifies the signature included *some* frames
// Global "frame" buffer contains the frame
// Global variable "frame_len" contains the frame length
bool verifyFrame() {
    bool retval = false;
    uint8_t dg_buff[64];
    uint8_t *digest;
    int sig_len = EVP_MD_size(EVP_sha1());
    if (frame_len >= (sig_len+3)) {
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
    }
    return retval;
}

// sendFrame: sends a frame to SerialPort or UDP socket
// The frame is in the global "frame" buffer
// It will add the CRC32 and perform the COBS stuffing
// If sending to SerialPort, It will also add the start-of-frame
// and end-of-frame 0x00
void sendFrame() {
    addCrc32();
    if (debug) {
        cout << "sending:" << hexdump(frame, frame_len) << endl;
    }
    if (serial_port) {
        cobs_frame[0] = 0x00;
        size_t cl = StuffData(frame, frame_len, cobs_frame+1);
        cl++;
        cobs_frame[cl++] = 0x00;
        int sn = write(serial_sock, cobs_frame, cl);
    }
    else {
        size_t cl = StuffData(frame, frame_len, cobs_frame);
        int sn = sendto(vic_sock, cobs_frame, cl, 0, (struct sockaddr *)&vic_to, sizeof(vic_to));
    }
}

// buildCommand: parses the given command and formats it into a
// frame to be sent.
// The frame is stored in the global "frame" buffer
// There is reserved locations for the nonce to be stored
// @string: command string to be parsed
// Currently only support remote control commands
void buildCommand(string command) {
    Strings parts = split(command, " ");
    string cmnd = parts[0];
    string what = parts[1];
    frame_len = 0;
    parts.erase(parts.begin(),parts.begin()+2);
    if (cmnd == "get") {
        if (what == "rtc") {
            frame[0] = 0x01;
            frame_len = 3;
        }
        else if (what == "vicversion") {
            frame[0] = 0x0A;
            frame_len = 3;
        }
        else if (what == "vicstatus") {
            frame[0] = 0x0D;
            frame_len = 3;
        }
    }
    else if (cmnd == "set") {
        if (what == "watchdog") {
            frame[0] = 0x07;
            cout << "watchdog value:" << parts[0] << endl;
            frame[3] = stringToInt(parts[0]);
            frame_len = 4;
        }
    }
    else {
        frame[0] = 0x21;
        if (cmnd == "locks") {
            frame[3] = 0x01;
            if (what == "lock") {
                frame[4] = 0x01;
            }
            else if (what == "unlock") {
                frame[4] = 0x02;
            }
            else if (what == "unlock_driver") {
                frame[4] = 0x03;
            }
        }
        else if (cmnd == "engine") {
            frame[3] = 0x02;
            if (what == "on") {
                frame[4] = 0x01;
            }
            else if (what == "off") {
                frame[4] = 0x02;
            }
        }
        else if (cmnd == "trunk") {
            frame[3] = 0x03;
            if (what == "open") {
                frame[4] = 0x01;
            }
            else if (what == "close") {
                frame[4] = 0x02;
            }
        }
        else if (cmnd == "leftdoor") {
            frame[3] = 0x04;
            if (what == "open") {
                frame[4] = 0x01;
            }
            else if (what == "close") {
                frame[4] = 0x02;
            }
        }
        else if (cmnd == "rightdoor") {
            frame[3] = 0x05;
            if (what == "open") {
                frame[4] = 0x01;
            }
            else if (what == "close") {
                frame[4] = 0x02;
            }
        }
        else if (cmnd == "panic") {
            frame[3] = 0x06;
            if (what == "on") {
                frame[4] = 0x01;
            }
            else if (what == "off") {
                frame[4] = 0x02;
            }
        }
        else if (cmnd == "sunroof") {
            frame[3] = 0x0C;
            frame[4] = stringToInt(what);
        }
        frame_len = 5;
    }
}

// addCrc32: add the CRC32 to the frame in the global "frame" buffer
void addCrc32(void) {
    uint32_t crc = crc32(frame, frame_len);
    frame[frame_len++] = (uint8_t)(crc >> 24);
    frame[frame_len++] = (uint8_t)((crc >> 16) & 0xFF);
    frame[frame_len++] = (uint8_t)((crc >> 8) & 0xFF);
    frame[frame_len++] = (uint8_t)(crc & 0xFF);
}

// checkCrc32: checks the CRC32 of the frame in the global "frame" buffer
// Reduces the frame_len by the length of the CRC32 (4 bytes)
// Returns: true if CRC32 matches
bool checkCrc32(void) {
    bool retval = false;
    if (frame_len >= 7) {
        frame_len -= 4;
        uint32_t crc = crc32(frame, frame_len);
        uint32_t r_crc = ((uint32_t)frame[frame_len] << 24) |
                         ((uint32_t)frame[frame_len+1] << 16) |
                         ((uint32_t)frame[frame_len+2] << 8) |
                         (uint32_t)frame[frame_len+3];
        retval = (crc == r_crc);
    }
    return retval;
}

// process_states: processes incoming StateChangeNotification frames
// @frame: pointer to the start of the state information
// @len: length of the state information
// Sends the resulting State information to VehicleAbstraction
void process_states(uint8_t *frame, int len) {
    uint8_t *p = frame;
    string res = "";
    while (len >= 2) {
        uint8_t type = *p++;
        uint8_t l = *p++;
        uint8_t *np = p + l;
        string state = process_state(type, l, p);
        if (res.length() != 0) {
            res += ",";
        }
        res += state;
        len -= l + 2;
        p = np;
    }
    sendToVa(res);
}

// process_state: processes a single state entry
// @type: type of the signal
// @l: length of the signal data
// @p: pointer to the signal data
// returns the resultant signal state string
// <signal>:<value>
string process_state(uint8_t type, int l, uint8_t *p) {
    uint8_t v = *p;
    uint16_t v2 = 0;
    uint32_t v4 = 0;
    if (l == 2) {
        v2 = ((int)p[0] << 8) | p[1];
    }
    if (l == 4) {
        v4 = ((uint32_t)p[0] << 24) |
             ((uint32_t)p[1] << 16) |
             ((uint32_t)p[2] << 8) |
             p[3];
    }
    string retval = "";
    switch(type) {
      case 1:
        retval = "ignition:";
        if (v == 1) {
            retval += "on";
        }
        else if (v == 0) {
            retval += "off";
        }
        else {
            retval += "sna";
        }
        break;
      case 2:
        retval = "engine:";
        if (v2 == 65535) {
            retval += "sna";
        }
        else {
            retval += toString(v2);
        }
        break;
      case 3:
        {
            retval = "can_a:";
            uint8_t cv = v & 0x03;
            string cs = "sna";
            if (cv == 0) {
                cs = "off";
            }
            else if (cv == 1) {
                cs = "active";
            }
            retval += cs;
            retval += ",can_b:";
            cv = (v >> 2) & 0x03;
            cs = "sna";
            if (cv == 0) {
                cs = "off";
            }
            else if (cv == 1) {
                cs = "active";
            }
            retval += cs;
        }
        break;
      case 4:
        retval = "doors:";
        for (int i=0; i < 4; i++) {
            int d = (v >> (i*2)) & 0x03;
            if (i > 0) {
                retval += "/";
            }
            string ds = "sna";
            if (d == 0) {
                ds = "c";
            }
            else if (d == 1) {
                ds = "o";
            }
            retval += ds;
        }
        break;
      case 5:
        retval = "locks:";
        for (int i=0; i < 4; i++) {
            int d = (v >> (i*2)) & 0x03;
            if (i > 0) {
                retval += "/";
            }
            string ds = "sna";
            if (d == 0) {
                ds = "u";
            }
            else if (d == 1) {
                ds = "l";
            }
            retval += ds;
        }
        break;
      case 6:
        retval = "trunk:";
        {
            string ts = "sna";
            if (v == 0) {
                ts = "c";
            }
            else if (v == 1) {
                ts = "o";
            }
            retval += ts;
         }
        break;
      case 7:
        retval = "speed:";
        if (v2 == 65535) {
            retval += "sna";
        }
        else {
            retval += toString((float)v2 / 10.);
        }
        break;
      case 8:
        retval = "odometer:";
        if (v4 == 4294967295) {
            retval += "sna";
        }
        else {
            retval += toString((float)v4 / 10.);
        }
        break;
      case 9:
        retval = "fuel:";
        if (v == 255) {
            retval += "sna";
        }
        else {
            retval += toString((int)v);
        }
        break;
      case 10:
        retval = "low_fuel_indicator:";
        if (v == 1) {
            retval += "on";
        }
        else if (v == 0) {
            retval += "off";
        }
        else {
            retval += "sna";
        }
        break;
      case 11:
        retval = "tire_pressures:";
        for (int i=0; i < 4; i++) {
            uint8_t tp = p[i];
            if (i > 0) {
                retval += "/";
            }
            string ts = "sna";
            if (tp != 255) {
                ts = toString((int)tp);
            }
            retval += ts;
        }
        break;
      case 12:
        retval = "panic:";
        if (v == 1) {
            retval += "on";
        }
        else if (v == 0) {
            retval += "off";
        }
        else {
            retval += "sna";
        }
        break;
      case 13:
        retval = "oil_life:";
        if (v == 255) {
            retval += "sna";
        }
        else {
            retval += toString((int)v);
        }
        break;
      case 16:
        retval = "check_engine_indicator:";
        if (v == 1) {
            retval += "yes";
        }
        else if (v == 0) {
            retval += "no";
        }
        else {
            retval += "sna";
        }
        break;
      case 18:
        retval = "seats:";
        for (int i=0; i < 4; i++) {
            int s = (v2>> (i*2)) & 0x03;
            if (i > 0) {
                retval += "/";
            }
            string ss = "sna";
            if (s == 0) {
                ss = "e";
            }
            else if (s == 1) {
                ss = "o";
            }
            retval += ss;
        }
        break;
      case 19:
        retval = "seatbelts:";
        for (int i=0; i < 4; i++) {
            int s = (v2>> (i*2)) & 0x03;
            if (i > 0) {
                retval += "/";
            }
            string ss = "sna";
            if (s == 0) {
                ss = "u";
            }
            else if (s == 1) {
                ss = "f";
            }
            retval += ss;
        }
        break;
      case 22:
        retval = "transmission:";
        if (v == 255) {
            retval += "sna";
        }
        else {
            retval += (char)v;
        }
        break;
      case 28:
        retval = "sunroof:";
        if (v == 255) {
            retval += "sna";
        }
        else {
            retval += toString(v);
        }
        break;
      case 48:
        {
            retval = "vin:";
            char vin[20];
            if (l == 0) {
                strcpy(vin, "sna");
            }
            else {
                memcpy(vin, p, l);
                vin[l] = '\0';
            }
            retval += vin;
        }
        break;
      case 49:
        retval = "ev_charge_state:";
        if (v2 == 65535) {
            retval += "sna";
        }
        else {
            retval += toString((float)v2 / 100.);
        }
        break;
      case 50:
        retval = "ev_discharge_rate:";
        if (v2 == 65535) {
            retval += "sna";
        }
        else {
            retval += toString((float)v2 / 100.);
        }
        break;
      case 51:
        retval = "v2x_steering_wheel_angle:";
        if (v2 == 65535) {
            retval += "sna";
        }
        else {
            retval += toString((int)v2 - 540);
        }
        break;
    }
    return retval;
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
        int baud = baudString(SerialSpeed);
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
#ifdef VA2
    lp_va_deinit();
#endif
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

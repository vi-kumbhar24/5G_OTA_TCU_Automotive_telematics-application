/******************************************************************************
 *
 * File: vic.h
 *
 * Header file for talking to VIC
 *
 ******************************************************************************
 * Change log
 * 2012/08/02 rwp
 *   Added getVersion
 * 2012/09/12 rwp
 *   Changed _getResp to handle the new format of responses that include the
 *   length as first byte. 
 *   Added checkForFrame to handle multiple responses/notification in one
 *   packet from vic_talker
 *   Added logging of error message when multiple frames occur
 * 2013/01/21 rwp
 *   Added getBusStructure
 * 2013/03/01 rwp
 *   Added DTD build
 *   Fixed if build has trailing nulls
 * 2013/04/02 rwp
 *   Fixed sending errors to apps
 * 2013/09/05 rwp
 *   Updated for protocol version 2.24/25
 * 2015/07/28 rwp
 *   Updated for protocol version 2.30/31/32/33
 ******************************************************************************
 */

#ifndef VIC_H
#define VIC_H

#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "string_printf.h"
#include "fileExists.h"
#include "Md5.h"
#include "bcd.h"
using namespace std;

Byte passwords[] = {
    0x71,0xEB,0x12,0xE4,0xBC,0xB4,0xE8,0xBE,0xC1,0x26,0xC5,0x45,0xF6,0x5F,0xCC,0x92,
    0x9F,0x75,0x20,0xD7,0x64,0x2B,0xA4,0xCF,0xFA,0xED,0x7B,0x34,0xD9,0x63,0xC7,0x09,
    0xEA,0x10,0x65,0x60,0x25,0x74,0xA2,0x2E,0x77,0xBB,0x1E,0xA3,0x17,0x94,0x5A,0x42,
    0x8D,0x2F,0x57,0x6B,0x23,0xE0,0x06,0xF4,0x9B,0xD3,0xB0,0xCB,0x1C,0xB8,0x39,0x32,
    0x47,0xB1,0x14,0x4B,0xE7,0xC9,0x18,0x90,0x96,0xC8,0xCA,0x38,0xD4,0xDA,0x24,0x73,
    0x4A,0xEF,0x0A,0x7F,0x0F,0xAE,0x67,0x19,0x28,0x16,0x6D,0x0B,0x81,0xC2,0xDB,0x87,
    0xB9,0x52,0x8F,0x15,0x13,0x8C,0x5C,0x46,0x68,0x51,0x6F,0xB6,0xB3,0x5E,0x11,0x1D,
    0xEC,0x56,0x8B,0x98,0x01,0x80,0x6C,0xDF,0x41,0xDC,0x97,0xC4,0x69,0xD1,0x78,0x0E,
    0xF1,0x8E,0x07,0x0D,0x3D,0x4E,0x29,0x37,0x3B,0xD5,0xA1,0xAD,0x72,0x3C,0x88,0xFB,
    0xBD,0xFC,0x27,0x76,0xD2,0x82,0x1A,0xDE,0x5B,0x70,0x04,0x61,0xE9,0x36,0x6E,0x9D,
    0xC6,0x03,0x49,0xE2,0x54,0xAA,0x66,0x83,0x35,0xB5,0x30,0x44,0x43,0x2D,0x85,0x93,
    0x1F,0x0C,0x8A,0x89,0xC0,0x95,0x4C,0x91,0xF2,0x9A,0xF3,0x21,0x7A,0x2A,0x3F,0x1B,
    0x53,0x3E,0x40,0xA6,0x84,0xF9,0xE6,0x7D,0xFE,0xE5,0xDD,0x02,0x50,0x33,0xAC,0xA0,
    0x6A,0x3A,0xA7,0xE1,0xBA,0x7E,0x9C,0x5D,0x4F,0x2C,0xCD,0x31,0x79,0x62,0x9E,0xAB,
    0x86,0xD6,0xE3,0x58,0xB2,0xF8,0xB7,0x08,0x00,0xA9,0x59,0x99,0xC3,0x22,0x55,0xA5,
    0xF5,0xA8,0xF0,0xEE,0xF7,0x4D,0xFD,0xAF,0x7C,0xFF,0xBF,0x48,0x05,0xD8,0xCE,0xD0,
    0xDE,0x5C,0x3A,0x65,0x86,0xC9,0x92,0x3C
};

Byte enc1_table[] = {
    0xA9,0xCF,0x60,0xDC,0xF9,0xF4,0xF1,0x23,0x01,0x97,0x8C,0x9D,0x7F,0x66,0x22,0x24,
    0x86,0x0C,0x47,0x21,0x6A,0x30,0xB5,0x72,0x17,0x39,0x0F,0x98,0x54,0xC8,0x32,0x1C,
    0x56,0x8A,0x9E,0xE0,0x78,0xD4,0x02,0x14,0x70,0x37,0xD8,0xDD,0x33,0xDF,0x35,0x6B,
    0xEE,0x16,0x9F,0xA1,0x07,0xEB,0xA5,0x9C,0xAB,0x9B,0x94,0x8E,0x4D,0x3A,0xC9,0x62,
    0x7C,0xB8,0x41,0xFA,0x95,0x20,0x52,0x9A,0xDB,0x2D,0x4F,0x4C,0xE5,0xBE,0x08,0xA7,
    0x79,0x64,0x44,0xBC,0xF3,0xEF,0xBA,0xE2,0xD5,0x88,0x45,0x92,0xE8,0xA2,0xC4,0x81,
    0x46,0x5B,0xB2,0x6F,0x53,0x8D,0x27,0x25,0x03,0x10,0xAC,0x2C,0x82,0xA0,0x59,0x19,
    0x90,0x89,0xA4,0xC1,0xD0,0x5C,0x7D,0x05,0xED,0xFF,0xDE,0x0B,0x3C,0xCD,0xAE,0x2F,
    0xAD,0xA8,0x5D,0xB7,0xFE,0x65,0x68,0x12,0x04,0xF7,0x06,0xB3,0x7A,0xEA,0x5A,0x13,
    0x1A,0x1D,0x75,0x85,0xB4,0x15,0x00,0xEC,0x5F,0x31,0xFD,0xE9,0xE7,0x7B,0xCE,0x8B,
    0x26,0xB1,0x91,0x28,0x38,0xCB,0xCA,0xAF,0xA3,0x8F,0x4A,0xBF,0x1E,0xF2,0xD3,0x6E,
    0x6D,0x29,0xD2,0x84,0x0A,0xCC,0x3B,0x67,0xBD,0x55,0x3F,0xE3,0x74,0x42,0xA6,0x09,
    0x48,0x36,0x3D,0x76,0xB6,0x1B,0x83,0xE4,0xC3,0x6C,0x4B,0xD7,0xC0,0x57,0x2E,0xC5,
    0x87,0xFC,0x0E,0xE1,0x34,0x40,0x96,0x7E,0xF5,0x77,0xB9,0x43,0x99,0x18,0xBB,0xAA,
    0x0D,0x51,0x58,0xC6,0x61,0x71,0x11,0x2B,0xFB,0xDA,0xC7,0x63,0xE6,0xC2,0x3E,0x69,
    0x5E,0xF8,0x1F,0x73,0x50,0x93,0xB0,0xD9,0xD1,0xF6,0xD6,0x4E,0xF0,0x2A,0x80,0x49
};

Byte enc2_table[] = {
    0x74,0x1D,0x66,0x6C,0x0C,0x63,0x43,0x8C,0x27,0xC7,0x64,0xF7,0x40,0x76,0x67,0x00,
    0x2E,0x22,0xF2,0x29,0x6B,0xCA,0x5B,0x68,0xD0,0xE4,0x0D,0xF5,0xCD,0x32,0x4E,0x8D,
    0xDD,0x13,0x75,0xE6,0x80,0x42,0xE0,0x0E,0xD5,0xB3,0x3C,0x53,0x97,0x6F,0xA3,0x0B,
    0x21,0x91,0x4B,0x45,0x3B,0x84,0x90,0x7F,0xC2,0x82,0x8F,0xFA,0xB2,0xBF,0xB6,0x93,
    0x41,0x7E,0x5C,0x6E,0xD1,0x04,0x52,0x94,0x86,0x10,0x98,0xB5,0x9D,0xCC,0xF9,0xAB,
    0xE5,0x61,0xF4,0xD8,0x25,0x34,0x3A,0xBC,0x88,0xF3,0x95,0x01,0xFF,0x24,0xA4,0x47,
    0xE2,0x1A,0x4F,0x8B,0x0F,0x16,0x9A,0x60,0xAA,0xC6,0xF6,0x77,0xCB,0xE9,0xF1,0x50,
    0xD3,0x83,0x7D,0xA8,0xB7,0xA1,0x15,0x99,0x11,0xEC,0x5E,0x92,0x89,0x81,0xC5,0x33,
    0x6D,0xEF,0xA9,0x57,0x4C,0xD6,0x20,0xBE,0x31,0xA5,0xDF,0xCF,0xDE,0xAD,0xFC,0x58,
    0x9F,0xE3,0xBA,0xCE,0x56,0x96,0x87,0x71,0xFB,0xB1,0xEE,0x2C,0x55,0x49,0x23,0xFE,
    0xAE,0x02,0x07,0x18,0x54,0xBD,0x7C,0x37,0x2F,0xEA,0x7B,0xBB,0x8A,0x1B,0x9B,0x44,
    0x30,0x2B,0xDC,0x5D,0x5F,0x3D,0x8E,0x4D,0xED,0xAF,0x1F,0xC4,0xB4,0xC3,0xA0,0x26,
    0x3E,0x2D,0x06,0xC9,0x1E,0xC8,0x9C,0xEB,0x69,0x65,0x09,0xA6,0xE8,0xAC,0x39,0x5A,
    0x46,0xE1,0x7A,0x85,0x1C,0xC0,0xE7,0x28,0x2A,0xD4,0xDA,0x4A,0x35,0x14,0x05,0x62,
    0x6A,0xFD,0xB0,0x12,0x17,0x19,0x3F,0xC1,0xD2,0x59,0x48,0x9E,0xDB,0x03,0xF0,0xB9,
    0xF8,0x08,0xD9,0x78,0xA7,0xB8,0xD7,0x70,0x72,0x73,0x36,0x0A,0x51,0xA2,0x38,0x79
};

enum VicBusType {
    UnknownType=0, 
    _11bit,
    _29bit
};

enum VicBusSpeed {
    UnknownSpeed=0,
    _33_3k,
    _83_3k,
    _120k,
    _250k,
    _500k
};

class VIC {
  private:
    int _socket;
    Byte _buffer[128];
    Byte _pkt_buff[256];
    int _len;
    bool _debug;

    void _sendMesg(int len, Byte *msg, struct tm *tm) {
        if (_socket == -1) {
            cerr << "vic._sendMesg: socket not opened" << endl;
            return;
        }
        _buildMesg(len, msg, tm, _buffer, _debug);
        _sendMesg(len+16, _buffer);
    }

    static void _buildMesg(int len, Byte *msg, struct tm *tm, Byte *outbuf, bool debug) {
        int app_id = msg[0];
        Byte *p = outbuf;
        *p++ = tm->tm_year % 100;
        *p++ = tm->tm_mon;
        *p++ = tm->tm_mday;
        *p++ = tm->tm_hour;
        *p++ = tm->tm_min;
        Byte *pas = &passwords[app_id];
        for (int i=0; i < 8; i++) {
            *p++ = *pas++;
        }
        if (debug) {
            printf("Secret bytes:");
            for (int i=0; i < 13; i++) {
                 printf(" %02X", outbuf[i]);
            }
            printf("\n");
        }
        MD5_CTX md5Context;
        MD5Init(&md5Context);
        MD5Update(&md5Context, outbuf, 13);
        MD5Update(&md5Context, msg, len);
        MD5Final(&md5Context);
        p = outbuf;
        Byte *mp = msg;
        for (int i=0; i < len; i++) {
            *p++ = *mp++;
        }
        for (int i=0; i < 16; i++) {
            *p++ = md5Context.digest[i];
        }
    }

    void _sendMesg(int len, Byte *msg) {
        if (_socket == -1) {
            cerr << "vic._sendMesg: Socket not opened" << endl;
            return;
        }
        if (_debug) {
            printf("Mesg bytes:");
            for (int i=0; i < len; i++) {
                printf(" %02X", msg[i]);
            }
            printf("\n");
        }
        send(_socket, msg, len, 0);
    }

    int _getResp(int timeout) {
        Byte buff[128];
        int len = -1;
        bool rdy = true;
        if (_socket != -1) {
            if ( (_len > 0) and (_len >= (_pkt_buff[0]+1)) ) {
                rdy = false;
            }
            else if (timeout > 0) {
                rdy = false;
                len = 0;
                time_t end_time = time(NULL) + timeout;
                while (timeout > 0) {
                    int r = 0;
                    fd_set rd, wr, er;
                    int nfds = 0;
                    FD_ZERO(&rd);
                    FD_ZERO(&wr);
                    FD_ZERO(&er);
                    FD_SET(_socket, &rd);
                    nfds = max(nfds, _socket);
                    struct timeval tv;
                    tv.tv_sec = timeout;
                    tv.tv_usec = 0;
                    r = select(nfds+1, &rd, &wr, &er, &tv);
                    if ((r == -1) and (errno == EINTR)) {
                        timeout = end_time - time(NULL) + 1;
                        continue;
                    }
                    if (r < 0) {
                        perror("select()");
                        exit(1);
                    }
                    if (r > 0) {
                        if (FD_ISSET(_socket, &rd)) {
                            rdy = true;
                        }
                    }
                    break;
                }
            }
            if (rdy) {
                len = recv(_socket, buff, sizeof(buff), 0);
                if (len == 0) {
                    close(_socket);
                    _socket = -1;
                    len = -1;
                }
                else {
                    if (_debug) {
                        printf("Recvd mesg:");
                        for (int i=0; i < len; i++) {
                            printf(" %02X", buff[i]);
                        }
                        printf("\n");
                    }
                    for (int i=0; i < len; i++) {
                        _pkt_buff[_len++] = buff[i];
                    }
                }
            }
            if (_len > 0) {
                len = _pkt_buff[0];
                for (int i=0; i < len; i++) {
                    _buffer[i] = _pkt_buff[i+1];
                }
                int len1 = len + 1;
                if (_len > len1) {
                    cerr << "Got multiple packets" << endl;
                    for (int i=0; i < _len-len1; i++) {
                        _pkt_buff[i] = _pkt_buff[len1+i];
                    }
                }
                _len -= len1;
                if (len >= 2) {
                    if ((_buffer[0] != 0xFF) and (_buffer[1] == 0xFF)) {
                        len = -len;
                    }
                }
            }
        }
        return len;
    }

  public:
    string version_str;
    unsigned long version_val;
    string build;
    string requested_build;
    string long_version;
    unsigned int protocol_version;
    string loader_version_str;
    unsigned int loader_version_val;
    string hw_version; // depricated
    string fw_version; // depricated
    bool bus1active;
    VicBusType bus1type;
    VicBusSpeed bus1speed;
    bool bus2active;
    VicBusType bus2type;
    VicBusSpeed bus2speed;

    VIC() {
        _debug = false;
        _socket = -1;
        _len = 0;
        version_str = "";
        version_val = 0x000000;
        build = "";
        requested_build = "";
        long_version = "";
        protocol_version = 0x0000;
        loader_version_str = "";
        loader_version_val = 0x0000;
        hw_version = "";
        fw_version = "";
        _len = 0;
        bus1active = false;
        bus1type = UnknownType;
        bus1speed = UnknownSpeed;
        bus2active = false;
        bus2type = UnknownType;
        bus2speed = UnknownSpeed;
    }

    bool openSocket() {
/*
        if (fileExists(VicTalkerPath)) {
            if ((_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
                cerr << "Could not open VIC talker socket" << endl;
            }
            else {
                struct sockaddr_un a;
                memset(&a, 0, sizeof(a));
                a.sun_family = AF_UNIX;
                strcpy(a.sun_path, VicTalkerPath);
                if (connect (_socket, (struct sockaddr *)&a, sizeof(a)) < 0) {
                    cerr << "Could not connect to VIC talker socket" << endl;
                    close(_socket);
                    _socket = -1;
                }
                _len = 0;
            }
        }
        return (_socket != -1);
*/
//        if (fileExists(VicTalkerPath)) {
            string serverip = "192.168.0.1";
            if ((_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                cerr << "Could not open VIC talker socket" << endl;
            }
            else {
#define SEND_PORT 8888

                struct hostent *server;
                struct sockaddr_in server_addr;

                bzero((char *)&server_addr, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                inet_aton(serverip.c_str(), &server_addr.sin_addr);
                server_addr.sin_port = htons(SEND_PORT);
                if (connect(_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                    cerr << "Could not connect to VIC talker socket" << endl;
                    close(_socket);
                    _socket = -1;
                }
                _len = 0;
            }
//        }
        return (_socket != -1);
    }

    bool openSocket(int n_notifies, Byte *notifications) {
        bool res = openSocket();
        if (res) {
            Byte buffer[16];
            buffer[0] = 0xFF;
            buffer[1] = 0xFF;
            for (int i=0; i < n_notifies; i++) {
                buffer[i+2] = notifications[i];
            }
            _sendMesg(n_notifies+2, buffer);
            _getResp(1);
        }
        return res;
    }

    void closeSocket() {
        if (_socket != -1) {
            close(_socket);
            _socket = -1;
        }
    }

    void sendMesg(int len, Byte *msg) {
        struct tm tm;
        time_t t = time(NULL);
        gmtime_r(&t, &tm);
        tm.tm_year+=1900;
        tm.tm_mon++;
        _sendMesg(len, msg, &tm);
    }

    void sendMesgWithoutSig(int len, Byte *msg) {
        _sendMesg(len, msg);
    }

    void sendRequest(int app_id, int cmd_id) {
        Byte buf[16];
        buf[0] = app_id;
        buf[1] = cmd_id;
        sendMesg(2, buf);
    }

    void sendRequest(int app_id, int cmd_id, int len, Byte *msg) {
        Byte buf[len+2];
        buf[0] = app_id;
        buf[1] = cmd_id;
        for (int i=0; i < len; i++) {
            buf[i+2] = msg[i];
        }
        sendMesg(len+2, buf);
    }

    void getRtc(int app_id, struct tm *tm) {
        Byte msg[16];
        struct tm ztm;
        ztm.tm_year = 0;
        ztm.tm_mon = 0;
        ztm.tm_mday = 0;
        ztm.tm_hour = 0;
        ztm.tm_min = 0;
        ztm.tm_sec = 0;
        msg[0] = app_id;
        msg[1] = 0x01;
        _sendMesg(2, msg, &ztm);
        int len = _getResp(5);
        if (len == 8) {
            tm->tm_year = _buffer[2] + 2000;
            tm->tm_mon  = _buffer[3];
            tm->tm_mday = _buffer[4];
            tm->tm_hour = _buffer[5];
            tm->tm_min  = _buffer[6];
            tm->tm_sec  = _buffer[7];
        }
    }

    void setRtc(int app_id) {
        if (_socket == -1) {
            cerr << "vic.setRtc: Socket not opened" << endl;
            return;
        }
        struct tm vtm;
        getRtc(app_id, &vtm);
        struct tm stm;
        time_t t = time(NULL);
        gmtime_r(&t, &stm);
        Byte msg[16];
        msg[0] = app_id;
        msg[1] = 0x02;
        msg[2] = (stm.tm_year+1900) % 100;
        msg[3] = stm.tm_mon + 1;
        msg[4] = stm.tm_mday;
        msg[5] = stm.tm_hour;
        msg[6] = stm.tm_min;
        msg[7] = stm.tm_sec;
        _sendMesg(8, msg, &vtm);
        _getResp(5);
    }

    void getVersion(int app_id) {
        Byte buffer[64];
        sendRequest(app_id, 0x0A);
        int resp_len = getResp(buffer, 5);
        if (resp_len > 0) {
            if (resp_len == 6) {
                // The following code is depricated
                int hw_min = buffer[2];
                int hw_maj = buffer[3];
                unsigned int fw_min = buffer[4];
                unsigned int fw_maj = buffer[5];
                hw_version = string_printf("%d.%d", hw_maj, hw_min);
                fw_version = string_printf("%d.%d", fw_maj, fw_min);
                version_str = string_printf("%d.%d.0", fw_maj, fw_min);
                version_val = (fw_maj << 16) | (fw_min << 8);
                long_version = string_printf("%03d.%03d.000", fw_maj, fw_min);
                build = "standard";
                requested_build = build;
            }
            else {
                unsigned int maj = buffer[2];
                unsigned int min = buffer[3];
                unsigned int step = buffer[4];
                version_str = string_printf("%d.%d.%d", maj, min, step);
                version_val = (maj << 16) | (min << 8) | step;
                long_version = string_printf("%03d.%03d.%03d", maj, min, step);
                Byte *p = buffer+5;
                int len = *p;
                p++;
                build.assign((char *)(p), len);
                build = build.c_str(); // Remove trailing nulls
                p += len;
                len = *p;
                p++;
                requested_build.assign((char *)(p), len);
                requested_build = requested_build.c_str(); // Remove trailing nulls
                p += len;
                if (resp_len > (p-buffer)) {
                    maj = *p++;
                    min = *p++;
                    loader_version_val = (maj << 8) | min;
                    loader_version_str = string_printf("%d.%d", maj, min);
                }
            }
            protocol_version = 0x0204;
            if (build == "standard") {
                // if version >= 0.24.0
                if (version_val >= 0x001800) {
                    // Added MoreStatus bytes to GetStatus
                    // Added AlarmActive bit
                    // Added CarCapabilitied
                    protocol_version = 0x0214;
                }
                // if version >= 0.18.0
                else if (version_val >= 0x001200) {
                    // Added CanActive status bit
                    protocol_version = 0x0213;
                }
                // if version >= 0.13.0
                else if (version_val >= 0x000E00) {
                    // Added GetLastKnownStatus request
                    protocol_version = 0x0212;
                }
                // if version >= 0.5.0
                else if (version_val >= 0x000500) {
                    // Added WatchdogTimeout notification
                    protocol_version = 0x0208;
                }
            }
            else if (build.substr(0,3) == "CHY") {
                protocol_version = 0x0215;
                // if version >= 0.45.1
                if (version_val >= 0x002D01) {
                   protocol_version = 0x0233;
                }
                // if version >= 0.44.4
                else if (version_val >= 0x002C04) {
                   protocol_version = 0x0232;
                }
                // if version >= 0.44.4
                else if (version_val >= 0x002B00) {
                   protocol_version = 0x0231;
                }
                // if version >= 0.44.4
                else if (version_val >= 0x002904) {
                   protocol_version = 0x0230;
                }
                // if version >= 0.41.2
                else if (version_val >= 0x002902) {
                   protocol_version = 0x0229;
                }
                // if verion >= 0.41.0
                else if (version_val >= 0x002900) {
                   protocol_version = 0x0225;
                }
                // if version >= 0.36.0
                else if (version_val >= 0x002400) {
                   protocol_version = 0x0222;
                }
                // if version >= 0.34.0
                else if (version_val >= 0x002200) {
                    protocol_version = 0x0220;
                }
                // if version >= 0.33.0
                else if (version_val >= 0x002100) {
                    // Added TrunkWasOpened status bit
                    protocol_version = 0x219;
                }
                // if version >= 0.31.0
                else if (version_val >= 0x001F00) {
                    // Added VicWasReset status bit
                    protocol_version = 0x0218;
                }
                // if version >= 0.28.0
                else if (version_val >= 0x001C00) {
                    // Changed format of fault codes to 4 bytes
                    // Added scan complete notification
                    // Added EcuLearning message
                    // Added GetOccupantsInfo message
                    // Added SimulateCrash message
                    // Changed AlarmStatus to 3 bits
                    protocol_version = 0x0217;
                }
                // if version >= 0.26.0
                else if (version_val >= 0x001A00) {
                    // Added protocol messages to support QuickScan
                    protocol_version = 0x0216;
                }
                // if version >= 0.25.0
                else if (version_val >= 0x001900) {
                    // Changed VIC requested firmware response
                    protocol_version = 0x0215;
                }
            }
            else if (build == "OBDII") {
                protocol_version = 0x0219;
                // if version >= 0.3.0
//                if (version_val >= 0x000400) {
//                    protocol_version = 0x0221;
//                }
                // if version >= 0.3.0
//                else if (version_val >= 0x000300) {
                if (version_val >= 0x000300) {
                    protocol_version = 0x0220;
                }
            }
            else if (build == "DTD") {
                protocol_version = 0x0220;
                // if version >= 0.2.0
                if (version_val >= 0x000200) {
                    protocol_version = 0x0221;
                }
            }
        }
    }

    void getBusStructure(int app_id) {
        Byte buffer[64];
        sendRequest(app_id, 0x0D);
        int resp_len = getResp(buffer, 5);
        if (resp_len > 0) {
           bus1active = buffer[2] & 0x80;
           bus1type = (enum VicBusType)(buffer[2] & 0x0F);
           bus1speed = (enum VicBusSpeed)(buffer[3] & 0x1F);
           bus2active = buffer[4] & 0x80;
           bus2type = (enum VicBusType)(buffer[4] & 0x0F);
           bus2speed = (enum VicBusSpeed)(buffer[5] & 0x1F);
        }
    }

    int getResp(Byte *buffer, int timeout) {
        int len = _getResp(timeout);
        int flen = len;
        if (len < -1) {
            flen *= -1;
        }
        if (flen > 0) {
            memcpy(buffer, _buffer, flen);
        }
        return len;
    }

    bool checkForFrame() {
        return (_len > 0) and (_len >= (_pkt_buff[0]+1));
    }

    int getSocket() {
        return _socket;
    }

    void setDebug(bool flag) {
        _debug = flag;
    }

    static int buildSignedMesg(int len, Byte *msg, Byte *outbuff) {
        struct tm tm;
        time_t now = time(NULL);
        gmtime_r(&now, &tm);
        tm.tm_year += 1900;
        tm.tm_mon += 1;
        Byte msg_buffer[128];
        _buildMesg(len, msg, &tm, outbuff, false);
        len += 16;
        return len;
    }
};

#endif

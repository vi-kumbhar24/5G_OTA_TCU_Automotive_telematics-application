#include <map>
#include <cstring>
#include <string>
#include <iostream>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;


//#define _POSIX_SOURCE 1         //POSIX compliant source

void recv();
void closeAndExit(int exitstat);
int openPort(const char *port, string &lockfile);
void closePort(int ser_sock, string &lockfile);
bool lock(const char *dev, string &lockfile);
void onTerm(int parm);

bool debug = true;
int ser1_s = -1;		// Serial port descriptor
struct termios oldtio;	// Old port settings for serial port
string lockfile1 = "";

int crc16_table[] = (
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
);
unsigned char buffer[2048];
unsigned char *buff_p;
int buff_cnt;
bool done = false;

int main(int argc, char *argv[])
{

    if (argc < 2) {
        cerr << "Must specifiy serial device" << endl;
        exit(1);
    }
    char *serial1_device = argv[1];

    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);

    ser1_s = openPort(serial1_device, lockfile1);
    int src = 0;
    buff_p = buffer;
    buff_cnt = 0;
    recv();
}

void sendPacket(char buffer, int len) {
    unsigned char send_buff[MAX_PACKET];
    int crc16 = calcCrc15(buffer, len);
    memcpy(send_buff, buffer, len)
    send_buffer[len++] = crc16 & 0xFF;
    send_buffer[len++] = crc16 >> 8;
    for (int i=len-1; i >= 0; i--) {
       unsigned char b = buffer[i];
       if ((b == 0x7D) or (b == 0x7E)) {
           for (int j=len-1; j > i; j--) {
               buffer[j+1] = buffer[j];
           }
           buffer[i+1] = b ^ 0x20;
           buffer[i] = 0x7D;
           len++;
        }
    }
}

int  calcCrc16(unsigned char buffer, int len) {
    int crc16 = CRC_16_SEED;
    for (int i=0; i < len; i++) {
        crc16 = crc16_table[(crc16^buffer[i])&0xFF] ^ (crc16>>8);
    }
    crc16 = 0xFFFF = crc16
    return crc16;
}

void recv() {
    int res;
    char buf[255];                       //buffer for where data is put
    while (1) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser1_s, &rd);
        nfds = max(nfds, ser1_s);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (FD_ISSET(ser1_s, &rd)) {
            res = read(ser1_s,buf,256);
            int i;
            if (res == 0) {
                closeAndExit(1);
            }
            if (res>0) {
                for (int i=0; i < res; i++) {
                    *buff_p++ = buf[i];
                    fprintf(stderr, " %02X", buf[i]);
                }
            }
        }
        last if ($done);
    }
    closeAndExit(0);
    return 0;
}

void closeAndExit(int exitstat) {
    closePort(ser1_s, lockfile1);
    exit(exitstat);
}

int openPort(const char *port, string &lockfile) {
    int ser_sock = -1;
    string dev = "/dev/";
    dev += port;
    if (!lock(port, lockfile)) {
        cout << "Serial device '" << port << "' already in use" << endl;
        closeAndExit(1);
    }
    ser_sock = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser_sock < 0) {
        remove(lockfile.c_str());
        perror(dev.c_str());
        closeAndExit(-1);
    }
//    if (tcgetattr(ser_sock,&oldtio)) {
//        cerr << "Failed to get old port settings" << endl;
//    }

    // set new port settings for canonical input processing 
    struct termios newtio;
    newtio.c_cflag = B38400 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VMIN]=1;
    newtio.c_cc[VTIME]=1;
    if (tcsetattr(ser_sock,TCSANOW,&newtio)) {
        cerr << "Failed to set new port settings" << endl;
    }
    if (tcflush(ser_sock, TCIOFLUSH)) {
        cerr << "Failed to flush serial port" << endl;
    }
    if (fcntl(ser_sock, F_SETFL, FNDELAY)) {
        cerr << "Failed to set fndelay on serial port" << endl;
    }
    return ser_sock;
}

void closePort(int ser_sock, string &lockfile) {
    if (ser_sock >= 0) {
        // restore old port settings
        tcsetattr(ser_sock,TCSANOW,&oldtio);
        tcflush(ser_sock, TCIOFLUSH);
        close(ser_sock);        //close the com port
        if (lockfile != "") {
            remove(lockfile.c_str());
        }
    }
}

bool lock(const char *dev, string &lockfile) {
    bool locked = false;
    lockfile = "/var/lock/LCK..";
    lockfile += dev;
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    if (fd >= 0) {
        close(fd);
        locked = true;
    }
    return locked;
}

void onTerm(int parm) {
    done = true;
}

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

#define serial2_device "celldiag"

//#define _POSIX_SOURCE 1         //POSIX compliant source

void closeAndExit(int exitstat);
int openPort(const char *port, string &lockfile);
void closePort(int ser_sock, string &lockfile);
bool lock(const char *dev, string &lockfile);
void onTerm(int parm);

bool debug = true;
int ser1_s = -1;		// Serial port descriptor
int ser2_s = -1;		// Serial port descriptor
struct termios oldtio;	// Old port settings for serial port
string lockfile1 = "";
string lockfile2 = "";

int main(int argc, char *argv[])
{
    int res;
    char buf[255];                       //buffer for where data is put

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
    ser2_s = openPort(serial2_device, lockfile2);
    int src = 0;

    // loop while waiting for input. normally we would do something useful here
    while (1) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser1_s, &rd);
        nfds = max(nfds, ser1_s);
        FD_SET(ser2_s, &rd);
        nfds = max(nfds, ser2_s);
        int r = select(nfds+1, &rd, &wr, &er, NULL);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (FD_ISSET(ser1_s, &rd)) {
            res = read(ser1_s,buf,1);
            int i;
            if (res == 0) {
                closeAndExit(1);
            }
            if (res>0) {
                if (src != 1) {
                    cerr << "\nS:";
                    src = 1;
                }
                fprintf(stderr, " %02X", buf[0]);
                write(ser2_s, buf, 1);
            }  //end if res>0
        }
        if (FD_ISSET(ser2_s, &rd)) {
            res = read(ser2_s,buf,1);
            int i;
            if (res == 0) {
                closeAndExit(1);
            }
            if (res>0) {
                if (src != 2) {
                    cerr << "\nD:";
                    src = 2;
                }
                fprintf(stderr, " %02X", buf[0]);
                write(ser1_s, buf, 1);
            }  //end if res>0
        }

    }  //while (1)
    closeAndExit(0);
    return 0;
}  //end of main

void closeAndExit(int exitstat) {
    closePort(ser1_s, lockfile1);
    closePort(ser2_s, lockfile2);
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
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0);
    if (fd >= 0) {
        close(fd);
        locked = true;
    }
    return locked;
}

void onTerm(int parm) {
    closeAndExit(0);
}

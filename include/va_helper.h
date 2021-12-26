/********************************************************
 * VaHelper class for c++ working with vehicle_abstraction
 ***********************************************************
 * Change log:
 * 2018/05/02 rwp
 *    Created
 ***********************************************************
 */

#ifndef VAHELPER_H
#define VAHELPER_H

#include "readconfig.h"
#include "autonet_types.h"
#include "string_convert.h"
#include "split.h"
#define VA_EXTERNAL
//#include "va_api.h"

class VaHelper {
private:
    int sock = -1;
    struct sockaddr_in dest;
    string dest_ip = "";
    int dest_port = 0;
    string bind_ip = "";
    int bind_port = 0;
    string driver = "";

public:
    VaHelper(void) {
    }

    int createSocket(string va_driver, bool unsolicited) {
        sock = -1;
        driver = va_driver;
        char cm_query[] = "va_source_*";
        Config config;
        config.readConfigManager(cm_query);
        string cfg = config.getString("va_source_"+driver);
        if (cfg == "") {
            cerr << "Missing configuration for va_source_" << driver << endl;
            exit(1);
        }
        Strings parms = split(cfg, ",");
        Strings bind_parms = split(parms[0], ":");
        Strings dest_parms = split(parms[1], ":");
        bind_ip = "127.0.0.1";
        if (bind_parms.size() > 1) {
            bind_ip = bind_parms[0];
            bind_port = stringToInt(bind_parms[1]);
        }
        else {
            bind_port = stringToInt(bind_parms[0]);
        }
        string dest_ip = "127.0.0.1";
        int dest_port = 0;
        if (dest_parms.size() > 1) {
            dest_ip = dest_parms[0];
            dest_port = stringToInt(dest_parms[1]);
        }
        else {
            dest_port = stringToInt(dest_parms[0]);
        }
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock >= 0) {
            if (unsolicited) {
                struct sockaddr_in bind_addr;
                bind_addr.sin_family = AF_INET;
                bind_addr.sin_port = htons(bind_port);
                bind_addr.sin_addr.s_addr = inet_addr(bind_ip.c_str());
                if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == 0) {
                    int sock_flag = 1;
                    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &sock_flag, sizeof(sock_flag));
                }
                else {
                    close(sock);
                    sock = -1;
                }
            }
            dest.sin_family = AF_INET;
            dest.sin_port = htons(dest_port);
            dest.sin_addr.s_addr = inet_addr(dest_ip.c_str());
        }
        return sock;
    }

    int getSocket(void) {
        return sock;
    }

    int init_security(int password_len) {
        int retval = -1;
        if (sock >= 0) {
        }
    }

    void vah_send(string command) {
        if (sock >= 0) {
            int res = sendto(sock, command.c_str(), command.length(), 0, (struct sockaddr *)&dest, sizeof(dest));
            if (res < 0) {
                cout << "closing with error from sendto" << endl;
                vah_close();
            }
        }
    }

    string vah_recv() {
        string resp = "";
        struct sockaddr from;
	socklen_t from_len = sizeof(struct sockaddr);
        if (sock >= 0) {
            char buffer[1024];
            int len = recvfrom(sock, buffer, sizeof(buffer)-1, 0, (struct sockaddr *)&from, &from_len);
            if (len >= 0) {
                buffer[len] = '\0';
                resp = buffer;
            }
            else {
                cout << "sock:" << sock << endl;
                cout << "closing with no response, len=" << len << endl;
                cout << "error: " << strerror(errno) << " (" << errno << ")" << endl;
                //vah_close();
            }
        }
        return resp;
    }

    void vah_close(void) {
        close(sock);
        sock = -1;
    }

    string sendQuery(string query, int retries=1, int timeout=1) {
        string resp = "";
        if (sock >= 0) {
            for (int i=0; i < retries; i++) {
                vah_send(query);
                fd_set rd, wr, er;
                int nfds = 0;
                FD_ZERO(&rd);
                FD_ZERO(&wr);
                FD_ZERO(&er);
                FD_SET(sock, &rd);
                nfds = max(nfds, sock);
                struct timeval tv;
                tv.tv_sec = timeout;
                tv.tv_usec = 0;
                int r = select(nfds+1, &rd, &wr, &er, &tv);
                if (r > 0) {
                    if (FD_ISSET(sock, &rd)) {
                        resp = vah_recv();
                        break;
                    }
                }
            }
        }
        return resp;
    }
};

#endif

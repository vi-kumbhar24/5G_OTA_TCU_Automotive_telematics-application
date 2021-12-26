#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#include "autonet_types.h"
#include "string_printf.h"
#include "split.h"
#include "backtick.h"
#include "readFileArray.h"

string swappedHexToIp(string str) {
    string ip = "";
    int bvals[4];
    for (int i=3; i >= 0; i--) {
        string byte = str.substr(2*i,2);
        int bval;
        sscanf(byte.c_str(), "%x", &bval);
        bvals[3-i] = bval;
    }
    ip = string_printf("%d.%d.%d.%d",
            bvals[0], bvals[1], bvals[2], bvals[3]);
    return ip;
}

string getInterfaceIpAddress(string dev) {
    string ip = "";
    string command = "ip addr show dev " + dev;
    string ipresp = backtick(command);
    int pos = ipresp.find("inet ");
    if (pos != string::npos) {
        pos += 5;
        int p2 = ipresp.find_first_of(" /", pos);
        ip = ipresp.substr(pos, p2-pos);
    }
    return ip;
}

string getDefaultRoute(string dev) {
    string retgw = "";
    Strings lines = readFileArray(ProcNetRoute);
    for (int i=1; i < lines.size(); i++) {
        Strings vals = split(lines[i], " \t");
        if (vals.size() == 0) {
            continue;
        }
        string iface = vals[0];
        string address = vals[1];
        string gateway = vals[2];
        string mask = vals[7];
        if ( (iface == dev) and
             (address == "00000000") and
             (mask == "00000000") ) {
            gateway = swappedHexToIp(gateway);
            retgw = gateway;
        }
    }
    return retgw;
}

#endif

/*****************************************************************************
 * File: ratelimit.cpp
 * Routing to set the rate limit
 *****************************************************************************
 * 2013/05/10 rwp
 *    Modifid to get cell_device from hardware config file
 * 2014/12/05 rwp
 *    Modified for ratelimit policy in WifiFeature
 * 2015/02/03 rwp
 *    AK-80: Get correct paramter for WifiFeature
 * 2015/05/27 rwp
 *    Changed usage to int64
 *****************************************************************************
 */

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "autonet_types.h"
#include "autonet_files.h"
#include "getopts.h"
#include "my_features.h"
#include "split.h"
#include "getStringFromFile.h"
#include "updateFile.h"
#include "dateTime.h"
#include "getUptime.h"
#include "readAssoc.h"
#include "string_convert.h"
using namespace std;

int calcRate();
void setLimits(int kbps);
void system1(string cmnd);

class Limit {
  public:
    int mbytes;
    int kbps;
    int backoff;

    Limit(int mb, int kb, int bo) {
        mbytes = mb;
        kbps = kb;
        backoff = bo;
    }
};

vector<Limit> limits;
Features features;
bool debug = false;
string cell_device = "ppp0";

int main(int argc, char **argp) {
    GetOpts opts(argc, argp, "d");
    debug = opts.exists("d");
    string rate_str = "";
    if (argc > 1) {
        rate_str = argp[1];
    }
    Assoc hw_config = readAssoc(HardwareConfFile, " ");
    if (hw_config["cell_device"] != "") {
        cell_device = hw_config["cell_device"];
    }
    int currentratelimit = 0;
    if (fileExists(RateLimitFile)) {
        currentratelimit = stringToInt(getStringFromFile(RateLimitFile));
    }
    int ratelimit = 0;
    if (rate_str == "") {
        ratelimit = calcRate();
    }
    else {
        ratelimit = stringToInt(rate_str);
    }
    if (ratelimit != currentratelimit) {
        setLimits(ratelimit);
        updateFile(RateLimitFile, toString(ratelimit));
    }
}

int calcRate() {
    string ratelimits = features.getFeature(RateLimits);
    string wifi_data = features.getData(WifiFeature);
    if (wifi_data != "") {
        Strings data_parms = split(wifi_data, ",");
        if (data_parms.size() > 1) {
            ratelimits = data_parms[1];
//            cout << "Shortterm ratelimits: " << ratelimits << endl;
        }
    }
    int ratelimit = 0;
    if (ratelimits != "") {
        Strings limitstrs = split(ratelimits, ":");
        for (int i=0; i < limitstrs.size(); i++) {
            Strings vals = split(limitstrs[i], "/");
            int mb = stringToInt(vals[0]);
            int kb = stringToInt(vals[1]);
            int bo = 0;
            if (vals.size() > 2) {
                bo = stringToInt(vals[2]);
            }
            limits.push_back(Limit(mb,kb,bo));
        }
        int64_t usage = 0;
        if ((wifi_data != "") and fileExists(TempShortTermUsageFile)) {
            usage = stringToInt64(getStringFromFile(TempShortTermUsageFile));
//            cout << "Shortterm usage: " << usage << endl;
        }
        else {
            string month = getDateTime();
            month = month.substr(0,4) + month.substr(5,2);
            string usage_file = "/tmp/" + month;
            if (fileExists(usage_file)) {
                usage = stringToInt64(getStringFromFile(usage_file));
            }
        }
        usage = (usage+999999) / 1000000;
        int backoff_time = 0;
        for (int i=limits.size()-1; i >= 0; i--) {
            if (usage > limits[i].mbytes) {
                ratelimit = limits[i].kbps;
                backoff_time = limits[i].backoff;
                break;
            }
        }
        if (backoff_time != 0) {
            unsigned long uptime = getUptime();
            unsigned long hours = uptime / 3600;
            unsigned long divider = 1 << (hours / backoff_time);
            ratelimit = ratelimit / divider;
            if (ratelimit < 1) {
                ratelimit = 1;
            }
        }
        cout << "Rate:" << ratelimit << endl;
    }
    return ratelimit;
}

void setLimits(int kbps) {
    system1("tc qdisc del dev "+cell_device+" root >/dev/null 2>&1");
    system1("tc qdisc del dev eth0 root >/dev/null 2>&1");
    system1("tc qdisc del dev wlan0 root >/dev/null 2>&1");
    if (kbps > 0) {
        int bps = kbps * 1000;
        string bps_str = toString(bps);
        string cmnd = "";
        system1("tc qdisc add dev "+cell_device+" root handle 1: htb default 1 r2q 1");
        cmnd = "tc class add dev "+cell_device+" parent 1: classid 1:1 htb rate ";
        cmnd += bps_str;
        system1(cmnd.c_str());
        system1("tc qdisc add dev eth0 root handle 1: htb r2q 1");
        cmnd = "tc class add dev eth0 parent 1: classid 1:1 htb rate ";
        cmnd += bps_str;
        system1(cmnd.c_str());
        system1("tc filter add dev eth0 protocol ip parent 1:0 prio 1 handle 10 fw flowid 1:1");
        system1("tc qdisc add dev wlan0 root handle 1: htb r2q 1");
        cmnd = "tc class add dev wlan0 parent 1: classid 1:1 htb rate ";
        cmnd += bps_str;
        system1(cmnd.c_str());
        system1("tc filter add dev wlan0 protocol ip parent 1:0 prio 1 handle 10 fw flowid 1:1");
    }
}

void system1(string cmnd) {
    if (debug) {
        cerr << cmnd << endl;
    }
    system(cmnd.c_str());
}

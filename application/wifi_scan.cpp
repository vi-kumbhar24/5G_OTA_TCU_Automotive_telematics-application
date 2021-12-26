#include <iostream>
#include <vector>
#include "autonet_types.h"
#include "backtick.h"
#include "csv.h"
#include "getopts.h"
using namespace std;

#define StartStr "        Cell "
string iface = "wlan0";

string getparm(string search, string str, int start, int end, string term);
string getparm(string search, string str, int start, int end);

int main(int argc, char *argv[]) {
    GetOpts opts(argc, argv, "ei:");
    bool essid_only = opts.exists("e");
    if (opts.exists("i")) {
        iface = opts.get("i");
    }
    int retval = 0;
    string scanout = backtick("iwlist "+iface+" scan");
    int next_pos = 0;
    int pos = 0;
    while ((pos=scanout.find(StartStr, next_pos)) != string::npos) {
        next_pos = scanout.find(StartStr, pos+strlen(StartStr));
        if (next_pos == string::npos) {
            next_pos = scanout.size();
        }
        string mode = getparm(scanout, "Mode:", pos, next_pos);
        if (mode != "Master") {
            continue;
        }
        int p1 = scanout.find("ESSID:\"", pos);
        if ( (p1 == string::npos) or (p1 > next_pos) ) {
            continue;
        }
        p1 += 7;
        int p2 = scanout.find("\"\n", p1); 
        if ( (p2 == string::npos) or (p2 > next_pos) ) {
            continue;
        }
        string essid = scanout.substr(p1, (p2-p1));
        if (essid_only) {
            cout << csvQuote(essid) << endl;
            continue;
        }
        string enc = getparm(scanout, "Encryption key:", pos, next_pos);
        if (enc != "off") {
            int p = scanout.find("WPA", pos);
            if ( (p != string::npos) and (p < next_pos) ) {
                enc = "WPA";
            }
            else {
                enc = "WEP";
            }
        }
        string address = getparm(scanout, "Address: ", pos, next_pos);
        string channel = getparm(scanout, "(Channel ", pos, next_pos, ")");
        string signal = getparm(scanout, "Signal level=", pos, next_pos);
        cout << address << "," << channel << "," << enc << "," << signal << "," << csvQuote(essid) << endl;
    }
    return retval;
};

string getparm(string search, string str, int start, int end, string term) {
    string retval = "";
    int p1 = search.find(str, start);
    if ( (p1 != string::npos) and (p1 < end) ) {
        p1 += str.size();
        int p2 = search.find_first_of(term, p1);
        if ( (p2 != string::npos) and (p2 <= end) ) {
            retval = search.substr(p1, (p2-p1));
        }
    }
    return retval;
}

string getparm(string search, string str, int start, int end) {
    return getparm(search, str, start, end, " \t\n\r");
}

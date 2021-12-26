#include <iostream>
#include "autonet_types.h"
#include "autonet_files.h"
#include "readAssoc.h"
#include "writeAssoc.h"
#include "getopts.h"
#include "split.h"
#include "str_system.h"
using namespace std;

void createGsmFiles(string apn, string userpass);

Assoc config;

int main(int argc, char *argv[]) {
    int exitstat = 0;
    if (argc < 2) {
        cout << "Usage: update_gsm_info [options]" << endl;
        cout << "Options:" << endl;
        cout << "  -i imsi       Updates IMSI" << endl;
        cout << "  -m mcn        Updates MCN" << endl;
        cout << "  -a apn        Updates APN" << endl;
        cout << "  -u user/pass  Updates user and password" << endl;
        cout << "  -p provider   Updates provider" << endl;
        cout << "  -c            Creates GSM config from existing info" << endl;
        exit(0);
    }
    GetOpts opts(argc, argv, "a:ci:m:p:u:");
    bool create_configs = opts.exists("c");
    string apn = opts.get("a");
    string imsi = opts.get("i");
    string mcn = opts.get("m");
    string userpass = opts.get("u");
    string provider = opts.get("p");
    config = readAssoc(GsmConfFile, ":");
    if (create_configs) {
        apn = config["apn"];
        userpass = config["user"];
        createGsmFiles(apn, userpass);
    }
    else {
        if (provider != "") {
            config["provider"] = provider;
        }
        if (imsi != "") {
            config["imsi"] = imsi;
        }
        if (mcn != "") {
            config["mcn"] = mcn;
        }
        if (apn != "") {
            config["apn"] = apn;
            config["user"] = userpass;
            createGsmFiles(apn, userpass);
        }
        string tmpfile = string(GsmConfFile) + ".tmp";
        writeAssoc(tmpfile, config, ":");
        rename(tmpfile.c_str(), GsmConfFile);
    }
    return exitstat;
}

void createGsmFiles(string apn, string userpass) {
    string user = "";
    string password = "";
    if (userpass != "") {
        Strings v = split(userpass, "/");
        user = v[0];
        if (v.size() > 1) {
            password = v[1];
        }
    }
    str_system("sed 's/APN/"+apn+"/' "+RoGsmChatFile+" >"+RwGsmChatFile);
    str_system("sed 's/USER/"+user+"/' "+RoGsmFile+" | sed 's/PASS/"+password+"/' >"+RwGsmFile);
}

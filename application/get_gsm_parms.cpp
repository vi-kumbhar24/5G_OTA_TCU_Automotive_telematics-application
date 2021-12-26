#include <iostream>
#include <stdlib.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "fileExists.h"
#include "getopts.h"
#include "readFileArray.h"
#include "getStringFromFile.h"
#include "csv.h"
using namespace std;

int main(int argc, char *argv[]) {
    GetOpts opts(argc, argv, "lq");
    bool list = opts.exists("l");
    bool quiet = opts.exists("q");
    if (argc != 2) {
        cout << "Usage: test_gsm_parms provider_number" << endl;
        exit(1);
    }
    string mcn = argv[1];
    string provider = "";
    string apn = "";
    string user = "";
    string password = "";
    bool found = false;
    bool multi = false;
    bool same_apn = true;
    Strings gsmlines = readFileArray(GsmParmsFile);
    if (fileExists(UserGsmParmsFile) and !list) {
        string user_parms = getStringFromFile(UserGsmParmsFile);
        gsmlines.push_back(user_parms);
    }
    for (int i=0; i < gsmlines.size(); i++) {
        Strings vals = csvSplit(gsmlines[i],",");
//        cerr << gsmlines[i] << " : " << vals.size() << endl;
        while (vals.size() < 5) {
            vals.push_back("");
        }
        if (vals[0] == mcn) {
            if (apn == "") {
                provider = vals[1];
                apn = vals[2];
                user = vals[3];
                password = vals[4];
                found = true;
            }
            else if ( (vals[2] != apn) or
                      (vals[3] != user) or
                      (vals[4] != password) ) {
                multi = true;
            }
            else {
                same_apn = true;
//                provider += "|" + vals[1];
            }
        }
    }
    if (multi or (list and same_apn)) {
        if (list) {
            for (int i=0; i < gsmlines.size(); i++) {
                Strings vals = csvSplit(gsmlines[i],",");
                while (vals.size() < 5) {
                    vals.push_back("");
                }
                if (vals[0] == mcn) {
                    cout << vals[2] << "," << vals[3] << "," << vals[4] << ",\"" << vals[1] << "\"" << endl;
                }
            }
        }
        else {
            if (!quiet) {
                cout << "Multiple APN" << endl;
            }
            exit(1);
        }
    }
    else if (found) {
        cout << apn << "," << user << "," << password << ",\"" << provider << "\"" << endl;
    }
    return 0;
}

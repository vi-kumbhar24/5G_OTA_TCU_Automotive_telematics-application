#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "getopts.h"
#include "fileExists.h"
#include "splitFields.h"
#include "string_convert.h"
using namespace std;

int main(int argc, char *argv[]) {
    GetOpts opts(argc, argv, "d:k");
    string dir = VinToModelDir;
    if (opts.exists("d")) {
        dir = opts.get("d");
    }
    bool make_only = opts.exists("k");
    string wmi_file = dir + "/vin_to_wmi";
    string file = dir + "/index.regex";
    if (argc != 2) {
        cout << "Usage: vin_to_make vin" << endl;
        exit(1);
    }
    string vin = toUpper(argv[1]);
    char year_cond = vin.at(6);
    char year_digit = vin.at(9);
    string year_codes = "ABCDEFGHJKLMNPRSTVWXY123456789";
    int year = 1980;
    if (year_cond >= 'A') {
        year = 2010;
    }
    for (int i=0; i < year_codes.size(); i++) {
        if (year_digit == year_codes.at(i)) {
            year += i;
            break;
        }
    }
    ifstream fin;
    while (file != "") {
        string next_file = "";
        if (!fileExists(file)) {
            cerr << "Unable to read file: " << file << endl;
            exit(1);
        }
        //cerr << "Opening:" << file << endl;
        fin.open(file.c_str());
        if (fin.is_open()) {
            string line;
            while (fin.good()) {
                getline(fin, line);
                if (fin.eof()) break;
                if (line.substr(0,1) == "#") continue;
                Strings fields = splitFields(line, ":");
                if (fields.size() < 2) continue;
                //cerr << "Checking: " << line << endl;
                string code = fields[0];
                string make = fields[1];
                string model = "";
                if (fields.size() > 2) {
                    model = fields[2];
                }
                //cerr << "code:" << code << " make:" << make << " model:" << model << endl;
                string pattern = "^" + code;
                regex_t rgx;
                regcomp(&rgx, pattern.c_str(), 0);
                if (regexec(&rgx, vin.c_str(), 0, NULL, 0) == 0) {
                    //cerr << "Matched" << endl;
                    if (make == "F") {
                        next_file = dir + "/" + model;
                        break;
                    }
                    if (model == "") {
                        model = "Model unknown";
                    }
                    if (make_only) {
                        cout << make << endl;
                    }
                    else {
                        cout << year << "," << make << "," << model << endl;
                    }
                    exit(0);
                }
                regfree(&rgx);
            }
            fin.close();
        }
        file = next_file;
    }
    if (!fileExists(wmi_file)) {
        cerr << "Unable to read file: " << wmi_file << endl;
        exit(1);
    }
    fin.open(wmi_file.c_str());
    if (fin.is_open()) {
        string line;
        while (fin.good()) {
            getline(fin, line);
            if (fin.eof()) break;
            if (line.substr(0,1) == "#") continue;
            int pos = line.find(" ");
            if (pos != string::npos) {
                string code = line.substr(0,pos);
                string make = line.substr(pos+1);
                if (code == vin.substr(0, code.size())) {
                    if (make_only) {
                        cout << make << endl;
                    }
                    else {
                        cout << year << "," << make << ",Model unknown" << endl;
                    }
                    exit(0);
                }
            }
        }
        fin.close();
    }
    cerr << "VIN did not match" << endl;
    return 1;
}

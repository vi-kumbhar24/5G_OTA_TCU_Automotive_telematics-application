#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "fileExists.h"
#include "string_convert.h"
using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Usage: vin_to_make vin" << endl;
        exit(1);
    }
    string vin = toUpper(argv[1]);
    if (!fileExists(VinToMakeFile)) {
        cerr << "Unable to read file: " << VinToMakeFile << endl;
        exit(1);
    }
    ifstream fin;
    fin.open(VinToMakeFile);
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
                string pattern = "^" + code;
                regex_t rgx;
                regcomp(&rgx, pattern.c_str(), 0);
                if (regexec(&rgx, vin.c_str(), 0, NULL, 0) == 0) {
                    cout << make << endl;
                    exit(0);
                }
                regfree(&rgx);
            }
        }
        fin.close();
    }
    if (!fileExists(VinToWmiFile)) {
        cerr << "Unable to read file: " << VinToWmiFile << endl;
        exit(1);
    }
    fin.open(VinToWmiFile);
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
                    cout << make << endl;
                    exit(0);
                }
            }
        }
        fin.close();
    }
    cerr << "VIN did not match" << endl;
    return 1;
}

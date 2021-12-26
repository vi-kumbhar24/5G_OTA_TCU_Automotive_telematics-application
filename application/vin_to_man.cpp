#include <iostream>
#include <stdlib.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "fileExists.h"
#include "readAssoc.h"
#include "string_convert.h"
using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Usage: vin_to_man vin" << endl;
        exit(1);
    }
    string vin = toUpper(argv[1]);
    if (!fileExists(VinToManFile)) {
        cerr << "Unable to read file: " << VinToManFile << endl;
        exit(1);
    }
    Assoc mans = readAssoc(VinToManFile, " ");
    Assoc::iterator it;
    for (it=mans.begin(); it != mans.end(); it++) {
        string code = (*it).first;
        string man = (*it).second;
        if (code == vin.substr(0, code.size())) {
            cout << man << endl;
            exit(0);
        }
    }
    cerr << "VIN did not match" << endl;
    return 1;
}

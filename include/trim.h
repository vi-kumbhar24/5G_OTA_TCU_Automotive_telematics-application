#ifndef TRIM_H
#define TRIM_H

#include <string>
using namespace std;

string ltrim(string str) {
    int pos;
    pos = str.find_first_not_of(" \n\t\r");
    if (pos != string::npos) {
        str.erase(0, pos);
    }
    return str;
}

string rtrim(string str) {
    int pos;
    pos = str.find_last_not_of(" \n\t\r");
    if (pos != string::npos) {
        str.erase(pos+1);
    }
    return str;
}

string trim(string str) {
    str = rtrim(ltrim(str));
    return str;
}

#endif

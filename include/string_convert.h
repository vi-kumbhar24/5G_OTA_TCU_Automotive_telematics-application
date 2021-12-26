#ifndef STRING_CONVERSION_H
#define STRING_CONVERSION_H

#include <sys/types.h>
#include <sstream>
using namespace std;

int stringToInt(string str) {
    istringstream ist(str);
    int i;
    ist >> i;
    return i;
}

unsigned long stringToULong(string str) {
    istringstream ist(str);
    unsigned long i;
    ist >> i;
    return i;
}

int64_t stringToInt64(string str) {
    istringstream ist(str);
    int64_t i;
    ist >> i;
    return i;
}

long stringToLong(string str) {
    istringstream ist(str);
    long i;
    ist >> i;
    return i;
}

double stringToDouble(string str) {
    istringstream ist(str);
    double d;
    ist >> d;
    return d;
}

string toString(int i) {
    stringstream out;
    out << i;
    return out.str();
}

string toString(unsigned int i) {
    stringstream out;
    out << i;
    return out.str();
}

string toString(long i) {
    stringstream out;
    out << i;
    return out.str();
}

string toString(unsigned long i) {
    stringstream out;
    out << i;
    return out.str();
}

#if __SIZEOF_LONG__ < 8
string toString(int64_t i) {
    stringstream out;
    out << i;
    return out.str();
}
#endif

string toString(double d) {
    stringstream out;
    out << d;
    return out.str();
}

string toUpper(string str) {
    for (int i=0; i < str.size(); i++) {
        str[i] = toupper(str[i]);
    }
    return str;
}

string toLower(string str) {
    for (int i=0; i < str.size(); i++) {
        str[i] = tolower(str[i]);
    }
    return str;
}

#endif

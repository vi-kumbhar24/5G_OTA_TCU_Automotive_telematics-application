#ifndef LOGERROR_H
#define LOGERROR_H

#include "dateTime.h"
#include "appendToFile.h"
using namespace std;

string LogErrorFile = "";

void setLogErrorFile(string file) {
    LogErrorFile = file;
}

void logError(string error) {
    string datetime = getDateTime();
    if (LogErrorFile == "") {
        cerr << datetime << " " << error << endl;
    }
    else {
        appendToFile(LogErrorFile, datetime+" "+error);
    }
}

#endif

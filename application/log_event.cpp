/*******************************************************************************************
 * File: log_event.cpp
 *
 * Routine to send events to the TruManager
 *******************************************************************************************
 * 2015/05/28 rwp
 *    Added ability to provide date/time string
 *******************************************************************************************
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "autonet_files.h"
#include "getopts.h"
#include "eventLog.h"
using namespace std;

EventLog eventlog;

int main(int argc, char *argv[]) {
     GetOpts opts(argc, argv, "t:");
     string timestamp = "";
     if (opts.exists("t")) {
         timestamp = opts.get("t");
     }
     string event = "";
     int i;
     for (i=1; i < argc; i++) {
         event += " ";
         event += argv[i];
     }
     if (event == "") {
         event = "Test event";
     }
     if (timestamp != "") {
         eventlog.append(timestamp, event);
     }
     else {
         eventlog.append(event);
     }
}

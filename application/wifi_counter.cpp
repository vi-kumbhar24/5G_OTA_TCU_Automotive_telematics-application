/*****************************************************************************
 * File wifi_counter.cpp
 * Daemon to watch /var/log/messages for STA ADD/DEL messages and update
 * /tmp/associated_sta with mac addresses of stations
 *****************************************************************************
 * 2013/04/19 rwp
 *    Fixed to monitor inode number of log file
 * 2014/05/16 rwp
 *    Fixed to look for connect/disconnect messages from new driver
 *****************************************************************************
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "tail.h"
#include "autonet_files.h"
#include "dateTime.h"
using namespace std;

#define LogFile "/var/log/messages"

int findMac(string mac);
void addMac(string mac);
void removeMac(string mac);
void writeStationList();
void log(string mesg);

bool debug = false;
Tail tailer;
vector<string> stations;

int main(int argc, char *argv[])
{
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    log("Started");
    umask(S_IWGRP | S_IWOTH);
    tailer.open(LogFile);
    struct stat st_info;
    off_t log_size = 0;
    ino_t file_ino = 0;
    int v = stat(LogFile, &st_info);
    if (v == 0) {
        log_size = st_info.st_size;
        file_ino = st_info.st_ino;
    }
    while (1) {
        string line;
        if (tailer.read(line)) {
            int pos = -1;
            if ((pos=line.find("NEW STA")) != string::npos) {
                string mac = line.substr(pos+8,17);
                addMac(mac);
            }
            else if ((pos=line.find("DEL STA")) != string::npos) {
                string mac = line.substr(pos+8,17);
                removeMac(mac);
            }
            else if ((pos=line.find(" STA ")) != string::npos) {
                string mac = line.substr(pos+5,17);
                if (line.find(" associated") != string::npos) {
                    addMac(mac);
                }
                else if (line.find("disassociated") != string::npos) {
                    removeMac(mac);
                }
            }
//            else if ((pos=line.find("new SDIO card")) != string::npos) {
//                system("/etc/init.d/wlan0 start");
//            }
        }
        else {
            sleep(1);
            int v = stat(LogFile, &st_info);
            if (v == 0) {
                off_t new_log_size = st_info.st_size;
                ino_t new_ino = st_info.st_ino;
                bool newfile = false;
                if (new_log_size < log_size) {
                    newfile = true;
                    log("Logfile is smaller");
                }
                if (new_ino != file_ino) {
                    newfile = true;
                    log("Logfile is different ino");
                }
                if (newfile) {
                    tailer.close();
                    tailer.open(LogFile);
                    file_ino = new_ino;
                }
                log_size = new_log_size;
            }
        }
    }
    return 0;
}

int findMac(string mac) {
    int idx = -1;
    int i = 0;
    for (i=0; i < stations.size(); i++) {
        if (stations[i] == mac) {
            idx = i;
            break;
        }
    }
    return idx;
}

void addMac(string mac) {
    if (findMac(mac) < 0) {
        stations.push_back(mac);
        if (debug) cout << "Add " << mac << endl;
        writeStationList();
    }
}

void removeMac(string mac) {
    int idx = findMac(mac);
    if (idx >= 0) {
        stations.erase(stations.begin()+idx);
        if (debug) cout << "Rem " << mac << endl;
        writeStationList();
    }
}

void writeStationList() {
    int i = 0;
    string tmpfile = StationListFile;
    tmpfile += ".tmp";
    ofstream fout(tmpfile.c_str());
    if (fout.is_open()) {
        for (i=0; i < stations.size(); i++) {
             fout << stations[i] << endl;
        }
        fout.close();
        rename(tmpfile.c_str(), StationListFile);
    }
}

void log(string mesg) {
    cout << getDateTime() << " " << mesg << endl;
}

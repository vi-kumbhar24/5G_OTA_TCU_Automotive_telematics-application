/*******************************************************************************
 * File: vic_loader
 *
 * Program that loads VIC firmware
 *
 ********************************************************************************
 * Change log:
 * 2012/08/02 rwp
 *    Change to use VIC version instead of file to determine current version
 *    file
 * 2012/12/03 rwp
 *    Added printing of VIC boot_loader version (if debug)
 * 2013/01/08 rwp
 *    Added support for imaging config data (Dow)
 * 2014/10/08 rwp
 *    Added date/time to output to help in debugging
 ********************************************************************************
 */

#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "backtick.h"
#include "dateTime.h"
#include "fileExists.h"
#include "updateFile.h"
#include "getopts.h"
#include "getStringFromFile.h"
#include "vic.h"
using namespace std;

void logIt(string msg);
bool programVic(string filename);
int getVicResponse(int len, Byte *buffer);
int getLoaderResponse(int len, Byte *buffer);

int app_id = 0x20;
VIC vic;
bool in_loader = false;
bool debug = false;
bool reset_it = false;
bool load_config = false;

int main(int argc, char *argv[]) {
    int exitstat = 1;
    Byte buffer[256];
    string original = "";
//    if (fileExists(VicVersionFile)) {
//        original = "/boot/" + getStringFromFile(VicVersionFile);
//    }
    GetOpts opts(argc, argv, "cdlr");
    load_config = opts.exists("c");
    debug = opts.exists("d");
    in_loader = opts.exists("l");
    reset_it = opts.exists("r");
    if ((argc < 2) and !in_loader) {
        cout << "Usage: vic_loader <firmware_file>" << endl;
        exit(1);
    }
    string filename = "";
    if (argc > 1) {
        filename = argv[1];
        if (!fileExists(filename)) {
            logIt("Firmware file does not exist: "+filename);
            exit(1);
        }
    }
    else {
        filename = backtick("ls /boot/VIC-*.dat | head -1");
        if (filename == "") {
            logIt("No VIC firmware file");
            system("log_event NotifyEngineering No VIC firmware file");
            exit(1);
        }
    }
    if (!vic.openSocket()) {
        logIt("Failed to open VIC socket");
        exit(1);
    }
    if (debug) logIt("Opened VIC socket");
    int n = 1;
    if (!in_loader) {
        vic.getVersion(app_id);
        original = "/boot/VIC-"+vic.build+"-"+vic.version_str+".dat";
        if (debug) cerr << "Sending Start Bootloader" << endl;
        buffer[0] = 0x42;
        n = getVicResponse(1, buffer);
//        usleep(20000);
        sleep(1);
    }
    if (n > 0) {
        exitstat = 0;
        bool programmed = false;
        bool upgraded = false;
        programmed = programVic(filename);
        if (programmed) {
            upgraded = true;
        }
        else {
            programmed = programVic(filename);
            if (programmed) {
                upgraded = true;
            }
            else if (original != "") {
                programmed = programVic(original);
                if (programmed) {
                    logIt("VIC upgrade failed");
                    system("log_event NotifySupport VIC upgrade failed");
                }
            }
        }
        if (!programmed) {
            logIt("VIC programming failed");
            system("log_event NotifySupport VIC programming failed");
        }
        else {
            if (upgraded) {
                string vic_version = filename;
                size_t pos = vic_version.find_last_of("/");
                if (pos != string::npos) {
                    vic_version.erase(0, pos+1);
                }
//                updateFile(VicVersionFile, vic_version);
//                cout << "VIC successfully upgraded" << endl;
            }
            if (in_loader or reset_it) {
                // Resetting the VIC will also power down the MX35
                if (debug) logIt("killing monitor");
                system("/etc/init.d/monitor stop");
                if (debug) logIt("stopping respawner");
                system("pkill -USR1 respawner");
                sleep(1);
                if (debug) logIt("killing gps_mon");
                system("pkill gps_mon");
                if (debug) logIt("killing event_logger");
                system("pkill event_logger");
                sleep(2);
                if (debug) logIt("Unmounting autonet/user partitions");
                system("/etc/init.d/S11mount stop");
                sleep(2);
                logIt("Resetting VIC");
                buffer[0] = 0x64;
                n = getLoaderResponse(1, buffer);
            }
        }
    }
    else {
        logIt("Did not get ack for LoadFirmwareCommand");
    }
    vic.closeSocket();
    return exitstat;
}

void logIt(string msg) {
    string datetime = getDateTime();
    cerr << datetime << " " << msg << endl;
}

bool programVic(string filename) {
    Byte buffer[256];
    int n = 0;
    bool ok = true;
    // Get bootloader Information
    buffer[0] = 0x58;
    buffer[1] = 0x01;
    n = getLoaderResponse(2, buffer);
    // Check if bootloader is running
    if (n < 5) {
        logIt("Failed to get response to GetApplicationInfo");
        ok = false;
    }
    else {
        string loaderdate;
        loaderdate.assign(buffer[4],8);
        string loadervers = string_printf("%02X.%02X", buffer[12], buffer[13]);
        if (debug) {
            logIt("Build date: "+loaderdate);
            logIt("Version: "+loadervers);
        }
        // Send Erase command
        buffer[0] = 0x50;
        if (load_config) {
            buffer[0] = 0x51;
        }
        n = getLoaderResponse(1, buffer);
        if (n != 4) {
            logIt("Failed to get response to Erase");
            ok = false;
        }
        else {
            if (debug) cerr << "Programming VIC";
            string line;
            ifstream fin(filename.c_str());
            if (fin.is_open()) {
                while (fin.good()) {
                    fin.read((char *)buffer+1, 72);
                    if (debug) cerr << ".";
                    n = fin.gcount();
                    if (n == 0) break;
                    buffer[0] = 0x5F;
                    n = getLoaderResponse(n+1, buffer);
                    if ((n != 4) or (buffer[2] == 0x85)) {
                        cerr << "Program failed" << endl;
                        ok = false;
                        break;
                    }
                }
                fin.close();
            }
            else {
                logIt("Failed to open: "+filename);
                ok = false;
            }
            if (ok) {
                if (debug) {
                    cerr << endl << "Programming complete" << endl;
                    cerr << "Verifying";
                }
                ifstream fin(filename.c_str());
                if (fin.is_open()) {
                    while (fin.good()) {
                        fin.read((char *)buffer+1, 72);
                        if (debug) cerr << "+";
                        n = fin.gcount();
                        if (n == 0) break;
                        buffer[0] = 0x60;
                        n = getLoaderResponse(n+1, buffer);
                        if ((n != 4) or (buffer[2] == 0x86)) {
                            cerr << "Verify failed" << endl;
                            ok = false;
                            break;
                        }
                    }
                    fin.close();
                    if (debug) cerr << endl;
                }
                else {
                    logIt("Failed to open: "+filename);
                    ok = false;
                }
            }
        }
    }
    if (ok) {
        if (debug) cerr << "Verification complete" << endl;
        buffer[0] = 0x54;
        if (load_config) {
            buffer[0] = 0x55;
        }
        n = getLoaderResponse(1, buffer);
        if (n > 5) {
            if (buffer[3] == 0x00) {
                if (debug) cerr << "Checksum matched" << endl;
            }
            else {
                logIt("Checksum did not match");
                fprintf(stderr, "Stored:%02X%02X Calculated:%02X%02X\n",
                        buffer[4], buffer[5], buffer[6], buffer[7]);
                ok = false;
            }
        }
        else {
            logIt("Failed to get Validation Response");
            ok = false;
        }
    }
    return ok;
}

int getVicResponse(int len, Byte *buffer) {
    if (len == 0) {
        vic.sendRequest(app_id, 0x06);
    }
    else {
        vic.sendRequest(app_id, 0x06, len, buffer);
    }
    len = vic.getResp(buffer, 10);
    if (len < 2) {
        len = 0;
    }
    if ((buffer[0] != app_id) or (buffer[1] != 0x06)) {
        len = 0;
    }
    return len;
}

int getLoaderResponse(int len, Byte *buffer) {
    Byte buff[256];
    buff[0] = app_id;
    buff[1] = 0x06;
    for (int i=0; i < len; i++) {
        buff[2+i] = buffer[i];
    }
    vic.sendMesgWithoutSig(len+2, buff);
    len = vic.getResp(buffer, 5);
//    cerr << "Recvd " << len << " bytes" << endl;
    if (len < 2) {
        len = 0;
    }
    if ((buffer[0] != app_id) or (buffer[1] != 0x06)) {
        len = 0;
    }
    return len;
}

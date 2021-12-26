/******************************************************************************
 * File: upgrade.cpp
 * Routine to upgrade the unis software
 ******************************************************************************
 * 2013/04/17 rwp
 *    Fixed findPath
 * 2013/07/07 rwp
 *    AUT-42: Don't verfify upgrade if we are upgrading a modified file system
 * 2013/07/07 rwp
 *    AUT-43: Perform untarring and upgrading during initial upgrade
 *    Added optional download speed parameter
 * 2014/11/07 rwp
 *    AK-45: Dont write version to UpgradedVersionFile
 * 2015/02/02 rwp
 *    AUT-159: Check for errors in dup_fs, flash_mount and existance of verify_fs.conf
 * 2019/03/07 cjh
 *     CONLAREINS-744 Remove content and non-firmware upgrades
 *     Move nearly all of the install functionality into the bundled install script.
 *     Remove alternate upgrade types (media and content)
 *     verify_fs can be done inside the installer instead of here
 *     No in-place upgrades
 ******************************************************************************
 */

/**
 * A note about states:
 *
 * starting: nothing has been done yet.  Identify which update package to get.
 * fetching: retrieving update package
 * untarring: un-archiving update package
 * upgrading: running installer
 * upgraded: Succeeded in upgrading.  Needs reboot into new kernel.
 * failed: failed.
 *
 * pending: remove.  Used to used for in-place updates.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "getopts.h"
#include "split.h"
#include "fileExists.h"
#include "getStringFromFile.h"
#include "backtick.h"
#include "str_system.h"
#include "updateFile.h"
#include "touchFile.h"
using namespace std;

typedef map<string,string> Assoc;
typedef map<string,Strings> StringsAssoc;

int upgradeFirmware(string to_version);
int upgrade(string from_vers, string to_vers);
void continueUpgrade();
string getTarfile();
int install(string tarfile);
int installFirmware();
void cleanAllFiles();
void setState(string state);
void getUpgradePathsFile();
bool readUpgradePaths(const char *file, string type);
Strings findPath(string from, string to);
void printPath(Strings path);

StringsAssoc upgrades;
string upgrade_state = "";
string upgraded_version = "";
string upgrade_fsId = "";
// bool inplace = false;
bool force = false;
bool perform_upgrade = false;
//string download_speed = "-r 20k";
string download_speed = "";

int main(int argc, char **argp) {
    int exitstat = 1;
    GetOpts opts(argc, argp, "cpFr:s:");
    bool continue_upgrade = opts.exists("c");
    perform_upgrade = opts.exists("p");
    force = opts.exists("F");
    string from_version = "";

    if (opts.exists("r")) {
        upgrade_fsId = opts.get("r");
    }
    // string upgrade_type = "FW";
    if (opts.exists("s")) {
        download_speed = "-r "+opts.get("s");
    }
    string mkdir_cmd = "mkdir -p ";
    mkdir_cmd += UpgradeDir;
    system(mkdir_cmd.c_str());
    chdir(UpgradeDir);
    string procs = backtick("pgrep upgrade");
    if (procs.find("\n") != string::npos) {
        cerr << "Upgrading already in progress" << endl;
        exit(1);
    }
    if (fileExists(UpgradeStateFile)) {
        upgrade_state = getStringFromFile(UpgradeStateFile);
    }
    if (upgrade_state != "") {
        if (upgrade_state == "fetching") {
            continueUpgrade();
            exit(0);
        }
        else if ( (upgrade_state == "upgrading") or
                  (upgrade_state == "untarring") ) {
            string tarfile = getTarfile();
            install(tarfile);
            exit(0);
        }
        else if (upgrade_state == "upgraded") {
            cerr << "Already upgraded...waiting reboot" << endl;
            exit(0);
        }
        else if (upgrade_state == "pending") {
            cerr << "Already fetched...waiting reboot" << endl;
            exit(0);
        }
        else {
           cleanAllFiles();
        }
    }
    else if (continue_upgrade) {
        // we were told to continue an upgrade, but no upgrade is in process
        exit(0);
    }
    if (argc != 2) {
        cout << "usage: upgrade [-r source_root] [-s speed] [-F] [-c] [-p] <to_version>" << endl;
        exit(1);
    }
    setState("starting");
    upgraded_version = "";
    string to_version = argp[1];
    exitstat = upgradeFirmware(to_version);

    if (upgraded_version != "") {
        cout << "Upgraded to: " << upgraded_version << endl;
    }
    return exitstat;
}

int upgradeFirmware(string to_version) {
    int exitstat = 1;
    getUpgradePathsFile();
    readUpgradePaths(UpgradePathsFile, "FW");
    int pos = 0;
    bool upgraded = false;
    string sw_version = getStringFromFile(SwVersionFile);
    sw_version.erase(sw_version.find_last_not_of(" \n\r\t")+1);  // trim whitespace

    if (to_version != sw_version) {
        exitstat = upgrade(sw_version, to_version);
        if (exitstat == 0) {
            upgraded_version = to_version;
            upgraded = true;
        }
    }

    if( ! upgraded ) {
        cleanAllFiles();
    }

    return exitstat;
}

int upgrade(string from_vers, string to_vers) {
    int exitstat = 1;
    Strings path;
    if (upgrades.size() != 0) {
        path = findPath(from_vers, to_vers);
    }
    if (path.size() != 0) {
//        printPath(path);
        string dest_vers = path[1];

        if (fileExists(UpgradedVersionFile)) {
            string upgraded_vers = getStringFromFile(UpgradedVersionFile);
            if (upgraded_vers == dest_vers) {
                cerr << "Cannot upgrade to " << dest_vers << "... previously failed" << endl;
                string log_cmd = "log_event 'NotifyEngineering Upgrade failed from:"+from_vers+" to:"+to_vers+"'";
                system(log_cmd.c_str());
                return 1;
            }
        }

        cerr << "Upgrading " << " from " << from_vers << " to " << dest_vers << endl;
        string tarfile = from_vers + "-" + dest_vers + ".tgz";
        setState("fetching");
        cerr << "Downloading: " + tarfile << endl;
        string cmnd = "updatefiles "+download_speed+" -i " + tarfile;
        exitstat = system(cmnd.c_str());
        if (exitstat == 0) {
            exitstat = install(tarfile);
            if (exitstat == 0) {
                upgraded_version = dest_vers;
            } else {
                string log_cmd = "log_event 'Upgrade to "+dest_vers+" failed due to installer exit code "+to_string(exitstat)+"'";
                system(log_cmd.c_str());
            }
        }
        else if (!fileExists(tarfile)) {
            unlink(UpgradeStateFile);
        }
    }
    else {
        cout << "No upgrade from " << from_vers << " to " << to_vers << endl;
        unlink(UpgradeStateFile);
    }
    return exitstat;
}

void continueUpgrade() {
    string tarfile = getTarfile();
    if (tarfile != "") {
        string cmnd = "updatefiles "+download_speed+" -c -i " + tarfile;
        int exitstat = system(cmnd.c_str());
        if (exitstat == 0) {
            install(tarfile);
        }
    }
    else {
        cleanAllFiles();
    }
}

string getTarfile() {
    string tarfile = backtick("ls *.tgz");
    return tarfile;
}

int install(string tarfile) {
    int exitstat = 1;
    bool ok = true;
    if (fileExists(tarfile)) {
        if (upgrade_state != "installing") {
            setState("untarring");
            cerr << "Untarring: " << tarfile << endl;
            string cmnd = "gunzip -c " + tarfile + " | tar -x 2>/dev/null";
            if (system(cmnd.c_str()) != 0) {
                setState("failed");
                cerr << "Corrupted tarfile: " << tarfile << endl;
                exitstat = 1;
                ok = false;
            }
        }
        if (ok) {
            if (fileExists("install")) {
                setState("upgrading");
                exitstat = installFirmware();
            }
            else {
                setState("failed");
                cerr << "Missing install file" << endl;
                exitstat = 1;
                ok = false;
            }
        }
        if (ok) {
            if ( exitstat == 0 ) {
                setState("upgraded");
                touchFile("/tmp/reboot_it");
            }
            else {
                cleanAllFiles();
            }
        }
    }
    else {
        cerr << "No upgrade file retrieved" << endl;
        unlink(UpgradeStateFile);
    }
    return exitstat;
}

int installFirmware() {
    int exitstat = 1;
    bool ok = true;
    string rootfsId = "";
    string opposite = "";
    int retval;

    if (fileExists(KernelIdFile)) {
        rootfsId = getStringFromFile(KernelIdFile);
    }

    if ((rootfsId == "1") or (rootfsId == "2")) {
        opposite = "2";
        if (rootfsId == "2") {
            opposite = "1";
        }
    }
    else if (rootfsId == "0") {
        if ( (upgrade_fsId == "1") or (upgrade_fsId == "2") ) {
            opposite = upgrade_fsId;
        }
        else {
            cerr << "Cannot upgrade...Not an upgradable rootfs" << endl;
            exitstat = 1;
            ok = false;
        }
    }
    else {
        cerr << "Cannot upgrade...Not an upgradable rootfs" << endl;
        exitstat = 1;
        ok = false;
    }

    string mmcblk = backtick("fw_printenv mmcblk | sed 's/mmcblk=//'");
    if ( mmcblk == "" ) {
        cerr << "Cannot find mmcblk" << endl;
        exitstat = 1;
        ok = false;
    }

    if (ok) {
        cerr << "Running installer" << endl;
        exitstat = str_system("./install -v -k "+opposite+" -m /dev/"+mmcblk);
        if( exitstat != 0 ) {
            ok = false;
            cerr << "installer returned error " << to_string(retval) << " " << endl;
        }
    }

    if(!ok) {
        // if the update failed, we restore the backup file system to be a mirror of our own
        system("dup_fs >/dev/null 2>&1");
    }

    return exitstat;
}

void cleanAllFiles() {
    cerr << "Removing all files" << endl;
    string cmnd = "rm -rf ";
    cmnd += UpgradeDir;
    cmnd += "/*";
    system(cmnd.c_str());
}

void setState(string state) {
    upgrade_state = state;
    updateFile(UpgradeStateFile, state);
}

void getUpgradePathsFile() {
    string cmnd = "updatefiles -i "+download_speed+" ";
    cmnd += UpgradePathsFile;
    system(cmnd.c_str());
}

/**
 * The type parameter is here for historical reasons.  It is always set to "FW"
 * See pre-5.2.12 versions of this file if you are curious.
 */
bool readUpgradePaths(const char *file, string type) {
    bool ok = false;
    if (fileExists(file)) {
        ifstream inf(file);
        if (inf.is_open()) {
            while (true) {
                string line;
                getline(inf, line);
                if (inf.eof()) break;
                Strings vals = split(line, " ");
                string in_type;
                string from;
                string to;
                if (vals.size() == 2) {
                    in_type = "FW";
                    from = vals[0];
                    to = vals[1];
                }
                else if (vals.size() == 3) {
                    in_type = vals[0];
                    from = vals[1];
                    to = vals[2];
                }
                else {
                    continue;
                }
                if (in_type != type) continue;
                if (upgrades.find(from) == upgrades.end()) {
                    Strings strs;
                    upgrades[from] = strs;
                }
                upgrades[from].push_back(to);
            }
            inf.close();
        }
    }
    return ok;
}

Strings findPath(string from, string to) {
    Strings shortestpath;
    if (upgrades.find(from) != upgrades.end()) {
        Strings tolist = upgrades[from];
        int minpathlen = 100;
        for (int i=0; i < tolist.size(); i++) {
            if (tolist[i] == to) {
                shortestpath.clear();
                shortestpath.push_back(to);
                break;
            }
            Strings path = findPath(tolist[i], to);
            if (path.size() != 0) {
                if (path.size() < minpathlen) {
                    minpathlen = path.size();
                    shortestpath = path;
                }
            }
        }
        if (shortestpath.size() != 0) {
            shortestpath.insert(shortestpath.begin(), from);
        }
    }
    return shortestpath;
}

void printPath(Strings path) {
    for (int i=0; i < path.size(); i++) {
        if (i != 0) {
            cout << " -> ";
        }
        cout << path[i];
    }
    cout << endl;
}

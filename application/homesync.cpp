/******************************************************************************
 * File: homesync.cpp
 *
 * Program to sync files to/from another system via wifi
 *
 ******************************************************************************
 * 2013/01/01 rwp
 *    Initial version
 * 2013/02/27 rwp
 *    Added time synchronization to perform_dowsync
 * 2018/08/31 rwp
 *    Changed config from local_features to ConfigManager
 ******************************************************************************
 */

#include <unistd.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "readAssoc.h"
#include "getStringFromFile.h"
#include "grepFile.h"
#include "split.h"
#include "backtick.h"
#include "readconfig.h"
#include "str_system.h"
#include "updateFile.h"
#include "writeStringToFile.h"
using namespace std;

string servers = "";
string username = "";
string password = "";
string basedir = "";
string partition = "";
string synctype = "";

void perform_dowsync();
void syncTime();

int main(int argc, char *argv[]) {
    int exitval = 0;
    Config config_mgr;
    char config_key[] = "homesync.*";
    config_mgr.readConfigManager(config_key);
    if (fileExists(SyncConfigFile)) {
        Assoc config = readAssoc(SyncConfigFile, " ");
        servers = config["servers"];
        username = config["username"];
        password = config["password"];
        basedir = config["basedir"];
        partition = config["partition"];
        synctype = config["synctype"];
        string cfg_type = config_mgr.getString("homesync.Type");
        string cfg_basedir = config_mgr.getString("homesync.Dir");
        string cfg_partition = config_mgr.getString("homesync.Partition");
        if (cfg_type != "") {
            synctype = cfg_type;
        }
        if (cfg_basedir != "") {
            basedir = cfg_basedir;
        }
        if (cfg_partition != "") {
            partition = cfg_partition;
        }
        if ( (servers != "") and
             (username != "") and
             (password != "") and
             (basedir != "") and
             (partition != "") and
             (synctype != "") ) {
            if (synctype == "dowsync") {
                perform_dowsync();
            }
            else if (synctype == "sleep") {
                updateFile(SyncStatusFile,"Dummy sleep sync, occurred");
                sleep(300);
            }
            else {
                updateFile(SyncStatusFile,"Unknown sync type");
            }
        }
    }
    else {
        updateFile(SyncStatusFile,"No sync config file");
    }
    return exitval;
}

void perform_dowsync() {
    syncTime();
    string PasswdFile = "/tmp/rsync.pswd";
    string unitid = getStringFromFile(ProcHostname);
    writeStringToFile(PasswdFile, password);
    chmod(PasswdFile.c_str(), S_IRUSR|S_IWUSR);
    Strings serverlist = split(servers, ",");
    bool success = false;
    int status = 0;
    for (int i=0; i < serverlist.size(); i++) {
        string server = serverlist[i];
        string rsync_cmnd = "rsync -v -r --remove-source-files --password-file="+PasswdFile+" ";
        rsync_cmnd += basedir+"/";
        rsync_cmnd += " rsync://"+username+"@"+server+"/root/"+unitid+"/ >"+RsyncLogFile+" 2>&1";
        writeStringToFile("/tmp/rsync.cmnd", rsync_cmnd);
        status = str_system(rsync_cmnd);
        if (status == 0) {
            success = true;
            break;
        }
    }
    if (success) {
        updateFile(SyncStatusFile,"Sync successful");
    }
    else {
        int exitval = WEXITSTATUS(status);
        if (exitval == 5) {
            string errorline = grepFile(RsyncLogFile, "@ERROR");
            if (errorline.find("auth failed") != string::npos) {
                updateFile(SyncStatusFile, "rsync: Invalid username/password");
            }
            else {
                updateFile(SyncStatusFile, "rsync error code 5: "+errorline);
            }
        }
        else if (exitval == 10) {
            string errorline = grepFile(RsyncLogFile, "rsync:");
            if (errorline.find("No route to host") != string::npos) {
                updateFile(SyncStatusFile, "rsync: Server not reachable");
            }
            else if (errorline.find("Connection refused") != string::npos) {
                updateFile(SyncStatusFile, "rsync: rsyncd is not running on server");
            }
            else if (errorline.find("Name or service not know") != string::npos) {
                updateFile(SyncStatusFile, "rsync: servername not found");
            }
            else {
                updateFile(SyncStatusFile, errorline);
            }
        }
        else if (exitval == 12) {
            string errorline = grepFile(RsyncLogFile, "rsync:");
            if (errorline.find("Permission denied") != string::npos) {
                updateFile(SyncStatusFile, "rsync: Incorrect permissions on destination directory");
            }
            else {
                updateFile(SyncStatusFile, errorline);
            }
        }
        else {
            string errorline = grepFile(RsyncLogFile, "rsync error:");
            int pos = errorline.find(")");
            if (pos != string::npos) {
                errorline = errorline.substr(pos+1);
            }
            updateFile(SyncStatusFile, errorline);
        }
    }
    unlink(PasswdFile.c_str());
}

void syncTime() {
    if (!fileExists(GpsTimeFile)) {
        string celltime = backtick("cell_shell gettime");
        Strings vals = split(celltime, "\n");
        if (vals.size() > 1) {
            celltime = vals[1];
        }
        int year = 0;
        sscanf(celltime.c_str(), "%d", &year);
        if (year < 2002) {
            system("ntpdate 0.pool.ntp.org");
        }
    }
}

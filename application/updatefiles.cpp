/************************************************************************************************
 * File: updatefiles.cpp
 * Routine to update files from the update server
 ************************************************************************************************
 * 2015/02/06 rwp
 *    Added WhitelistFile to list of automatic files to update
 * 2015/05/04 rwp
 *    Use servername if serverip not found in SeverKeysFile
 * 2019/03/07 cjh
 *    Use BuildVarsFile to build a more detailed path for wget/curl calls.
 *    Adding getStringFromFile greatly enlarged the binary size.
 ************************************************************************************************
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "backtick.h"
#include "fileExists.h"
#include "fileStats.h"
#include "getopts.h"
#include "grepFile.h"
#include "readconfig.h"
#include "split.h"
#include "ssl_keys.h"
#include "string_printf.h"
#include "writeBinaryFile.h"
#include "getStringFromFile.h"
#include "readBuildVars.h"
using namespace std;

#define DefaultServers "update.autonetmobile.net"

const char *default_files[] = {
    ServerKeysFile,
    WhitelistFile,
    MediaVersionsFile
};

int getAndVerify(string serverip, string file, string rate, bool inplace, bool cont);
int getWithoutVerify(string serverip, string file, string rate, bool inplace, bool cont);
int getFile(string serverip, string file, string rate, bool cont);
int getFile(string serverip, string basefile, string destfile, string rate, bool cont);
bool beginsWith(string str, const char *patt);
long stringToLong(string str);

bool debug = false;
string temp_filetimes_file = "";
string temp_filetimes_sig_file = "";
string machine = "";
string distro = "";

int main(int argc, char **argp) {
    bool noverify = false;
    string rate_flag = "";
    Strings servers;
    GetOpts opts(argc, argp, "cdinp:r:");
    debug = opts.exists("d");
    noverify = opts.exists("n");
    bool continue_download = opts.exists("c");
    bool inplace = opts.exists("i");
    string rate = "";
    if (opts.exists("r")) {
        rate = opts.get("r");
    }
    string prlfile = opts.get("p");
    Strings filelist;
    pid_t pid = getpid();
    if (argc > 1) {
        for (int i=1; i < argc; i++) {
            filelist.push_back(argp[i]);
        }
    }
    else {
        for (int i=0; i < sizeof(default_files)/sizeof(char *); i++) {
            filelist.push_back(default_files[i]);
        }
//        string mdn = grepFile(RwCellInfoFile, "MDN");
//        if (mdn != "") {
//            string prlfile = WholesalePrlFile;
//            if (mdn.find("415") != string::npos) {
//                prlfile = RetailPrlFile;
//            }
        if (prlfile != "") {
            filelist.push_back(prlfile);
        }
    }

    // Get machine specs
    string bvFile = BuildVarsFile;
    bvFile.append(getStringFromFile(KernelIdFile));
    Assoc bldvars = readBuildVars(bvFile);
    if (bldvars.find("machine") != bldvars.end()) {
        machine = bldvars["machine"];
    } else {
        exit(1);
    }
    if (bldvars.find("Distro") != bldvars.end()) {
        distro = bldvars["Distro"];
    } else {
        exit(1);
    }


    // Get server addresses
    Config config(ConfigFile);
    string servers_str = config.getString("unit.update-servers");
    if (servers_str == "") {
        servers_str = DefaultServers;
    }
    int exitstat = 0;
    servers = split(servers_str, " ");
    for (int svr_idx=0; svr_idx < servers.size(); svr_idx++) {
        string server = servers[svr_idx];
        string ipstr = backtick("resolvedns "+server);
        if (ipstr == "") {
            exitstat = 2;
            continue;
        }
        Strings ips = split(ipstr, " ");
        for (int ip_idx = 0; ip_idx < ips.size(); ip_idx++) {
            string serverip = ips[ip_idx];
            if (noverify) {
                for (int f_idx=0; f_idx < filelist.size(); f_idx++) {
                    string file = filelist[f_idx];
                    exitstat = getWithoutVerify(serverip, file, rate, inplace, continue_download);
                    if (exitstat == 2) {
                        break;
                    }
                }
            }
            else {
                temp_filetimes_file = string_printf("%s.%d", FiletimesFile, pid);
                temp_filetimes_sig_file = string_printf("%s.%d.sig", FiletimesFile, pid);
                string basefile = FiletimesFile;
                basefile = basefile.substr(basefile.find_last_of('/')+1);
                exitstat = getFile(serverip, basefile, temp_filetimes_file, rate, false);
                string basefile_sig = FiletimesFile;
                basefile = basefile.substr(basefile_sig.find_last_of('/')+1);
                exitstat = getFile(serverip, basefile_sig, temp_filetimes_sig_file, rate, false);
                //verify_file
                if (!createServerKeyFile(serverip)) {
                    if (!createServerKeyFile(server)) {
                        continue;
                    }
                }
                for (int f_idx=0; f_idx < filelist.size(); f_idx++) {
                    string file = filelist[f_idx];
                    exitstat = getAndVerify(serverip, file, rate, inplace, continue_download);
                    if (exitstat == 2) {
                        break;
                    }
                }
                string keyfile = "/tmp/" + serverip + ".pubkey";
                unlink(keyfile.c_str());
                break;
            }
        }
    }
    if ((temp_filetimes_file != "") and fileExists(temp_filetimes_file)) {
        unlink(temp_filetimes_file.c_str());
    }
    if ((temp_filetimes_sig_file != "") and fileExists(temp_filetimes_sig_file)) {
        unlink(temp_filetimes_sig_file.c_str());
    }
}

int getAndVerify(string serverip, string file, string rate, bool inplace, bool cont) {
    int exitstat = 0;
    string serverkeyfile = "/tmp/" + serverip + ".pubkey";
    string basefile = file.substr(file.find_last_of('/')+1);
    string fileline = grepFile(temp_filetimes_file, basefile.c_str());
    if (fileline == "") {
        cerr << "No verification informaton for file: " << file << endl;
        exitstat = 1;
    }
    else {
        Strings vals = split(fileline, " ");
        string ftime = vals[1];
        string sig = vals[2];
        bool get = true;
        if (!cont and fileExists(file)) {
            if (fileTime(file) >= stringToLong(ftime)) {
                get = false;
                cerr << "Local file " << file << " is newer then on server" << endl;
            }
        }
        if (get) {
            string destfile = file;
            if (!inplace) {
                destfile = "/tmp/" + basefile;
            }
            exitstat = getFile(serverip, destfile, rate, cont);
            if (exitstat == 0) {
                string sigfile = "/tmp/" + basefile + ".sig";
                string binsig = "";
                for (int i=0; i < sig.size(); i+=2) {
                    unsigned int val;
                    sscanf(sig.substr(i, 2).c_str(), "%2X", &val);
                    unsigned char ch = val;
                    binsig.append(1, ch);
                }
                writeBinaryFile(sigfile, binsig);
                string cmnd = "openssl dgst -sha1";
                cmnd += " -verify " + serverkeyfile;
                cmnd += " -signature " + sigfile;
                cmnd += " " + destfile;
                cmnd += " >/dev/null 2>&1";
                if (system(cmnd.c_str()) == 0) {
//                    cerr << "File verified" << endl;
                    if (!inplace) {
                        string mvcmnd1 = "mv -f " + destfile;
                        mvcmnd1 += " " + file + ".tmp";
                        string mvcmnd2 = "mv -f " + file + ".tmp";
                        mvcmnd2 += " " + file;
                        if (beginsWith(file, "/") and !beginsWith(file, "/autonet/")) {
                            system("remountrw");
                            system(mvcmnd1.c_str());
                            system(mvcmnd2.c_str());
                            system("remountro");
                        }
                        else {
                            system(mvcmnd1.c_str());
                            system(mvcmnd2.c_str());
                        }
                    }
                }
                else {
                    cerr << "File " << file << " not verified" << endl;
                    unlink(destfile.c_str());
                }
                unlink(sigfile.c_str());
            }
        }
    }
    return exitstat;
}

int getWithoutVerify(string serverip, string file, string rate, bool inplace, bool cont) {
    int exitstat = 0;
    string basefile = file.substr(file.find_last_of('/')+1);
    string destfile = file;
    if (!inplace) {
        destfile = "/tmp/" + basefile;
    }
    exitstat = getFile(serverip, destfile, rate, cont);
    if (exitstat == 0) {
        if (!inplace) {
            string mvcmnd1 = "mv -f " + destfile;
            mvcmnd1 += " " + file + ".tmp";
            string mvcmnd2 = "mv -f " + file + ".tmp";
            mvcmnd2 += " " + file;
            if (beginsWith(file, "/") and !beginsWith(file, "/autonet/")) {
                system("remountrw");
                system(mvcmnd1.c_str());
                system(mvcmnd2.c_str());
                system("remountro");
            }
            else {
                system(mvcmnd1.c_str());
                system(mvcmnd2.c_str());
            }
        }
    }
    return exitstat;
}

int getFile(string serverip, string file, string rate, bool cont) {
    string basefile = file.substr(file.find_last_of('/')+1);
    int retval = getFile(serverip, basefile, file, rate, cont);
    return retval;
}

int getFile(string serverip, string basefile, string destfile, string rate, bool cont) {
#ifdef WGET
    string rate_flag = "";
    if (rate != "") {
        rate_flag = "--limit-rate="+rate+" ";
    }
    string cont_flag = "";
    if (cont) {
        cont_flag = "-c ";
    }
    string cmnd = "wget -q --timeout=180 ";
    cmnd += rate_flag;
    cmnd += cont_flag;
    cmnd += "-O " + destfile;
    cmnd += " http://"+serverip+":5080/updates/"+machine+"/"+distro+"/"+basefile;
#else
    string rate_flag = "";
    if (rate != "") {
        rate_flag = "--limit-rate "+rate+" ";
    }
    string cont_flag = "";
    if (cont) {
        cont_flag = "-C - ";
    }
    string cmnd = "curl -s --connect-timeout 180 -R ";
    cmnd += rate_flag;
    cmnd += cont_flag;
    cmnd += "-o " + destfile;
    cmnd += " http://"+serverip+":5080/updates/"+machine+"/"+distro+"/"+basefile;
#endif
    time_t starttime= time(NULL);
    int stat = system(cmnd.c_str());
    if (stat != 0) {
        if ((time(NULL)-starttime) >= 180) {
            stat = 2;
        }
    }
    return stat;
}

bool beginsWith(string str, const char *patt) {
    bool match = false;
    if (strncmp(str.c_str(), patt, strlen(patt)) == 0) {
        match = true;
    }
    return match;
}

long stringToLong(string str) {
    long val = 0;
    sscanf(str.c_str(), "%ld", &val);
    return val;
}

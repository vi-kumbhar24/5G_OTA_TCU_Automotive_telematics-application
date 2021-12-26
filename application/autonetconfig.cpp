/******************************************************************************
 * File: autonet_config.cpp
 * Routine to config the wifi
 ******************************************************************************
 * 2013/03/22 rwp
 *    Change modprobe to set devmode
" >/dev/null 2>&1"); * 2014/04/15 rwp
 *    Redirect stdout on commands to /dev/null
 * 2014/05/26 rwp
 *    Changes to not get running config (fails for wilink8)
 *    Changed to always use hostapd (only works for wilink8)
 * 2014/07/31 rwp
 *    AK-6: kill and restart hostapd for kanaan
 * 2014/10/30 rwp
 *    AK-39: Dont allow WEP keys of 16 ascii or 32 hex characters
 * 2016/06/01 rwp
 *    Strip trailing space from ESSID so we can force autonet-xxxx
 ******************************************************************************
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "getopts.h"
#include "autonet_files.h"
#include "autonet_types.h"
#include "split.h"
#include "getParm.h"
#include "backtick.h"
#include "fileExists.h"
#include "str_system.h"
using namespace std;

typedef list<string> StringList;

Assoc config;
Assoc running_config;
Assoc options;
Assoc formats;
bool debug;
bool kanaan = true;

void initFormats();
void printConfig();
bool readConfig(const char *file);
void updateConfFile();
void getOptions();
void setRunningConfig();
void getRunningConfig();
bool performConfig(int numargs, char **args);
bool checkKeys();
void performWifiConfig();
string makeHexKey();
void performLoginConfig();
void performRadioMacsConfig();
void updateInterfacesFile();
void updateHostapdFile();
bool checkFormat(string val, string fmt);

int main(int argc, char **argp) {
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    umask(S_IWGRP | S_IWOTH);
    GetOpts opts(argc, argp, "ds");
    initFormats();
    readConfig(WifiConfigFile);
    getOptions();
    setRunningConfig();
//    getRunningConfig();
    if (options["radio"].find(config["radio"]) == string::npos) {
        config["radio"] = running_config["radio"];
    }
    debug = opts.exists("d");
    if (opts.exists("s")) {
//        performWifiConfig();
        performLoginConfig();
        performRadioMacsConfig();
    }
    else if (argc > 1) {
        performConfig(argc-1, argp+1);
    }
    else {
        printConfig();
    }
}

void initFormats() {
    formats["enc"] = "asc";
    formats["enc_key"] = "asc";
    formats["essid"] = "asc";
    formats["frag"] = "dec";
    formats["login_page"] = "date";
    formats["macs"] = "macaddr";
    formats["nologin_macs"] = "macaddr";
    formats["rts"] = "dec";
}

void printConfig() {
    cout << "radio [" << options["radio"] << "]" << endl;
    cout << "radio " << config["radio"] << endl;
//    cout << "running radio " << running_config["radio"] << endl;
    cout << "essid [" << options["essid"] << "]" << endl;
    cout << "essid \"" << config["essid"] << "\"" << endl;
//    cout << "running essid " << running_config["essid"] << endl;
    cout << "channel [" << options["channel"] << "]" << endl;
    cout << "channel " << config["channel"] << endl;
//    cout << "running channel " << running_config["channel"] << endl;
    cout << "enc_type [" << options["enc_type"] << "]" << endl;
    cout << "enc_type " << config["enc_type"] << endl;
//    cout << "running enc_type " << running_config["enc_type"] << endl;
    cout << "enc_key [" << options["enc_key"] << "]" << endl;
    string enc_key = config["enc_key"];
    if (enc_key != "") {
        cout << "enc_key \"" << enc_key << "\"" << endl;
    }
//    cout << "running enc_key " << running_config["enc_key"] << endl;
    cout << "txpower [" << options["txpower"] << "]" << endl;
    cout << "txpower " << config["txpower"] << endl;
    cout << "mac_filter [" << options["mac_filter"] << "]" << endl;
    cout << "mac_filter " << config["mac_filter"] << endl;
    cout << "macs [" << options["macs"] << "]" << endl;
    string macs = config["macs"];
    if (macs != "") {
        cout << "macs " << macs << endl;
    }
    cout << "login_page [" << options["login_page"] << "]" << endl;
    cout << "login_page " << config["login_page"] << endl;
    cout << "nologin_macs [" << options["nologin_macs"] << "]" << endl;
    string nologin_macs = config["nologin_macs"];
    if (nologin_macs != "") {
        cout << "nologin_macs " << nologin_macs << endl;
    }
}

bool readConfig(const char *file) {
    bool ok = false;
    ifstream infile;
    infile.open(file);
    if (infile.is_open()) {
        while (!infile.eof()) {
            string ln;
            getline(infile, ln);
            if (!infile.eof()) {
                if (ln.size() == 0) {
                    continue;
                }
                while ((ln.size() > 0) and (ln.substr(0) == " ")) {
                    ln = ln.substr(1);
                }
                if (ln.size() == 0) {
                    continue;
                }
                if (strncmp(ln.c_str(), "#", 1) == 0) {
                    continue;
                }
                while ((ln.size() > 0) and (ln.substr(ln.size()-1) == " ")) {
                    ln = ln.substr(0, ln.size()-1);
                }
                int pos;
                if ((pos=ln.find(" ")) != string::npos) {
                    string var = ln.substr(0, pos);
                    string val = ln.substr(pos+1);
                    while (val.substr(0,1) == " ") {
                        val = val.substr(1);
                    }
                    if ( (val.at(0) == '"') and
                         (val.at(val.size()-1) == '"') ) {
                        val = val.substr(1, val.size()-2);
                    }
                    config[var] = val;
//                    cout << var << " = " << val << endl;
                }
            }
        }
        ok = true;
    }
    return ok;
}

void updateConfFile() {
    string tmpfile = WifiConfigFile;
    tmpfile += ".tmp";
    StringList vars;
    Assoc::iterator it;
    for (it=config.begin(); it != config.end(); it++) {
         string var = (*it).first;
         vars.push_back(var);
    }
    vars.sort();
//    system("remountrw");
    ofstream outf(tmpfile.c_str());
    if (outf.is_open()) {
        StringList::iterator v_it;
        for (v_it=vars.begin(); v_it != vars.end(); v_it++) {
            string var = *v_it;
            string val = config[var];
            if (var != "") {
                if ( (var == "essid") or
                     (var == "enc_key") ) {
                    val = "\"" + val + "\"";
                }
                outf << var << " " << val << endl;
            }
        }
        outf.close();
        rename(tmpfile.c_str(), WifiConfigFile);
    }
//    system("remountro");
}

void getOptions() {
    options["radio"] = "802.11bgn";
    options["essid"] = "<essid>";
    options["enc_type"] = "off wep wpa wpa2";
    options["enc_key"] = "<ascii_string>|<hex_string";
    options["channel"] = "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16";
    options["txpower"] = "0 4 8 12 15 17";
    options["mac_filter"] = "off on";
    options["macs"] = "<macaddr,...>";
    options["login_page"] = "on off <date>";
    options["nologin_macs"] = "<macaddr,...>";
    options["frag"] = "off";
    options["rts"] = "off";
    options["rate"] = "auto";
    options["retry"] = "off";
}

void setRunningConfig() {
    running_config["radio"] = config["radio"];
    running_config["essid"] = config["essid"];
    running_config["enc_type"] = config["enc_type"];
    running_config["enc_key"] = config["enc_key"];
    running_config["channel"] = config["channel"];
    running_config["txpower"] = config["txpower"];
    running_config["frag"] = config["frag"];
    running_config["rts"] = config["rts"];
    running_config["rate"] = config["rate"];
    running_config["retry"] = config["retry"];
}

void getRunningConfig() {
    string iwconfig = backtick("iwconfig wlan0 2>/dev/null");
//rcg
    if (iwconfig == "") {
       return;
    }
    string radio = getParm(iwconfig, "AR6000 ");
    options["radio"] = "802.11bgn";
    running_config["radio"] = "802.11bgn";
    string essid = getParm(iwconfig, "ESSID:");
    if ( (essid != "") and
         (essid.at(0) == '"') and
         (essid.at(essid.size()-1) == '"') ) {
        essid = essid.substr(1, essid.size()-2);
    }
    options["essid"] = "<essid>";
    running_config["essid"] = essid;
    string enc_key = getParm(iwconfig, "Encryption key:");
    if (enc_key == "off") {
        running_config["enc_type"] = "off";
        running_config["enc_key"] = "";
    }
    else {
        running_config["enc_type"] = "wep";
        running_config["enc_key"] = enc_key;
    }
//    options["enc_type"] = "off wep wpa wpa/wpa2 wpa2";
    options["enc_type"] = "off wep wpa wpa2";
    options["enc_key"] = "<ascii_string>|<hex_string";
    string channels = backtick("iwlist wlan0 channel 2>/dev/null");
    string opt_channels;
    Strings ch_lines = split(channels, "\n");
    for (int i=0; i < ch_lines.size(); i++) {
        string ln = ch_lines[i];
        string ch = getParm(ln, " Channel ");
        if (ch != "") {
            while (ch.substr(0,1) == "0") {
                ch = ch.substr(1);
            }
            if (opt_channels != "") {
                opt_channels += " ";
            }
            opt_channels += ch;
        }
    }
    string channel = getParm(channels, "(Channel ", ")");
    options["channel"] = opt_channels;
    running_config["channel"] = channel;
    string txpower = getParm(iwconfig, "Tx-Power=");
    options["txpower"] = "0 4 8 12 15 17";
    running_config["txpower"] = txpower;
    options["mac_filter"] = "off on";
    options["macs"] = "<macaddr,...>";
    options["login_page"] = "on off <date>";
    options["nologin_macs"] = "<macaddr,...>";
    options["frag"] = "off";
    running_config["frag"] = "off";
    options["rts"] = "off";
    running_config["rts"] = "off";
    options["rate"] = "auto";
    running_config["rate"] = "auto";
    options["retry"] = "off";
    running_config["retry"] = "off";
}

bool performConfig(int numargs, char **args) {
    bool ok = true;
    bool radio_changed = false;
    bool login_changed = false;
    bool radio_macs_changed = false;
    bool config_changed = false;
    for (int i=0; i < numargs; i++) {
        string parm = args[i];
        if (options.find(parm) == options.end()) {
            cerr << "Error: Unknown parameter " << parm << endl;
        }
        string opt_list = options[parm];
        i++;
        if (i >= numargs) {
            cerr << "Error: " << parm << " requires an argument" << endl;
            ok = false;
            break;
        }
        string val = args[i];
        Strings val_list = split(opt_list, " ");
        bool found = false;
        bool entered_val = false;
        for (int j=0; j < val_list.size(); j++) {
            if (val_list[j].substr(0,1) == "<") {
                entered_val = true;
            }
            else if (val == val_list[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (entered_val) {
                string fmt = formats[parm];
                if (fmt == "") {
                    cerr << "Error: No format found for " << parm << endl;
                    ok = false;
                    break;
                }
                bool multi = false;
                if ( (parm == "macs") or
                     (parm == "nologin_macs")) {
                    multi = true;
                }
                if (multi) {
                    Strings vals = split(val, ",");
                    for (int j=0; j < vals.size(); j++) {
                        if (!checkFormat(vals[j], fmt)) {
                            ok = false;
                            break;
                        }
                    }
                    if (!ok) {
                        cerr << "Error: Invalid argument '" << val << "' for " << parm << endl;
                    }
                }
                else {
                    if (!checkFormat(val, fmt)) {
                        cerr << "Error: Invalid argument '" << val << "' for " << parm << endl;
                        ok = false;
                        break;
                    }
                }
            }
            else {
                cerr << "Error: Invalid argument '" << val << "' for " << parm << endl;
                ok = false;
                break;
            }
        }
        if (config[parm] != val) {
            config_changed = true;
        }
        config[parm] = val;
        if ( (parm == "radio") or
             (parm == "essid") or
             (parm == "channel") or
             (parm == "enc_type") or
             (parm == "txpower") or
             (parm == "enc_key") or
             (parm == "rts") or
             (parm == "frag") or
             (parm == "rate") or
             (parm == "retry") ) {
            radio_changed = true;
        }
        if ( (parm == "login_page") or
             (parm == "nologin_macs") ) {
            login_changed = true;
        }
        if ( (parm == "mac_filter") or
             (parm == "macs") ) {
            radio_macs_changed = true;
        }
    }
    if (ok) {
        ok = checkKeys();
    }
    if (ok) {
        if (config_changed) {
            updateConfFile();
            if (radio_changed) {
//kanaan                updateInterfacesFile();
                updateHostapdFile();
            }
            system("backup_autonet");
        }
        if (radio_changed) {
            performWifiConfig();
        }
        if (login_changed) {
            performLoginConfig();
        }
        if (radio_macs_changed) {
            performRadioMacsConfig();
        }
    }
    return ok;
}

bool checkKeys() {
    bool ok = false;
    string type = config["enc_type"];
    string key = config["enc_key"];
    int keylen = key.size();
    if (type == "off") {
        ok = true;
    }
    else if (type == "wep") {
//kanaan change
//        if ((keylen == 5) or (keylen == 13) or (keylen == 16)) {
        if ((keylen == 5) or (keylen == 13)) {
           if (key.find_first_of(" \t\n\r") == string::npos) {
               ok = true;
           }
        }
//kanaan change
//        else if ((keylen == 10) or (keylen == 26) or (keylen == 32)) {
        else if ((keylen == 10) or (keylen == 26)) {
           if (key.find_first_not_of("0123456789abcdefABCDEF") == string::npos) {
               ok = true;
           }
        }
    }
    else {
       if (keylen == 64) {
           if (key.find_first_not_of("0123456789abcdefABCDEF") == string::npos) {
               ok = true;
           }
       }
       else if ((keylen >= 8) and (keylen <= 63)) {
           if (key.find_first_of(" \t\n\r") == string::npos) {
               ok = true;
           }
       }
    }
    if (!ok) {
        cerr << "Invalid key value" << endl;
    }
    return ok;
}

void performWifiConfig() {
    if (kanaan) {
        system("pkill hostapd 2>/dev/null");
        sleep(1);
        str_system((string)"hostapd -B "+HostapdConfFile+" >/dev/null 2>&1");
    }
    else {
        system("pkill hostapd 2>/dev/null");
        system("ifdown wlan0");
        system("rmmod ar6000");
        sleep(1);
        string modname = "ar6103 devmode=ap,sta";
        if (fileExists(Ar6102File)) {
            modname = "ar6102";
        }
        str_system("modprobe "+modname);
        sleep(1);
        system("ifup wlan0");
        string type = config["enc_type"];
        if ( (type == "off") or (type == "wep") ) {
//        string iwconfig_cmd = "iwconfig wlan0";
//        string essid = config["essid"];
//        for (int i=0; i < essid.size(); i++) {
//            if (essid.at(i) == '\'') {
//                essid.replace(i, 1, "'\"'\"'");
//                i += 4;
//            }
//        }
//        iwconfig_cmd += " mode master";
//        iwconfig_cmd += " essid '" + essid + "'";
//        iwconfig_cmd += " channel " + config["channel"];
//        string enc = config["enc_type"];
//        if (strncmp(enc.c_str(), "wpa", 3) == 0) {
//            enc = "off";
//        }
//        else if (enc != "off") {
//            enc = makeHexKey();
//        }
//        iwconfig_cmd += " enc " + enc;
//        if (debug) cout << iwconfig_cmd << endl;
//        int exitstat = system(iwconfig_cmd.c_str());
//        if (exitstat != 0) {
//            cerr << "iwconfig failed" << endl;
//        }
//        system("iwconfig wlan0 commit");
        }
        else {
            str_system((string)"hostapd -B "+HostapdConfFile+" >/dev/null 2>&1");
        }
        str_system("iwconfig wlan0 txpower " + config["txpower"]);
    }
    /* CONLAREINS-702 Killing all instances of dnsmasq disables LXC networking.
       If it is really necessary to kill dnsmasq, use the PID from /run/dnsmasq.pid
       file to kill the right one. The LXC-invoked instance keeps its pid file elsewhere.
    */
    system("pkill -HUP dnsmasq");
}

string makeHexKey() {
    string enc = config["enc_key"];
    int enclen = enc.size();
    if ( (enclen == 5) or
         (enclen == 13) or
         (enclen == 16) ) {
        string enchex = "";
        for (int i=0; i < enclen; i++) {
            char hexbuf[3];
            unsigned char ch = enc.at(i);
            sprintf(hexbuf, "%02x", ch);
            enchex += hexbuf;
        }
        enc = enchex;
    }
    return enc;
}

void performLoginConfig() {
    system("autonet_router");
}

void performRadioMacsConfig() {
}

void updateInterfacesFile() {
    Strings iwcommands;
    string type = config["enc_type"];
    if ( (type == "off") or (type == "wep") ) {
        string cmd;
        cmd = "mode master";
        iwcommands.push_back(cmd);
        string essid = config["essid"];
        if (essid.substr(essid.size()-1) == " ") {
            essid.erase(essid.size()-1, 1);
        }
        for (int i=0; i < essid.size(); i++) {
            if (essid.at(i) == '\'') {
                essid.replace(i, 1, "'\"'\"'");
                i += 4;
            }
        }
        cmd = "essid '" + essid + "'";
        iwcommands.push_back(cmd);
        cmd = "channel " + config["channel"];
        iwcommands.push_back(cmd);
        string enc = config["enc_type"];
        if (enc == "wep") {
            enc = makeHexKey();
        }
        cmd = "enc " + enc;
        iwcommands.push_back(cmd);
        cmd = "commit";
        iwcommands.push_back(cmd);
    }
//    system("remountrw");
    ifstream inf(NetworkInterfacesFile);
    if (inf.is_open()) {
        string tmpfile = NetworkInterfacesFile;
        tmpfile += ".tmp";
        ofstream outf(tmpfile.c_str());
        if (outf.is_open()) {
            bool in_interface = false;
            bool stripping = false;
            while (!inf.eof()) {
                string ln;
                getline(inf, ln);
                if (!inf.eof()) {
                    if (!in_interface) {
                        outf << ln << endl;
                        if ( (strncmp(ln.c_str(), "iface ", 5) == 0) and
                             (ln.find("wlan0") != string::npos) ) {
                            in_interface = true;
                        }
                    }
                    else {
                        if (!stripping) {
                            outf << ln << endl;
                        }
                        if (ln == "#WiFiAdmin start") {
                            stripping = true;
                        }
                        else if (ln == "#WiFiAdmin end") {
                            for (int i=0; i < iwcommands.size(); i++) {
                                string cmnd = "\tpre-up iwconfig wlan0 ";
                                cmnd += iwcommands[i];
                                outf << cmnd << endl;
                            }
                            outf << ln << endl;
                            in_interface = false;
                            stripping = false;
                        }
                    }
                }
            }
            outf.close();
            rename(tmpfile.c_str(), NetworkInterfacesFile);
        }
        inf.close();
    }
    updateHostapdFile();
//    system("remountro");
}

void updateHostapdFile() {
    string enc_type = config["enc_type"];
//    if ( (enc_type == "off") or (enc_type == "wep") ) {
//        unlink(HostapdConfFile);
//    }
//    else {
        string tmpfile = HostapdConfFile;
        tmpfile += ".tmp";
        ofstream outf(tmpfile.c_str());
        if (outf.is_open()) {
            if (kanaan) {
//                outf << "driver=wl18xx" << endl;
            }
            else {
                outf << "driver=ar6000" << endl;
            }
            outf << "interface=wlan0" << endl;
            string hw_mode = config["radio"];
            hw_mode = hw_mode.substr(6);
            if (hw_mode == "bgn") {
                hw_mode = "g";
            }
            outf << "hw_mode=" << hw_mode << endl;
//            outf << "channel_num=" << config["channel"] << endl;
            outf << "channel=" << config["channel"] << endl;
            string essid = config["essid"];
            if (essid.substr(essid.size()-1) == " ") {
                essid.erase(essid.size()-1, 1);
            }
            outf << "ssid=" << essid << endl;
            outf << "auth_algs=3" << endl;
            if (enc_type == "off") {
            }
            else if (enc_type == "wep") {
                outf << "# WEP Configuration" << endl;
                outf << "wep_default_key=0" << endl;
                string key = config["enc_key"];
                int len = key.length();
                if ((len == 5) or (len == 13) or (len == 16)) {
                    outf << "wep_key0=\"" << key << "\"" << endl;
                }
                else {
                    outf << "wep_key0=" << key << endl;
                }
            }
            else {
                outf << "# WPA Configuration" << endl;
//                outf << "ieee8021x=0" << endl;
//                outf << "eapol_version=1" << endl;
                outf << "wpa_key_mgmt=WPA-PSK" << endl;
//                outf << "wpa_group_rekey=600" << endl;
//                outf << "wpa_gmk_rekey=86400" << endl;
                if (enc_type == "wpa2") {
                    outf << "wpa=2" << endl;
                    outf << "wpa_pairwise=CCMP" << endl;
                }
                else if (enc_type == "wpa") {
                    outf << "wpa=1" << endl;
                    outf << "wpa_pairwise=TKIP" << endl;
                }
                else {
                    outf << "wpa=3" << endl;
                    outf << "wpa_pairwise=TKIP CCMP" << endl;
                }
                string key = config["enc_key"];
                bool hexkey = false;
                if (key.size() == 64) {
                    hexkey = true;
                    for (int i=0; i < key.size(); i++) {
                        if (!isxdigit(key.at(i))) {
                            hexkey = false;
                            break;
                        }
                    }
                }
                if (hexkey) {
                    outf << "wpa_psk=" << key << endl;
                }
                else {
                    outf << "wpa_passphrase=" << key << endl;
                }
            }
            outf.close();
            rename(tmpfile.c_str(), HostapdConfFile);
        }
//    }
}

bool checkFormat(string val, string fmt) {
    bool ok = false;
    if (fmt == "dec") {
        ok = true;
        for (int i=0; i < val.size(); i++) {
            char ch = val.at(i);
            if (!isdigit(ch)) {
                ok = false;
                break;
            }
        }
    }
    else if (fmt == "asc") {
        ok = true;
        for (int i=0; i < val.size(); i++) {
            char ch = val.at(i);
            if (!isprint(ch)) {
                ok = false;
                break;
            }
        }
    }
    else if (fmt == "macaddr") {
        for (int i=0; i < val.size(); i++) {
            if ( (val.at(i) == ':') or
                 (val.at(i) == '-') ) {
                val.replace(i,1,"");
                i--;
            }
        }
        if (val.size() == 12) {
            ok = true;
            for (int i=0; i < val.size(); i++) {
                char ch = val.at(i);
                if (!isxdigit(ch)) {
                    ok = false;
                    break;
                }
            }
        }
    }
    else if (fmt == "date") {
        int year;
        int mon;
        int day;
        int cnt = sscanf(val.c_str(), "%d/%d/%d", &day, &mon, &year);
        if (cnt == 3) {
            ok = true;
        }
    }
    return ok;
}

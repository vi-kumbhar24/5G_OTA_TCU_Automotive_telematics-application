/******************************************************************************
 * File: config_aplist.cpp
 *
 * Routine to configure the list of APs that we will attempt to connect to
 ******************************************************************************
 * Change log:
 * 2012/10/12 rwp
 *    Initial version
 * 2013/02/06 rwp
 *    Fixed add_entries for segfault
 ******************************************************************************
 */

#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <string>
#include <vector>
#include <unistd.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "backtick.h"
#include "base64.h"
#include "csv.h"
#include "fileExists.h"
#include "getopts.h"
#include "Md5.h"
#include "readFileArray.h"
#include "split.h"
#include "string_convert.h"
#include "str_system.h"
using namespace std;
//#define ApListFile "ap_list"
//#define CurrentApsFile "aps"

class ApEntry {
public:
    string essid;
    string md5_essid;
    string ip_info;
    string wifi_info;
    string enc_wifi;
    string enc_ip;

    ApEntry() {
        essid = "";
        md5_essid = "";
        ip_info = "";
        wifi_info = "";
        enc_wifi = "";
        enc_ip = "";
    }
};
vector<ApEntry> ap_list;

class MacEntry {
public:
    string mac;
    string md5_mac;
    Strings enc_entries;
    Strings dec_entries;

    MacEntry() {
        mac = "";
        md5_mac = "";
    }
};
vector<MacEntry> mac_list;

class EssidEntry {
public:
    string essid;
    string md5_essid;
    Strings enc_entries;
    Strings dec_entries;

    EssidEntry() {
        essid = "";
        md5_essid = "";
    }
};
vector<EssidEntry> essid_list;

string mac_address = "";
bool first_only = false;
string scan_file = "";

void usage();
void delete_ap(int idx);
void delete_macs();
void delete_mac(int idx);
void delete_entries(int idx);
void check_wifi_info(string wifi_info);
void write_ap_file();
void write_entries(ofstream &out, Strings enc_entries, string key);
void write_strings(ofstream &out, Strings strings);
void decrypt_aplist();
void decrypt_ap(int idx, string essid, string key);
void decrypt_essid(int idx, string essid, string key);
void decrypt_mac(int idx, string mac, string key);
Strings decrypt_entries(string key, Strings enc_entries);
void add_essid(string essid, string md5_essid);
void add_mac(string mac);
Strings blank_entries();
void add_entries(int idx);
string make_key(string secret);
string encrypt(string key, string str);
string decrypt(string key, string encstr);
string md5(string text);
string md5_base64(string text);

int main(int argc, char *argp[]) {
    int retval = 0;
    GetOpts opts(argc, argp, "1adf:m:");
    bool add = opts.exists("a");
    bool del = opts.exists("d");
    mac_address = opts.get("m");
    if (opts.exists("f")) {
        scan_file = opts.get("f");
    }
    first_only = opts.exists("1");
    if (add & del) {
        cerr << "Cannot do both adding and deleting" << endl;
        exit(1);
    }
    if (scan_file == "") {
        scan_file = CurrentApsFile;
        str_system("wifi_scan -i wlan1 -e >"+scan_file);
    }
    decrypt_aplist();
    if (add) {
        if (argc != 4) {
            usage();
            exit(1);
        }
        string essid = argp[1];
        string wifi_info = argp[2];
        string ip_info = argp[3];
        check_wifi_info(wifi_info);
        string md5_essid = md5_base64(essid);
        string key = make_key(essid);
        string enc_wifi = encrypt(key, wifi_info);
        string enc_ip = encrypt(key, ip_info);
        ApEntry ap_ent;
        ap_ent.essid = essid;
        ap_ent.md5_essid = md5_essid;
        ap_ent.wifi_info = wifi_info;
        ap_ent.enc_wifi = enc_wifi;
        ap_ent.ip_info = ip_info;
        ap_ent.enc_ip = enc_ip;
        bool found = false;
        for (int i=0; i < ap_list.size(); i++) {
            if (ap_list[i].essid == essid) {
                ap_list[i] = ap_ent;
                found = true;
                break;
            }
        }
        if (!found) {
            ap_list.push_back(ap_ent);
            add_essid(essid, md5_essid);
        }
        if (mac_address != "") {
            found = false;
            for (int i=0; i < mac_list.size(); i++) {
                if (mac_list[i].mac == mac_address) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                add_mac(mac_address);
            }
        }
        write_ap_file();
    }
    else if (del) {
        if (argc < 2) {
            usage();
            exit(1);
        }
        list<int> indexes;
        for (int i=1; i < argc; i++) {
            string arg = argp[i];
            if (arg.find_first_not_of("0123456789") != string::npos) {
                cerr << "Invalid parameter: " << arg << ", must be a number" << endl;
                exit(1);
            }
            int idx = stringToInt(arg);
            if (( idx == 0) or (idx > ap_list.size())) {
                cerr << "Invalid AP index: " << arg << endl;
                exit(1);
            }
            indexes.push_back(idx);
        }
        indexes.sort();
        indexes.unique();
        while (indexes.size() > 0) {
            int idx = indexes.back();
            indexes.pop_back();
            delete_ap(idx-1);
        }
        delete_macs();
        write_ap_file();
    }
    else {
//        cout << "essid_list:" << endl;
//        for (int i=0; i < essid_list.size(); i++) {
//             cout << i << ":" << essid_list[i].essid << endl;
//             cout << essid_list[i].md5_essid << endl;
//             cout << "enc_entries:";
//             for (int j=0; j < essid_list[i].enc_entries.size(); j++) {
//                 cout << " " << essid_list[i].enc_entries[j];
//             }
//             cout << endl;
//             cout << "dec_entries:";
//             for (int j=0; j < essid_list[i].dec_entries.size(); j++) {
//                 cout << " " << essid_list[i].dec_entries[j];
//             }
//             cout << endl;
//        }
        for (int i=0; i < ap_list.size(); i++) {
            string essid = ap_list[i].essid;
            if (essid != "") {
                string wifi_info = ap_list[i].wifi_info;
                string ip_info = ap_list[i].ip_info;
                if (first_only) {
                    cout <<  csvQuote(wifi_info) << " " << csvQuote(ip_info) << " " << csvQuote(essid) << endl;
                    break;
                }
                int pos = wifi_info.find(":");
                if (pos != string::npos) {
                    wifi_info = wifi_info.substr(0,pos+1);
                    wifi_info += "*****";
                }
                cout << wifi_info << " " << ip_info << " " << csvQuote(essid) << endl;
            }
            else if (!first_only) {
                cout << "Encrypted" << endl;
            }
        }
    }
    return retval;
}

void usage() {
    cout << "Usage:" << endl;
    cout << "   config_aplist [-m macaddr]" << endl;
    cout << "   config_aplist [-m macaddr] -d idx ..." << endl;
    cout << "   config_aplist [-m macaddr] -a essid wificonfig ipconfig" << endl;
    cout << "Where:" << endl;
    cout << "   wificonifig: off | enctype:pass" << endl;
    cout << "   ipconfig: dhcp | ipaddr/nm:route:nameserver" << endl;
}

void delete_ap(int idx) {
    ap_list.erase(ap_list.begin()+idx);
    essid_list.erase(essid_list.begin()+idx);
    delete_entries(idx);
}

void delete_macs() {
    for (int idx=mac_list.size()-1; idx >= 0; idx--) {
         bool keep = false;
         for (int i=0; i < essid_list.size(); i++) {
             if (mac_list[idx].enc_entries[i] != "xxxx") {
                 keep = true;
                 break;
             }
         }
         if (!keep) {
             delete_mac(idx);
         }
    }
}

void delete_mac(int idx) {
    mac_list.erase(mac_list.begin()+idx);
    int mac_idx = idx + essid_list.size();
    delete_entries(mac_idx);
}

void delete_entries(int idx) {
    for (int i=0; i < essid_list.size(); i++) {
        essid_list[i].enc_entries.erase(essid_list[i].enc_entries.begin()+idx);
        if (essid_list[i].dec_entries.size() != 0) {
            essid_list[i].dec_entries.erase(essid_list[i].dec_entries.begin()+idx);
        }
    }
    for (int i=0; i < mac_list.size(); i++) {
        mac_list[i].enc_entries.erase(mac_list[i].enc_entries.begin()+idx);
        if (mac_list[i].dec_entries.size() != 0) {
            mac_list[i].dec_entries.erase(mac_list[i].dec_entries.begin()+idx);
        }
    }
}

void check_wifi_info(string wifi_info) {
    if (wifi_info != "off") {
        int pos = wifi_info.find(":");
        if (pos == string::npos) {
            cout << "Bad wifi info format" << endl;
            exit(1);
        }
        string type = wifi_info.substr(0,pos);
        string key = wifi_info.substr(pos+1);
        int len = key.size();
        if (type == "wep") {
            if ((len != 5) and (len != 13)) {
                int pos = key.find_first_not_of("0123456789ABCDEFabcdef");
                if ( (pos != string::npos) or
                     ((len != 10) and (len != 26)) ) {
                    cout << "Invalid WEP key: " << key << endl;
                    exit(1);
                }
            }
        }
        else if (type == "wpa") {
            if ((len < 8) or (len > 63)) {
                cout << "Invalid WPA key: " << key << endl;
                exit(1);
            }
        }
        else {
            cout << "Invalid encyrption type" << endl;
            exit(1);
        }
    }
}

void write_ap_file() {
    if (ap_list.size() == 0) {
        unlink(ApListFile);
    }
    else {
        string tmpfile = string(ApListFile)+".tmp";
        ofstream out(tmpfile.c_str());
        for (int i=0; i < ap_list.size(); i++) {
            out << "ap " << ap_list[i].md5_essid << " " << ap_list[i].enc_wifi << " " << ap_list[i].enc_ip << endl;
        }
        for (int i=0; i < essid_list.size(); i++) {
            out << "essid " << essid_list[i].md5_essid;
            if (essid_list[i].essid == "") {
                write_strings(out, essid_list[i].enc_entries);
            }
            else {
                string key = make_key(essid_list[i].essid);
                write_entries(out, essid_list[i].enc_entries, key);
            }
            out << endl;
        }
        for (int i=0; i < mac_list.size(); i++) {
            out << "mac " << mac_list[i].md5_mac;
            if (mac_list[i].mac == "") {
                write_strings(out, mac_list[i].enc_entries);
            }
            else {
                string key = make_key(mac_list[i].mac);
                write_entries(out, mac_list[i].enc_entries, key);
            }
            out << endl;
        }
        out.close();
        rename(tmpfile.c_str(), ApListFile);
    }
}

void write_entries(ofstream &out, Strings enc_entries, string key) {
    for (int i=0; i < enc_entries.size(); i++) {
        string enc_entry = enc_entries[i];
        if (enc_entry == "xxxx") {
            string dec_entry = "";
            if (i < essid_list.size()) {
                int essid_idx = i;
                dec_entry = essid_list[essid_idx].essid;
            }
            else {
                int mac_idx = i - essid_list.size();
                dec_entry = mac_list[mac_idx].mac;
            }
            if (dec_entry != "") {
                enc_entry = encrypt(key, dec_entry);
            }
        }
        out << " " + enc_entry;
    }
}

void write_strings(ofstream &out, Strings strings) {
    for (int i=0; i < strings.size(); i++) {
        out << " " << strings[i];
    }
}

void decrypt_aplist() {
    if (fileExists(ApListFile)) {
        Strings lines = readFileArray(ApListFile);
        for (int i=0; i < lines.size(); i++) {
            Strings args = split(lines[i], " ");
            if (args[0] == "ap") {
                ApEntry ap_ent;
                ap_ent.md5_essid = args[1];
                ap_ent.enc_wifi = args[2];
                ap_ent.enc_ip = args[3];
                ap_list.push_back(ap_ent);
            }
            else if (args[0] == "mac") {
                string md5_mac = args[1];
                args.erase(args.begin(), args.begin()+2);
                MacEntry mac_ent;
                mac_ent.md5_mac = md5_mac;
                mac_ent.enc_entries = args;
                mac_list.push_back(mac_ent);
            }
            else {
                string md5_essid = args[1];
                args.erase(args.begin(), args.begin()+2);
                EssidEntry essid_ent;
                essid_ent.md5_essid = md5_essid;
                essid_ent.enc_entries = args;
                essid_list.push_back(essid_ent);
            }
        }
        if (ap_list.size() > 0) {
            if (fileExists(scan_file)) {
                Strings essids = readFileArray(scan_file);
                for (int i=0; i < essids.size(); i++) {
                    string essid = csvUnquote(essids[i]);
                    string md5_essid = md5_base64(essid);
                    for (int j=0; j < essid_list.size(); j++) {
                        EssidEntry essid_ent = essid_list[j];
                        if (essid_ent.essid == "") {
                            if (md5_essid == essid_ent.md5_essid) {
                                string key = make_key(essid);
                                string enc_essid = essid_ent.enc_entries[j];
                                string dec_essid = decrypt(key, enc_essid);
                                if (essid == dec_essid) {
                                    decrypt_ap(j, essid, key);
                                    if (first_only) {
                                        break;
                                    }
                                    decrypt_essid(j, essid, key);
                                }
                            }
                        }
                    }
                }
            }
            if ((mac_address != "") and (!first_only)) {
                string md5_mac = md5_base64(mac_address);
                for (int i=0; i < mac_list.size(); i++) {
                    if (mac_list[i].mac == "") {
                        if (mac_list[i].md5_mac == md5_mac) {
                            string key = make_key(mac_address);
                            string enc_mac = mac_list[i].enc_entries[i+essid_list.size()];
                            string dec_mac = decrypt(key, enc_mac);
                            if (dec_mac == mac_address) {
                                decrypt_mac(i, mac_address, key);
                            }
                        }
                    }
                }
            }
        }
    }
}

void decrypt_ap(int idx, string essid, string key) {
    ap_list[idx].essid = essid;
    string enc_ip = ap_list[idx].enc_ip;
    ap_list[idx].wifi_info = decrypt(key, ap_list[idx].enc_wifi);
    if ( enc_ip != "") {
        ap_list[idx].ip_info = decrypt(key, enc_ip);
    }
}

void decrypt_essid(int idx, string essid, string key) {
    essid_list[idx].essid = essid;
    essid_list[idx].dec_entries = decrypt_entries(key, essid_list[idx].enc_entries);
}

void decrypt_mac(int idx, string mac, string key) {
    mac_list[idx].mac = mac;
    mac_list[idx].dec_entries = decrypt_entries(key, mac_list[idx].enc_entries);
}

Strings decrypt_entries(string key, Strings enc_entries) {
    Strings dec_entries;
    for (int i=0; i < enc_entries.size(); i++) {
        string enc_ent = enc_entries[i];
        string dec_ent = "xxxx";
        if (enc_ent != "xxxx") {
            if (i < essid_list.size()) {
                int idx = i;
                if (essid_list[idx].essid != "") {
                    dec_ent = essid_list[idx].essid;
                }
                else {
                    dec_ent = decrypt(key, enc_ent);
                    string essid = dec_ent;
                    string essid_key = make_key(essid);
                    decrypt_essid(idx, essid, essid_key);
                    decrypt_ap(idx, essid, essid_key);
                }
            }
            else {
                int idx = i - essid_list.size();
                if (mac_list[idx].mac != "") {
                    dec_ent = mac_list[idx].mac;
                }
                else {
                    dec_ent = decrypt(key, enc_ent);
                    string mac = dec_ent;
                    string mac_key = make_key(mac);
                    decrypt_mac(idx, mac, mac_key);
                }
            }
        }
        dec_entries.push_back(dec_ent);
    }
    return dec_entries;
}

void add_essid(string essid, string md5_essid) {
    string key = make_key(essid);
    EssidEntry essid_ent;
    essid_ent.essid = essid;
    essid_ent.md5_essid = md5_essid;
    essid_ent.enc_entries = blank_entries();
    essid_ent.dec_entries = essid_ent.enc_entries;
    int essid_idx = essid_list.size();
    essid_list.push_back(essid_ent);
    add_entries(essid_idx);
}

void add_mac(string mac) {
    MacEntry mac_ent;
    mac_ent.mac = mac;
    mac_ent.md5_mac = md5_base64(mac);
    mac_ent.enc_entries = blank_entries();
    mac_ent.dec_entries = mac_ent.enc_entries;
    int mac_idx = mac_list.size() + essid_list.size();
    mac_list.push_back(mac_ent);
    add_entries(mac_idx);
}

Strings blank_entries() {
    Strings entries;
    for (int i=0; i < (essid_list.size()+mac_list.size()); i++) {
         entries.push_back("xxxx");
    }
    return entries;
}

void add_entries(int idx) {
    for (int i=0; i < essid_list.size(); i++) {
        essid_list[i].enc_entries.insert(essid_list[i].enc_entries.begin()+idx, "xxxx");
        if (essid_list[i].dec_entries.size() >= idx) {
            essid_list[i].dec_entries.insert(essid_list[i].dec_entries.begin()+idx, "xxxx");
        }
    }
    for (int i=0; i < mac_list.size(); i++) {
        mac_list[i].enc_entries.insert(mac_list[i].enc_entries.begin()+idx, "xxxx");
        if (mac_list[i].dec_entries.size() >= idx) {
            mac_list[i].dec_entries.insert(mac_list[i].dec_entries.begin()+idx, "xxxx");
        }
    }
}

string make_key(string secret) {
    string key = md5_base64(secret + md5_base64(secret));
    return key;
}

string encrypt(string key, string str) {
    string encstr = "";
//    str.replace("'", "'\"'\"'");
    encstr = backtick("echo '"+str+"' | openssl enc -aes-256-cbc -a -A -salt -pass pass:"+key);
    return encstr;
}

string decrypt(string key, string encstr) {
    string decstr = "";
    decstr = backtick("echo '"+encstr+"' | openssl enc -d -aes-256-cbc -a -A -salt -pass pass:"+key);
    return decstr;
}

string md5(string text) {
    MD5_CTX mdContext;
    int bytes;
    string md5 = "";
    MD5Init (&mdContext);
    MD5Update (&mdContext, (unsigned char *)text.c_str(), text.size());
    MD5Final (&mdContext);
    for (int i=0; i < 16; i++) {
        char buf[5];
        sprintf(buf, "%02x", mdContext.digest[i]);
        md5 += buf;
    }
    return md5;
}

string md5_base64(string text) {
    MD5_CTX mdContext;
    int bytes;
    string md5 = "";
    MD5Init (&mdContext);
    MD5Update (&mdContext, (unsigned char *)text.c_str(), text.size());
    MD5Final (&mdContext);
    md5 = base64_encode(mdContext.digest, 16);
    return md5;
}

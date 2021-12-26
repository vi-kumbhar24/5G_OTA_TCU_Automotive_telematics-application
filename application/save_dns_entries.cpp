/****************************************************************************************
 * File: save_dns_entries.cpp
 *
 * Program to save cached DNS entries so next time while we don't have a connection the
 * names can still be resolved. This allow the redirect to login/status screen to work
 *
 ****************************************************************************************
 * Change log
 * 2012/09/13 rwp
 *    Changed the writing of the files to write to a temp file and then renamed
 ****************************************************************************************
 */

#include <fstream>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include "autonet_types.h"
#include "split.h"
using namespace std;

#define EtcHostsFile "/autonet/etc/hosts.tmp"
#define SavedHostsFile "/autonet/etc/savedhosts"
#define DnsmasqLogFile "/var/log/dnsmasq.log"

void markAllOld();
bool isIpAddr(string str);
int findHost(string name);
int findAlias(string name);
void linkAliases();
void readHosts(string file);
void writeHosts(string file);
void writeEtcHosts(string file);

class Host {
  public:
    string name;
    time_t seen;
    bool old;
    Strings ip_list;
    string cname;
    Strings aliases;

    Host() {
        name = "";
        seen = 0;
        old = false;
        cname = "";
    }
};

vector<Host> hosts;

main() {
    string line;
    string file = DnsmasqLogFile;
    system("pkill -USR1 dnsmasq");
    sleep(1);
    readHosts(SavedHostsFile);
    ifstream fin(file.c_str());
    if (fin.is_open()) {
        bool in_cache = false;
        while (fin.good()) {
            getline(fin, line);
            if (fin.eof()) break;
            Strings parms = split(line, " ");
            if (parms[0] != "dnsmasq:") {
                in_cache = false;
                continue;
            }
            if (parms[1] == "Host") {
//                cout << "Starting cache" << endl;
                in_cache = true;
                markAllOld();
                continue;
            }
            if (in_cache) {
//                cout << "line: " << line << endl;
                if (parms.size() < 4) {
                    in_cache = false;
                    continue;
                }
                string name = parms[1];
                string ip = parms[2];
                if (name.find(".") == string::npos) {
//                    in_cache = false;
                    continue;
                }
                if (ip.find(".") == string::npos) {
//                    in_cache = false;
                    continue;
                }
//                if (parms.size() < 9) continue;
//                cerr << name << " : " << ip << endl;
//                cerr << "Finding host:" << name << endl;
                int idx = findHost(name);
                //cerr << "idx=" << idx << endl;
                if (idx == -1) {
//                    cerr << "Adding host" << endl;
                    Host host;
                    host.name = name;
                    idx = hosts.size();
                    hosts.push_back(host);
                }
                //cerr << "Got here" << endl;
                if (hosts[idx].old) {
//                    cerr << "Clearing ip_list" << endl;
                    hosts[idx].ip_list.clear();
                    hosts[idx].cname = "";
                }
                if (isIpAddr(ip)) {
//                    cerr << "Adding ip address:" << ip << endl;
                    hosts[idx].ip_list.push_back(ip);
                }
                else {
                    hosts[idx].cname = ip;
                }
                hosts[idx].seen = time(NULL);
                hosts[idx].old = false;
            }
        }
        fin.close();
    }
    else {
        cerr << "Failed to open: " << file << endl;
    }
    linkAliases();
    writeEtcHosts(EtcHostsFile);
    writeHosts(SavedHostsFile);
    system("backup_autonet");
}

void markAllOld() {
    for (int i=0; i < hosts.size(); i++) {
        hosts[i].old = true;
    }
}

bool isIpAddr(string str) {
    bool isip = false;
    int n1, n2, n3, n4;
    if (sscanf(str.c_str(), "%d.%d.%d.%d", &n1, &n2, &n3, &n4) == 4) {
        isip = true;
    }
    return isip;
}

int findHost(string name) {
    int idx = -1;
    for (int i=0; i < hosts.size(); i++) {
        //cerr << "checking " << name << " == " << hosts[i].name << endl;
        if (hosts[i].name == name) {
            idx = i;
            break;
        }
    }
    return idx;
}

int findAlias(string name) {
    int idx = -1;
    idx = findHost(name);
    if (idx != -1) {
        if (hosts[idx].cname != "") {
            idx = findAlias(hosts[idx].cname);
        }
    }
    return idx;
}

void linkAliases() {
    for (int i=0; i < hosts.size(); i++) {
        if (hosts[i].cname != "") {
            int idx = findAlias(hosts[i].cname);
            if (idx != -1) {
                hosts[idx].aliases.push_back(hosts[i].name);
            }
        }
    }
}

void readHosts(string file) {
    string line;
    ifstream fin(file.c_str());
    if (fin.is_open()) {
        bool in_cache = false;
        while (fin.good()) {
            getline(fin, line);
            if (fin.eof()) break;
            Strings parms = split(line, " ");
            string name = parms[0];
            string seen_str = parms[1];
            Host host;
            host.name = name;
            sscanf(seen_str.c_str(), "%d", &host.seen);
            host.old = true;
            if (isIpAddr(parms[2])) {
                for (int i=2; i < parms.size(); i++) {
                    host.ip_list.push_back(parms[i]);
                }
            }
            else {
                host.cname = parms[2];
            }
            hosts.push_back(host);
        }
        fin.close();
    }
    else {
        cerr << "Failed to open: " << file << endl;
    }
}

void writeHosts(string file) {
//    cerr << "writing hosts" << endl;
    string tmpfile = file + ".tmp";
    ofstream out(tmpfile.c_str());
    if (out.is_open()) {
        for (int i=0; i < hosts.size(); i++) {
            Host host = hosts[i];
            out << host.name << " " << host.seen;
            if (host.cname == "") {
                for (int j=0; j < host.ip_list.size(); j++) {
                    out << " " << host.ip_list[j];
                }
            }
            else {
                out << " " << host.cname;
            }
            out << endl;
        }
        out.close();
        rename(tmpfile.c_str(), file.c_str());
    }
    else {
        cerr << "Could not write to " << file << endl;
    }
}

void writeEtcHosts(string file) {
//    cerr << "writing etc/hosts" << endl;
    string tmpfile = file;
    ofstream out(tmpfile.c_str());
    if (out.is_open()) {
        for (int i=0; i < hosts.size(); i++) {
            Host host = hosts[i];
            if (host.cname == "") {
                for (int j=0; j < host.ip_list.size(); j++) {
                    out << host.ip_list[j] << " " << host.name;
                    for (int k=0; k < host.aliases.size(); k++) {
                        out << " " << host.aliases[k];
                    }
                    out << endl;
                }
            }
        }
        out.close();
        rename(tmpfile.c_str(), file.c_str());
    }
    else {
        cerr << "Could not write to " << file << endl;
    }
}

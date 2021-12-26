#ifndef READCONFIG_H
#define READCONFIG_H

#include <map>
#include <string>
#include <fstream>
#include "config_api.h"
using namespace std;



typedef map<string,string> ConfigMap;

class Config {
private:
    ConfigMap config;
    string filename;

    bool _matched_var(string ln, string newvar) {
        bool matched = false;
        if (ln.size() == 0) {
            return false;
        }
        while ((ln.size() > 0) and (ln.substr(0) == " ")) {
            ln = ln.substr(1);
        }
        if (ln.size() == 0) {
            return false;
        }
        if (strncmp(ln.c_str(), "#", 1) == 0) {
            return false;
        }
        
        int pos;
        if ((pos=ln.find(" ")) != string::npos) {
            string var = ln.substr(0, pos);
            if (var == newvar) {
                matched = true;
            }
        }
        return matched;
    }

public:
    Config() {
    }

    Config(string file) {
        if (file == "ConfigManager") {
            char key[] = "unit.*";
            readConfigManager(key);
        }
        else {
            open(file);
        }
    }

    void readConfigManager(char *key) {
        char ip[] = "127.0.0.1";
        KVPair * kv = NULL;
        for (int i=0; i < 10; i++) {
            kv = getValues(key, ip);
            if (kv != NULL) {
                break;
            }
            sleep(1);
        }
        if (kv == NULL) {
            cerr << "Could not get config from ConfigManager" << endl;
        }
        else {
            KVPair * this_kv = kv;
            while (this_kv) {
                config[this_kv->key] = this_kv->val;
                this_kv = this_kv->next;
            }
	    //  Free allocated storage
	    freeKVList(kv);
        }
    }

    bool open(string file) {
        bool ok = false;
        ifstream infile;
        infile.open(file.c_str());
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
                    int pos;
                    if ((pos=ln.find(" ")) != string::npos) {
                        string var = ln.substr(0, pos);
                        string val = ln.substr(pos+1);
                        while (val.substr(0,1) == " ") {
                            val = val.substr(1);
                        }
                        config[var] = val;
                    }
                }
            }
            filename = file;
            ok = true;
        }
        return ok;
    }

    bool exists(string var) {
        bool found = false;
        if (config.find(var) != config.end()) {
            found = true;
        }
        return found;
    }

    bool isTrue(string var) {
        bool istrue = false;
        string val = getString(var);
        if (val == "true") {
            istrue = true;
        }
        return istrue;
    }

    string getString(string var) {
        string retval = "";
        if (exists(var)) {
            retval = config[var];
        }
        return retval;
    }
    int getInt(string var) {
        int retval = 0;
        if (exists(var)) {
            int val = 0;
            if (sscanf(config[var].c_str(), "%d", &val) == 1) {
                retval = val;
            }
        }
        return retval;
    }

//   string getVars() {
//        string vars;
//        for(ConfigMap::iterator it=config.begin(); it != config.end(); it++) {
//            vars.push_back(it->first);
//        }
//        return vars;
 //   }

    void updateConfig(string newvar, string newval) {
        ifstream old;
        old.open(filename.c_str());
        if (old.is_open()) {
            string newfile = filename + ".tmp";
            ofstream newf;
            newf.open(newfile.c_str());
            if (newf.is_open()) {
                bool changed = false;
                while (!old.eof()) {
                    string ln;
                    getline(old, ln);
                    if (!old.eof()) {
                        if (_matched_var(ln,newvar)) {
                            newf << newvar << " " << newval << endl;
                            changed = true;
                        }
                        else {
                            newf << ln << endl;
                        }
                    }
                }
                if (!changed) {
                    newf << newvar << " " << newval << endl;
                }
                newf.close();
                rename(newfile.c_str(), filename.c_str());
            }
            old.close();
        }
    }
};

#endif

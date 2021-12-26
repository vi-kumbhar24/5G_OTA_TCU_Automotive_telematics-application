#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "autonet_types.h"
#include "getopts.h"
#include "split.h"
#include "Md5.h"

using namespace std;

void readConfig(string file);
void readMd5(string file);
void readdir(string dir);
string encode(string name);
string decode(string name);
string MDFile (string filename);
void checkMissing();
int stringToInt(string str);

class Md5Entry {
  public:
    char type;
    string md5;
    int size;
    bool seen;

    Md5Entry() {
    }

    Md5Entry(string type_s, string md5_s) {
        type = type_s.at(0);
        md5 = md5_s;
        seen = false;
    }

    Md5Entry(string type_s, int sz, string md5_s) {
        type = type_s.at(0);
        size = sz;
        md5 = md5_s;
        seen = false;
    }
};

string basedir = ".";
typedef map<string,bool> Ignores;
Ignores ignores;
typedef map<string,Md5Entry> Md5List;
Md5List md5s;
bool check = false;
bool failed = false;
string configfile = "verify_fs.conf";

int main(int argc, char *argv[]) {
    GetOpts opts(argc, argv, "c:f:");
    if (opts.exists("c")) {
        check = true;
        readMd5(opts.get("c"));
    }
    if (opts.exists("f")) {
        configfile = opts.get("f");
    }
    int exitstat = 0;
    if (argc > 1) {
        basedir = argv[1];
    }
    readConfig(configfile);
    readdir("/");
    if (check) {
        checkMissing();
    }
    if (failed) {
        exitstat = 1;
    }
    return exitstat;
}

void readConfig(string file) {
    string line;
    ifstream fin(file.c_str());
    if (fin.is_open()) {
        while (fin.good()) {
            getline(fin, line);
            if (fin.eof()) break;
            string first = line.substr(0,1);
            line = line.substr(1);
            if (first == "-") {
                ignores[line] = true;
            }
        }
        fin.close();
    }
    else {
        cerr << "Failed to open: " << file << endl;
    }
}

void readMd5(string file) {
    ifstream inp(file.c_str());
    if (inp.is_open()) {
        while (inp.good()) {
            string line;
            getline(inp, line);
            if (inp.eof()) {
                break;
            }
            Strings vals = split(line, " ");
            if (vals.size() < 2) {
                cerr << "Invalid md5 line: " << line << endl;
                continue;
            }
            string name = decode(vals[0]);
            if (vals.size() < 3) {
                vals.push_back("");
            }
            if (vals.size() < 4) {
                md5s[name] = Md5Entry(vals[1], vals[2]);
            }
            else {
                md5s[name] = Md5Entry(vals[1], stringToInt(vals[2]), vals[3]);
            }
        }
        inp.close();
    }
}

void readdir(string currdir) {
    Strings files;
    struct dirent *dirp;
    string dname = basedir;
    if (currdir != "/") {
        dname += currdir;
    }
    DIR *dp = opendir(dname.c_str());
    if (dp != NULL) {
        while ((dirp = readdir(dp)) != NULL) {
            string fname = dirp->d_name;
            if ((fname == ".") or (fname == "..")) {
                continue;
            }
            files.push_back(fname);
        }
        closedir(dp);
        sort(files.begin(), files.end());
        for (Strings::iterator it=files.begin(); it != files.end(); it++) {
            string fname = "/" + *it;
            if (currdir != "/") {
                fname = currdir + fname;
            }
            string enc_name = encode(fname);
            if (ignores.find(fname) != ignores.end()) {
                continue;
            }
            string file = basedir + fname;
            struct stat st_buf;
            if (lstat(file.c_str(), &st_buf) == 0) {
                int mode = st_buf.st_mode;
                int size = st_buf.st_size;
                if (S_ISDIR(mode)) {
                    if (check) {
                        if (md5s.find(fname) != md5s.end()) {
                            md5s[fname].seen = true;
                            if (md5s[fname].type == 'd') {
                                readdir(fname);
                            }
                            else {
                                cerr << "Dir did not match dir in list: " << fname << endl;
                                failed = true;
                            }
                        }
                        else {
                            cerr << "Dir not in list: " << fname << endl;
                        }
                    }
                    else {
                        cout << enc_name << " dir" << endl;
                        readdir(fname);
                    }
                }
                else if (S_ISLNK(mode)) {
                    char buff[512];
                    int count = readlink(file.c_str(), buff, sizeof(buff));
                    if (count >= 0) {
                        buff[count] = '\0';
                        if (check) {
                            if (md5s.find(fname) != md5s.end()) {
                                md5s[fname].seen = true;
                                if (md5s[fname].type == 's') {
                                    if (md5s[fname].md5 != buff) {
                                        cerr << "Symlink did not match: " << fname << endl;
                                        failed = true;
                                    }
                                }
                                else {
                                    cerr << "Symlink did not match a symlink in list: " << fname << endl;
                                    failed = true;
                                }
                            }
                            else {
                                cerr << "Symlink not in list: " << fname << endl;
                            }
                        }
                        else {
                            cout << enc_name << " slnk " << buff << endl;
                        }
                    }
                    else {
                        cerr << "Error getting target file for " << fname << endl;
                        failed = true;
                    }
                }
                else if (S_ISCHR(mode)) {
                    if (check) {
                        if (md5s.find(fname) != md5s.end()) {
                            md5s[fname].seen = true;
                            if (md5s[fname].type != 'c') {
                                cerr << "Character file did not match in list: " << fname << endl;
                                failed = true;
                            }
                        }
                        else {
                            cerr << "Character file not in list: " << fname << endl;
                        }
                    }
                    else {
                        cout << enc_name << " char" << endl;
                    }
                }
                else if (S_ISBLK(mode)) {
                    if (check) {
                        if (md5s.find(fname) != md5s.end()) {
                            md5s[fname].seen = true;
                            if (md5s[fname].type != 'b') {
                                cerr << "Block file did not match in list: " << fname << endl;
                                failed = true;
                            }
                        }
                        else {
                            cerr << "Block file not in list: " << fname << endl;
                        }
                    }
                    else {
                        cout << enc_name << " block" << endl;
                    }
                }
                else if (S_ISFIFO(mode)) {
                    if (check) {
                        if (md5s.find(fname) != md5s.end()) {
                            md5s[fname].seen = true;
                            if (md5s[fname].type != 'F') {
                                cerr << "Fifo file did not match in list: " << fname << endl;
                                failed = true;
                            }
                        }
                        else {
                            cerr << "Fifo file not in list: " << fname << endl;
                        }
                    }
                    else {
                        cout << enc_name << " Fifo" << endl;
                    }
                }
                else if (S_ISSOCK(mode)) {
                    if (check) {
                        if (md5s.find(fname) != md5s.end()) {
                            md5s[fname].seen = true;
                            if (md5s[fname].type != 's') {
                                cerr << "Socket file did not match in list: " << fname << endl;
                                failed = true;
                            }
                        }
                        else {
                            cerr << "Socket file not in list: " << fname << endl;
                        }
                    }
                    else {
                        cout << enc_name << " sock" << endl;
                    }
                }
                else {
                    string md5 = MDFile(file);
                    if (check) {
                        if (md5s.find(fname) != md5s.end()) {
                            md5s[fname].seen = true;
                            if (md5s[fname].type == 'f') {
                                if (md5s[fname].md5 != md5) {
                                    cerr << "File did not match: " << fname << endl;
                                    failed = true;
                                }
                            }
                            else {
                                cerr << "File did not match a file in list: " << fname << endl;
                                failed = true;
                            }
                        }
                        else {
                            cerr << "File not in list: " << fname << endl;
                        }
                    }
                    else {
                        cout << enc_name << " file " << size << " " << md5 << endl;
                    }
                }
            }
            else {
                cerr << "Could not stat file " << file << endl;
                failed = true;
            }
        }
    }
}

string encode(string name) {
    size_t pos;
    while ((pos=name.find("%")) != string::npos) {
         name.replace(pos, 1, "%25");
    } 
    while ((pos=name.find(" ")) != string::npos) {
         name.replace(pos, 1, "%20");
    } 
    return name;
}

string decode(string name) {
    size_t pos;
    while ((pos=name.find("%20")) != string::npos) {
         name.replace(pos, 3, " ");
    } 
    while ((pos=name.find("%25")) != string::npos) {
         name.replace(pos, 3, "%");
    } 
    return name;
}

string MDFile (string filename) {
    FILE *inFile = fopen (filename.c_str(), "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];
    string md5 = "";

    if (inFile == NULL) {
        cerr << "Could not open " << filename << endl;
    }
    else {

        MD5Init (&mdContext);
        while ((bytes = fread (data, 1, 1024, inFile)) != 0) {
            MD5Update (&mdContext, data, bytes);
        }
        MD5Final (&mdContext);
        for (int i=0; i < 16; i++) {
            char buf[5];
            sprintf(buf, "%02x", mdContext.digest[i]);
            md5 += buf;
        }
        fclose (inFile);
    }
    return md5;
}

void checkMissing() {
    Md5List::iterator it;
    for (it=md5s.begin(); it != md5s.end(); it++) {
        string fname = (*it).first;
        if (!md5s[fname].seen) {
            bool match_ign = false;
            Ignores::iterator i_it;
            for (i_it=ignores.begin(); i_it != ignores.end(); i_it++) {
                string ign = (*i_it).first;
                if (fname == ign) {
                    match_ign = true;
                    break;
                }
                ign += "/";
                if (strncmp(fname.c_str(), ign.c_str(), ign.size()) == 0) {
                    match_ign = true;
                    break;
                }
            }
            if (match_ign) {
                continue;
            }
            cerr << "Missing: " << fname << endl;
            failed = true;
        }
    }
}

int stringToInt(string str) {
    int val = 0;
    sscanf(str.c_str(), "%d", &val);
    return val;
}

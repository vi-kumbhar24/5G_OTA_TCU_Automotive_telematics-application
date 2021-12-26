#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <dirent.h>
#include <string.h>

using namespace std;

typedef vector<int> Pids;

class GetPids:public vector<int> {
  private:
    map<int,int> pid_ppid;

    vector<int> children(int pid) {
        vector<int> kids;
        kids.push_back(pid);
        int i;
        for (i=0; i < size(); i++) {
            int p = at(i);
            if (pid_ppid[p] == pid) {
                vector<int> grandkids = children(p);
                int j;
                for (j=0; j < grandkids.size(); j++) {
                    kids.push_back(grandkids[j]);
                }
            }
        }
        return kids;
    }

    void getParents() {
        int i;
        char pidstr[10];
        for (i=0; i < size(); i++) {
            int pid = at(i);
            sprintf(pidstr, "%d", pid);
            string statfile = "/proc/";
            statfile += pidstr;
            statfile += "/status";
            ifstream file(statfile.c_str());
            if (file.is_open()) {
                string ln;
                while (file.good()) {
                    getline(file, ln);
                    if (file.eof()) break;
                    if (strncmp(ln.c_str(), "PPid:\t", 6) == 0) {
                        string ppidstr = ln.substr(6);
                        int ppid;
                        sscanf(ppidstr.c_str(), "%d", &ppid);
                        pid_ppid[pid] = ppid;
                        cout << pid << " -> " << ppid << endl;
                    }
                }
                file.close();
            }
        }
    }

  public:
    GetPids() {
        struct dirent *dirp;
        DIR *dp = opendir("/proc");
        if (dp != NULL) {
            while ((dirp = readdir(dp)) != NULL) {
                char first_ch = dirp->d_name[0];
                if ((first_ch >= '0') and (first_ch <= '9')) {
                    int pid;
                    sscanf(dirp->d_name, "%d", &pid);
                    pid_ppid[pid] = 0;
                    push_back(pid);
                }
            }
            closedir(dp);
            getParents();
        }
        else {
            cerr << "Failed to open dir" << endl;
        }
    }

    void listChildren(int pid) {
        vector<int> kids = children(pid);
        int i;
        for (i=0; i < kids.size(); i++) {
            cout << kids[i] << " ";
        }
        cout << endl;
    }
};

main() {
    GetPids pids;
    pids.listChildren(1);
}

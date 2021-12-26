#include <iostream>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "autonet_types.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
#include "str_system.h"
using namespace std;

#define URX (S_IRUSR | S_IXUSR)
#define ALLRWX (S_IRWXU | S_IRWXG | S_IRWXO)
bool checkBase();
bool checkCommand(string command);
bool checkDir(string dir);

int main(int argc, char *argv[], char *env[]) {
    if (!checkBase()) {
        cerr << "Invalid permissions on homedir" << endl;
        exit(1);
    }
    system("echo rshell started >>/tmp/rshell.log");
    str_system("echo argc:"+toString(argc)+" >>/tmp/rshell.log");
    for (int i=0; i < argc; i++) {
        str_system("echo "+string(argv[i])+" >>/tmp/rshell.log");
    }
    if ( (argc == 3) and
         (strcmp(argv[1],"-c") == 0) and
         (strncmp(argv[2], "scp ", 4) == 0) ) {
        Strings arg_strings = split(argv[2], " ");
        char *args[arg_strings.size()+1];
        int i = 0;
        bool from = false;
        bool to = false;
        bool todir = false;
        bool recursive = false;
        bool use_stdin = false;
        bool invalid = false;
        for (i=0; i < arg_strings.size(); i++) {
            string arg = arg_strings[i];
            if (arg.substr(0,1) == "-") {
                if (arg == "-t") {
                    to = true;
                }
                else if (arg == "-f") {
                    from = true;
                }
                else if (arg == "-d") {
                    todir = true;
                }
                else if (arg == "-r") {
                    recursive = true;
                }
                else if (arg == "--") {
                    use_stdin = true;
                }
                else {
                    invalid = true;
                }
            }
            args[i] = (char *)(arg_strings[i].c_str());
//            str_system("echo args:"+arg_strings[i]+" >>/tmp/rshell.log");
        }
        args[i] = (char *)NULL;
//        if (arg_strings[arg_strings.size()-2] != "--") {
//            invalid = true;
////            system("echo 'No --' >>/tmp/rshell.log");
//        }
        string dir = arg_strings[arg_strings.size()-1];
        string file = "";
        int pos = dir.find("/");
        if (pos != string::npos) {
            file = dir.substr(pos+1);
            dir = dir.substr(0,pos);
        }
        if (!checkDir(dir)) {
            invalid = true;
//            system("echo 'dest dir wrong permissions' >>/tmp/rshell.log");
        }
        else if (file.find("/") != string::npos) {
            invalid = true;
//            system("echo 'Dest file contains /' >>/tmp/rshell.log");
        }
        else if (to) {
            if (todir and (file != "")) {
                invalid = true;
//                system("echo 'Invalid dest dir' >>/tmp/rshell.log");
            }
        }
        else if (from) {
            if (recursive) {
                if (file != "") {
                    invalid = true;
//                    system("echo 'Invalid recursive dest' >>/tmp/rshell.log");
                }
            }
        }
        else {
            invalid = true;
//            system("echo 'no -t or -f' >>/tmp/rshell.log");
        }
        if (invalid) {
            cerr << "Invalid scp command" << endl;
            exit(1);
        }
//        str_system("echo execing:"+string(argv[2])+" >>/tmp/rshell.log");
        execvp(args[0], args);
        exit(1);
    }
    while (true) {
        char line_buf[256];
        cout << "> ";
        cout.flush();
        cin.getline(line_buf,256);
        if (cin.eof()) {
            cout << endl;
            break;
        }
        string line;
        line.assign(line_buf);
        if (line == "") {
            continue;
        }
//        str_system("echo "+line+" >>/tmp/rshell.log");
        Strings args = split(line, " ");
        string command = args[0];
        if ( (command == "exit") or
             (command == "quit") or
             (command == "logout") ) {
            break;
        }
        if (!checkCommand(command)) {
            cout << "Invalid command" << endl;
            continue;
        }
        if (line.find_first_of("`$\\()!><&|") != string::npos) {
            cout << "Invalid command" << endl;
            continue;
        }
        line = "./"+line;
        str_system(line);
    }
    return 0;
}

bool checkBase() {
    bool base_ok = false;
    struct stat st_info;
    int v = stat(".", &st_info);
    if (st_info.st_uid == getuid()) {
        if ((st_info.st_mode & ALLRWX) == URX) {
            base_ok = true;
        }
    }
    return base_ok;
}

bool checkCommand(string command) {
    bool command_ok = false;
    struct dirent *dirp;
    struct stat st_info;
    DIR *dp = opendir(".");
    if (dp != NULL) {
        while ((dirp = readdir(dp)) != NULL) {
            if (dirp->d_name[0] == '.') continue;
            int v = stat(dirp->d_name, &st_info);
            if (!S_ISREG(st_info.st_mode)) continue;
            if (st_info.st_uid != getuid()) continue;
            if ((st_info.st_mode & S_IRWXU) != URX) continue;
            if (strcmp(dirp->d_name, command.c_str()) == 0) {
                command_ok = true;
                break;
            }
        }
        closedir(dp);
    }
    else {
        cerr << "Failed to open dir" << endl;
    }
    return command_ok;
}

bool checkDir(string dir) {
    bool dir_ok = false;
    struct dirent *dirp;
    struct stat st_info;
    DIR *dp = opendir(".");
    if (dp != NULL) {
        while ((dirp = readdir(dp)) != NULL) {
            if (dirp->d_name[0] == '.') continue;
            int v = stat(dirp->d_name, &st_info);
            if (!S_ISDIR(st_info.st_mode)) continue;
            if (st_info.st_uid != getuid()) continue;
//            str_system("echo "+string_printf("%s %03o",dirp->d_name,st_info.st_mode)+" >>/tmp/rshell.log");
            if ((st_info.st_mode & ALLRWX) != S_IRWXU) continue;
            if (strcmp(dirp->d_name, dir.c_str()) == 0) {
                dir_ok = true;
                break;
            }
        }
        closedir(dp);
    }
    else {
        cerr << "Failed to open dir" << endl;
    }
    return dir_ok;
}

/***********************************************************************************
 * File: respawner.cpp
 *
 * Daemon to (re)spawn tasks
 *
 ***********************************************************************************
 * Change log:
 * 2012/08/02 rwp
 *    Modified to take "if <cond_file>" in configuration file
 * 2012/09/18 rwp
 *    Modified to append to logfile
 * 2013/04/03 rwp
 *    Changed USR1/USR2 signals to suspend/resume spawning
 * 2018/05/29 srh
 *    Added optional logging parameter to command line
 ***********************************************************************************
 */
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "autonet_types.h"
#include "fileExists.h"
#include "readFileArray.h"
#include "dateTime.h"
#include "split.h"
using namespace std;

#define ConfFile "/etc/respawn.conf"

int forkIt(string command);
void onHup(int sig);
void onUsr1(int sig);
void onUsr2(int sig);
void onChild(int sig);
void onTerm(int sig);

typedef enum {NotRunning, Running, Failed, Killing, Killed} ProcState;

class Command {
  public:
    int pid;
    string command;
    time_t respawn_time;
    ProcState state;
    bool keep;
    string cond_file;

    Command() {
        pid = 0;
        command = "";
        respawn_time = 0;
        keep = true;
        state = NotRunning;
        cond_file = "";
    }
};

typedef vector<Command> Commands;
Commands commands;
bool reread = false;
bool respawn = true;

Commands readCommands(string file);

int main(int argc, char *argv[]) {
	//  Check for log file
	if (argc > 2 && strcmp(argv[1], "-l") == 0)
	{
		ofstream errorFile(argv[2]);
		cerr.rdbuf(errorFile.rdbuf());
	}
    signal(SIGCHLD, onChild);
    signal(SIGHUP, onHup);
    signal(SIGTERM, onTerm);
    signal(SIGUSR1, onUsr1);
    signal(SIGUSR2, onUsr2);
    commands = readCommands(ConfFile);
    for (int i=0; i < commands.size(); i++) {
        bool runit = true;
        if ( (commands[i].cond_file != "") and
             !fileExists(commands[i].cond_file) ) {
           runit = false;
        }
        if (runit) {
            commands[i].state = Running;
            string cmnd = commands[i].command;
            cerr << getDateTime() << " Starting " << cmnd << endl;
            commands[i].pid = forkIt(cmnd);
        }
    }
    while (1) {
        select(0,NULL,NULL,NULL,NULL);
        bool killing = false;
        if (reread) {
            Commands new_cmnds = readCommands(ConfFile);
            for (int j=0; j < commands.size(); j++) {
                commands[j].keep = false;
            }
            for (int i=0; i < new_cmnds.size(); i++) {
                string newcmd = new_cmnds[i].command;
                bool found = false;
                for (int j=0; j < commands.size(); j++) {
                    if ( (newcmd == commands[j].command) and
                         (new_cmnds[i].cond_file == commands[j].cond_file) ) {
                        commands[j].keep = true;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    commands.push_back(new_cmnds[i]);
                    respawn = true;
                }
            }
            for (int i=0; i < commands.size(); i++) {
                 if (!commands[i].keep) {
                     cerr << getDateTime() << " Killing " << commands[i].command << endl;
                     commands[i].state = Killing;
                     kill(commands[i].pid, SIGTERM);
                     killing = true;
                 }
            }
            reread = false;
        }
        if (killing) {
            sleep(1);
        }
        if (respawn) {
            for (int i=0; i < commands.size(); i++) {
                if (commands[i].state == Failed) {
                    commands[i].state = Running;
                    cerr << getDateTime() << " Restarting " << commands[i].command << endl;
                    commands[i].pid = forkIt(commands[i].command);
                }
                else if (commands[i].state == NotRunning) {
                    bool runit = true;
                    if ( (commands[i].cond_file != "") and
                         !fileExists(commands[i].cond_file) ) {
                       runit = false;
                    }
                    if (runit) {
                        commands[i].state = Running;
                        cerr << getDateTime() << " Starting " << commands[i].command << endl;
                        commands[i].pid = forkIt(commands[i].command);
                    }
                }
            }
        }
        for (int i=commands.size()-1; i >= 0; i--) {
            if (commands[i].state == Killed) {
                commands.erase(commands.begin()+i);
            }
        }
    }
    cerr << getDateTime() << " Exiting" << endl;
}

Commands readCommands(string file) {
    Strings command_list = readFileArray(ConfFile);
    Commands cmnds;
    for (int i=0; i < command_list.size(); i++) {
        string cmnd = command_list[i];
        if (cmnd.substr(0,1) == "#") {
            continue;
        }
        Command command;
        if (strncmp(cmnd.c_str(), "if ", 3) == 0) {
            cmnd = cmnd.substr(3);
            int pos = cmnd.find(" ");
            if (pos == string::npos) {
                continue;
            }
            string file = cmnd.substr(0, pos);
            cmnd = cmnd.substr(pos+1);
            command.cond_file = file;
        }
        command.command = cmnd;
        command.state = NotRunning;
        cmnds.push_back(command);
    }
    return cmnds;
}

int forkIt(string command) {
    Strings args = split(command, " ");
    int n = args.size();
    char *arg_array[n+1];
    string logfile = "/dev/null";
    int j = 0;
//    arg_array[j++] = (char *)args[0].c_str();
if (n == 0)
    return 0;
    for (int i=0; i < n; i++) {
        if (strncmp(args[i].c_str(), ">", 1) == 0) {
            logfile = args[i].substr(1);
        }
        else {
            arg_array[j++] = (char *)args[i].c_str();
        }
    }
    arg_array[j] = NULL;
    pid_t pid = fork();
    if (pid < 0) {
        cerr << getDateTime() << " Forking failed" << endl;
    }
    else if (pid == 0) {
        close(0);
        bool append = false;
//        int type = O_CREAT | O_WRONLY | O_TRUNC;
        int type = O_CREAT | O_WRONLY;
        if (strncmp(logfile.c_str(), ">", 1) == 0) {
            logfile.erase(0, 1);
            append = true;
        }
        else {
            type |= O_TRUNC;
        }
        int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        int fd = open(logfile.c_str(), type, mode);
        if (append) {
            lseek(fd, 0, SEEK_END);
        }
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            if (execvp(arg_array[0], arg_array) == -1) {
//                cerr << getDateTime() << " execv failed" << endl;
            }
//            cerr << getDateTime() << " execv finished" << endl;
        }
        else {
            cerr << getDateTime() << " Failed to create log" << endl;
        }
        exit(1);
    }
    return pid;
}

void onHup(int sig) {
    reread = true;
    signal(SIGHUP, onHup);
}

void onUsr1(int sig) {
    respawn = false;
    signal(SIGUSR1, onUsr1);
}

void onUsr2(int sig) {
    respawn = true;
    signal(SIGUSR1, onUsr1);
}

void onChild(int sig) {
    for (int i=0; i < commands.size(); i++) {
        int status = 0;
        int pid = commands[i].pid;
        if (pid != 0) {
            pid_t child = waitpid(pid,&status,WNOHANG);
            if (child == pid) {
                if (commands[i].state == Killing) {
                    commands[i].state = Killed;
                }
                else {
                    if (WIFEXITED(status)) { 
                        cerr << getDateTime() << " Child exited: " << WEXITSTATUS(status) << " " << commands[i].command << endl;
                    } else if (WIFSIGNALED(status)) { 
                        cerr << getDateTime() << " Child signalled: " << WTERMSIG(status) << " " << commands[i].command << endl;
                    } else
                        cerr << getDateTime() << " Child exited: " << commands[i].command << endl;
                    commands[i].state = Failed;
                }
            }
        }
    }
    signal(SIGCHLD, onChild);
}

void onTerm(int sig) {
    signal(SIGCHLD, SIG_IGN);
    cerr << getDateTime() << " Got TERM" << endl;
    for (int i=0; i < commands.size(); i++) {
        kill(commands[i].pid, SIGTERM);
        commands[i].state = Killing;
    }
    sleep(1);
    exit(0);
}

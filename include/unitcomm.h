#ifndef UNITCOMM_H
#define UNITCOMM_H
/*
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <iostream>
*/
using namespace std;

typedef int Socket;


/*
Function check the contains statistics about network interfaces & return true if network is up
*/
bool wanUp(void);


void handleConnections(void);
void closeAndExit(int exitstat);
/* string function check the swversion from the /etc/local/swVersion
*/
string getSwVersion(void);
/*
*/
int findConnection(string ip, int port);
int findCommand(string ip, int port, int cmndid);

/*function return the ip address using resolvdns binarie for the srevre url
*/
string getIpAddress(string name);
/*Afeter sucessful connection of the socket retun the postive value of fd
*/
Socket readerSocket(const char *name);

void killAllCommands();
void killCommandProcs(int pid);
void on_hup(int sig);
void on_int(int sig);
void on_usr1(int sig);
void on_child(int sig);



#endif

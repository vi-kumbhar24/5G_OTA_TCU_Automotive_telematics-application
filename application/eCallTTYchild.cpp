/**********************************************************************************
 * File: eCallTTY.cpp
 * Routine to perform eCall using TTY
 **********************************************************************************
 * 2013/04/03 rwp
 *    Changed to use USR1/USR2 to suspend/resume respawning
 **********************************************************************************
 */

#include <iostream>
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include "backtick.h"
#include "autonet_files.h"
#include "vic.h"
#include "touchFile.h"
#include "dateTime.h"
#include "getStringFromFile.h"
#include "logError.h"
#include <math.h>
#include "readAssoc.h"
#include "getField.h"
#include "my_features.h"
#include <cstdarg>
#include "updateFile.h"
#include "appendToFile.h"
#include "eCallTTY.h"

extern "C" {
#include <common_defs.h>
#include <tty_lib.h>
}


using namespace std;


void controlCar(string dev_s, string act_s);
int getVicResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact);
void getTime();
void setTime();
void getVersion();
void getStatus();
void getTirePressures();
void getOilLife();
void getFuelLevel();
string getVin();
//void getSpeed();
int getSpeed();
void getOdometer();
void closeAndExit(int exitstat);
void openPort(const char *port);
void closePort();
bool lock(const char *dev);
void killConnection();
void resumeConnection();
string getCellResponse(string command);
string getCellResponse(string command, int timeout);
string getCellResponse(string command, string resp, int timeout);
string getRestOfLine();
bool beginsWith(string str, string match);
bool alrm_sig = false;
void onTerm(int parm);
void onHup(int parm);
void onAlarm(int parm);


VIC vic;
int app_id = 0x20;
//bool debug = true;
bool debug = false;
int ser_s = -1;		// Serial port descriptor
struct termios oldtio;	// Old port settings for serial port
string lockfile = "";
#define EcallLogFile "/var/log/eCall.log"

char sLatitude[255];
char sLongitude[255];
char sLatLong[255];
int iAirbagsDeployed, iImpactSensors, iOrientation, iSpeed, iSeatsOccupied, iSeatbelts;
string eCallNumber = "";
string eCallModem = "";
string serial_device = "";
bool usingIntele = false;
bool hangupRequested = false;
int hangupReason;
int timeBetweenRedials = 0;
int busyBackoff = 4;
Features features;

bool switchToVoice();



TTY_CONFIG      u_config;
TTY_STATUS      u_status;
TTY_STATISTICS  u_statistics;

#define MAX_USER_MSG 256
/* receive and send message buffers */
char snd_msg[MAX_USER_MSG], rcv_msg[MAX_USER_MSG];
/* send and receive msg size parameters */
int snd_msg_size, rcv_msg_size;

void start_ttyrx() {
    short res;

    if (usingIntele)
       return;
 
    res = tty_start_rcv();
    if (res == TTY_CMD_SUCCESS) {
        // success
    } else {
        // failure
    }
}
void stop_ttyrx() {
    short res;

    if (usingIntele)
       return;
 
    res = tty_stop_rcv();
    if (res == TTY_CMD_SUCCESS) {
        // success
    } else {
        // failure
    }
}

string getCharsFromPSAP(int timeout) {
    string retval = "";
    if (usingIntele) {
        char buf[255];
    
    	// look for characters
        time_t response_time = time(NULL);
        while ((time(NULL) - response_time) <= timeout) {
            fd_set rd, wr, er;
            int nfds = 0;
            FD_ZERO(&rd);
            FD_ZERO(&wr);
            FD_ZERO(&er);
            FD_SET(ser_s, &rd);
            nfds = max(nfds, ser_s);
    
            struct timeval select_time;
            select_time.tv_sec = 1;
            select_time.tv_usec = 0;
    
            int r = select(nfds+1, &rd, &wr, &er, &select_time);
            if (r != 0) {
                if (FD_ISSET(ser_s, &rd)) {
                    int res = read(ser_s, buf, 255);
                    if (res == 0) {
                        closeAndExit(ECALL_SERIAL);
                    }
                    if (res > 0) {
                        for (int i=0; i < res; i++) {
                            retval += buf[i];
                            if (debug) logError("ch: " + retval);
                        }
                        return retval;
                    } 
                }
            }
        }
    } else {
        retval = "";
        time_t response_time = time(NULL);
        while ((time(NULL) - response_time) <= timeout) {
            memset(rcv_msg,0,MAX_USER_MSG);
            int res = tty_get_msg(rcv_msg, MAX_USER_MSG-1);
            if (res > 0) {
                for (int i=0; i < res; i++) {
                    retval += rcv_msg[i];
                }
                if (debug) logError("ch: " + retval);
                break;
            }
        }
    }

    // return
    return retval;
}


string getUnsolicited() {
    string retval = "";
    int res;
    char buf[255];
    static string unsol_line = "";

//    while (1) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser_s, &rd);
        nfds = max(nfds, ser_s);

        struct timeval select_time;
        select_time.tv_sec = 1;
        select_time.tv_usec = 0;

        int r = select(nfds+1, &rd, &wr, &er, &select_time);
	if (r != 0) {
            if (FD_ISSET(ser_s, &rd)) {
                res = read(ser_s,buf,255);
                if (res == 0) {
                    closeAndExit(ECALL_SERIAL);
                }
                if (res>0) {
                    int i;
                    for (i=0; i < res; i++) {
                        char ch = buf[i];
                        if (ch == '\r') {
                            ch = '\n';
                        }
                        if (ch == '\n') {
                            if  (unsol_line != "") {
                                if (debug) {
                                    string datetime = getDateTime();
                                    cerr << datetime << " " << unsol_line << endl;
                                }
				retval = unsol_line;
                                unsol_line = "";
                                return retval;
                            }
                        }
                        else {
                            unsol_line += ch;
                            //if (debug) logError("'" + unsol_line + "'");
                        }
                    }
                }  //end if res>0
            }
    //    }  //while (1)
    }

    return retval;
}


//
// cell modem voice call handling
//
#define CALL_IDLE               0
#define CALL_DIALING            1
#define CALL_RINGING            2
#define CALL_BUSY      		3
#define CALL_NOCARRIER      	4
#define CALL_NODIALTONE      	5
#define CALL_CONNECTED          6
#define CALL_INCOMING           7
#define CALL_ERROR              8
#define CALL_ANSWERED           9
int    callState = CALL_IDLE;
void setCallState(int state) { callState = state; }

void answerCall(void)
{
    // log it
    logError("answering call");

    // answer it
    getCellResponse("ata", 1);

    // set state
    setCallState(CALL_ANSWERED);
    if (usingIntele) {
        setCallState(CALL_CONNECTED);
    }
}
void placeCall()
{
    // log it
    logError("placing eCall: " + eCallNumber);

    // place ecall
    if (usingIntele) {
        getCellResponse("atdt" + eCallNumber, 0);
    }
    else {
        getCellResponse("at+cdv=" + eCallNumber, 10);
    }
    setCallState(CALL_DIALING);
}

string escapeCall()
{
    // escape to command prompt
    sleep(1);
/*
    write(ser_s, "+", 1);
    tcflush(ser_s, TCOFLUSH);
    usleep(250000);
    write(ser_s, "+", 1);
    tcflush(ser_s, TCOFLUSH);
    usleep(250000);
    write(ser_s, "+", 1);
    tcflush(ser_s, TCOFLUSH);
*/
    write(ser_s, "+++", 3);
    tcflush(ser_s, TCOFLUSH);
    sleep(2);

    // hopefully return "OK"
    return getCellResponse("", 5); 
}

void hangupCall()
{
    // save state before hangup
    if (callState != CALL_IDLE) {
        hangupReason = callState;
    }

    // do hangup
    if (usingIntele) {
        // escape to command mode
        if ((callState == CALL_CONNECTED) || (callState == CALL_RINGING) || (callState == CALL_DIALING) || (callState == CALL_ERROR)) {
            // wait for any xmt to complete
            sleep(2);
            escapeCall();
        }

        // hangup up
        if (callState != CALL_IDLE) {
            // log it
            logError("hanging up");

            // intele on busy/nodialtone hangs itself up
            //   just snag the ok
            if ((callState == CALL_BUSY) || (callState == CALL_NODIALTONE))   {
                getCellResponse("", 5);
            }
            else {
                // hangup
                getCellResponse("ath", 5);
            }

            // sleep in case we dial again next,
            //   so it doesn't do a flash hook
            sleep(4);
        }

        // set idle
        setCallState(CALL_IDLE);
    }
    else {
        // hangup up
        if (callState != CALL_IDLE) {
            // log it
            logError("hanging up");

            // hangup
            string s = getCellResponse("at+chv", 5);

            // sleep in case we dial again next,
            //   so it doesn't do a flash hook
            sleep(4);

            // set idle
            setCallState(CALL_IDLE);
        }
    }
}

int monitorCall()
{
    if (hangupRequested) {
        //logError("hangup requested by user");
        hangupCall();

        // reset flag
        hangupRequested = false;
    }
    else {
        string s;
        if ((s = getUnsolicited()) != "") {
            if (usingIntele) {
                // log it
                logError("unsolicited: " + s);
    
                // process unsolicited message
                if ((s == "ERROR") || (s == "TTY TIMEOUT")) {
                    setCallState(CALL_ERROR);
                    hangupCall();
                }
                else if (s == "NO DIALTONE") {
                    setCallState(CALL_NODIALTONE);
                    hangupCall();
                }
                else if (s == "NO CARRIER") {
                    setCallState(CALL_NOCARRIER);
                    hangupCall();
                }
                else if (s == "BUSY") {
                    setCallState(CALL_BUSY);
                    hangupCall();
                }
                else if (s == "RINGING") {
                    setCallState(CALL_RINGING);
                }
                else if (s == "CONNECT") {
                    logError("connected as ascii, not baudot");
                    setCallState(CALL_ERROR);
                    hangupCall();
                }
                else if (s == "CONNECT TDD") {
                    setCallState(CALL_CONNECTED);
                }
                else if (s == "RING") {
                    setCallState(CALL_INCOMING);
                }
            }
            else {
                //_OLCC: 5,0,30,0,0,"9193415003",128 
                /*
                    0 active
                    1 held
                    2 dialing (MO call)
                    3 alerting (MO call)
                    4 incoming (MT call)
                    5 waiting (MT call)
                   30 hangup
                */
                static time_t callPlacedTime = 0L;

                string ss = getField(s, " ,", 3);
                if (s == "RING")
                    ss = "4";
                if (ss == "0") {
                    // ********************************************************
                    // the internal modem does not detect busy, but will hangup
                    // after 30 seconds it the line is busy. so we will use
                    // that fact to simulate a busy.
                    //   note that we cant differentiate between busy and
                    //    a pickup, then hangup

                    // save time
                    callPlacedTime = time(NULL);
                    if (callState == CALL_ANSWERED) callPlacedTime = 0L;

                    logError("unsolicited: ACTIVE");
                    setCallState(CALL_CONNECTED);
                }
                else if (ss == "2") {
                    logError("unsolicited: DIALING");
                    setCallState(CALL_DIALING);
                }
                else if (ss == "3") {
                    logError("unsolicited: RINGING");
                    setCallState(CALL_RINGING);
                }
                else if (ss == "4") {
                    logError("unsolicited: INCOMING");
                    setCallState(CALL_INCOMING);
                }
                else if (ss == "30") {
                    // catch case where -HUP came
                    if (callState == CALL_IDLE) {
                        logError("unsolicited: HANGUP");
                    }
                    else {
                        // simulate busy -- see note above
                        if ((time(NULL) - callPlacedTime) < 45) {
                            logError("unsolicited: BUSY");
                            setCallState(CALL_BUSY);
                        }
                        else {
                            logError("unsolicited: HANGUP");
                        }
                        hangupCall();
                    }

                    // clear time
                    callPlacedTime = 0L;
                }
                else {
                   logError("unsolicited: " + ss);
                }
            }
        }
    }
    return callState;
}

int getCallState() { return monitorCall(); }
bool hasCallTerminated() { return (getCallState() == CALL_IDLE); }
bool isCallInProgress() { return (getCallState() != CALL_IDLE); }
bool isCallConnected() { return (getCallState() == CALL_CONNECTED); }
bool isCallRinging() { return (getCallState() == CALL_RINGING); }
bool isCallIncoming() { return (getCallState() == CALL_INCOMING); }



//
// helper routines
//
string itoa(int v) {
    char buf[255];
    sprintf(buf, "%i", v);
    return buf;
}
int countBits(int val, int numBits)
{
    int i;
    int num = 0;
    for (i = 0; i < numBits; i++) {
        if (val & 0x1) num++;
        val >>= 1;
    }
    return num;
}
bool isVoiceAvailable() {
    return fileExists("/tmp/hasVoice");
}
string getLatLong() {
    return sLatLong;
}
int getPreImpactSpeed() {
    return iSpeed;
}
int getSeatsOccupied() {
    return countBits(iSeatsOccupied, 8);
}
string sGetSeatsOccupied() {
    if (iSeatsOccupied == 0xff)
        return "N/A"; 
    return itoa(countBits(iSeatsOccupied, 8));
}
int getSeatsBelted() {
    return countBits(iSeatbelts, 8);
}
int getAirbagsDeployed() {
    return countBits(iAirbagsDeployed, 16);
}
string getMDN() {
    Assoc cellinfo = readAssoc(TempCellInfoFile, " ");
    string mdn = cellinfo["MDN"];
    mdn = mdn.insert(3, ".");
    mdn = mdn.insert(7, ".");
    return mdn;
}
string getVIN() {
   string s = getVin();
   if (s == "N/A") {
       // use charger vin
       s = "2C3CDXBG3CH117290";
   }
   return s;
}
string getOrientation() {
    /*
        Orientation of vehicle. 1 byte.
            Values TBD.
            This is the orientation of the vehicle.
                E.g right-side-up, up-side-down, on-left-side, on-right-side, nose-down, rear-down.
    */

    string s = "";

    // more severe have priority
    if (iOrientation == 0) {
        s = "UPRIGHT";
    }
    else {
        s = "INVERTED";
    }
    return s;
}
string getImpactLocation() {
    /*
        impact_sensors: 2 byte bit field. Bit definitions TBD.
        Bit0: Impact_H – Rollover event2
        Bit1: Impact_I – Rollover
        Bit2: Impact_K – Driver side event
        Bit3: Impact_L – More severe side event
        Bit4: Impact_M – Passenger side event
        Bit5: Impact_N – Reserved
        Bit6: Impact_O – Pedestrian event
        Bit7: Impact_X – Any impact event
        Bit8: Impact_A – Front pretesnioner event
        Bit9: Impact_B – More severe rear event
        Bit10: Impact_C – More severe front event
        Bit11: Impact_D – Less severe side event
        Bit12: Impact_E – Reserved
        Bit13: Impact_F – Less severe front event
        Bit14: Impact_G – Reserved
        Bit15: Spare

        // or [FRONT/BACK/RIGHT/LEFT HIT]
        // or [VEH INVERTED]
    */

    string s = "";

    // more severe have priority
    if (iImpactSensors & 0x0400) {
        s = "FRONT";
    }
    else if (iImpactSensors & 0x0200) {
        s = "REAR";
    }
    else if (iImpactSensors & 0x0004) {
        s = "LEFT";
    }
    else if (iImpactSensors & 0x0010) {
        s = "RIGHT";
    }
    else {
        s = "N/A";
    }

    return s;
}




//
// dump eCall NENA formatted message
//
string dumpEcallMessage(string message) {
    string retval = "";
    char buf[255];
    char *p = buf;

    strcpy(buf, message.c_str());

    // dump
    string CR = "[CR]";
    string LF = "[LF]";
    string NL = "[CR][LF]";
    string LS = "[LS]";
    string FS = "[FS]";
    string CS = "[dd]";			// checksum
    string SPACE = "[Space]";

    // loop thru message
    for ( ; *p != 0; p++) {
        // printf("%02x ", (int)(*p));
        switch (*p) {
            case '\n':	// LF
		retval += LF;
                break;
            case ' ':	// SPACE
		retval += SPACE;
                break;
            case '\r':	// CR
		retval += CR;
                break;
            case 27:	// FS
		retval += FS;
                break;
            case 31:	// LS
		retval += LS;
                break;
            default:
		retval += *p;
                break;
        }
    }
    //cout << "dumpEcallMessage: " << retval << endl;
    return retval;
}

//
// create eCall NENA formatted message
//
string createEcallMessage(string msgType, string line1, ...)
{
    // init list
    va_list args;

    string retval;
    string nena = "";
    char buf[255];
    //cout << "** " << msgType << line1 << line2 << endl;

    string CR = "\r";		// baudot codes
    string LF = "\n"; 
    string NL = "\r\n";
    string LS = "\x1f";
    string FS = "\x1b";
    string CS = "[CS]";
    string SPACE = " ";
    string SOM =  LS + LS + LS + LS;
    string EOM = SPACE + "GA";

    // Initializing args to store all values after line1
    va_start ( args, line1 );

    //
    // [4xLS][NL][CS][MessageType]* 
    // [CR][Line 1 Screen Display]* 
    // [NL][Line n Screen Display]* 
    // [Space]GA 
    //
    // * signifies that message continues into the next
    //   line, shown in one continuous string. New lines
    //   occur only in response to [NL].
    //
    // nena += LS + LS + LS + LS + NL + "00" + msgType;
    nena += NL + "00" + msgType;
    nena += CR + line1;
    string s;
    while ((s = va_arg (args, char *)) != "" ) {
        nena += NL + s;
    }
    nena += SPACE + "GA";

    // clean up list
    va_end ( args );

    // return
    return retval = nena;
}

//
// concats msg2 onto end of msg1
//
string concatEcallMessage(string msg1,  string msg2) 
{
   // erase " GA" from msg1
   msg1.erase(msg1.length()-3, msg1.length());
   return msg1 + msg2;
}



void sendTTYMsg(string s)
{
    logError("sendTTYmsg: " + dumpEcallMessage(s));
    //if (debug) printf("********%s\n********\n", s.c_str());
    if (usingIntele) {
        getCellResponse(s, 0);

        // sleep for how long it takes to send the message
        sleep((s.length()+4)/5); 
    }
    else {
        int res;

        // stop rx and clear rx buffer
        stop_ttyrx();
        tty_get_msg(rcv_msg, MAX_USER_MSG-1);

        // send message
        memcpy(snd_msg, s.c_str(), s.length());
        alarm((s.length()+8)/4);
        if ((res = tty_snd_msg(snd_msg, s.length())) > 0) {
            // success
        } else {
            // failure
            //process_error(res);
        }
        if (alrm_sig) {
            alrm_sig = false;
        }
        alarm(0);

        // sleep for how long it takes to send the message
        // sleep((s.length()+4)/5); 

        // start rx
        start_ttyrx();
    
        logError("sendTTYmsg: done");
    }
}

string getTTYcommand()
{
    if (usingIntele) {
        return "";
    }
    else {
        //return getTTYmessage();
        // **todo
    }
}
string getTTYresponse() { return ""; }



//
// processCmdFromPSAP()
//
bool processCmdFromPSAP(string chars)
{
    bool retval = true;
    char buf[255], buf1[255];
    static string cmd ="";

    // if timeout. clear cmd buffer
    if (chars == "") {
        cmd = "";
        return false;
    }

    // get any new chars and chomp to 4
    cmd += chars;
    if (cmd.length() > 4) {
        cmd.erase(0, cmd.length()-4);
    }

    // make lowercase
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd.length() == 4) {
        logError("processCmdFromPSAP: " + cmd);
    }

    // sasa Send Airbag deployed notice
    if (cmd == "sasa") {
        // ??  -- send same as 'spsp'
        //sprintf(buf, "PASSENGERS %i, BELTED %i",  getSeatsOccupied(), getSeatsBelted());
        sprintf(buf, "PASSENGERS %s, BELTED %i",  sGetSeatsOccupied().c_str(), getSeatsBelted());
        sprintf(buf1, "AIRBAGS OPEN %i", getAirbagsDeployed());
        sendTTYMsg(createEcallMessage("AJ", buf, buf1, ""));
    }

    // scsc Send Callback number
    else if (cmd == "scsc") {
        sendTTYMsg(createEcallMessage("AL", "CALLID " + getMDN(), ""));
    }

    // sdsd Send Delta-v
    else if (cmd == "sdsd") {
        // ??  - send same as 'svsv'
        sprintf(buf, "SPEED %iMPH DELTA-V %iMPH", getPreImpactSpeed(), getSpeed());
        sendTTYMsg(createEcallMessage("AH", buf, ""));
    }

    // sese Send Engine off, a physical operation command
    else if (cmd == "sese") {
        controlCar("engine", "off");
        sendTTYMsg(createEcallMessage("AK", "DONE", ""));
    }

    // sgsg Switch to verbose communications (reserved)
    else if (cmd == "sgsg") {
        // reserved
    }

    // shsh Switch to non-verbose communications (reserved)
    else if (cmd == "shsh") {
        // reserved
    }

    // sksk Terminate call in its entirety, a physical operation command
    else if (cmd == "sksk") {
        sendTTYMsg(createEcallMessage("AK", "DONE", ""));
        hangupCall();
    }

    // slsl Send Location (using current event description Message Type)
    else if (cmd == "slsl") {
        sendTTYMsg(createEcallMessage("AA", "VEH CRASH", sLatLong, ""));
    }

    // soso Send Orientation of vehicle (inverted or not inverted)
    else if (cmd == "soso") {
        sprintf(buf, "%s HIT", getImpactLocation().c_str());
        sprintf(buf1, "VEH %s", getOrientation().c_str());
        sendTTYMsg(createEcallMessage("AI", buf, buf1, ""));
    }

    // spsp Send Passenger tally including seat belt status
    else if (cmd == "spsp") {
        //sprintf(buf, "PASSENGERS %i, BELTED %i",  getSeatsOccupied(), getSeatsBelted());
        sprintf(buf, "PASSENGERS %s, BELTED %i",  sGetSeatsOccupied().c_str(), getSeatsBelted());
        sprintf(buf1, "AIRBAGS OPEN %i", getAirbagsDeployed());
        sendTTYMsg(createEcallMessage("AJ", buf, buf1, ""));
    }

    // ssss Send Side on which vehicle was impacted
    else if (cmd == "ssss") {
        sprintf(buf, "%s HIT", getImpactLocation().c_str());
        sprintf(buf1, "VEH %s", getOrientation().c_str());
        sendTTYMsg(createEcallMessage("AI", buf, buf1, ""));
    }

    // stst Switch to Talk (voice) mode
    else if (cmd == "stst") {
        if (isVoiceAvailable()) {
            switchToVoice();
        } else {
            sendTTYMsg(createEcallMessage("AG", "VOICE CHANNEL NOT AVAILABLE", ""));
        }
    }

    // susu Sends Unlock doors command, a physical operation command
    else if (cmd == "susu") {
        controlCar("locks", "unlock");
        sendTTYMsg(createEcallMessage("AK", "DONE", ""));
    }

    // svsv Send Velocity (pre-impact)
    else if (cmd == "svsv") {
        // ??  - send same as 'sdsd'
        sprintf(buf, "SPEED %iMPH DELTA-V %iMPH", getPreImpactSpeed(), getSpeed());
        sendTTYMsg(createEcallMessage("AH", buf, ""));
    }

    // vvvv[nn] Limit VehicleVelocity to [nn], physical operation command
    else if (cmd == "vvvv") {
        // unavailable
    }

    // swsw Send supplemental Website access information (Type AG) message
    else if (cmd == "swsw") {
        // ??
	// MORE INFO SEE CASE NUM
	// [VIN] [EventID] AT
	// WWW.CALLCENTER.COM GA
        //sprintf(buf, "[%s] [%i] AT", getVIN().c_str(), 0x01);
        //sprintf(buf1, "WWW.AUTONETMOBILE.COM");
        //sendTTYMsg(createEcallMessage("AG", "MORE INFO SEE CASE NUM", buf, buf1, ""));
    }

    // sxsx Start Sending eXpanded parameter set
    else if (cmd == "sxsx") {
        // unavailable
    }

    // invalid command
    else {
        retval = false;
    }

    // if successful, forget the command
    if (retval == true) {
        //if (debug) logError("processCmdFromPSAP: " + cmd);
        cmd = "";
    }
    return retval;
}

void processCmdsFromPSAP()
{
    if (debug) logError("waiting for more commands from PSAP");

    // wait for up to 2 minutes for a command
    time_t response_time = time(NULL);
    while ((time(NULL) - response_time) <= 2*60) {
        if (usingIntele) {
            if  (callState == CALL_IDLE) {
                return;
            }
        } 
        else if (hasCallTerminated()) {
            return;
        }

        // get chars, check for valid command
        string cc;
        if ((cc = getCharsFromPSAP(5)) != "") {
           response_time = time(NULL);
           //processCmdFromPSAP(cc); 
        }
        processCmdFromPSAP(cc); 
    }

    // timed out
    logError("no command recv'd from PSAP for 2 minutes");
    hangupCall();
}

//
// switchToVoice()
//    return true if switch back to data
//
bool switchToVoice()
{
    if (!usingIntele) {
        string cmd = "";
        string cc = "";
    
        // log fact
        logError("switching to voice");
    
        // while talking
        while (!hasCallTerminated()) {
            // check for switch back to data mode
            //     multiple taps on space bar 
            if ((cc = getCharsFromPSAP(5)) != "") {
                cmd += cc;
                if (cmd.length() > 2) {
                    cmd.erase(0, cmd.length()-2);
                }
                if (cmd == "  ") {
                    // log fact
                    logError("processCmdFromPSAP: [multiple spaces]");
                    logError("switching to data");
                    sendTTYMsg(createEcallMessage("AK", "DONE", ""));
    
                    return true;
                }
            } else {
                cmd = "";
            }
        }
    }
    else {
        string cmd = "";
        string cc = "";
    
        // log fact
        logError("switching to voice");
    
        // wait for up to 2 minutes for a char to keep talking
        time_t response_time = time(NULL);
        while ((time(NULL) - response_time) < 2*60) {
    
            if (callState == CALL_IDLE) {
                return false;
            }
    
            // check for switch back to data mode
            //     multiple taps on space bar 
            if ((cc = getCharsFromPSAP(5)) != "") {
                response_time = time(NULL);
                cmd += cc;
                if (cmd.length() > 2) {
                    cmd.erase(0, cmd.length()-2);
                }
                if (cmd == "  ") {
                    // log fact
                    logError("processCmdFromPSAP: [multiple spaces]");
                    logError("switching to data");
                    sendTTYMsg(createEcallMessage("AK", "DONE", ""));
    
                    return true;
                }
                // just for demo, since intele cant detect hangup
                else if (cmd == "SK") {
                    logError("processCmdFromPSAP: sk");
                    hangupCall();
                    return false;
                }
            } else {
                cmd = "";
            }

            // HUP
            if (hangupRequested) {
                hangupCall();
                hangupRequested = false;
            }
        }
    
        // timed out
        logError("timed out waiting for char from PSAP");
        hangupCall();
    }
}


//
// send preamble
//
bool sendPreamble()
{
    if (usingIntele) {
        string preamble = " ";

        // wait up to 5 minutes or until connected
        time_t start_time = time(NULL);
        while ((time(NULL) - start_time) < 5*60) {
            // send it
            sendTTYMsg(preamble);

            // connected?
            if (isCallConnected()) {
               return true;
            }

            // disconnected?
            if (hasCallTerminated()) {
               return false;
            }
        }
    }
    else {
	// todo:
	//  just return true for now, so we can get on to the introduction
        return true;
  
        //string preamble = FS + FS + FS + FS + FS + FS;
        string preamble = "\x1b\x1b\x1b\x1b\x1b\x1b";
    
        // send it up to 4x or until a char recvd
        //for (int i = 0; i < 4; i++) { 

        // wait up to 5 minutes or until a char recvd
        time_t start_time = time(NULL);
        while ((time(NULL) - start_time) < 5*60) {
            // send it
            sendTTYMsg(preamble);

            // wait for response
            if (getCharsFromPSAP(5) != "") {
                return true;
            }

            // disconnected?
            if (hasCallTerminated()) {
               return false;
            }
        }
    }
    return false;
}

//
// send introduction
//
bool sendTTYintroduction()
{
    // build intro msg
    //string intro = createEcallMessage("AA", "VEH CRASH", sLatLong, "");
    //string intro = concatEcallMessage(intro, createEcallMessage("AM", "TYPE STST FOR GO-TO-VOICE", ""));

    string CR="\r";
    string NL="\r\n";
    string iintro = "";
    iintro += NL + "VEH CRASH";
    iintro += NL + getLatLong();
    iintro += NL + getImpactLocation() + " HIT";
    iintro += NL + "VEH " + getOrientation();
    if (sGetSeatsOccupied() != "N/A") {
        iintro += NL + "PASSENGERS " + sGetSeatsOccupied();
    }
    iintro += NL + "DRIVER BELTED " + "YES";
    iintro += NL + "AIRBAGS OPEN " + itoa(getAirbagsDeployed());
    iintro += NL + "IMPACT SPEED " + itoa(getPreImpactSpeed()) + "MPH";
    iintro += NL + "VIN " + getVIN();
    if (isVoiceAvailable()) {
        iintro += NL + "TYPE STST FOR GO-TO-VOICE";
    }
    else {
        iintro += NL + "VOICE CHANNEL NOT AVAILABLE";
    }
    iintro += " GA";
    //string intro = createEcallMessage("XX", iintro, "");
    string intro = iintro;

    // repeat for up to 5 minutes
    time_t start_time = time(NULL);
    while ((time(NULL) - start_time) < 5*60) {
        // send msg
        sendTTYMsg(intro);

        // only terminate intro on a valid command recv'd
        //     give them 20 seconds after the intro to send 1st char,
        //     then 5 seconds in-between chars thereafter
        //     "" is a timeout
        string cc = getCharsFromPSAP(20);
        while (cc != "") {
           if (processCmdFromPSAP(cc) == true) {
               return true;
           }
           //cc = getCharsFromPSAP(5);
           cc = getCharsFromPSAP(10);
        }

        // no valid command yet from PSAP
        // clear cmd buffer on timeout
        processCmdFromPSAP(cc);

        // call terminated?
        if (usingIntele) { 
            if (hangupRequested) {
                sleep(10);
                hangupCall();
                hangupRequested = false;
                return false;
            }
        }
        else { 
            // did we lose the call?
            if (hasCallTerminated()) {
                return false;
            }
        }
    }

    // return failure
    return false;
}


void stopMonitor()
{
    touchFile(DontConnectFile);
    touchFile(NoCellCommandsFile);
    appendToFile(PppLogFile, "ecall:killed");
    system("pkill pppd");
    system("pkill -USR1 respawner");
    sleep(1);
    system("pkill ow_talker");
    system("pkill gps_mon");
    sleep(1);

    // swap drivers
    if (usingIntele) {
        system("rmmod hso");
        sleep(2);
        system("modprobe pl2303");
        sleep(2);
    }
}
void startMonitor()
{
    // swap drivers
    if (usingIntele) {
        system("rmmod pl2303");
        sleep(2);
        system("modprobe hso");
        sleep(2);
    }
    unlink(DontConnectFile);
    unlink(NoCellCommandsFile);
    system("pkill -USR2 respawner");
}


void waitForPSAPCallback(int timeout)
{
    // log
    logError("waiting for callback from PSAP");

    // wait for PSAP to callback
    time_t start_time = time(NULL);
    while ((time(NULL) - start_time) < timeout) {
        if (isCallIncoming()) {
            // answer it 
            answerCall();

            // start rx
            start_ttyrx();

            while (isCallInProgress()) {
                if (isCallConnected()) {
                    // go right to voice
                    if (switchToVoice()) {
                        processCmdsFromPSAP();
                    }

                    // hangup ecall
                    hangupCall();
                }
            }

            // reset timer
            start_time = time(NULL);

            // log
            logError("waiting for callback from PSAP");
        }
    }

    // log 
    logError("waiting for callback ended");
}



/*
args
1: airbags (int) 0x8000 if airbags deployed, 0 if not deployed
2: impact sensors (int)  0x500 highest bit 
3: orientation (dec) 0x0 (normal)
4: speed (dec) (128ths km/s) (/128 to get km)
5: seats occupied (int) 0xff (not available)
6: seatbelts (int) 0x1 driver selt belt fastened
*/
int main(int argc, char *argp[])
{
    bool bWaitForCallback = false;
    bool bLogEvent = false;
    bool fail = true;

    // command line args
    if (argc < 6) {
         cout << "Usage: 6 args (int iAirbagsDeployed, iImpactSensors, iOrientation, iSpeed, iSeatsOccupied, iSeatbelts)" << endl;
         exit(0);
    }

    if ((string(argp[1]) != "-w") && (string(argp[1]) != "-f") && (string(argp[1]) != "-p")) {
        iAirbagsDeployed = atoi(argp[1]);
        iImpactSensors   = atoi(argp[2]);
        iOrientation     = atoi(argp[3]);
        iSpeed           = (int)(atoi(argp[4])/128./1.60934 +.5);
        iSeatsOccupied   = atoi(argp[5]);
        iSeatbelts       = atoi(argp[6]);
    } else {
        iAirbagsDeployed = atoi(argp[2]);
        iImpactSensors   = atoi(argp[3]);
        iOrientation     = atoi(argp[4]);
        iSpeed           = (int)(atoi(argp[5])/128./1.60934 +.5);
        iSeatsOccupied   = atoi(argp[6]);
        iSeatbelts       = atoi(argp[7]);
    }
    if (string(argp[1]) == "-w") {
        bWaitForCallback = true;
    }
    if ((string(argp[1]) == "-f") || (string(argp[1]) == "-p")) {
        bLogEvent = true;
        if (string(argp[1]) == "-f") {
            fail = true;
        } else {
            fail = false;
        }
    }

    // open log file 
    setLogErrorFile(EcallLogFile);

    // start
    logError("entering eCall");

    // get ecall number
    eCallNumber = features.getFeature(EcallNumber);
    eCallModem  = features.getFeature(EcallModem);
    if (eCallModem == "intele") {
        logError("using intele modem");
        usingIntele = true;
    }
    else {
        logError("using internal modem");
        usingIntele = false;
    }


    // get gps_location
    if (!fileExists(GpsPositionFile)) {
        logError("no gps_position");

        // notify support
        logError("sending Event: eCall FAILED notification");
	system("log_event 'NotifyOfCrash FAILED No gps position'");

        // cleanup
        logError("exiting eCall");
        //closeAndExit(0);
        return ECALL_NO_GPS_POSITION;
    }

    // updateEcallFile
    updateFile(EcallFile, "running");

    // get gps position
    string gpsPosition = getStringFromFile(GpsPositionFile);
    if (debug) logError("gps position: " + gpsPosition);
    double latitude, longitude;
    sscanf(gpsPosition.c_str(),"%lf/%lf", &latitude, &longitude);
    sprintf(sLatitude, "%s%010.6lf", (latitude >= 0) ? "N" : "S",  fabs(latitude));
    sprintf(sLongitude, "%s%010.6lf", (longitude >= 0) ? "E" : "W",  fabs(longitude));
    sprintf(sLatLong, "%s:%s", sLatitude, sLongitude);


    // open vic 
    vic.openSocket();

    // set path
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);

    // catch signals
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onHup);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, onAlarm);

    // stop monitor
    stopMonitor();

    // flush events
    system("pkill -USR1 event_logger");

    // set modem
    if (usingIntele) {
        serial_device = "ttyUSB0";
    } else { 
        serial_device = "celldiag";
    }

    // open port
    openPort(serial_device.c_str());

    // get modem into correct mode
    if (usingIntele) {
        if (getCellResponse("at", 2) != "OK") {
            // wait for the 'at' to be sent over the modem
            sleep(2);
    
            // escape out of data mode
            if (escapeCall() != "OK") {
                logError("ERROR: Can't talk to modem. Exiting");
                closeAndExit(ECALL_SERIAL);
            }
        }

        // reset modem 
        getCellResponse("atz", 5);

        // turn off echo 
        getCellResponse("ate0", 5);

        // turn off speaker
        getCellResponse("atm0", 5);
    }
    else {    
        // throw away old unsolicited responses
        getCellResponse("at", 5);

        // turn on unsolicited messages
        getCellResponse("at_olcc=1", 5);

        // turn on rcv level
        getCellResponse("at_opcmprof=6", 5);
        getCellResponse("at+clvl=1", 5);
        getCellResponse("at_opcmcalib=\"rxvol\",0", 5);

        // start codec
        system("codec &");

        // set baud to 45
        tty_get_config(&u_config);
        u_config.baud_rate = BAUD_L;
        tty_set_config(u_config);
        if (debug) {
           tty_get_config(&u_config);
           printf("BAUD RATE: %d\n", (int ) u_config.baud_rate);
        }
    }


    // log params
    logError("******** params from vic ********");
    logError("location: " + getLatLong());
    logError("pre-impact speed: " + itoa(getPreImpactSpeed()) + "MPH");
    logError("seats occupied: " + sGetSeatsOccupied());
    logError("seats belted: " + itoa(getSeatsBelted()));
    logError("airbags deployed: " + itoa(getAirbagsDeployed()));
    logError("orientation: " + getOrientation());
    logError("impact location: " + getImpactLocation());
    logError("mdn: " + getMDN());
    logError("vin: " + getVIN());
    logError("********** end params ***********");


    //
    //    1. When we call 911, we should wait up to 5 minutes for a TTY session to establish.
    //    2. We should retry a maximmum of 3 times.
    //       If still unsucessusful we should notify our call center so we can take appropriatte acton.
    //    3. After the call is terminated we should go into auto-answer mode,
    //       so that if the PSAP needs to return the call they can. We should go right into bluetooth voice mode.
    // 

    if (!bWaitForCallback && !bLogEvent) {
        // place ecall
        placeCall();

        // wait up to 5 minutes for call to connect
        time_t start_time = time(NULL);
        while ((time(NULL) - start_time) < 5*60) {
            if (isCallConnected() || hasCallTerminated()) {
                break;
            }

            // intele preamble
            if (usingIntele) {
                sleep(2);
                break;
            }
        }

        // if connected, send preamble/introduction
        //if (!usingIntele && isCallConnected()) {
        if (usingIntele || (!usingIntele && isCallConnected())) {
            // send preamble
            logError("sending preamble");
            if (sendPreamble() == false) {
                //logError("no response to preamble");
                hangupCall();
            }
        }

        if (isCallConnected()) {
            // send introduction
            logError("sending introduction");
            if (sendTTYintroduction() == true) {
                processCmdsFromPSAP();
                fail = false;
                logError("eCall passed");
            } else {
                fail = true;
                logError("eCall failed");
            }
    	}

        // hangup ecall
        hangupCall();
    }

    if (bWaitForCallback) {
        // wait for callbacks only if voice is available
        if (!isVoiceAvailable()) {
            logError("voice is not available");
            closeAndExit(ECALL_NO_VOICE);
        } else {
            logError("voice is available");
    
            // wait for 5 minutes for possible PSAP callback
            waitForPSAPCallback(5*60);
    
            // restart monitor, so it brings up ppp and sends event
            logError("starting monitor to send events");
            updateFile(EcallFile, "suspended");
            closePort();
            startMonitor();
    
            // wait for 2 minutes or eventcount go away
            time_t wait_time = time(NULL);
            while ((time(NULL) - wait_time) < 2*60) {
                if (!fileExists(EventCountFile)) {
                    break;
                }
    
                // play nicely with others
                sleep(2);
            }
    
            // stopMonitor
            logError("stopping monitor");
            updateFile(EcallFile, "running");
            stopMonitor();
            openPort(serial_device.c_str());
    
            // wait for 60 minutes for possible PSAP callback
            waitForPSAPCallback(60*60);
        }
    }

    if (bLogEvent) {
        // log event
        string eevent = "NotifyOfCrash";
        if (fail)
            eevent += " FAILED";
        else
            eevent += " PASSED";
        eevent += " gps:" + gpsPosition;
        eevent += " prespeed:" + itoa(getPreImpactSpeed());
        eevent += " occupants:" + sGetSeatsOccupied();
        eevent += " seatbelts:" + itoa(getSeatsBelted());
        eevent += " airbags:" + itoa(getAirbagsDeployed());
        eevent += " orientation:" + getOrientation();
        eevent += " impacts:" + getImpactLocation();
        if (isVoiceAvailable())
            eevent += " voice:Y";
        else
            eevent += " voice:N";

        logError("logging event: " + eevent);
        string command = "log_event '" + eevent + "'";
        system(command.c_str());
     }


    // cleanup
    if (fail == false) {
        closeAndExit(ECALL_PASS);
    } else {
        closeAndExit(ECALL_FAIL);
    }
    return 0;
}


//
// vic i/f
//
void controlCar(string dev_s, string act_s) {
    Byte buffer[256];
    Byte dev = 0;
    Byte act = 0;
    if (dev_s == "locks") {
        dev = 1;
        if (act_s == "lock") {
            act = 1;
        }
        else if (act_s == "unlock") {
            act = 2;
        }
        else if (act_s == "unlock_driver") {
            act = 3;
        }
    }
    else if (dev_s == "engine") {
        dev = 2;
        if (act_s == "on") {
            act = 1;
        }
        else if (act_s == "off") {
            act = 2;
        }
    }
    else if (dev_s == "trunk") {
        dev = 3;
        if (act_s == "open") {
            act = 1;
        }
        else if (act_s == "close") {
            act = 2;
        }
    }
    else if (dev_s == "leftdoor") {
        dev = 4;
        if (act_s == "open") {
            act = 1;
        }
        else if (act_s == "close") {
            act = 2;
        }
    }
    else if (dev_s == "rightdoor") {
        dev = 5;
        if (act_s == "open") {
            act = 1;
        }
        else if (act_s == "close") {
            act = 2;
        }
    }
    else if (dev_s == "panic") {
        dev = 6;
        if (act_s == "on") {
            act = 1;
        }
        else if (act_s == "off") {
            act = 2;
        }
    }
    if ((dev != 0) and (act != 0)) {
        buffer[0] = dev;
        buffer[1] = act;
        vic.sendRequest(app_id, 0x21, 2, buffer);
        int len = getExpectedResponse(0x21, buffer, 2, true);
        if (len > 0) {
            sleep(1);
            //getStatus();
        }
    }
    else {
        cerr << "Invalid device or action" << endl;
        exit(1);
    }
}

int getVicResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    vic.sendRequest(app_id, cmnd_id);
    int len = getExpectedResponse(cmnd_id, buffer, expected, exact);
    return len;
}

int getExpectedResponse(int cmnd_id, Byte *buffer, int expected, bool exact) {
    int len = vic.getResp(buffer,1);
    if (len == 0) {
        cerr << "No response to command:" << cmnd_id << endl;
    }
    else if (len < 2) {
        cerr << "Response too short for command:" << cmnd_id << " len=" << len << endl;
        len = 0;
    }
    else if (buffer[0] != app_id) {
        cerr << "Response has wrong app_id:" << app_id << " got:" << buffer[0] << endl;
        len = 0;
    }
    else if (buffer[1] != cmnd_id) {
        cerr << "Response has wrong cmnd_id" << cmnd_id << " got:" << buffer[1] << endl;
        len = 0;
    }
    else if (exact and (len != expected)) {
        cerr << "Invalid response length for command:" << cmnd_id << " len:" << len << " expected:" << expected << endl;
        len = 0;
    }
    else if (!exact and (len < expected)) {
        cerr << "Response to short for command:" << cmnd_id << " len:" << len << " expected:" << expected << endl;
        len = 0;
    }
    return len;
}

void getTime() {
     struct tm tm;
     vic.getRtc(app_id, &tm);
     printf("Date/time: %04d/%02d/%02d %02d:%02d:%02d\n",
            tm.tm_year, tm.tm_mon, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void setTime() {
    string time_str = backtick("cell_shell gettime");
    int year, mon, mday, hour, min, sec;
    int pos = time_str.find("\n");
    time_str.erase(0,pos);
    sscanf(time_str.c_str(), "%d/%d/%d %d:%d:%d",
           &year, &mon, &mday, &hour, &min, &sec);
    struct tm tm;
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = mday;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    struct timeval tv;
    tv.tv_sec = timegm(&tm);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    vic.setRtc(app_id);
}

void getVersion() {
    Byte buffer[64];
    int len = getVicResponse(0x0A, buffer, 6, true);
    if (len > 0) {
        int hw_maj = buffer[2];
        int hw_min = buffer[3];
        int fw_maj = buffer[4];
        int fw_min = buffer[5];
        cout << "VIC Version:";
        cout << " HW:" << hw_maj << "." << hw_min;
        cout << " FW:" << fw_maj << "." << fw_min << endl;
    }
}

void getStatus() {
    Byte buffer[256];
    int len = getVicResponse(0x20, buffer, 9, true);
    if (len > 0) {
        cout << "Car state:" << endl;
        Byte status = buffer[2];
        Byte locks = buffer[3];
        Byte doors = buffer[4];
        unsigned int ign_time = ((int)buffer[5] << 8) | buffer[6];
        Byte batval = buffer[7];
        if (status & (1 << 0)) {
            cout << "  ignition on for " << ign_time << " min" << endl;
        }
        else {
            cout << "  ignition off for " << ign_time << " min" << endl;
        }
        if (status & (1 << 1)) {
            cout << "  engine on" << endl;
        }
        else {
            cout << "  engine off" << endl;
        }
        if (status & (1 << 2)) {
            cout << "  vehicle is moving" << endl;
        }
        if (status & (1 << 3)) {
            cout << "  emergency occured" << endl;
        }
        if (status & (1 << 4)) {
            cout << "  check engine status set" << endl;
        }
        if (status & (1 << 5)) {
            cout << "  driver's seatbelt fastened" << endl;
        }
        else {
            cout << "  driver's seatbelt unfastened" << endl;
        }
        if (status & (1 << 6)) {
            cout << "  panic is on" << endl;
        }
        string locks_state = "unlocked";
        if (locks == 0x1F) {
           locks_state = "locked";
        }
        else if (locks == 0x1E) {
           locks_state = "driver_unlocked";
        }
        cout << "  locks " << locks_state << endl;
        if (doors & (1 << 2)) {
            cout << "  leftdoor opened" << endl;
        }
        else {
            cout << "  leftdoor closed" << endl;
        }
        if (doors & (1 << 3)) {
            cout << "  rightdoor opened" << endl;
        }
        else {
            cout << "  rightdoor closed" << endl;
        }
        if (doors & (1 << 4)) {
            cout << "  trunk opened" << endl;
        }
        else {
            cout << "  trunk closed" << endl;
        }
        printf("  batval %.1f\n", batval/10.);
    }
}

void getTirePressures() {
    Byte buffer[64];
    int len = getVicResponse(0x23, buffer, 9, true);
    if (len > 0) {
        int front_ideal = buffer[2];
        int rear_ideal = buffer[3];
        int left_front = buffer[4];
        int right_front = buffer[5];
        int left_rear = buffer[6];
        int right_rear = buffer[7];
        int spare = buffer[8];
        cout << "Tire pressures:" << endl;
        cout << "  front_ideal: " << front_ideal << "psi" << endl;
        cout << "  rear_ideal: " << rear_ideal << "psi" << endl;
        cout << "  left_front: " << left_front << "psi" << endl;
        cout << "  right_front: " << right_front << "psi" << endl;
        cout << "  left_rear: " << left_rear << "psi" << endl;
        cout << "  right_rear: " << right_rear << "psi" << endl;
        cout << "  spare: " << spare << "psi" << endl;
    }
}

void getOilLife() {
    Byte buffer[64];
    int len = getVicResponse(0x24, buffer, 3, true);
    if (len > 0) {
        int oil_life = buffer[2];
        cout << "Oil life: " << oil_life << "%" << endl;
    }
}

void getFuelLevel() {
    Byte buffer[64];
    int len = getVicResponse(0x25, buffer, 3, true);
    if (len > 0) {
        int liters = buffer[2];
        int gals = (int)(liters * 0.2642 + 0.5);
        cout << "Fuel level: " << liters << "L (" << gals << "gal)" << endl;
    }
}

string getVin() {
    string retval = "N/A";
    Byte buffer[64];
    int len = getVicResponse(0x26, buffer, 3, false);
    if (len > 0) {
        int vin_len = buffer[2];
        if (len != (vin_len+3)) {
            // cerr << "Invalid VIN length" << endl;
        }
        else if (vin_len == 0) {
            // cout << "VIN: (no vin given)" << endl;
        }
        else {
            char vin[64];
            strncpy(vin, (char *)(&buffer[3]), vin_len);
            vin[vin_len] = '\0';
            // cout << "VIN: " << vin << endl;
            retval = vin;
        }
    }
    return retval;
}

/*
void getSpeed() {
    Byte buffer[64];
    int len = getResponse(0x27, buffer, 4, true);
    if (len > 0) {
        unsigned int speed_vic = ((int)buffer[2]<<8) | buffer[3];
        if (speed_vic != 0xFFFF) {
            double speed = speed_vic / 128.;
            int mph = (int)(speed / 1.60934 + 0.5);
            int kph = (int)(speed + 0.5);
            cout << "Speed: " << kph << "kph (" << mph << "mph)" << endl;
        }
        else {
            cout << "Speed: unknown" << endl;
        }
    }
}
*/
int getSpeed() {
    Byte buffer[64];
    int len = getVicResponse(0x27, buffer, 4, true);
    if (len > 0) {
        unsigned int speed_vic = ((int)buffer[2]<<8) | buffer[3];
        if (speed_vic != 0xFFFF) {
            double speed = speed_vic / 128.;
            int mph = (int)(speed / 1.60934 + 0.5);
            //int kph = (int)(speed + 0.5);
            //cout << "Speed: " << kph << "kph (" << mph << "mph)" << endl;
            return mph;
        }
        else {
            //cout << "Speed: unknown" << endl;
            return 0;
        }
    }
    return 0;
}



void getOdometer() {
    Byte buffer[64];
    int len = getVicResponse(0x28, buffer, 5, true);
    if (len > 0) {
        long int odom = ((long int)buffer[2]<<16) +
                        ((long int)buffer[3]<<8) +
                        buffer[4];
        double km = odom / 10.;
        double mi = km / 1.60934;
        printf("Odometer: %.1fkm (%.1fmi)\n", km, mi);
    }
}

void closeAndExit(int exitstat) {
    if (callState != CALL_IDLE) {
        // terminate ecall
        hangupCall();
    }

    if (!usingIntele) {
        // turn off unsolicited messages
        getCellResponse("at_olcc=0", 5);

        // kill codec
        system("pkill codec");
    }

    // close
    closePort();

    // remove eCall flag
    unlink(EcallFile);

    // restart monitor
    startMonitor();

    // exit
    logError("exiting eCall");
    exit(exitstat);
}

void openPort(const char *port) {
    string dev = "/dev/";
    dev += port;
    if (!lock(port)) {
        logError("Serial device '" + (std::string)port + "' already in use");
        //cout << "Serial device '" << port << "' already in use" << endl;
        closeAndExit(1);
    }
    ser_s = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser_s < 0) {
        logError("open error: " + serial_device);
        remove(lockfile.c_str());
        closeAndExit(-1);
    }
    if (tcgetattr(ser_s,&oldtio)) {
        cerr << "Failed to get old port settings" << endl;
    }

    // set new port settings for canonical input processing 
    struct termios newtio;
    if (usingIntele) {
        newtio.c_cflag = B300 | CS8 | CLOCAL | CREAD;
    } else {
        newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    }
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VMIN]=1;
    newtio.c_cc[VTIME]=1;
    if (tcsetattr(ser_s,TCSANOW,&newtio)) {
        cerr << "Failed to set new port settings" << endl;
    }
    if (tcflush(ser_s, TCIOFLUSH)) {
        cerr << "Failed to flush serial port" << endl;
    }
    if (fcntl(ser_s, F_SETFL, FNDELAY)) {
        cerr << "Failed to set fndelay on serial port" << endl;
    }
}

void closePort() {
    if (ser_s >= 0) {
        // restore old port settings
        tcsetattr(ser_s,TCSANOW,&oldtio);
        tcflush(ser_s, TCIOFLUSH);
        close(ser_s);        //close the com port
        if (lockfile != "") {
            remove(lockfile.c_str());
        }
    }
}

bool lock(const char *dev) {
    bool locked = false;
    lockfile = "/var/lock/LCK..";
    lockfile += dev;
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    if (fd >= 0) {
        close(fd);
        locked = true;
    }
    return locked;
}

void killConnection() {
    cerr << getDateTime() << " Disabling connections" << endl;
    touchFile(DontConnectFile);
    touchFile(NoCellCommandsFile);
    system("pkill pppd");
    sleep(1);
}

void resumeConnection() {
    cerr << getDateTime() << " Re-enabling connections" << endl;
    unlink(NoCellCommandsFile);
    unlink(DontConnectFile);
}

string getCellResponse(string command) {
    return getCellResponse(command, "OK", 1);
}

string getCellResponse(string command, int timeout) {
    //return getCellResponse(command, "OK\n", timeout);
    return getCellResponse(command, "OK", timeout);
}

string getCellResponse(string command, string ans, int timeout) {
    bool done = false;
    string outstr = "";
    string cmndstr = command;
    cmndstr += "\r";
    time_t start_time = time(NULL);
    write(ser_s, cmndstr.c_str(), cmndstr.size());

    // if timeout is zero, return immediately
    if (timeout == 0) {
        return "";
    }

    while (!done) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser_s, &rd);
        nfds = max(nfds, ser_s);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (r > 0) {
            if (FD_ISSET(ser_s, &rd)) {
                char buf[255];
                // getting multiple messages in buffer
                //    process a char at a time to eliminate that
                //int res = read(ser_s,buf,255);
                int res = read(ser_s,buf,1);
                int i;
                if (res>0) {
                    for (i=0; i < res; i++) {
                        char ch = buf[i];
                        if (ch == '\r') {
                            ch = '\n';
                        }
                        outstr += ch;
                        if (ch == '\n') {
                            if (outstr.find(ans) != string::npos) {
                                done = true;
                            }
                        }
                    }
                }  //end if res>0
            }
        }
        else {
            if ((time(NULL) - start_time) >= timeout) {
                if (debug) {
                    cerr << "Timeout:" << command << endl;
                    while ((outstr.size() > 0) and (outstr.at(0) == '\n')) {
                         outstr.erase(0,1);
                    }
                    while ((outstr.size() > 0) and (outstr.at(outstr.size()-1) == '\n')) {
                        outstr.erase(outstr.size()-1);
                    }
                    size_t pos;
                    while ((pos=outstr.find("\n\n")) != string::npos) {
                        outstr.erase(pos,1);
                    }
                    cerr << outstr << endl;
                }
                done = true;
            }
        }
    }  //while (!done)
    while ((outstr.size() > 0) and (outstr.at(0) == '\n')) {
         outstr.erase(0,1);
    }
    while ((outstr.size() > 0) and (outstr.at(outstr.size()-1) == '\n')) {
        outstr.erase(outstr.size()-1);
    }
    size_t pos;
    while ((pos=outstr.find("\n\n")) != string::npos) {
        outstr.erase(pos,1);
    }
    cmndstr = command + "\n";
    if (strncmp(outstr.c_str(),cmndstr.c_str(),cmndstr.size()) == 0) {
        outstr = outstr.substr(cmndstr.size());
    }
//    if (outstr.find("ERROR") != string::npos) {
//        cerr << command << endl;
//    }
    if ((pos=outstr.find("\nOK")) != string::npos) {
        outstr.erase(pos);
    }
    while ((pos=outstr.find("\n")) != string::npos) {
        outstr.replace(pos,1,";");
    }
    if (debug) logError("outstr: '" + outstr + "'");
    return outstr;
}

string getRestOfLine() {
    bool done = false;
    string outstr = "";
    while (!done) {
        fd_set rd;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_SET(ser_s, &rd);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        nfds = max(nfds, ser_s);
        int r = select(nfds+1, &rd, NULL, NULL, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (r > 0) {
            if (FD_ISSET(ser_s, &rd)) {
                char buf[255];
                int res = read(ser_s,buf,255);
                int i;
                if (res>0) {
                    for (i=0; i < res; i++) {
                        char ch = buf[i];
                        if (ch == '\r') {
                            ch = '\n';
                        }
                        if (ch == '\n') {
                            done = true;
                            break;
                        }
                        outstr += ch;
                    }
                }  //end if res>0
            }
        }
        else {
            done = 1;
        }
    }  //while (!done)
    return outstr;
}

bool beginsWith(string str, string match) {
    return (strncmp(str.c_str(), match.c_str(), match.size()) == 0);
}

void onTerm(int parm) {
    logError("TERM recvd");
    closeAndExit(0);
}

void onHup(int parm) {
    logError("HUP recvd");
    hangupRequested = true;
    signal(SIGHUP, onHup);
}

void onAlarm(int sig) {
    logError("ALARM recvd");
    closeAndExit(ECALL_HANGUP);
    alrm_sig = true;
    signal(SIGALRM, onAlarm);
}




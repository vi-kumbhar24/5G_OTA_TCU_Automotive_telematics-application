/**********************************************************************************
 * File: eCallTTY.cpp
 * Routine to perform eCall using TTY
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


using namespace std;


#define EcallLogFile "/var/log/eCall.log"

int timeBetweenRedials = 0;
int busyBackoff = 4;


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
    char cmd[256];
    bool fail;
    int ret, retries;

    // command line args
    if (argc < 6) {
         cout << "Usage: 6 args (int iAirbagsDeployed, iImpactSensors, iOrientation, iSpeed, iSeatsOccupied, iSeatbelts)" << endl;
         exit(0);
    }

    // open log file 
    //setLogErrorFile(EcallLogFile);

    // start
    //logError("entering eCall");

    // set path
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);

    //
    //    1. When we call 911, we should wait up to 5 minutes for a TTY session to establish.
    //    2. We should retry a maximmum of 3 times.
    //       If still unsucessusful we should notify our call center so we can take appropriatte acton.
    //    3. After the call is terminated we should go into auto-answer mode,
    //       so that if the PSAP needs to return the call they can. We should go right into bluetooth voice mode.
    // 
    fail = true;
    retries = 3;
    while (retries-- > 0) {

        // place ecall
        sprintf(cmd, "eCallTTYchild %s %s %s %s %s %s &> /dev/null",
                      argp[1], argp[2], argp[3], argp[4], argp[5], argp[6]);
        ret = system(cmd);

        // get exit status
        ret=WEXITSTATUS(ret);
        switch (ret) {
            case ECALL_PASS:
                fail = false;
                retries = 0;
                break;

            case ECALL_NO_GPS_POSITION:
                fail = true;
                retries = 0;
                break;

            case ECALL_FAIL:
            case ECALL_HANGUP:
            case ECALL_BUSY:
            case ECALL_OPEN_SERIAL:
            case ECALL_SERIAL:
            case ECALL_NO_VOICE:
                fail = true;
                break;

            default:
                break;
        }

        // busy backoff
        if ((retries == 0) && (ret == ECALL_BUSY) && (busyBackoff > 0)) {
            logError("entering busy backoff");
            sleep(1*60);
            busyBackoff--;
            retries = 3;
            //timeBetweenRedials = 60;
        }
        
        // time between redials
        if ((retries > 0) && (timeBetweenRedials > 0)) {
            logError("delaying between redial attempt");
            sleep(timeBetweenRedials);
        }
    }

    // log event
    //    event is already logged by child on 'no gps'
    if (ret != ECALL_NO_GPS_POSITION) {
        if (fail) {
            sprintf(cmd, "eCallTTYchild -f %s %s %s %s %s %s &> /dev/null",
                          argp[1], argp[2], argp[3], argp[4], argp[5], argp[6]);
        } else {
            sprintf(cmd, "eCallTTYchild -p %s %s %s %s %s %s &> /dev/null",
                          argp[1], argp[2], argp[3], argp[4], argp[5], argp[6]);
        }
        system(cmd);
    }

    // wait for callback
    sprintf(cmd, "eCallTTYchild -w %s %s %s %s %s %s &> /dev/null",
                  argp[1], argp[2], argp[3], argp[4], argp[5], argp[6]);
    system(cmd);

    // done
    return 0;
}


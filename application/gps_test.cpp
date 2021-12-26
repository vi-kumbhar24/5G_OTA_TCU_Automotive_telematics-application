#include <unistd.h>
#include <math.h>
#include <gps.h>
#include <string.h>

int _gpsdGps(void);

struct gps_data_t _gps_data;

const char * _dirToCompass(int dir)
{
    static const char *p;

    p = "N";
    if (dir <= 157) {
        if (dir <= 67) {
            if (dir > 22) {
                p = "NE";
            }
        }
        else {
            if (dir <= 112) {
                p = "E";
            }
            else {
                p = "SE";
            }
        }
    }
    else if (dir <= 337) {
        if (dir <= 247) {
            if (dir <= 202) {
                p = "S";
            }
            else {
                p = "SW";
            }
        }
        else {
            if (dir <= 292) {
                p = "W";
            }
            else {
                p = "NW";
            }
        }
    }
    return p;
}



int main(int argc, char *argv[])
{
    int rc;
    bool have_opened_gpsd = false;

    while(true) {
        if (!have_opened_gpsd) {
            fprintf(stderr, "opening gpsd socket\n");
            if ((rc = gps_open("localhost", "2947", &_gps_data)) == -1) {
                fprintf(stderr, "code: %d, reason: %s\n", rc, gps_errstr(rc));
                sleep(1);
                continue;
            }
            fprintf(stderr, "opened gpsd socket\n");

            //gps_stream(&_gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
            gps_stream(&_gps_data, WATCH_ENABLE, NULL);

            have_opened_gpsd = true;
        }
        if (_gpsdGps() < 0) {
            fprintf(stderr, "closing gpsd socket\n");

            // close socket
            gps_stream(&_gps_data, WATCH_DISABLE, NULL);
            gps_close (&_gps_data);

            // reopen
            have_opened_gpsd = false;
        }
    }
}

int _gpsdGps() {
    int rc, dir;
 
    /* wait for 2 seconds to receive data */
    if (gps_waiting (&_gps_data, 2000000)) {
        /* read data */
        if ((rc = gps_read(&_gps_data)) == -1) {
            fprintf(stderr, "error occured reading gps data. code: %d, reason: %s\n", rc, gps_errstr(rc));
            return -1;
        } else {
            if (rc == 0) {
                fprintf(stderr, "error rc == 0!!!!!!!!\n");
                return -1;
            }

            /* Display data from the GPS receiver. */
            if ((_gps_data.status == STATUS_FIX) &&
                (_gps_data.fix.mode == MODE_2D || _gps_data.fix.mode == MODE_3D) &&
                !isnan(_gps_data.fix.latitude) &&
                !isnan(_gps_data.fix.longitude)) {
                    fprintf(stderr, "lat/long: %f/%f  ", _gps_data.fix.latitude,
                                                         _gps_data.fix.longitude);
                    fprintf(stderr, "speed: %0.3f,%d  ", _gps_data.fix.speed,
                                                       (int)(_gps_data.fix.speed * 2.23694 + 0.5));
                    dir = (int) _gps_data.fix.track;
                    if (dir < 0)
                        dir += 360;
                    fprintf(stderr, "track: %0.2f,%s\n", _gps_data.fix.track,
                                                         _dirToCompass(dir));
            }
        }
    }
    return 0;
}


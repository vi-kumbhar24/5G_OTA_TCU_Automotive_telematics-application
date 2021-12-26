/******************************************************************************
 * File: gps_mon.cpp
 *
 * Monitors NMEA sentences from GPS receiver to determine location
 * Also performs speedlimit checking and geofence crossings
 *
 ******************************************************************************
 * Change log:
 * 2012/08/02 rwp
 *    Changed to always update GpsPositionFile (for location timestamp)
 *    Changed speedlimit behavior if schedule starttime > endtime
 *    Added date/time to logged events
 * 2012/08/10 rwp
 *    Added more checking on NMEA sentence checksum
 * 2012/08/11 rwp
 *    Added detection of loss of fix (too long)
 *    Added detection of loss/gain fix looping
 *    Added detection of too fast setting of systime
 * 2012/08/13 rwp
 *    Added detection of moving too far or too fast
 * 2012/08/14 rwp
 *    Added logging of satellite info
 * 2012/08/18 rwp
 *    Moved checkTime to right before update
 *    Addeded setting of location_time in checkTime
 *    Added checking of prev_location_time to moving to far check
 * 2012/08/21 rwp
 *    Added debug_position condition around sending GPS debug events
 * 2012/08/27 rwp
 *    Changed debugging of satellite info for reporting SNR
 * 2012/08/29 rwp
 *    Sorted satellite info into azimuth order
 * 2012/09/22 rwp
 *    Added logging of movement (DebugMovingFile enables it)
 * 2012/11/15 rwp
 *    Changed to use features.h isEnabled for enabled_until
 * 2012/11/16 rwp
 *    Don't check fences if features changed and fences disabled
 * 2012/12/12 rwp
 *    Always check fences and speeds (for Valet/Curfew)
 *    Added reset GPS code
 * 2013/01/31 rwp
 *    Added speedlimit event_id
 * 2013/05/31 rwp
 *    Set no_gps flag when removing gps_present file
 * 2013/06/20 rwp
 *    AUT-39: Increase fleet tracking accuracy to 6 digits
 * 2014/01/23 rwp
 *    AUT-100: Control fleet tracking with FleetFeature
 * 2014/02/06 rwp
 *    AUT-100: Added the ability for FleetFeature to override server config
 * 2014/03/17 rwp
 *    Added demo mode
 * 2014/03/30 rwp
 *    Fixed demo mode for setting GpsPresentFile
 * 2014/04/07 rwp
 *    Added ability to use 2D fix
 * 2014/08/19 rwp
 *    Changed so 2D fix + num sats > 3 assumes 3D fix
 ******************************************************************************
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "getopts.h"
#include "eventLog.h"
#include "logError.h"
#include "split.h"
#include "splitFields.h"
#include "backtick.h"
#include "string_convert.h"
#include "string_printf.h"
#include "getStringFromFile.h"
#include "getToken.h"
#include "getField.h"
#include "writeStringToFile.h"
#include "readFileArray.h"
#include "dateTime.h"
#include "updateFile.h"
#include "touchFile.h"
#include "my_features.h"
#include "hiresTime.h"
#include "convertTimezone.h"
#include "appendToFile.h"
using namespace std;


//#define _POSIX_SOURCE 1         //POSIX compliant source

#define PI 3.14159265

void monitorGps();
void demoGps();
void processLine(string line);
void update();
double calcMovement();
double calcDist(double lat1, double lon1, double lat2, double lon2);
double deg2rad(double deg);
void dumpQueue(string reason);
void gpsErrorEvent(string reason);
void resetGps();
void checkFeatures();
void getDebug();
void getLogPosition();
void getGeoFences(bool initial);
void checkGeoFences();
void clearAllGeoFences();
void getSpeedLimits();
void checkSpeed();
void stopAllSpeedlimits();
void getFleetConfig();
void getDisableLocation();
string getIpAddress(string name);
string getPositionString();
string makePositionString(double lat, double lng);
double makeDegrees(string ddmm, string nsew);
void checkTime();
string makeTime(string time_str);
string makeDate(string date_str);
void closeAndExit(int exitstat);
void closeAll();
void cleanFiles();
void openPort(const char *port);
void closePort();
bool lock(const char *dev);
bool beginsWith(string str, string match);
void onTerm(int parm);
void onHup(int parm);

string serial_device = "usbgps";
bool debug_option = false;
bool debug = false;
bool log_position = false;
bool debug_position = false;
bool debug_satellites = false;
bool debug_moving = false;
bool no_gps_reset = false;
time_t dump_start;
int ser_s = -1;		// Serial port descriptor
struct termios oldtio;	// Old port settings for serial port
string lockfile = "";
bool no_gps = true;
bool got_position = false;
bool got_time = false;
bool gps_reset = false;
EventLog eventlog;
Timezones timezones;
string hostname = "";
int fleet_backoffs[8] ={5, 10, 20, 30, 60, 120, 240, 900};
int fleet_max_backoffs = 8;
Strings debug_queue;
bool got_bug = false;

struct SatInfo {
  public:
    string prn;
    string elev;
    string azim;
    string snr;
};
typedef map<string,SatInfo> Sats;
Sats sats;

struct GpsInfo {
  public:
    string date;
    string time;
    string raw_time;
    double latitude;
    double longitude;
    double altitude;
    double track;
    double speed;
    double hdop;
    time_t location_time;
    bool position_known;
    int fix3d;
    int num_sats;
    double last_latitude;
    double last_longitude;
    double prev_latitude;
    double prev_longitude;
    double last_track;
    double last_speed;
    int last_fix3d;
    int last_sample_time;
    bool moving;
    bool was_moving;
    int stopped_counter;
    bool gpgga_seen;
    bool gpgsa_seen;
    bool gprmc_seen;
    double last_hr_time;
    vector<double> speeds;
    bool location_disabled;
    time_t last_location_time;
    time_t prev_location_time;
    time_t fix_loss_time;
    time_t loop_fix_loss_time;
    int fix_loss_cnt;
    time_t set_time_time;
    int set_time_cnt;
    string last_sats_str;

    GpsInfo() {
        date = "";
        time = "";
        raw_time = "";
        latitude = 0.0;
        longitude = 0.0;
        altitude = 0.0;
        track = 0.0;
        speed = 0.0;
        hdop = 0.0;
        location_time = 0;
        fix3d = 0;
        num_sats = 0;
        position_known = false;
        last_latitude = 0.0;
        last_longitude = 0.0;
        prev_latitude = 0.0;
        prev_longitude = 0.0;
        last_track = 0.0;
        last_speed = 0.0;
        last_fix3d = 0;
        last_sample_time = 0;
        moving = false;
        was_moving = true;
        stopped_counter = 1;
        gpgga_seen = false;
        gpgsa_seen = false;
        gprmc_seen = false;
        last_hr_time = 0.0;
        location_disabled = false;;
        last_location_time = 0;
        prev_location_time = 0;
        fix_loss_time = 0;
        loop_fix_loss_time = 0;
        fix_loss_cnt = 0;
        set_time_time = 0;
        set_time_cnt = 0;
        last_sats_str = "Empty";
    }

    double calcAverageSpeed() {
        speeds.push_back(speed);
        if (speeds.size() > 10) {
            speeds.erase(speeds.begin());
        }
        double tot = 0.0;
//        cerr << time << " Speeds: ";
        for (int i=0; i < speeds.size(); i++) {
//            cerr << speeds.at(i) << " ";
            tot += speeds.at(i);
        }
        double avg = tot / speeds.size();
//        cerr << "Avg=" << avg << endl;
        return avg;
    }
};

class IgnitionState {
  public:
    bool off;
    bool turned_off;

    IgnitionState() {
        off = false;
        turned_off = false;
    }
};

class FleetTracker {
  private:
    int report_time;
    int send_time;
    bool outstanding_packet;
    string server_name;
    string server_ip;
    int server_port;
    int resend_cnt;
    Strings queue;
    int moving_interval;
    int stopped_interval;
    int stopped_counter;
    bool was_moving;
    int _socket;
    bool _enabled;

    void cleanQueue(string date, string time) {
        if (queue.size() > (86400/moving_interval)) {
            string dt = date + "T" + time;
            time_t now = mkTime(dt);
            time_t minus5 = now - 86400*5;
            while (queue.size() > 0) {
                string dt1 = getField(queue[0], ",", 1);
                if (mkTime(dt1) < minus5) {
                    logError("Deleting:"+queue[0]);
                    queue.erase(queue.begin());
                }
                else {
                    break;
                }
            }
        }
    }

    time_t mkTime(string dt) {
         struct tm tm;
         Strings vals = split(dt, "-/:T");
         tm.tm_year = stringToInt(vals[0]) - 1900;
         tm.tm_mon = stringToInt(vals[1]) - 1;
         tm.tm_mday = stringToInt(vals[2]);
         tm.tm_hour = stringToInt(vals[3]);
         tm.tm_min = stringToInt(vals[4]);
         tm.tm_sec = stringToInt(vals[5]);
         time_t tv = timegm(&tm);
         return tv;
    }

  public:

    FleetTracker() {
        report_time = 0;
        send_time = 0;
        outstanding_packet = false;
        server_name = "";
        server_ip = "";
        server_port = 0;
        resend_cnt = 0;
        moving_interval = 0;
        stopped_interval = 0;
        stopped_counter = 0;
        was_moving = false;
        _socket = -1;
        _enabled = false;
    }

    bool enabled() {
        bool retval = false;
        if (_enabled) {
            retval = true;
        }
        return retval;
    }

    void config(string server, int port, int moving_int, int stopped_int) {
        if ((server != server_name) or (port != server_port)) {
           server_ip = "";
        }
        server_name = server;
        server_port = port;
        moving_interval = moving_int;
        stopped_interval = stopped_int;
        _enabled = true;
    }

    void readQueue(string file) {
        if (enabled()) {
            if (fileExists(file)) {
                queue = readFileArray(file);
            }
        }
        if (fileExists(file)) {
            unlink(file.c_str());
        }
    }

    void writeQueue(string file) {
        if (enabled()) {
            ofstream outf(file.c_str());
            if (outf.is_open()) {
                for (int i=0; i < queue.size(); i++) {
                    outf << queue[i] << endl;;
                }
                outf.close();
            }
        }
    }

    void clear() {
        server_name = "";
        server_ip = "";
        if (_socket != -1) {
            close(_socket);
            _socket = -1;
        }
        _enabled = false;
    }

    void queuePacket(string hostname, GpsInfo gpsinfo, IgnitionState ignition) {
        unsigned int now = time(NULL);
        unsigned int elapsed = now - report_time;
        if (gpsinfo.moving) {
            was_moving = true;
            stopped_counter = 2;
        }
        if (elapsed >= moving_interval) {
            int mph = (int)(gpsinfo.last_speed * 1.15077945 + 0.5);
            int dir = (int)(gpsinfo.last_track + 0.5) % 360;
            string lat_str = string_printf("%.6f", gpsinfo.last_latitude);
            string lon_str = string_printf("%.6f", gpsinfo.last_longitude);
            string report = hostname;
            string date = gpsinfo.date;
            date.replace(4,1,"-");
            date.replace(7,1,"-");
            report += ","+date+"T"+gpsinfo.time;
            report += ","+lat_str;
            report += ","+lon_str;
            report += ","+toString(mph);
            report += ","+toString(dir);
            report += ","+toString(!ignition.off);
            report += ",0-0-0-0-0";
            if (was_moving or (elapsed >= stopped_interval)) {
                if (debug) {
                    logError("Should report:"+report);
                }
                cleanQueue(date,gpsinfo.time);
                queue.push_back(report);
                report_time = now;
            }
            if (!gpsinfo.moving) {
                if (was_moving) {
                    if (--stopped_counter <= 0) {
                        was_moving = false;
                    }
                }
            }
        }
    }

    void sendPackets() {
        if (queue.size() > 0) {
            bool conn_up = false;
            if (fileExists(CellularStateFile)) {
                string conn_state = getStringFromFile(CellularStateFile);
                conn_up = conn_state.find(" up ") != string::npos;
            }
            if (_socket < 0) {
                if (conn_up) {
                    _socket = socket(PF_INET, SOCK_DGRAM, 0);
                    if (_socket < 0) {
                        logError("Could not open FleetTracker socket");
                    }
                    else {
                        struct linger linger_opt;
                        linger_opt.l_onoff = 0;
                        linger_opt.l_linger = 0;
                        setsockopt(_socket, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));
                        int sock_flag = 1;
                        setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &sock_flag, sizeof(sock_flag));
                    }
                }
            }
            if (_socket != -1) {
                if (conn_up) {
                    if ( !outstanding_packet or 
                         ((time(NULL)-send_time) >= fleet_backoffs[resend_cnt]) ) {
                        if (server_ip == "") {
                             server_ip = getIpAddress(server_name);
                        }
                        if (server_ip != "") {
                            struct sockaddr_in to;
                            to.sin_family = AF_INET;
                            to.sin_addr.s_addr = inet_addr(server_ip.c_str());
                            to.sin_port = htons(server_port);
                            string frame = queue[0];
                            frame += "\n";
                            if (debug) {
                                logError("Sending: "+frame);
                            }
                            sendto(_socket, frame.c_str(), frame.size(), 0, (struct sockaddr *)&to, sizeof(to));
                            send_time = time(NULL);
                            if (outstanding_packet) {
                                resend_cnt++;
                                if (resend_cnt >= fleet_max_backoffs) {
                                    resend_cnt = fleet_max_backoffs - 1;
                                    server_ip = "";
                                }
                            }
                            else {
                                outstanding_packet = true;
                                resend_cnt = 0;
                            }
                        }
                    }
                }
                else {
                    close(_socket);
                    _socket = -1;
                    server_ip = "";
                }
            }
        }
    }

    void ackPacket(char *packet) {
        if (outstanding_packet and (queue.size() > 0)) {
            if (strncmp(packet, queue[0].c_str(), 30) == 0) {
                queue.erase(queue.begin());
                outstanding_packet = false;
            }
        }
    }

    int getSocket() {
        return _socket;
    }
};

class Point {
  public:
    double x;
    double y;

    Point() {
        x = 0;
        y = 0;
    }

    Point(double xv, double yv) {
        x = xv;
        y = yv;
    }
};

class GeoFence {
  private:
    typedef enum {Unknown, Inside, Outside} FenceState;
    typedef enum {Circle, Polygon} FenceType;
    int _id;
    string _descr;
    FenceState _state;
    FenceType _type;
    Point _center;
    double _radius;
    vector<Point> _points;
    bool _initial;

    void _makeCircle(string descr) {
        _type = Circle;
        Strings vals = split(descr, "(),");
        double y = stringToDouble(vals[0]);
        double x = stringToDouble(vals[1]);
        double r = stringToDouble(vals[2]);
        _center = Point(x, y);
        _radius = r;
    }

    void _makePolygon(string descr) {
        _type = Polygon;
        Strings vals = split(descr, "(),");
        for (int i=0; i < vals.size(); i+=2) {
            double y = stringToDouble(vals[i]);
            double x = stringToDouble(vals[i+1]);
            _points.push_back(Point(x,y));
        }
    }

    bool _insideCircle(double x, double y) {
        bool retval = false;
        double lat2 = _deg2rad(_center.y);
        double lon2 = _deg2rad(_center.x);
        double lat1 = _deg2rad(y);
        double lon1 = _deg2rad(x);
        double r = 3959.;	// Earth's radius in miles
        double d = acos(sin(lat1)*sin(lat2) +
                    cos(lat1)*cos(lat2) * cos(lon2-lon1)) * r;
        if (d <= _radius) {
            retval = true;
        }
        return retval;
    }

    double _deg2rad(double deg) {
        double rad = deg / 180. * PI;
        return rad;
    }

    bool _insidePolygon(double x, double y) {
        bool retval = false;
        int left = 0;
        int right = 0;
        for (int i=0; i < _points.size(); i++) {
            int j = i - 1;
            if (j < 0) {
                j = _points.size() - 1;
            }
            double x1 = _points[j].x;
            double y1 = _points[j].y;
            double x2 = _points[i].x;
            double y2 = _points[i].y;
            if (y1 > y2) {
                double yt = y2;
                y2 = y1;
                y1 = yt;
                double xt = x2;
                x2 = x1;
                x1 = xt;
            }
            if ( (y >= y1) and (y < y2) ) {
                double xval = x;
                if (y1 != y2) {
                    double m = (x2-x1) / (y2-y1);
                    double b = x1 - y1*m;
                    xval = y*m + b;
                }
                if (xval < x) {
                    left++;
                }
                else {
                    right++;
                }
            }
        }
        if ( ((left%2) == 1) and
             ((right%2) == 1) ) {
            retval = true;
        }
        else if ( ((left%2) == 0) and
                  ((right%2) == 0) ) {
            retval = false;
        }
        else {
            cerr << "Invalid case left=" << left
                 << " right=" << right << endl;
        }
        return retval;
    }

  public:
    GeoFence(int id, string descr, bool initial) {
        _id = id;
        _descr = descr;
        _state = Unknown;
        _initial = initial;
        if (descr.find("),(") == string::npos) {
            _makeCircle(descr);
        }
        else {
            _makePolygon(descr);
        }
    }

    string crossedFence(double x, double y) {
        string dir = "";
        FenceState old_state = _state;
        string st = "";
        if (is_inside(x, y)) {
            _state = Inside;
            dir = "in";
            st = "inside";
        }
        else {
            _state = Outside;
            st = "outside";
            dir = "out";
        }
        if ( (_state == old_state) or _initial ) {
            dir = "";
        }
        _initial = false;
        return dir;       
    }

    bool is_inside(double x, double y) {
        bool retval = false;
        if (_type == Polygon) {
            retval = _insidePolygon(x, y);
        }
        else {
            retval = _insideCircle(x, y);
        }
        return retval;
    }

    int id() {
        return _id;
    }

    string descr() {
        return _descr;
    }

    void setUnknown() {
        _state = Unknown;
    }
};

class SpeedLimit {
  private:
    int _id;
    string _descr;
    int _limit;
    string _units;
    int _days;
    int _start_time;
    int _end_time;
    string _timezone;
    enum {NotExceeded, Exceeding, Exceeded} state;
    int _max_speed;
    double _max_longitude;
    double _max_latitude;
    int _max_dir;
    string _max_time;
    double _stopped_longitude;
    double _stopped_latitude;
    int _stopped_dir;
    time_t _time_stopped_exceeding;
    time_t _last_exceeding_time;
    int _event_id;

    int _hexToInt(string hex) {
        int val;
        sscanf(hex.c_str(), "%x", &val);
        return val;
    }

    int _timeToInt(string tstr) {
        int val = 0;
        int hh = 0;
        int mm = 0;
        int ss = 0;
        sscanf(tstr.c_str(), "%d:%d:%d", &hh, &mm, &ss);
        val = hh*3600 + mm*60 + ss;
        return val;
    }

    string _dirToCompass(int dir) {
        string str = "N";
        if (dir <= 157) {
            if (dir <= 67) {
                if (dir > 22) {
                    str = "NE";
                }
            }
            else {
                if (dir <= 112) {
                    str = "E";
                }
                else {
                    str = "SE";
                }
            }
        }
        else if (dir <= 337) {
            if (dir <= 247) {
                if (dir <= 202) {
                    str = "S";
                }
                else {
                    str = "SW";
                }
            }
            else {
                if (dir <= 292) {
                    str = "W";
                }
                else {
                    str = "NW";
                }
            }
        }
        return str;
    }

  public:
    SpeedLimit(int id, string descr) {
        _descr = descr;
        _id = id;
        Strings vals = split(descr, ",");
        if (vals.size() == 6) {
            _limit = stringToInt(vals[0]);
            _units = vals[1];
            _days = _hexToInt(vals[2]);
            _start_time = _timeToInt(vals[3]);
            _end_time = _timeToInt(vals[4]);
            _timezone = vals[5];
        }
        state = NotExceeded;
        _max_speed = 0;
        _event_id = 0;
        logError("Created SpeedLimit:"+toString(_id));
        logError("Speed:"+toString(_limit)+_units);
        logError("Days:"+toString(_days));
        logError("Times:"+toString(_start_time)+"-"+toString(_end_time));
        logError("Timezone:"+_timezone);
    }

    string descr() {
        return _descr;
    }

    int id() {
        return _id;
    }

    string timezone() {
        return _timezone;
    }

    string checkSpeed(int day, long int hhmmss, double dspeed, double lng, double lat, int dir) {
        string msg = "";
        bool check = false;
        bool exceeding = false;
        int speed = 0;
        if (_units == "mph") {
            speed = int(dspeed * 1.15077945 + 0.5);
        }
        else if (_units == "kph") {
            speed = int(dspeed * 1.85200 + 0.5);
        }
//        cerr << "speed:"<<speed<<" limit:"<<_limit << endl;
//        cerr << "time:"<<hhmmss<<endl;
//        cerr << "start:"<<_start_time<<" end:"<<_end_time<<endl;
        if (speed > _limit) {
            if (_start_time < _end_time) {
                if ( (hhmmss > _start_time) and
                     (hhmmss < _end_time) ) {
                    if ((_days & (1<<day)) != 0) {
                        exceeding = true;
                    }
                }
            }
            else {
                if (hhmmss >= _start_time) {
                    if ((_days & (1<<day)) != 0) {
                        exceeding = true;
                    }
                }
                else if (hhmmss < _end_time) {
                    int next_days = _days;
                    int hibit = next_days & 0x40;
                    next_days = (next_days << 1) | (hibit >> 6);
                    if ((next_days & (1<<day)) != 0) {
                        exceeding = true;
                    }
                }
            }
        }
        if (exceeding) {
            if (state == NotExceeded) {
                _event_id++;
                msg = getDateTime();
                msg += " Speedlimit Exceeded";
                msg += " id:" + toString(_id);
                msg += " evid:" + toString(_event_id);
                msg += " position:" + makePositionString(lat,lng);
                msg += " dir:" + toString(dir);
            }
            state = Exceeding;
            if (speed > _max_speed) {
                _max_speed = speed;
                _max_longitude = lng;
                _max_latitude = lat;
                _max_dir = dir;
                _max_time = getDateTime();
            }
            time(&_last_exceeding_time);
        }
        else {
            if (state == Exceeding) {
                state = Exceeded;
                time(&_time_stopped_exceeding);
                _stopped_latitude = lat;
                _stopped_longitude = lng;
                _stopped_dir = dir;
            }
            if (state == Exceeded) {
                time_t now;
                time(&now);
                if ( ((now-_last_exceeding_time) >= 180) or
                      ((speed < 1) and (_limit > 0)) ) {
                    msg = getDateTime(_time_stopped_exceeding);
                    msg += " Speedlimit NotExceeded";
                    msg += " id:" + toString(_id);
                    msg += " evid:" + toString(_event_id);
                    msg += " position:" + makePositionString(_stopped_latitude,_stopped_longitude);
                    msg += " dir:" + toString(_stopped_dir);
                    msg += " maxspeed:" + toString(_max_speed);
                    msg += " maxpos:" + makePositionString(_max_latitude,_max_longitude);
                    msg += " maxdir:" + toString(_max_dir);
                    msg += " maxdate:" + _max_time.substr(0,10);
                    msg += " maxtime:" + _max_time.substr(11);
                    state = NotExceeded;
                    _max_speed = 0;
                }
            }
        }
        return msg;
    }

    string stopped(double lng, double lat, int dir) {
        string msg = "";
        if ((state == Exceeded) or (state == Exceeding)) {
            if (state == Exceeding) {
                time(&_time_stopped_exceeding);
                _stopped_latitude = lat;
                _stopped_longitude = lng;
                _stopped_dir = dir;
            }
            msg = getDateTime(_time_stopped_exceeding);
            msg += " Speedlimit NotExceeded";
            msg += " id:" + toString(_id);
            msg += " evid:" + toString(_event_id);
            msg += " position:" + makePositionString(_stopped_latitude,_stopped_longitude);
            msg += " dir:" + toString(_stopped_dir);
            msg += " maxspeed:" + toString(_max_speed);
            msg += " maxpos:" + makePositionString(_max_latitude,_max_longitude);
            msg += " maxdir:" + toString(_max_dir);
            msg += " maxdate:" + _max_time.substr(0,10);
            msg += " maxtime:" + _max_time.substr(11);
            state = NotExceeded;
            _max_speed = 0;
        }
        return msg;
    }
};

GpsInfo gpsinfo;
FleetTracker fleet;
Features features;
IgnitionState ignition;
vector<GeoFence> geoFences;
vector<SpeedLimit> speedLimits;
double first_latitude = 0.0;
double first_longitude = 0.0;
double max_avg = 0.0;
//bool geofences_enabled = false;
//bool speedlimits_enabled = false;
bool demo_mode = false;
bool use2d = false;

int main(int argc, char *argv[])
{
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    umask(S_IWGRP | S_IWOTH);
    GetOpts opts(argc, argv, "d2");
    debug_option = opts.exists("d");
    if (debug_option) {
        debug = true;
    }
    use2d = opts.exists("2");
    if (fileExists(DemoModeFile)) {
        demo_mode = true;
    }
    if (fileExists(DebugPositionFile)) {
        debug_position = true;
    }
    if (fileExists(DebugSatellitesFile)) {
        debug_satellites = true;
    }
    if (fileExists(DebugMovingFile)) {
        debug_moving = true;
    }
    if (fileExists(NoGpsResetFile)) {
        no_gps_reset = true;
    }
    if (argc > 1) {
        serial_device = argv[1];
    }
    hostname = getStringFromFile("/etc/hostname");
    logError("Hostname: "+hostname);

    string prev_position = "";
    time_t prev_position_time = 0;
    if (fileExists(GpsPositionFile)) {
        prev_position = getStringFromFile(GpsPositionFile);
        prev_position_time = fileTime(GpsPositionFile);
    }
    else if (fileExists(PreviousPositionFile)) {
        prev_position = getStringFromFile(PreviousPositionFile);
        prev_position_time = fileTime(PreviousPositionFile);
    }
    if (prev_position != "") {
        updateFile(GpsLastPositionFile,prev_position);
        int pos = prev_position.find("/");
        string lat = prev_position.substr(0,pos);
        string lng = prev_position.substr(pos+1);
        gpsinfo.latitude = stringToDouble(lat);
        gpsinfo.longitude = stringToDouble(lng);
        gpsinfo.last_latitude = gpsinfo.latitude;
        gpsinfo.last_longitude = gpsinfo.longitude;
        gpsinfo.prev_latitude = gpsinfo.latitude;
        gpsinfo.prev_longitude = gpsinfo.longitude;
        gpsinfo.last_location_time = prev_position_time;
        gpsinfo.prev_location_time = prev_position_time;
        first_latitude = gpsinfo.latitude;
        first_longitude = gpsinfo.longitude;
        gpsinfo.position_known = true;
        logError("prevposition:"+lat+"/"+lng);
    }
    getDebug();
    getGeoFences(true);
    getSpeedLimits();
    getFleetConfig();
    getDisableLocation();
    getLogPosition();
    fleet.readQueue(FleetQueueFile);

    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onHup);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);

    while (true) {
        if (demo_mode) {
            demoGps();
        }
        else if (fileExists("/dev/"+serial_device)) {
            monitorGps();
        }
        sleep(1);
    }
}

void monitorGps() {
    int res = 0;
    char buf[256];                       //buffer for where data is put
    string line = "";

    openPort(serial_device.c_str());
    time_t starttime = time(NULL);

    struct timeval tv;
    // loop while waiting for input. normally we would do something useful here
    while (1) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser_s, &rd);
        nfds = max(nfds, ser_s);
        int fltsock = fleet.getSocket();
        if (fltsock != -1) {
            FD_SET(fltsock, &rd);
            nfds = max(nfds, fltsock);
        }
        tv.tv_sec = 60;
        tv.tv_usec = 0;
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("select()");
            exit(1);
        }
        else if (r == 0) {
            if (!no_gps and !gps_reset) {
                logError("No GPS data received");
                gpsErrorEvent("No GPS data received");
                cleanFiles();
                no_gps = true;
            }
        }
        else if (FD_ISSET(ser_s, &rd)) {
            res = read(ser_s,buf,255);
            if (res == 0) {
                break;
            }
            if (res > 0) {
                buf[res] = '\0';
//                cerr << buf;
                int i;
                for (i=0; i < res; i++) {
                    char ch = buf[i];
//                    fprintf(stderr, "%02x ", ch);
                    if (ch == '\r') {
                        ch = '\n';
                    }
                    if (ch == '\n') {
                        if  (line != "") {
//                            cerr << "got:" << line << endl;
                            if (starttime != 0) {
                                if ((time(NULL)-starttime) < 2) {
                                    line = "";
                                    continue;
                                }
                                starttime = 0;
                            }
                            processLine(line);
                            line = "";
                            continue;
                        }
                    }
                    else {
                        line += ch;
                    }
                }
            }  //end if res>0
        }
        else if ((fltsock != -1) and FD_ISSET(fltsock, &rd)) {
            struct sockaddr_in from;
            socklen_t fromlen = sizeof(from);
            char buffer[128];
            int n = recvfrom(fltsock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from, &fromlen);
            if (n > 0) {
                buffer[n] = '\0';
                fleet.ackPacket(buffer);
            }
        }
        if (fleet.enabled()) {
            fleet.sendPackets();
        }
    }  //while (1)
//    closeAndExit(0);
    logError("GPS port closed");
    no_gps = true;
//    gpsErrorEvent("GPS port closed");
    closeAll();
}

void demoGps() {
    while (true) {
        if (fileExists("/tmp/gps_input_info")) {
            touchFile(GpsPresentFile);
            string input = getStringFromFile("/tmp/gps_input_info");
            Strings vals = split(input, "/");
            if (vals.size() == 4) {
                gpsinfo.latitude = stringToDouble(vals[0]);
                gpsinfo.longitude = stringToDouble(vals[1]);
                gpsinfo.speed = stringToDouble(vals[2]);
                gpsinfo.track = stringToDouble(vals[3]);
                gpsinfo.fix3d = 3;
                update();
            }
        }
        else {
            unlink(GpsPresentFile);
        }
        sleep(1);
    }
}

void processLine(string line) {
    if (!beginsWith(line, "$")) {
        logError("Does not begin with '$'");
        return;
    }
    line = line.substr(1);
    int pos = 0;
    if ((pos = line.find('*')) == string::npos) {
        logError("Does not have checksum");
        return;
    }
//    if (!got_position) cerr << line << endl;
    string chksum_s = line.substr(pos+1);
    if (chksum_s.size() != 2) {
        logError("Incorrect checksum length");
        return;
    }
    int chksum = 0;
    char ch;
    int n = sscanf(chksum_s.c_str(), "%x%c", &chksum, &ch);
    if (n != 1) {
        logError("Invalid checksum format");
        return;
    }
    line.erase(pos);
    int sum = 0;
    for (int i=0; i < line.size(); i++) {
        unsigned ch = line.at(i);
        sum ^= ch;
    }
    if (sum != chksum) {
        logError("Checksum does not match");
        return;
    }
    if (!debug and debug_position) {
        if (!got_bug) {
            debug_queue.push_back(getDateTime()+" "+line);
            if (debug_queue.size() > 500) {
                debug_queue.erase(debug_queue.begin());
            }
        }
        else {
            logError(line);
            if ((time(NULL)-dump_start) >= 600) {
                logError("Stop logging");
                got_bug = false;
            }
        }
    }
    if (no_gps) {
        touchFile(GpsPresentFile);
        no_gps = false;
    }
    if (fileExists(IgnitionOffFile)) {
        if (!ignition.off) {
            ignition.turned_off = true;
            ignition.off = true;
        }
    }
    else {
        ignition.off = false;
    }
    Strings flds = splitFields(line, ",");
    string type = flds[0];
    double hr_time = hiresTime();
    if ( (gpsinfo.last_hr_time != 0.0) and
         ((hr_time-gpsinfo.last_hr_time) > .5) ) {
        gpsinfo.gpgga_seen = false;
        gpsinfo.gpgsa_seen = false;
        gpsinfo.gprmc_seen = false;
        sats.clear();
    }
    gpsinfo.last_hr_time = hr_time;
    if (debug) {
        logError(line);
    }
    if (beginsWith(line, "GPGGA")) {
        if (flds.size() < 10) {
            logError("Not enough fields for GPCGA");
            return;
        }
        string time_str = flds[1];
        string lat_str = flds[2];
        string ns = flds[3];
        string lon_str = flds[4];
        string ew = flds[5];
        string num_sats = flds[7];
        string alt_str = flds[9];
        double lat = makeDegrees(lat_str, ns);
        double lon = makeDegrees(lon_str, ew);
        double alt = stringToDouble(alt_str);
        gpsinfo.altitude = alt;
        string time = makeTime(time_str);
        updateFile("/tmp/gps_num_sats", num_sats);
        gpsinfo.num_sats = stringToInt(num_sats);
        if (debug) {
            cerr << "GGA: " << time << " " << lat << "/" << lon << endl;
        }
        if (fileExists("/autonet/admin/debug_antenna")) {
            string dt = getDateTime();
            string loc_str = string_printf("%s,%s,%.6f,%.6f",
                             dt.c_str(),num_sats.c_str(),lat,lon);
            appendToFile("/autonet/admin/gps_loc.log", loc_str);
        }
        gpsinfo.gpgga_seen = true;
    }
    else if (beginsWith(line, "GPRMC")) {
        if (flds.size() < 10) {
            logError("Not enough fields for GPRMC");
            return;
        }
        string time_str = flds[1];
        string lat_str = flds[3];
        string ns = flds[4];
        string lon_str = flds[5];
        string ew = flds[6];
        string spd_str = flds[7];
        string trk_str = flds[8];
        string date_str = flds[9];
        double lat = makeDegrees(lat_str, ns);
        double lon = makeDegrees(lon_str, ew);
        double track = stringToDouble(trk_str);
        double speed = stringToDouble(spd_str);
        string time = makeTime(time_str);
        string date = makeDate(date_str);
        if (debug) {
            cerr << "RMC: date:" << date << " time:" << time << " " << lat << "/" << lon << " Dir:" << trk_str << " Speed:" << spd_str <<endl;
        }
        gpsinfo.latitude = lat;
        gpsinfo.longitude = lon;
        gpsinfo.speed = speed;
        gpsinfo.track = track;
        gpsinfo.date = date;
        gpsinfo.time = time;
        gpsinfo.raw_time = time_str;
        gpsinfo.gprmc_seen = true;
    }
    else if (beginsWith(line, "GPGSA")) {
        if (flds.size() < 18) {
            logError("Not enough fields for GPGSA");
            return;
        }
        string fix_str = flds[2];
        int fix3d = stringToInt(fix_str);
        if (use2d and (fix3d == 2)) {
            fix3d = 3;
        }
        string pdop = flds[15];
        string hdop = flds[16];
        string vdop = flds[17];
        gpsinfo.fix3d = fix3d;
        gpsinfo.hdop = stringToDouble(hdop);
        if (debug) {
            cerr << "GSA: 3dFix:" << fix_str << endl;
        }
        if (debug_satellites) {
            string sats_str = "";
            multimap<string,string> azim_prn;
            Assoc sats_list;
            for (int i=0; i < gpsinfo.num_sats; i++) {
                string prn = flds[3+i];
                azim_prn.insert(pair<string,string>(sats[prn].azim, prn));
                sats_list[prn] = "*";
            }
            multimap<string,string>::iterator it;
            for (it=azim_prn.begin(); it != azim_prn.end(); it++) {
                string prn = (*it).second;
                sats_str += " "+prn+","+sats[prn].elev+","+sats[prn].azim+","+sats[prn].snr;
//                sats_str += " "+prn+","+sats[prn].elev+","+sats[prn].azim;
            }
//            if (gpsinfo.last_sats_str != sats_str) {
//                gpsinfo.last_sats_str = sats_str;
                sats_str = getDateTime()
                           + string_printf(" %.7f/%.7f", gpsinfo.latitude, gpsinfo.longitude)
                           + sats_str;
                appendToFile("/user/gps_sat.log", sats_str);
//            }
            sats_str = "";
            for (int i=1; i <= 32; i++) {
                string prn = string_printf("%02d", i);
                sats_str += prn + ":";
                if (sats.find(prn) != sats.end()) {
                    sats_str += sats[prn].elev + "," + sats[prn].azim + "," + sats[prn].snr;
                }
                if (sats_list.find(prn) != sats_list.end()) {
                    sats_str += "*";
                }
                sats_str += "\n";
            }
            sats_str += "Num Sats: " + toString(gpsinfo.num_sats) + "\n";
            updateFile("/tmp/sats", sats_str);
        }
        gpsinfo.gpgsa_seen = true;
    }
    else if (beginsWith(line, "GPGSV")) {
        for (int i=0; i < 4; i++) {
            if (flds.size() > i*4+4) {
                string prn = flds[i*4+4];
                string elev = flds[i*4+5];
                string azim = flds[i*4+6];
                string snr = flds[i*4+7];
                sats[prn].prn = prn;
                sats[prn].elev = elev;
                sats[prn].azim = azim;
                sats[prn].snr = snr;
            }
        }
    }
    if ( gpsinfo.gpgga_seen and
         gpsinfo.gpgsa_seen and
         gpsinfo.gprmc_seen ) {
        checkTime();
        update();
        if (fileExists(RefPositionFile) and debug_position) {
            string refpos = "Reference Position:" + getStringFromFile(RefPositionFile);
            if (!got_bug) {
                debug_queue.push_back(refpos);
                if (debug_queue.size() > 500) {
                    debug_queue.erase(debug_queue.begin());
                }
            }
            else {
                logError(refpos);
            }
        }
        gpsinfo.gpgga_seen = false;
        gpsinfo.gpgsa_seen = false;
        gpsinfo.gprmc_seen = false;
    }
}

void update() {
    checkFeatures();
    if ((gpsinfo.fix3d == 2) and (gpsinfo.num_sats > 3)) {
        gpsinfo.fix3d = 3;
    }
    if ( (gpsinfo.longitude != gpsinfo.last_longitude) or
         (gpsinfo.latitude != gpsinfo.last_latitude) or
         (gpsinfo.fix3d != gpsinfo.last_fix3d) ) {
        if ( (gpsinfo.fix3d > 2) and
             !((gpsinfo.longitude == 0.0) and (gpsinfo.latitude == 0.0)) ) {
            if (!gpsinfo.position_known) {
                first_latitude = gpsinfo.latitude;
                first_longitude = gpsinfo.longitude;
            }
            if (log_position) {
                logError(string_printf("gps %.8f/%.8f/%.1f/%.0f",gpsinfo.latitude,gpsinfo.longitude,gpsinfo.speed,gpsinfo.track));
            }
            gpsinfo.position_known = true;
            int speed_mph = int(gpsinfo.speed*1.15077945 + 0.5);
            updateFile("/tmp/gps_speed", toString(speed_mph));
            bool moved = false;
            double dist = calcMovement();
//            double avg_speed = gpsinfo.calcAverageSpeed();
//            if (avg_speed > 0.0) {
//                if (avg_speed > max_avg) {
//                    max_avg = avg_speed;
//                }
//                cerr << gpsinfo.time << " Avg speed:" << avg_speed << " MaxAvg:" << max_avg << endl;
//            }
            if (dist > 0.) {
                gpsinfo.moving = true;
//                cout << gpsinfo.date << " " << gpsinfo.time << " moved " << dist << " " << speed_mph << endl;
                moved = true;
            }
            else {
                gpsinfo.moving = false;
            }
            if (moved or !got_position or ignition.turned_off) {
                if (fileExists("/autonet/admin/debug_moving")) {
                    string movedstr = string_printf("Moved: %.6f/%.6f",gpsinfo.latitude,gpsinfo.longitude);
                    logError(movedstr);
                }
                ignition.turned_off = false;
                // We don't want to check if geofences are enbabled, because ther might be other fences e.g. valet fences
                if (!gpsinfo.location_disabled) {
                    checkGeoFences();
                }
                if (fileExists(RefPositionFile)) {
                    string ref_pos = getStringFromFile(RefPositionFile);
                    Strings flds = splitFields(ref_pos, "/");
                    double ref_lat = stringToDouble(flds[0]);
                    double ref_lon = stringToDouble(flds[1]);
                    double delta = calcDist(gpsinfo.latitude, gpsinfo.longitude, ref_lat, ref_lon);
//                    cerr << "gps: " << gpsinfo.latitude << "/" << gpsinfo.longitude << endl;
//                    cerr << "Delta: " << delta << endl;
                    if (delta > 1000) {
                        string errstr = string_printf("Too foar off ref: %.6f/%.6f delta:%.0f",ref_lat,ref_lon,delta);
                        logError(errstr);
//                        cerr << "ref:" << ref_lat << "/" << ref_lon << " delta:" << delta << endl;
                        if (!got_bug) {
                            dumpQueue(errstr);
                        }
                    }
                }
                    
//                string pos = getPositionString();
//                updateFile(GpsPositionFile, pos);
                gpsinfo.last_latitude = gpsinfo.latitude;
                gpsinfo.last_longitude = gpsinfo.longitude;
                gpsinfo.last_location_time = gpsinfo.location_time;
                gpsinfo.last_speed = gpsinfo.speed;
                gpsinfo.last_track = gpsinfo.track;
                gpsinfo.last_fix3d = gpsinfo.fix3d;
            }
            string pos = makePositionString(gpsinfo.last_latitude, gpsinfo.last_longitude);
            updateFile(GpsPositionFile, pos);
            if (fleet.enabled() and !gpsinfo.location_disabled) {
                fleet.queuePacket(hostname, gpsinfo, ignition);
            }
            if (!ignition.off) {
                // We don't want to check if speedlimits are enbabled, because ther might be other speedlimits e.g. valet speedlimits
                if (!gpsinfo.location_disabled) {
                    checkSpeed();
                }
            }
            if (!got_position) {
                logError("Position acquired "+string_printf("%.6f/%.6f",gpsinfo.latitude,gpsinfo.longitude));
                got_position = true;
                gps_reset = false;
                unlink(NoGpsPositionFile);
                unlink(GpsLastPositionFile);
                gpsinfo.fix_loss_time = 0;
                if ( (gpsinfo.loop_fix_loss_time != 0) and
                     ((time(NULL)-gpsinfo.loop_fix_loss_time) >= 10) ) {
                    gpsinfo.loop_fix_loss_time = 0;
                }
            }
        }
        else {
            if (got_position) {
                logError("Position no longer available "+string_printf("%.6f/%.6f",gpsinfo.prev_latitude,gpsinfo.prev_longitude));
                got_position = false;
//                unlink(GpsPositionFile);
                rename(GpsPositionFile,GpsLastPositionFile);
                touchFile(NoGpsPositionFile);
                gpsinfo.fix_loss_time = time(NULL);
                if (gpsinfo.loop_fix_loss_time == 0) {
                    gpsinfo.loop_fix_loss_time = time(NULL);
                    gpsinfo.fix_loss_cnt = 1;
                }
                else if ((time(NULL)-gpsinfo.loop_fix_loss_time) < 10) {
                    gpsinfo.fix_loss_cnt++;
                    if (gpsinfo.fix_loss_cnt == 4) {
                        logError("Loosing fix loop");
                        gpsErrorEvent("Loosing fix loop");
                        resetGps();
                    }
                    gpsinfo.loop_fix_loss_time = time(NULL);
                }
                else {
                    gpsinfo.loop_fix_loss_time = 0;
                }
            }
        }
    }
    if ( (gpsinfo.fix_loss_time != 0) and
         ((time(NULL)-gpsinfo.fix_loss_time) > 600) ) {
        logError("Lost fix too long");
        gpsErrorEvent("Lost fix too long");
        resetGps();
        gpsinfo.fix_loss_time = 0;
    }
}

double calcMovement() {
    double dist = 0.;
    double df1 = calcDist(gpsinfo.prev_latitude,gpsinfo.prev_longitude,gpsinfo.latitude,gpsinfo.longitude);
    double df2 = calcDist(gpsinfo.last_latitude,gpsinfo.last_longitude,gpsinfo.latitude,gpsinfo.longitude);
    double df3 = calcDist(first_latitude,first_longitude,gpsinfo.latitude,gpsinfo.longitude);
    if ( (df1 > 1.0) and (gpsinfo.speed > 0.0) ) {
        if (debug) {
            logError(string_printf("%s,%.0f,%.0f,%.0f,%.1f,%.0f,%.1f",
                     gpsinfo.time.c_str(),
                     df3,df2,df1,
                     gpsinfo.speed,gpsinfo.track,gpsinfo.hdop));
//            cerr << gpsinfo.time << "," << df3 << ",";
//            cerr << df2 << "," << df1 << "," ;
//            cerr << gpsinfo.speed << "," << gpsinfo.track << ",";
//            cerr << gpsinfo.hdop << endl;
        }
    }
    if ( (df1 > 300) and ((gpsinfo.location_time-gpsinfo.prev_location_time) == 1) ) {
        string errstr = "Moved too far "+toString(df1)+"ft";
        logError(errstr);
        gpsErrorEvent(errstr);
        resetGps();
    }
    else {
        time_t delta_t = gpsinfo.location_time - gpsinfo.last_location_time;
        if ( (gpsinfo.last_location_time != 0) and (delta_t > 0) ) {
            time_t fps = df2 / delta_t;
            if (fps > 300) {
                string errstr = "Moved too fast "+toString(fps)+"fps";
                logError(errstr);
                gpsErrorEvent(errstr);
                resetGps();
            }
        }
    }
    if (ignition.off) {
//        if (debug_position and (df2 > 500)) {
//            string errstr = "Ignition off delta:"+toString(df2);
//            logError(errstr);
//            if (!got_bug) {
//                dumpQueue(errstr);
//            }
//        }
        if ( ((df1 > 100.) and (df2 > 100.) and (gpsinfo.speed > 0)) or
             (df2 > 500.) ) {
            if (debug) {
                logError("Moved:"+toString(df1)+","+toString(df2)+":"+toString(gpsinfo.speed));
            }
            dist = df2;
        }
    }
    else {
        if ( ((df1 > 25.) and (df2 > 25.) and (gpsinfo.speed > 0)) or
             (df2 > 100.) ) {
            if (debug) {
                logError("Moved:"+toString(df1)+","+toString(df2)+":"+toString(gpsinfo.speed));
            }
            dist = df2;
        }
    }
    gpsinfo.prev_latitude = gpsinfo.latitude;
    gpsinfo.prev_longitude = gpsinfo.longitude;
    gpsinfo.prev_location_time = gpsinfo.location_time;
    return dist;
}

double calcDist(double lat1, double lon1, double lat2, double lon2) {
    lat2 = deg2rad(lat2);
    lon2 = deg2rad(lon2);
    lat1 = deg2rad(lat1);
    lon1 = deg2rad(lon1);
    double r = 3959.;	// Earth's radius in miles
    double d = acos(sin(lat1)*sin(lat2) +
                    cos(lat1)*cos(lat2) * cos(lon2-lon1)) * r;
    double df = d * 5280.;	// distant in feet
    return df;
}

double deg2rad(double deg) {
    double rad = deg / 180. * PI;
    return rad;
}

void dumpQueue(string reason) {
    gpsErrorEvent(reason);
    for (int i=0; i < debug_queue.size(); i++) {
        cerr << debug_queue[i] << endl;
    }
    logError("End of dump queue... Starting dumping");
    got_bug = true;
    dump_start = time(NULL);
}

void gpsErrorEvent(string reason) {
    if (debug_position) {
        string event = "NotifyEngineering GPS_error "+reason;
        eventlog.append(event);
        eventlog.signalMonitor();
    }
}

void resetGps() {
    if (!no_gps_reset and !gps_reset) {
        logError("Resetting GPS");
        string resp = backtick("cell_shell resetgps");
        gps_reset = true;
    }
}

void checkFeatures() {
    if (features.checkFeatures()) {
        if (features.featureChanged(GpsDebug)) {
            getDebug();
        }
        if (features.featureChanged(LogPosition)) {
            getLogPosition();
        }
        if ( features.featureChanged(GeoFences) or
             features.featureChanged(GeoFenceFeature) ) {
            getGeoFences(false);
        }
        if ( features.featureChanged(SpeedLimits) or
             features.featureChanged(FindMySpeedFeature) ) {
            getSpeedLimits();
        }
        if (features.featureChanged(Fleet) or features.featureChanged(FleetFeature)) {
            getFleetConfig();
        }
        if (features.featureChanged(DisableLocationFeature)) {
            getDisableLocation();
        }
    }
}

void getDebug() {
    if (features.exists(GpsDebug)) {
        debug = true;
    }
    else if (!debug_option) {
        debug = false;
    }
}

void getLogPosition() {
    if (features.exists(LogPosition)) {
        log_position = true;
    }
    else {
        log_position = false;
    }
}

void getGeoFences(bool initial) {
    string fences_str = features.getFeature(GeoFences);
//    bool was_enabled = geofences_enabled;
//    geofences_enabled = features.isEnabled(GeoFenceFeature);
//    if (!geofences_enabled and was_enabled) {
//        clearAllGeoFences();
//    }
    Strings fences = split(fences_str, "|");
    vector<int> new_ids;
    for (int i=0; i < fences.size(); i++) {
        string fence = fences[i];
        int pos = fence.find(":");
        int id = stringToInt(fence.substr(0,pos));
        new_ids.push_back(id);
        string descr = fence.substr(pos+1);
        bool match = false;
        for (int j=0; j < geoFences.size(); j++) {
            if (id == geoFences[j].id()) {
                if (descr == geoFences[j].descr()) {
                    match = true;
                }
                else {
                    geoFences.erase(geoFences.begin()+j);
                }
                break;
            }
        }
        if (!match) {
//            GeoFence geofence(id,descr,initial&geofences_enabled);
            GeoFence geofence(id,descr,initial);
            geoFences.push_back(geofence);
        }
    }
    for (int j=0; j < geoFences.size(); j++) {
        int old_id = geoFences[j].id();
        bool match = false;
        for (int i=0; i < new_ids.size(); i++) {
            if (new_ids[i] == old_id) {
                match = true;
                break;
            }
        }
        if (!match) {
            geoFences.erase(geoFences.begin()+j);
            j--;
        }
    }
    if (!gpsinfo.location_disabled) {
        checkGeoFences();
    }
}

void checkGeoFences() {
    if ((gpsinfo.longitude != 0.0) or (gpsinfo.latitude != 0.0)) {
        for (int i=0; i < geoFences.size(); i++) {
            string loc = "outside";
            int id = geoFences[i].id();
            if (gpsinfo.position_known) {
                string dir = geoFences[i].crossedFence(gpsinfo.longitude, gpsinfo.latitude);
                if (dir != "") {
                    string pos = getPositionString();
                    string event = "Fencecrossing id:";
                    event += toString(id);
                    event += " dir:" + dir;
                    event += " position:" + pos;
                    eventlog.append(event);
                    eventlog.signalMonitor();
                    logError("Unit went "+dir+" fence #"+toString(id));
                }
            }
        }
    }
}

void clearAllGeoFences() {
    for (int j=0; j < geoFences.size(); j++) {
        geoFences[j].setUnknown();
    }
}

void getSpeedLimits() {
    //logError("Getting speedlimits");
    string limits_str = features.getFeature(SpeedLimits);
//    bool was_enabled = speedlimits_enabled;
//    speedlimits_enabled = features.isEnabled(FindMySpeedFeature);
//    if (!speedlimits_enabled and was_enabled) {
//        stopAllSpeedlimits();
//    }
    Strings limits = split(limits_str, "|");
    vector<int> new_ids;
    for (int i=0; i < limits.size(); i++) {
        string limit_str = limits[i];
        int pos = limit_str.find(":");
        int id = stringToInt(limit_str.substr(0,pos));
        new_ids.push_back(id);
        string descr = limit_str.substr(pos+1);
        bool match = false;
        for (int j=0; j < speedLimits.size(); j++) {
            if (id == speedLimits[j].id()) {
                if (descr == speedLimits[j].descr()) {
                    match = true;
                }
                else {
                    //logError("Removing old SpeedLimit:"+toString(id));
                    speedLimits.erase(speedLimits.begin()+j);
                }
                break;
            }
        }
        if (!match) {
            SpeedLimit speedlimit(id,descr);
            speedLimits.push_back(speedlimit);
            string tz = speedlimit.timezone();
            if (timezones.find(tz) == timezones.end()) {
                timezones[tz] = Timezone(tz);
            }
        }
    }
    for (int j=0; j < speedLimits.size(); j++) {
        int old_id = speedLimits[j].id();
        bool match = false;
        for (int i=0; i < new_ids.size(); i++) {
            if (new_ids[i] == old_id) {
                match = true;
                break;
            }
        }
        if (!match) {
            logError("Removing SpeedLimit:"+toString(old_id));
            speedLimits.erase(speedLimits.begin()+j);
            j--;
        }
    }
}

void checkSpeed() {
    map<string,long int> tz_times;
    time_t now;
    time(&now);
    for (int i=0; i < speedLimits.size(); i++) {
//        cerr << "Checking speed limits(" << speedLimits[i].id() << "): " << gpsinfo.speed << endl;
        long int hhmmss = 0;
        int day = 0;
        long int convertedtime = 0;
        string tz = speedLimits[i].timezone();
        if (tz_times.find(tz) != tz_times.end()) {
            convertedtime = tz_times[tz];
            day = convertedtime / 86400l;
            hhmmss = convertedtime % 86400l;
        }
        else {
            struct tm tm;
            string tz_abr = "";
            timezones[tz].gettime(now, &tm, tz_abr);
            day = tm.tm_wday;
            hhmmss = tm.tm_hour*3600l + tm.tm_min*60l + tm.tm_sec;
        }
        int dir = int(gpsinfo.track+0.5) % 360;
        string msg = speedLimits[i].checkSpeed(day, hhmmss, gpsinfo.speed, gpsinfo.longitude, gpsinfo.latitude, dir);
        if (msg != "") {
            logError(msg);
            eventlog.appendWithTimestamp(msg);
            eventlog.signalMonitor();
        }
    }
}

void stopAllSpeedlimits() {
    int dir = int(gpsinfo.last_track+0.5) % 360;
    for (int i=0; i < speedLimits.size(); i++) {
        string msg = speedLimits[i].stopped(gpsinfo.last_longitude,gpsinfo.last_latitude,dir);
        if (msg != "") {
            logError(msg);
            eventlog.appendWithTimestamp(msg);
            eventlog.signalMonitor();
        }
    }
}

void getFleetConfig() {
    string fleet_str = features.getFeature(Fleet);
    bool enabled = features.isEnabled(FleetFeature);
    if (enabled and fleet_str != "") {
        Strings vals = splitFields(fleet_str, ",");
        string server = vals[0];
        string moving_str = vals[1];
        string stopped_str = vals[2];
        string fleet_data = features.getData(FleetFeature);
        vals = splitFields(fleet_data, ",");
        if ((vals.size() > 0) and (vals[0] != "")) {
           moving_str = vals[0];
        }
        if ((vals.size() > 1) and (vals[1] != "")) {
           stopped_str = vals[1];
        }
        if ((vals.size() > 2) and (vals[2] != "")) {
           server = vals[2];
        }
        cout << "FleetConfig:" << server << " MovingInterval:" << moving_str << " StoppedInterval:" << stopped_str << endl;
        vals = splitFields(server, ":");
        server = vals[0];
        string port_str = vals[1];
        int port = stringToInt(port_str);
        int moving = stringToInt(moving_str);
        int stopped = stringToInt(stopped_str);
        fleet.config(server,port,moving,stopped);
    }
    else {
        fleet.clear();
    }
}

string getIpAddress(string name) {
    string ip = "";
    if (name.find_first_not_of("0123456789.") == string::npos) {
        ip = name;
    }
    else {
        string resp = backtick("resolvedns "+name);
        if (resp != "") {
            ip = getToken(resp, 0);
        }
    }
    return ip;
}

void getDisableLocation() {
    string enable_str = features.getFeature(DisableLocationFeature);
    gpsinfo.location_disabled = false;
    if (enable_str != "") {
        gpsinfo.location_disabled = true;
    }
}

string getPositionString() {
    return makePositionString(gpsinfo.latitude, gpsinfo.longitude);
}

string makePositionString(double lat, double lng) {
    return string_printf("%.8f/%.8f", lat, lng);
}

double makeDegrees(string ddmm, string nsew) {
    double deg = 0.0;
    double min = 0.0;
    if ((nsew == "N") or (nsew == "S")) {
        sscanf(ddmm.c_str(), "%2lf%9lf", &deg, &min);
    }
    else {
        sscanf(ddmm.c_str(), "%3lf%9lf", &deg, &min);
    }
    deg += min / 60.;
    if ((nsew == "S") or (nsew == "W")) {
        deg *= -1;
    }
    return deg;
}

void checkTime() {
    string date_str = gpsinfo.date;
    string time_str = gpsinfo.raw_time;
    if ((date_str.size() >= 6) and (time_str.size() >= 6)) {
        if (!got_time) {
            got_time = true;
            logError("Time now available");
            touchFile(GpsTimeFile);
            unlink(NoGpsTimeFile);
        }
        string hour_str = time_str.substr(0,2);
        string min_str = time_str.substr(2,2);
        string sec_str = time_str.substr(4);
        string year_str = date_str.substr(0,4);
        string month_str = date_str.substr(5,2);
        string day_str = date_str.substr(8,2);
        int hour = stringToInt(hour_str);
        int min = stringToInt(min_str);
        double dsec = stringToDouble(sec_str);
        int sec = (int)(dsec+0.5);
        int day = stringToInt(day_str);
        int month = stringToInt(month_str);
        int year = stringToInt(year_str);
        struct tm tm;
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = min;
        tm.tm_sec = sec;
        time_t gmt = timegm(&tm);
        time_t now = time(NULL);
        time_t diff = now - gmt;
        gpsinfo.location_time = gmt;
        if (abs(diff) > 2) {
            logError("Setting date/time:"+date_str+" "+time_str);
            tm.tm_sec = (int)dsec;
            struct timeval tv;
            tv.tv_sec = timegm(&tm);
            tv.tv_usec = (suseconds_t)((dsec-tm.tm_sec)*1000000.);
            settimeofday(&tv, NULL);
            if ( (gpsinfo.set_time_time != 0) and
                 ((time(NULL)-gpsinfo.set_time_time) < 3600) ) {
                gpsinfo.set_time_cnt++;
                if (gpsinfo.set_time_cnt == 3) {
                    logError("Time setting loop");
                    gpsErrorEvent("Time setting loop");
                }
            }
            else {
                gpsinfo.set_time_time = time(NULL);
                gpsinfo.set_time_cnt = 0;
            }
        }
    }
    else {
        if (got_time) {
            got_time = false;
            logError("Time no longer available");
            unlink(GpsTimeFile);
            touchFile(NoGpsTimeFile);
        }
    }
}

string makeTime(string time_str) {
    string time = "";
    if (time_str != "") {
        string hr = time_str.substr(0,2);
        string mn = time_str.substr(2,2);
//        string sc = time_str.substr(4,2);
        string sc = time_str.substr(4);
        int sec = int(stringToDouble(sc)+0.5);
        time = string_printf("%s:%s:%02d", hr.c_str(), mn.c_str(), sec);
//        time = hr + ":" + mn + ":" + sc;
    }
    return time;
}

string makeDate(string date_str) {
    string date = "";
    if (date_str != "") {
        string dy = date_str.substr(0,2);
        string mn = date_str.substr(2,2);
        string yr = date_str.substr(4,2);
        date = "20" + yr + "/" + mn + "/" + dy;
    }
    return date;
}

void closeAndExit(int exitstat) {
    closeAll();
    exit(exitstat);
}

void closeAll() {
    cleanFiles();
    closePort();
}

void cleanFiles() {
    unlink(GpsPresentFile);
    unlink(GpsTimeFile);
    unlink(NoGpsTimeFile);
    unlink(NoGpsPositionFile);
    no_gps = true;
}

void openPort(const char *port) {
    string dev = "/dev/";
    dev += port;
    if (!lock(port)) {
        cout << "Serial device '" << port << "' already in use" << endl;
        closeAndExit(1);
    }
    ser_s = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser_s < 0) {
        perror(serial_device.c_str());
        closeAndExit(-1);
    }
    // save current port settings 
    if (tcgetattr(ser_s,&oldtio)) {
        cerr << "Failed to get old port settings" << endl;
    }

    // set new port settings for canonical input processing 
    struct termios newtio;
    newtio.c_cflag = B4800 | CS8 | CSTOPB | CLOCAL | CREAD;
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
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0);
    if (fd >= 0) {
        close(fd);
        locked = true;
    }
    return locked;
}

bool beginsWith(string str, string match) {
    return (strncmp(str.c_str(), match.c_str(), match.size()) == 0);
}

void onTerm(int parm) {
    if (gpsinfo.position_known) {
        string pos = makePositionString(gpsinfo.last_latitude, gpsinfo.last_longitude);
        writeStringToFile(PreviousPositionFile, pos);
        fleet.writeQueue(FleetQueueFile);
    }
    closeAndExit(0);
}

void onHup(int parm) {
    stopAllSpeedlimits();
}

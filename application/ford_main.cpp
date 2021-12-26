#include <iostream>
#include <string>
#include <cstring>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "backtick.h"
#include "csv.h"
#include "split.h"
#include "splitFields.h"
#include "getToken.h"
#include "string_convert.h"
#include "string_printf.h"
#include "str_system.h"
#include "updateFile.h"
#include "logError.h"
#include "getStringFromFile.h"
#include "getUptime.h"
#include "eventLog.h"
#include "readAssoc.h"
#include "writeAssoc.h"
#include "touchFile.h"
#include "fileExists.h"
#include "split.h"
#include "vic.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

using namespace std;

#define VERSION 1
#define FAIL    -1

Assoc ford_config;
vector<int> children;
void on_child(int sig);


int app_id = 3;
unsigned long Uptime = 0;
//Byte vic_notifications[6] = {0xF0,0xF1,0xF2,0xF4,0xF5,0xF6};
Byte vic_notifications[6] = {0xF1,0xF6};
#define CellHoldTime 120


string KeyOnBundleSaved;
void saveKeyOnBundle();


void dumpBytes(Byte *b, int len);
void dumpBytesOffset(Byte *b, int len, int offset);
void sToBCD(Byte *p, string s);



#define VIC_AUTH_TYPE_INITIAL  1
#define VIC_AUTH_TYPE_FINAL    2
#define VIC_AUTH_RESPONSE_ACCEPT  1
#define VIC_AUTH_RESPONSE_REJECT  2

int getVicResponse(int reqid, int reqlen, Byte *buffer);
void processVicMessage(int len, Byte *buffer);
void decodeVicStatus(int len, Byte *buffer);
void handleVicStatus(int len, Byte *buffer);
long int getOdometerX10();
int getFuelLevel();
void getVicVersion();
void doIgnitionProcess();
void openVic();
void vicWait(int timeout);
void sendVicAuthorizationRequest(int type, string old_user, string new_user);


class PowerSchedule {
  public:
    int ignition_off_time;
    int cycle_cell_time;

    PowerSchedule(int ig_off_time, int cc_time) {
        ignition_off_time = ig_off_time;
        cycle_cell_time = cc_time;
    }
};


class CarState {
  public:
    bool ignition;
    bool engine;
    bool moving;
    bool can_active;
    bool can_stopped;
    bool vic_reset;
    unsigned int ign_time;
    //float batval;
    Byte batval;
    long last_check_time;
    float lowbatval;
    float critbatval;
    int dead_time;
    bool reboot_on_dead;
    bool remote_started;
    unsigned long command_hold;
    bool ignition_was_on;
    bool ignition_already_on;
    bool engine_was_on;
    time_t ignition_off_time;
    time_t ignition_on_time;
    string powered_off_state;
    time_t powered_off_time;
    unsigned int check_fault_time;
    int locks;
    int doors;
    int vic_status;
    vector<PowerSchedule> power_schedule;
    int cell_hold_time;
    string vin;
    unsigned int stay_on_delay;
    unsigned long low_bat_time;
    bool low_battery;
    unsigned int ignition_change_time;
    unsigned int ignition_delay;
    string ecall_state;
    bool location_disabled;
    int capabilities;
    string capabilities_str;
    int alarm_state;
    bool get_quickscan;
    int quickscan_idx;
    int quickscan_ecus;
    int quickscan_cnt;
    string quickscan_str;
//    string vic_version;
//    string vic_version_short;
//    string vic_build;
//    string vic_requested_build;
    int stopped_fuel_level;
    bool trunk_opened;
    bool valet_mode;
    bool trunk_opened_sent;
    bool keep_on;
    bool obd2_unit;

    CarState() {
        ignition = false;
        engine = false;
        moving = false;
        can_active = false;
        can_stopped = false;
        vic_reset = false;
        ign_time  = 0;
        batval = 0.;
        last_check_time = 0;
        lowbatval = 10.5;
        lowbatval = 6.0;
        dead_time = 30;
        reboot_on_dead = true;
        remote_started = false;
        command_hold = 0;
        ignition_was_on = false;
        engine_was_on = false;
        powered_off_state = "";
        powered_off_time = 0;
        ignition_off_time = 0;
        ignition_on_time = 0;
        check_fault_time = 0;
        locks = 0;
        doors = 0;
        vic_status = 0;
        cell_hold_time = CellHoldTime;
        vin = "";
        stay_on_delay = 0;
        low_bat_time = 0;
        low_battery = false;
        ignition_change_time = 0;
        ignition_delay = 0;
        ecall_state = "";
        location_disabled = false;
        capabilities_str = "none";
        alarm_state = 0x07; // SNA
        get_quickscan = false;
        quickscan_idx = 0;
        quickscan_ecus = 0;
        quickscan_cnt = 0;
        quickscan_str = "";
//        vic_version = "";
//        vic_version_short = "";
//        vic_build = "";
//        vic_requested_build = "";
        stopped_fuel_level = -1;
        trunk_opened = false;
        valet_mode = false;
        trunk_opened_sent = false;
        keep_on = false;
        obd2_unit = false;
    }

    void defaultValues() {
        lowbatval = 10.5;
        critbatval = 6.0;
        dead_time = 30;
        reboot_on_dead = true;
    }

    void setValues(string val_str) {
        Strings vals = split(val_str, "/");
        lowbatval = stringToDouble(vals[0]);
        critbatval = stringToDouble(vals[1]);
//        dead_time = stringToInt(vals[1]);
//        reboot_on_dead = false;
//        if (vals[1] == "1") {
//            reboot_on_dead = true;
//        }
    }

    void setSchedule(string sched_str) {
        power_schedule.clear();
        cell_hold_time = CellHoldTime;
        Strings strs = split(sched_str, ",");
        int steps = strs.size();
        int val1, val2;
        char ch;
        for (int i=0; i < steps; i++) {
//            if ((i == 0) and (sscanf(strs[i].c_str(), "%d%c", &val1, &ch) == 1)) {
//               cell_hold_time = val1;
//               continue;
//            }
            if (sscanf(strs[i].c_str(), "%d/%d%c", &val1, &val2, &ch) == 2) {
                int ig_off_time = val1;
                int cc_time = val2;
                power_schedule.push_back(PowerSchedule(val1, val2));
            }
            else {
                break;
            }
        }
        if (power_schedule.size() == 0) {
            power_schedule.push_back(PowerSchedule(0,0));
        }
    }

    int getPowerScheduleLength() {
        return power_schedule.size();
    }

    int getIgnitionOffTime(int idx) {
        return power_schedule[idx].ignition_off_time;
    }

    int getCycleCellTime(int idx) {
        return power_schedule[idx].cycle_cell_time;
    }

    void make_capabilities_string() {
        string cap_str = "";
        if (capabilities & 0x8000) {
            cap_str += ":HFM";
        }
        if (capabilities & 0x4000) {
            cap_str += ":REON";
        }
        if (capabilities & 0x2000) {
            cap_str += ":RSO";
        }
        if (capabilities & 0x1000) {
            cap_str += ":RTO";
        }
        if (capabilities & 0x0800) {
            cap_str += ":RDU";
        }
        if (cap_str == "") {
            cap_str = "none";
        }
        else {
            cap_str.erase(0,1);
        }
        capabilities_str = cap_str;
    }
};
CarState car_state;
VIC vic;





// predefs
void process_request(SSL *, Byte *, int);
bool forkit();


#define APPID_RVF 6
#define RVF_REQUEST 1
#define RVF_COMMAND 2
#define RVF_STATUS 3
#define APPID_CPPM_PROVISION 16
#define APPID_CPPM_APPLICATION 14
#define CPPM_PROVISION_REQUEST 1
#define CPPM_PROVISION_SUCCESS 2
#define CPPM_PROVISION_ERROR 3

#define CPPM_APPLICATION_DATA 1

#define REMOTE_VEHICLE_FUNCTION	6

class AppHeader {
public:
    Byte private_flag0;
    Byte app_id;
    Byte private_flag1;
    Byte msg_type;
    Byte version_flag;
    Byte vers;
    Byte msg_cntl_flag0;
    Byte msg_cntl_flag1;
    Byte msg_cntl_flag2;
    Byte msg_cntl_flag3;
    unsigned int msg_len;

    AppHeader(int appid, int msgtyp) {
        private_flag0 = 0;
        app_id = appid;
        private_flag1 = 0;
        msg_type = msgtyp;
        version_flag = 0;
        vers = VERSION;
        msg_cntl_flag0 = 0;
        msg_cntl_flag1 = 0;
        msg_cntl_flag2 = 0;
        msg_cntl_flag3 = 0;
        msg_len = 0;

        if ((appid == APPID_CPPM_PROVISION) || (appid == APPID_CPPM_APPLICATION)) {
            private_flag0 = 1;
            private_flag1 = 1;
        }
    }

    string pack(string b) {
        //Byte bbuf[1024];
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        msg_len = b.size();
        bbuf[0] = (private_flag0 << 6) | app_id;
        bbuf[1] = (private_flag1 << 6) | msg_type;
        bbuf[2] = vers << 4;

        if (msg_len <= 255) {
            bbuf[3] = msg_len &0xff;
            buf.assign(bbuf, 4);
        } else {
            msg_cntl_flag2 = 1;
            bbuf[2] |= msg_cntl_flag2 << 1;
            bbuf[3] = (msg_len >> 8) & 0xff;
            bbuf[4] = msg_len & 0xff;
            buf.assign(bbuf, 5);
        }
      
        return buf + b;
    }
};


#define RVF_FUNCTION_COMMAND_PERMIT 0
#define RVF_FUNCTION_COMMAND_REJECT 1
#define RVF_FUNCTION_COMMAND_ENABLE 2
#define RVF_FUNCTION_COMMAND_START 2
#define RVF_FUNCTION_COMMAND_DISABLE 3
#define RVF_FUNCTION_COMMAND_STOP 3
#define RVF_FUNCTION_COMMAND_REQUEST 4
#define RVF_FUNCTION_COMMAND_QUERY 5
#define RVF_FUNCTION_COMMAND_NACK 6

class FunctionCommand {
public:
    Byte ie_identifier;
    Byte more_flag;
    Byte length;
    Byte function_command;

    FunctionCommand(int cmd) {
        ie_identifier = 0;
        more_flag = 0;
        length = 0;
        function_command = cmd;
    }

    string pack(string b) {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));

        length = 1 + b.size();
        bbuf[0] |= (ie_identifier << 6) | (more_flag << 5) | length;
        bbuf[1] = function_command; 
        
        buf.assign(bbuf, 2);
        return buf + b;
    }
};

#define RVF_FUNCTION_STATUS_PERMITTED	0
#define RVF_FUNCTION_STATUS_REJECTED	1 
#define RVF_FUNCTION_STATUS_ENABLED	2
#define RVF_FUNCTION_STATUS_DISABLED	3
#define RVF_FUNCTION_STATUS_COMPLETED	4
#define RVF_FUNCTION_STATUS_IN_PROGRESS	5

class FunctionStatus {
public:
    Byte ie_identifier;
    Byte more_flag;
    Byte length;
    Byte function_status;

    FunctionStatus(int stat) {
        ie_identifier = 0;
        more_flag = 0;
        length = 0;
        function_status = stat;
    }

    string pack(string b) {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));

        length = 1 + b.size();
        bbuf[0] |= (ie_identifier << 6) | (more_flag << 5) | length;
        bbuf[1] = function_status; 
        
        buf.assign(bbuf, 2);
        return buf + b;
    }
};


#define ERROR_ELEMENT_STATUS_OK       0
#define ERROR_ELEMENT_STATUS_FAILED  29

class ErrorIE {
public:
    Byte ie_identifier;
    Byte more_flag;
    Byte length;
    Byte error_msg;

    ErrorIE(int stat) {
        ie_identifier = 0;
        more_flag = 0;
        length = 0;
        error_msg = stat;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));

        length = 1;
        bbuf[0] |= (ie_identifier << 6) | (more_flag << 5) | length;
        bbuf[1] = error_msg; 
        
        buf.assign(bbuf, 2);
        return buf;
    }
};

#define RVF_CONTROLLED_ENTITY_DOOR_LOCKS 0
#define RVF_CONTROLLED_ENTITY_QUERY_REQUEST 4
#define RVF_CONTROLLED_ENTITY_REMOTE_START 16
#define RVF_CONTROLLED_ENTITY_ALARM_SYSTEM 17
#define RVF_CONTROLLED_ENTITY_VEHICLE_STATUS 18
#define RVF_CONTROLLED_ENTITY_CPPM_SERVICE 19
#define RVF_CONTROLLED_ENTITY_ADMINISTRATION 21
#define RVF_CONTROLLED_ENTITY_CURRENT_GPS_POSITION 22
#define RVF_CONTROLLED_ENTITY_SCHEDULER_DATA 23
#define RVF_CONTROLLED_ENTITY_DRIVE_CONDITIONING_DATA 24
#define RVF_CONTROLLED_ENTITY_FAVORITE_LOCATION_DATA 25
#define RVF_CONTROLLED_ENTITY_TARGET_SOC 29
#define RVF_CONTROLLED_ENTITY_PHEV 30
#define RVF_CONTROLLED_ENTITY_CONFIGURATION 31
#define RVF_CONTROLLED_ENTITY_PANIC_REQUEST 32
#define RVF_CONTROLLED_ENTITY_CHIRP_REQUEST 33

class ControlFunction {
public:
    Byte ie_identifier;
    Byte more_flag;
    Byte length;
    Byte addl_flag;
    Byte controlled_entity;

    ControlFunction(int cntl_ent) {
        ie_identifier = 0;
        more_flag = 0;
        addl_flag = 0;
        controlled_entity = cntl_ent;
        length = 1;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));

        bbuf[0] |= (ie_identifier << 6) | (more_flag << 5) | length;
        bbuf[1] =(addl_flag << 7) | controlled_entity; 
        
        buf.assign(bbuf, 2);
        return buf;
    }
};


#define RVF_RAWDATA_REQUEST_NACK				0
#define RVF_RAWDATA_REQUEST_CHARGE_NOW				1
#define RVF_RAWDATA_REQUEST_REMOTE_START			2
#define RVF_RAWDATA_REQUEST_CANCEL_REMOTE_START			3
#define RVF_RAWDATA_REQUEST_CANCEL_ALARM			4
#define RVF_RAWDATA_REQUEST_UNLOCK_DOORS			5
#define RVF_RAWDATA_REQUEST_LOCK_DOORS				6
#define RVF_RAWDATA_REQUEST_CURRENT_GPS				7
#define RVF_RAWDATA_REQUEST_PROVISION_DATA			8
#define RVF_RAWDATA_REQUEST_REMOTE_DATA_AND_STATUS_QUERY	9
#define RVF_RAWDATA_REQUEST_GPS_PING				15
#define RVF_RAWDATA_REQUEST_REMOTE_DATA_REPORT			16
#define RVF_RAWDATA_REQUEST_INITIAL_AUTHORIZATION		24
#define RVF_RAWDATA_REQUEST_NOT_PROVISIONED			25
#define RVF_RAWDATA_REQUEST_VALUE_CHARGING			26
#define RVF_RAWDATA_REQUEST_SET_TARGETS				27
#define RVF_RAWDATA_REQUEST_CALENDER_INFO			30
#define RVF_RAWDATA_REQUEST_DRIVE_CONDITIONING_INFO		31
#define RVF_RAWDATA_REQUEST_CHARGE_LOCATION_INFO		32
#define RVF_RAWDATA_REQUEST_pHEV_TRIP_DISTANCE			33
#define RVF_RAWDATA_REQUEST_SERVICE_EXPIRED_ALERT		34
#define RVF_RAWDATA_REQUEST_FINAL_AUTHORIZATION			35
#define RVF_RAWDATA_REQUEST_GET_USER_SETTINGS			36
#define RVF_RAWDATA_REQUEST_CLEAR_USER_SETTINGS			37
#define RVF_RAWDATA_REQUEST_SET_CONFIG_PARAMETERS		38
#define RVF_RAWDATA_REQUEST_UPDATE_NUT_SCHEDULE			39
#define RVF_RAWDATA_REQUEST_ENABLE_LONG_TERM_PARKING		40
#define RVF_RAWDATA_REQUEST_UPDATE_DEFAULTS_DRIVE_CONDITIONING	41
#define RVF_RAWDATA_REQUEST_UPDATE_FAVORITE_LOCATION		42
#define RVF_RAWDATA_REQUEST_DISABLE_LONG_TERM_PARKING		43
#define RVF_RAWDATA_REQUEST_SERVICE_EXPIRES_SOON		44
#define RVF_RAWDATA_REQUEST_UPDATE_DEFAULTS_NUTS		45
#define RVF_RAWDATA_REQUEST_UPDATE_DEFAULTS_FAVORITE_LOCATIONS	46
#define RVF_RAWDATA_REQUEST_CPP_STATUS_DISPLAY			47


class RawDataIE {
public:
    Byte ie_identifier;
    Byte more_flag;
    int length;
    string data;

    RawDataIE() {
       ie_identifier = 1;
       more_flag = 0;
       length = 0;
       data = "";
    }

    RawDataIE(string d) {
       ie_identifier = 1;
       more_flag = 0;
       length = 0;
       data = d;
    }

    string pack(string b) {
        char bbuf[1024];
        string buf = "";

        // if empty, return empty
        if (b.size() == 0) {
           return buf;
        }

        bzero(bbuf, sizeof(bbuf));
        length = b.size();
        if (length < 32) {
            bbuf[0] = (ie_identifier << 6) | (more_flag << 5) | length;
            buf.assign(bbuf, 1);
        } else {
            bbuf[0] = (ie_identifier << 6) | (1 << 5) | (length >> 7);
            bbuf[1] = (length & 0x7f);
            buf.assign(bbuf, 2);
        }
        return buf + b;
    }
};


class RvfRawDataIE {
public:
    Byte ie_identifier;
    Byte more_flag;
    int length;
    int request_command;
    int session_id;
    string data;

    RvfRawDataIE() {
       ie_identifier = 0;
       more_flag = 1;
       length = 0;
       data = "";
    }

    RvfRawDataIE(int cmd) {
       ie_identifier = 0;
       more_flag = 1;
       length = 0;
       request_command = cmd;
       data = "";
    }

    RvfRawDataIE(string d) {
       ie_identifier = 0;
       more_flag = 1;
       length = 0;
       data = d;
    }

    string pack(string b) {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        length = b.size();
        bbuf[0] = (ie_identifier << 6) | (1 << 5) | (length >> 7);
        bbuf[1] = (length & 0x7f);
        bbuf[2] = request_command;
        bbuf[3] = session_id;
        buf.assign(bbuf, 4);
        return buf + b;
    }
};


#define CPPM_TRIP_INFORMATION 0
#define CPPM_INITIAL_AUTHORIZATION_ALERT 26
#define CPPM_FINAL_AUTHORIZATION_ALERT 45

class CPPMRawDataIE {
public:
    Byte ie_identifier;
    Byte more_flag;
    int length;
    int msg_type;
    int session_id;
    string data;

    CPPMRawDataIE() {
       ie_identifier = 0;
       more_flag = 1;
       length = 0;
       data = "";
    }

    CPPMRawDataIE(int msgtype) {
       ie_identifier = 0;
       more_flag = 1;
       length = 0;
       msg_type = msgtype;
       data = "";
    }

    CPPMRawDataIE(int msgtype, string d) {
       ie_identifier = 0;
       more_flag = 1;
       length = 0;
       msg_type = msgtype;
       data = d;
    }

    string pack(string b) {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        length = 2 + b.size();
        bbuf[0] = (ie_identifier << 6) | (1 << 5) | (length >> 7);
        bbuf[1] = (length & 0x7f);
        bbuf[2] = ( 0 << 7) | msg_type;
        //bbuf[3] = session_id;
        bbuf[3] = 0;
        buf.assign(bbuf, 4);
        return buf + b;
    }
};


class TextIE {
public:
    Byte ie_identifier;
    Byte more_flag;
    int length;
    string text;

    TextIE() {
       ie_identifier = 1;
       more_flag = 0;
       length = 0;
       text = "";
    }

    TextIE(string t) {
       ie_identifier = 1;
       more_flag = 0;
       length = 0;
       text = t;
    }

   string pack(string b) {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        length = b.size();
        if (length < 32) {
            bbuf[0] = (ie_identifier << 6) | (more_flag << 5) | length;
            buf.assign(bbuf, 1);
        } else {
            bbuf[0] = (ie_identifier << 6) | (1 << 5) | (length >> 7);
            bbuf[1] = (length & 0x7f);
            buf.assign(bbuf, 2);
        }
        return buf + b;
   }
};


class VehicleDescriptor {
public:
    Byte ie_identifier;
    Byte more_flag;
    Byte length;
    Byte addl_flag;
    Byte language;
    Byte vin_flag;
    Byte tcu_esn;
    Byte color;
    Byte model;
    Byte plate;
    Byte imei;
    string vin;
    TextIE vin_ie;

    VehicleDescriptor(string v) {
        ie_identifier = 0;
        more_flag = 0;
        length = 0;
        addl_flag = 0;
        language = 0;
        vin_flag = 0;
        tcu_esn = 0;
        color = 0;
        model = 0;
        plate = 0;
        imei = 0;
        vin = v;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));

        length = 20; 
        bbuf[0] = (ie_identifier << 6) | (more_flag << 5) | length;
        bbuf[1] = (addl_flag << 7) | (language << 6) | (vin_flag << 5) | (tcu_esn << 4) | (tcu_esn << 3) | (color << 2) | (plate << 1) | imei;

        buf.assign(bbuf, 2);
        buf += vin_ie.pack(vin);
        return buf;
    }
};

class VersionIE {
public:
    Byte ie_identifier;
    Byte more_flag;
    Byte length;
    Byte car_id;
    Byte tcu_id;
    Byte major_hw;
    Byte major_sw;

    VersionIE() {
        ie_identifier = 0;
        more_flag = 0;
        length = 4;
        car_id = 4; 
        tcu_id  = 0x80; 
        major_hw = 0;
        major_sw = 0;
    };

 
    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));

        bbuf[0] = (ie_identifier << 6) | (more_flag << 5) | length;
        bbuf[1] = car_id;
        bbuf[2] = tcu_id;
        bbuf[3] = major_hw;
        bbuf[4] = major_sw;
        
        buf.assign(bbuf, 5);
        return buf;
    }
};

class Timestamp {
public:
    Byte Year;
    Byte Month;
    Byte Day;
    Byte Hour;
    Byte Minutes;
    Byte Seconds;

    Timestamp(struct tm *t) {
        Year = (t->tm_year + 1900) % 100;
        Month = t->tm_mon + 1;
        Day = t->tm_mday;
        Hour = t->tm_hour;
        Minutes = t->tm_min;
        Seconds = t->tm_sec;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";
        bbuf[0] = (Year << 2) | (Month >> 6);
        bbuf[1] = (Month << 6) | (Day << 1) | (Hour >> 7);
        bbuf[2] = (Hour << 3) | (Minutes >> 4);
        bbuf[3] = (Minutes << 5) | Seconds;

        buf.assign(bbuf, 4);
        return buf;
    }
};


#define INVEHICLE_AUTHORIZATION_DENY       0
#define INVEHICLE_AUTHORIZATION_CONFIRMED  1

class InVehicleAuthorizationData {
public:
    Byte auth_resp;
    Byte key_id;
    Byte pkey_id;
    Byte peps;
    Byte last_lock;

    InVehicleAuthorizationData(int resp) {
        auth_resp = resp;
        key_id = 0;
        pkey_id = 0;
        peps = 0;
        last_lock = 0;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";
        bbuf[0] = auth_resp;
        bbuf[1] = key_id;
        bbuf[2] = pkey_id;
        bbuf[3] = (peps << 3) | last_lock; 

        buf.assign(bbuf, 4);
        return buf;
    }
};

class ProvisioningDataBundle {
public:
    string vin;
    string esn;
    string imei;
    string sim_msisdn;
    string sim_imsi;
    string sim_iccid;
    Byte fw_vers_imx[3];
    Byte fw_vers_vic[3];
    Byte fw_vers_gold[3];
    Byte config_vers[3];
    Byte bus_architecture;

    ProvisioningDataBundle(string v) {
        vin = v;
        esn = "";
        imei = backtick("cell_shell getprofile | grep imei: | sed 's/imei://'");
        sim_msisdn = "";
        sim_imsi = backtick("cell_shell getprofile | grep imsi: | sed 's/imsi://'");
        sim_iccid = "";
        bzero(fw_vers_imx, sizeof(fw_vers_imx));
        bzero(fw_vers_vic, sizeof(fw_vers_vic));
        bzero(fw_vers_gold, sizeof(fw_vers_gold));
        bzero(config_vers, sizeof(config_vers));
        bus_architecture = 0;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        strcpy(&bbuf[0], vin.c_str());
        sToBCD((Byte *)&bbuf[25], imei);
        sToBCD((Byte *)&bbuf[41], sim_imsi);

/*
        buf += vin;
        if (esn.size() == 0)        { buf.append(8, 0x00); }
        if (imei.size() == 0)       { buf.append(8, 0x00); }
        if (sim_msisdn.size() == 0) { buf.append(8, 0x00); }
        if (sim_imsi.size() == 0)   { buf.append(8, 0x00); }
        if (sim_iccid.size() == 0)  { buf.append(8, 0x00); }
        buf.append((char *)fw_vers_imx, 3); 
        buf.append((char *)fw_vers_vic, 3); 
        buf.append((char *)fw_vers_gold, 3); 
*/
        buf.assign(bbuf, 65);
        return buf;
    }
};

class RemoteDataBundle {
public:
    Byte CSoC;
    unsigned int DTE;
    Byte ArbitratedPlugState;
    Byte PePCState;
    Byte CabinTemperature;
    Byte ChargerPowerType;
    Byte ChargingStatus;
    unsigned int TimeToTargetDistance;
    unsigned int TimeToTargetSOC;
    unsigned int TimeToFullCharge;
    Byte BatteryPerformanceState;
    Byte ChargerPlugState;
    Byte NextChargeBeginTimeYear;
    Byte NextChargeBeginTimeMonth;
    Byte NextChargeBeginTimeDay;
    Byte NextChargeBeginTimeHour;
    Byte NextChargeBeginTimeMinutes;
    Byte NextChargeBeginTimeSeconds;
    Byte NextChargeEndTimeYear;
    Byte NextChargeEndTimeMonth;
    Byte NextChargeEndTimeDay;
    Byte NextChargeEndTimeHour;
    Byte NextChargeEndTimeMinutes;
    Byte NextChargeEndTimeSeconds;
    unsigned int ChargerPowerDraw;
    Byte ChargeNowDurationToFullChargeHours;
    unsigned int AmbientTemperature;

    RemoteDataBundle() {
         CSoC = 0;
         DTE = 0;
         ArbitratedPlugState = 0;
         PePCState = 0;
         CabinTemperature = 0;
         ChargerPowerType = 0;
         ChargingStatus = 0;
         TimeToTargetDistance = 0;
         int TimeToTargetSOC = 0;
         int TimeToFullCharge = 0;
         BatteryPerformanceState = 0;
         ChargerPlugState = 0;
         NextChargeBeginTimeYear = 0;
         NextChargeBeginTimeMonth = 0;
         NextChargeBeginTimeDay = 0;
         NextChargeBeginTimeHour = 0;
         NextChargeBeginTimeMinutes = 0;
         NextChargeBeginTimeSeconds = 0;
         NextChargeEndTimeYear = 0;
         NextChargeEndTimeMonth = 0;
         NextChargeEndTimeDay = 0;
         NextChargeEndTimeHour = 0;
         NextChargeEndTimeMinutes = 0;
         NextChargeEndTimeSeconds = 0;
         ChargerPowerDraw = 0;
         ChargeNowDurationToFullChargeHours = 0;
         AmbientTemperature = 0;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        buf.assign(bbuf, 26);
        return buf;
    }
};

class GPSInfoBundle {
public:
    Byte LatitudeDegrees;
    unsigned int LatitudeMinutesDec;
    Byte LatitudeMinutes;
    unsigned int LongitudeDegrees;
    unsigned int LongitudeMinutesDec;
    Byte LongitudeMinutes;
    Byte TimeYear;
    Byte TimeMonth;
    Byte TimeDay;
    Byte TimeHours;
    Byte TimeMinutes;
    Byte TimeSeconds;
    Byte CompassDirection;
    unsigned int GPSAltitude;
    Byte GPSHemisphereEast; 
    Byte GPSHemisphereSouth; 
    Byte Fault;
    unsigned int GPSHeading;
    unsigned int GPSSpeed;
    Byte GPSActualPosition;
    Byte GPSDimension;
    unsigned int VehicleSpeed;

    string GPSPosition;

    GPSInfoBundle() {
        LatitudeDegrees = 0;
        LatitudeMinutesDec = 0;
        LatitudeMinutes = 0;
        LongitudeDegrees = 0;
        LongitudeMinutesDec = 0;
        LongitudeMinutes = 0;
        TimeYear = 0;
        TimeMonth = 0;
        TimeDay = 0;
        TimeHours = 0;
        TimeMinutes = 0;
        TimeSeconds = 0;
        CompassDirection = 0;
        GPSAltitude = 0;
        GPSHemisphereEast = 0; 
        GPSHemisphereSouth = 0; 
        Fault = 0;
        GPSHeading = 0;
        GPSSpeed = 0;
        GPSActualPosition = 0;
        GPSDimension = 0;
        VehicleSpeed = 0;
 
        if (fileExists("/tmp/gps_position")) {
            GPSPosition = backtick("cat /tmp/gps_position");
        } else if (fileExists("/tmp/gps_last_position")) {
            GPSPosition = backtick("cat /tmp/gps_last_position");
        } else {
            GPSPosition = "0./0.";
        }

        {
            Strings vals = split(GPSPosition, "/");
            double latitude = stringToDouble(vals[0]);
            double longitude = stringToDouble(vals[1]);
            int intpart;

            if (latitude < 0) {
               GPSHemisphereSouth = 1;
               latitude *= -1;
            }
            LatitudeDegrees = (int)latitude; 
            LatitudeMinutes = (int)((latitude - LatitudeDegrees) * 60);
            LatitudeMinutesDec = (int)((((latitude - LatitudeDegrees) * 60) - LatitudeMinutes) * 10000);

            if (longitude >= 0) {
               GPSHemisphereEast = 1;
            } else {
                longitude *= -1;
            }
            LongitudeDegrees = (int)longitude; 
            LongitudeMinutes = (int)((longitude - LongitudeDegrees) * 60);
            LongitudeMinutesDec = (int)((((longitude - LongitudeDegrees) * 60) - LongitudeMinutes) * 10000);
        }
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        Strings vals = split(GPSPosition, "/");
        double latitude = stringToDouble(vals[0]);
        double longitude = stringToDouble(vals[1]);

        sprintf(bbuf, "%+010.8f/%+010.8f", latitude, longitude);
printf("lat/long: %s\n", bbuf);
//        buf.assign(bbuf, 25);

        bzero(bbuf, sizeof(bbuf));
        bbuf[0] = LatitudeDegrees;
        bbuf[1] = (LatitudeMinutesDec >> 8) & 0x3f;
        bbuf[2] = LatitudeMinutesDec & 0xff;
        bbuf[3] = LatitudeMinutes & 0x3f;
        bbuf[4] = (LongitudeDegrees >> 8) & 0x1;
        bbuf[5] = LongitudeDegrees & 0xff;
        bbuf[6] = (LongitudeMinutesDec >> 8) & 0x3f;
        bbuf[7] = LongitudeMinutesDec & 0xff;
        bbuf[8] = LongitudeMinutes & 0x3f;
        bbuf[16] = (GPSHemisphereEast & 0x3) << 3;
        bbuf[16] = (GPSHemisphereSouth & 0x3) << 1;

        buf.assign(bbuf, 23);
        return buf;
    }
};


class KeyOnBundle {
public:
    unsigned long Odometer;
    unsigned long ModemTime;
    Byte HECYear;
    Byte HECMonth;
    Byte HECDay;
    Byte HECHour;
    Byte HECMinute;
    Byte HECSecond;
    Byte CSoC;
    unsigned int DTE;
    GPSInfoBundle gps;
    Byte APIMSoftwareVersion;

    KeyOnBundle(struct tm *t) {
        Odometer = getOdometerX10();
        ModemTime = 0;
        HECYear = (t->tm_year + 1900) % 100;
        HECMonth = t->tm_mon + 1;
        HECDay = t->tm_mday;
        HECHour = t->tm_hour;
        HECMinute = t->tm_min;
        HECSecond = t->tm_sec;
        CSoC = 0;
        DTE = 0;
        APIMSoftwareVersion = 0;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        bbuf[0] = (Odometer >> 16) & 0xff;
        bbuf[1] = (Odometer >> 8) & 0xff;
        bbuf[2] = (Odometer >> 0) & 0xff;
        bbuf[7] = HECYear;
        bbuf[8] = HECMonth;
        bbuf[9] = HECDay;
        bbuf[10] = HECHour;
        bbuf[11] = HECMinute;
        bbuf[12] = HECSecond;

        string gps_info = gps.pack();
        memcpy(&bbuf[16], gps_info.c_str(), gps_info.size());

        buf.assign(bbuf, 40);
        return buf;
    }
};

class KeyOffBundle {
public:
    unsigned long Odometer;
    unsigned long ModemTime;
    Byte HECYear;
    Byte HECMonth;
    Byte HECDay;
    Byte HECHour;
    Byte HECMinute;
    Byte HECSecond;
    Byte CSoC;
    GPSInfoBundle gps;
    unsigned int EnergyConsumedKWH;
    unsigned int EnergyConsumedLiters;
    unsigned int TripLength;
    Byte NextChargeBeginTimeYear;
    Byte NextChargeBeginTimeMonth;
    Byte NextChargeBeginTimeDay;
    Byte NextChargeBeginTimeHour;
    Byte NextChargeBeginTimeMinutes;
    Byte NextChargeBeginTimeSeconds;
    Byte NextChargeEndTimeYear;
    Byte NextChargeEndTimeMonth;
    Byte NextChargeEndTimeDay;
    Byte NextChargeEndTimeHour;
    Byte NextChargeEndTimeMinutes;
    Byte NextChargeEndTimeSeconds;
    Byte ChargePointArrivalFlag;
    Byte ChargeNowDurationHours;
    Byte GSMSignalStrength;
    Byte GSMSignalQuality;
    Byte GSMDRXLevl;
    Byte GSMRoamingFlag;
    Byte GSMNeighbors;
    unsigned int RegenPercentages1;
    unsigned int RegenPercentages2;
    unsigned int RegenPercentages3;
    unsigned long RegenPercentages4;
    unsigned int FuelLevel;
    unsigned int DriverScores1;
    unsigned int DriverScores2;
    unsigned int DriverScores3;
    unsigned int DriverScores4;
    unsigned int DriverScores5;
    unsigned int DriverScores6;
    Byte KeyInUSe;
    unsigned int FuelEcon1;
    unsigned int FuelEcon2;
    unsigned int FuelEcon3;
    Byte WrenchLight1;
    Byte WrenchLight2;
    Byte WrenchLight3;
    Byte WrenchLight4;
    Byte WrenchLight5;
    Byte WrenchLight6;
    Byte WrenchLight7;
    Byte WrenchLight8;
    Byte WrenchLight9;
    Byte WrenchLight10;

    KeyOffBundle(struct tm *t) {
        Odometer = getOdometerX10();
        ModemTime = 0;
        HECYear = (t->tm_year + 1900) % 100;
        HECMonth = t->tm_mon + 1;
        HECDay = t->tm_mday;
        HECHour = t->tm_hour;
        HECMinute = t->tm_min;
        HECSecond = t->tm_sec;
        CSoC = 0;
    
        EnergyConsumedKWH = 0;
        EnergyConsumedLiters = 0;
        TripLength = 0;
        NextChargeBeginTimeYear = 0;
        NextChargeBeginTimeMonth = 0;
        NextChargeBeginTimeDay = 0;
        NextChargeBeginTimeHour = 0;
        NextChargeBeginTimeMinutes = 0;
        NextChargeBeginTimeSeconds = 0;
        NextChargeEndTimeYear = 0;
        NextChargeEndTimeMonth = 0;
        NextChargeEndTimeDay = 0;
        NextChargeEndTimeHour = 0;
        NextChargeEndTimeMinutes = 0;
        NextChargeEndTimeSeconds = 0;
        ChargePointArrivalFlag = 0;
        ChargeNowDurationHours = 0;
        GSMSignalStrength = 0;
        GSMSignalQuality = 0;
        GSMDRXLevl = 0;
        GSMRoamingFlag = 0;
        GSMNeighbors = 0;
        RegenPercentages1 = 0;
        RegenPercentages2 = 0;
        RegenPercentages3 = 0;
        RegenPercentages4 = 0;
        FuelLevel = (unsigned int)getFuelLevel();
        DriverScores1 = 0;
        DriverScores2 = 0;
        DriverScores3 = 0;
        DriverScores4 = 0;
        DriverScores5 = 0;
        DriverScores6 = 0;
        KeyInUSe = 0;
        FuelEcon1 = 0;
        FuelEcon2 = 0;
        int FuelEcon3 = 0;
        WrenchLight1 = 0;
        WrenchLight2 = 0;
        WrenchLight3 = 0;
        WrenchLight4 = 0;
        WrenchLight5 = 0;
        WrenchLight6 = 0;
        WrenchLight7 = 0;
        WrenchLight8 = 0;
        WrenchLight9 = 0;
        WrenchLight10 = 0;
    } 

    string pack() {
        char bbuf[1024];
        string buf = "";

        bzero(bbuf, sizeof(bbuf));
        bbuf[0] = (Odometer >> 16) & 0xff;
        bbuf[1] = (Odometer >> 8) & 0xff;
        bbuf[2] = (Odometer >> 0) & 0xff;
        bbuf[7] = HECYear;
        bbuf[8] = HECMonth;
        bbuf[9] = HECDay;
        bbuf[10] = HECHour;
        bbuf[11] = HECMinute;
        bbuf[12] = HECSecond;

        string gps_info = gps.pack();
//dumpBytesOffset((Byte *)gps_info.c_str(), gps_info.size(), 4);
        memcpy(&bbuf[13], gps_info.c_str(), gps_info.size());

        bbuf[65]  = (FuelLevel & 0x300) >> 8;
        bbuf[66]  = (FuelLevel & 0xff);

        buf.assign(bbuf, 85);
        return buf;
    }
};



class VehicleStatusBundle {
public:
    Byte DriverWindowPosition;
    Byte PassengerWindowPosition;
    Byte RearPassengerWindowPosition;
    Byte RearDriverWindowPosition;
    Byte RearRightDoorAjarStatus;
    Byte RearLeftDoorAjarStatus;
    Byte PassengerDoorAjarStatus;
    Byte InnerTailgateAjarStatus;
    Byte HoodAjarStatus;
    Byte DriverDoorAjarStatus;
    Byte TailgateAjarStatus;
    Byte DoorsLockedStatus;
    Byte RemoteStartSuccess;
    Byte RemoteStartStatus;
    Byte RemoteStartDurationRemaining;
    Byte RemoteStartSetting;
    Byte CabinTemperature;
    unsigned int pHevFuelLevel;
    Byte CSoC;
    Byte VBAT;
    Byte TirePressure;
    Byte IgnitionStatus;
    Byte AlarmMode;
    Byte AlarmSource;
    unsigned int VehicleSpeed;
    Byte TripReadyFlag;
    unsigned int AmbientTemperature;

    VehicleStatusBundle() {
         // get status from vic
         Byte buffer[64];
         int len;
do {
         //len = getVicResponse(0x20, 0, buffer);
         len = getVicResponse(0x22, 0, buffer);
} while (len == 0);
         //int len = getVicResponse(0x20, 0, buffer);
         decodeVicStatus(len, buffer);

         // get speed
do {
         len = getVicResponse(0x27, 0, buffer);
} while (len == 0);
         VehicleSpeed = (buffer[2] << 8) | buffer[3];

         DriverWindowPosition = 0;
         PassengerWindowPosition = 0;
         RearPassengerWindowPosition = 0;
         RearDriverWindowPosition = 0;
         RearRightDoorAjarStatus = (car_state.doors & 0x08) ? 1 : 0;
         RearLeftDoorAjarStatus = (car_state.doors & 0x04) ? 1 : 0; 
         PassengerDoorAjarStatus = (car_state.doors & 0x02) ? 1 : 0;
         InnerTailgateAjarStatus = 0;
         HoodAjarStatus = 0;
         DriverDoorAjarStatus = (car_state.doors & 0x01) ? 1 : 0;;
         TailgateAjarStatus = (car_state.doors & 0x10) ? 1 : 0;;
         DoorsLockedStatus = car_state.locks;
         RemoteStartSuccess = 0;
         RemoteStartStatus = 0;
         RemoteStartDurationRemaining = 0;
         RemoteStartSetting = 0;
         CabinTemperature = 0;
         pHevFuelLevel = (unsigned int)getFuelLevel();
         CSoC = 0;
         VBAT = car_state.batval;
         TirePressure = 0;
         IgnitionStatus = car_state.ignition;
         AlarmMode = 0;
         AlarmSource = 0;
         //VehicleSpeed = 0;
         TripReadyFlag = 0;
         AmbientTemperature = 0;
    }

    string pack() {
        char bbuf[1024];
        string buf = "";

        bbuf[0]  = (DriverWindowPosition & 0x7) << 3;
        bbuf[0] |= (PassengerWindowPosition & 0x7);
        bbuf[1]  = (RearPassengerWindowPosition & 0x7) << 3;
        bbuf[1] |= (RearDriverWindowPosition & 0x7);
        bbuf[2]  = (RearRightDoorAjarStatus & 0x1) << 6;
        bbuf[2] |= (RearLeftDoorAjarStatus & 0x1) << 5;
        bbuf[2] |= (PassengerDoorAjarStatus & 0x1) << 4;
        bbuf[2] |= (InnerTailgateAjarStatus & 0x1) << 3;
        bbuf[2] |= (HoodAjarStatus & 0x1) << 2;
        bbuf[2] |= (DriverDoorAjarStatus & 0x1) << 1;
        bbuf[2] |= (TailgateAjarStatus & 0x1) << 0;
        bbuf[3]  = (DoorsLockedStatus & 0x1f);
        bbuf[4]  = (RemoteStartSuccess & 0x3) << 1;
        bbuf[4] |= (RemoteStartStatus & 0x1);
        bbuf[5]  = RemoteStartDurationRemaining;
        bbuf[6]  = (RemoteStartSetting & 0x7f);
        bbuf[7]  = CabinTemperature;
        bbuf[8]  = (pHevFuelLevel & 0x300) >> 8;
        bbuf[9]  = (pHevFuelLevel & 0xff);
        bbuf[10]  = CSoC;
        bbuf[11]  = VBAT;
        bbuf[12]  = (TirePressure & 0xf);
        bbuf[13]  = (IgnitionStatus & 0xf);
        bbuf[14]  = (AlarmMode & 0x3);
        bbuf[15]  = AlarmSource;
        bbuf[16]  = (VehicleSpeed & 0xff00) >> 8;
        bbuf[17]  = (VehicleSpeed & 0xff);
        bbuf[18]  = (TripReadyFlag & 0xf);
        bbuf[19]  = (AmbientTemperature & 0x300) >> 8;
        bbuf[20]  = (AmbientTemperature & 0xff);

        buf.assign(bbuf, 21);
        return buf;
    }
};

void dumpBytes(Byte *b, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        printf("%02x ", b[i]);
    }
    printf("\n");
}

void dumpBytesOffset(Byte *b, int len, int offset)
{
     int i, j;
     for (i = 0; i < len; i++) {
         if ((i!=0) && (i%10) == 0) {
             printf("\n");
         }
         if (i%10 == 0) {
             for (j = 0; j < offset; j++) {
                 printf(" ");
             }
         }
         
         printf("%02x ", b[i]);
     }
     printf("\n");
}

#define Socket int
Socket open_server()
{
    Socket sockfd;
    int len, result;
    struct sockaddr_in address;

    //Create socket
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket create failed.\n") ;
        return -1 ;
    }

    //Name the socket as agreed with server.
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ford_config["server"].c_str());
    address.sin_port = htons(atoi(ford_config["port"].c_str()));
    len = sizeof(address);

    result = connect(sockfd, (struct sockaddr *)&address, len);
    if(result == -1)
    {
        perror("Error has occurred");
        exit(-1);
    }

    return sockfd;
}

/*---------------------------------------------------------------------*/
/*--- InitCTX - initialize the SSL engine.                          ---*/
/*---------------------------------------------------------------------*/
SSL_CTX* InitCTX(void)
{   SSL_METHOD *method;
    SSL_CTX *ctx;

    SSL_library_init();
    OpenSSL_add_all_algorithms();               /* Load cryptos, et.al. */
    SSL_load_error_strings();                   /* Bring in and register error messages */
    method = (SSL_METHOD *)SSLv3_client_method();               /* Create new client-method instance */
    ctx = SSL_CTX_new(method);                  /* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}

/*---------------------------------------------------------------------*/
/*--- ShowCerts - print out the certificates.                       ---*/
/*---------------------------------------------------------------------*/
void ShowCerts(SSL* ssl)
{   X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl);       /* get the server's certificate */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);                                                     /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);                                                     /* free the malloc'ed string */
        X509_free(cert);                                        /* free the malloc'ed certificate copy */
    }
    else
        printf("No certificates.\n");
}


void send_provisioning(string vin) {
    if (forkit()) {
        SSL_CTX *ctx;
        SSL *ssl;

        ctx = InitCTX();
        Socket sock = open_server();
        if (sock > 0) {
           Byte rcvbuffer[1024];

           ssl = SSL_new(ctx);                                 /* create new SSL connection state */
           SSL_set_fd(ssl, sock);                              /* attach the socket descriptor */
           if ( SSL_connect(ssl) == FAIL )                     /* perform the connection */
               ERR_print_errors_fp(stderr);
           else {
                printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
                ShowCerts(ssl);                                /* get any certs */

                // defines
                AppHeader app_hdr(APPID_CPPM_PROVISION, CPPM_PROVISION_REQUEST);
                VersionIE vers;
                time_t now = time(0);
                Timestamp ts(localtime(&now));
                RawDataIE rd;
                ProvisioningDataBundle pdb(vin);
    
                // build packet 
                string pkt_buf = app_hdr.pack(vers.pack() + ts.pack() + rd.pack(pdb.pack()));
    
                // send it
                cout << "sending provisioning" << endl; 
                cout << "   sent: "; 
                dumpBytes((Byte *)pkt_buf.c_str(), pkt_buf.size());
                //send(sock, pkt_buf.c_str(), pkt_buf.size(), 0);
                SSL_write(ssl, pkt_buf.c_str(), pkt_buf.size());
    
                // get response
                cout << "   recv: "; 
                //int rcv_len = recv(sock, rcvbuffer, sizeof(rcvbuffer), 0);
                int rcv_len = SSL_read(ssl, rcvbuffer, sizeof(rcvbuffer));
                dumpBytes(rcvbuffer, rcv_len);

                SSL_free(ssl);                                   /* release connection state */
            }
            close(sock);                                         /* close socket */
            SSL_CTX_free(ctx);                                   /* release context */
        }
        exit(0);
    }
}

void get_request() {
    if (forkit()) {
        SSL_CTX *ctx;
        SSL *ssl;

        ctx = InitCTX();
        Socket sock = open_server();
        if (sock > 0) {
           Byte rcvbuffer[1024];

           ssl = SSL_new(ctx);                                 /* create new SSL connection state */
           SSL_set_fd(ssl, sock);                              /* attach the socket descriptor */
           if ( SSL_connect(ssl) == FAIL )                     /* perform the connection */
               ERR_print_errors_fp(stderr);
           else {
                printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
                ShowCerts(ssl);                                /* get any certs */

                // defines
                AppHeader app_hdr(APPID_RVF, RVF_REQUEST);
                time_t now = time(0);
                Timestamp ts(localtime(&now));
                VehicleDescriptor v_desc(ford_config["vin"]);
                ControlFunction cntl_func(RVF_CONTROLLED_ENTITY_QUERY_REQUEST);
                FunctionCommand func_cmd(RVF_FUNCTION_COMMAND_QUERY);
    
                // build packet
                string pkt_buf = app_hdr.pack(ts.pack() + v_desc.pack() + cntl_func.pack() + func_cmd.pack(""));
    
                // send it
                cout << "sending get_request" << endl; 
                cout << "   sent: "; 
                dumpBytes((Byte *)pkt_buf.c_str(), pkt_buf.size());
                //send(sock, pkt_buf.c_str(), pkt_buf.size(), 0);
                SSL_write(ssl, pkt_buf.c_str(), pkt_buf.size());
    
                // get response
                //int rcv_len = recv(sock, rcvbuffer, sizeof(rcvbuffer), 0);
                int rcv_len = SSL_read(ssl, rcvbuffer, sizeof(rcvbuffer));
                cout << "   recv: "; 
                dumpBytes(rcvbuffer, rcv_len);
    
                // process request
                //process_request(sock, rcvbuffer, rcv_len);
                process_request(ssl, rcvbuffer, rcv_len);

                SSL_free(ssl);                                   /* release connection state */
            }
            close(sock);                                         /* close socket */
            SSL_CTX_free(ctx);                                   /* release context */
        }
        exit(0);
    }
}

//void send_initial_auth_in_progress(Socket sock) {
void send_initial_auth_in_progress(SSL *ssl) {
//    if (sock > 0) {
        Byte rcvbuffer[1024];

        // defines
        AppHeader app_hdr(APPID_RVF, RVF_STATUS);
        time_t now = time(0);
        Timestamp ts(localtime(&now));
        VehicleDescriptor v_desc(ford_config["vin"]);
        ControlFunction cntl_func(RVF_CONTROLLED_ENTITY_ADMINISTRATION);
        FunctionStatus func_stat(RVF_FUNCTION_STATUS_IN_PROGRESS);
        RvfRawDataIE rd(RVF_RAWDATA_REQUEST_INITIAL_AUTHORIZATION);
        ErrorIE eie(ERROR_ELEMENT_STATUS_OK);

        // build packet
        //string pkt_buf = app_hdr.pack(ts.pack() + cntl_func.pack() + func_stat.pack("") + rd.pack("") + eie.pack());
        string pkt_buf = app_hdr.pack(ts.pack() + cntl_func.pack() + func_stat.pack(rd.pack("")) + eie.pack());

        // send it
        cout << "sending initial auth in progress" << endl; 
        cout << "   sent: "; 
        dumpBytes((Byte *)pkt_buf.c_str(), pkt_buf.size());
        //size_t retval = send(sock, pkt_buf.c_str(), pkt_buf.size(), 0);
        SSL_write(ssl, pkt_buf.c_str(), pkt_buf.size());
//    }
}

//void send_final_auth_in_progress(Socket sock) {
void send_final_auth_in_progress(SSL *ssl) {
//    if (sock > 0) {
        Byte rcvbuffer[1024];

        // defines
        AppHeader app_hdr(APPID_RVF, RVF_STATUS);
        time_t now = time(0);
        Timestamp ts(localtime(&now));
        VehicleDescriptor v_desc(ford_config["vin"]);
        ControlFunction cntl_func(RVF_CONTROLLED_ENTITY_ADMINISTRATION);
        FunctionStatus func_stat(RVF_FUNCTION_STATUS_IN_PROGRESS);
        RvfRawDataIE rd(RVF_RAWDATA_REQUEST_FINAL_AUTHORIZATION);
        ErrorIE eie(ERROR_ELEMENT_STATUS_OK);

        // build packet
        //string pkt_buf = app_hdr.pack(ts.pack() + v_desc.pack() + cntl_func.pack() + func_stat.pack("") + rd.pack(""));
        string pkt_buf = app_hdr.pack(ts.pack() + cntl_func.pack() + func_stat.pack(rd.pack("")) + eie.pack());

        // send it
        cout << "sending final auth in progress" << endl; 
        cout << "   sent: "; 
        dumpBytes((Byte *)pkt_buf.c_str(), pkt_buf.size());
        //send(sock, pkt_buf.c_str(), pkt_buf.size(), 0);
        SSL_write(ssl, pkt_buf.c_str(), pkt_buf.size());
//    }
}

//void send_vehicle_status(Socket sock) {
void send_vehicle_status(SSL *ssl) {
//    if (sock > 0) {
        Byte rcvbuffer[1024];

        // defines
        AppHeader app_hdr(APPID_RVF, RVF_STATUS);
        time_t now = time(0);
        Timestamp ts(localtime(&now));
        ControlFunction cntl_func(RVF_CONTROLLED_ENTITY_VEHICLE_STATUS);
        FunctionStatus func_stat(RVF_FUNCTION_STATUS_COMPLETED);
        RvfRawDataIE rd(RVF_RAWDATA_REQUEST_REMOTE_DATA_AND_STATUS_QUERY);
        RemoteDataBundle rdb;
        VehicleStatusBundle vsb;
        ErrorIE eie(ERROR_ELEMENT_STATUS_OK);

        // build packet
        string pkt_buf = app_hdr.pack(ts.pack() + cntl_func.pack() + func_stat.pack(rd.pack(rdb.pack() + vsb.pack())) + eie.pack());

        // send it
        cout << "sending vehicle status" << endl; 
        cout << "   sent: "; 
        dumpBytes((Byte *)pkt_buf.c_str(), pkt_buf.size());
        //send(sock, pkt_buf.c_str(), pkt_buf.size(), 0);
        SSL_write(ssl, pkt_buf.c_str(), pkt_buf.size());
//    }
}

void send_cppm_alert(int alert, int resp) {
    if (forkit()) {
        SSL_CTX *ctx;
        SSL *ssl;

        ctx = InitCTX();
        Socket sock = open_server();
        if (sock > 0) {
           Byte rcvbuffer[1024];

           ssl = SSL_new(ctx);                                 /* create new SSL connection state */
           SSL_set_fd(ssl, sock);                              /* attach the socket descriptor */
           if ( SSL_connect(ssl) == FAIL )                     /* perform the connection */
               ERR_print_errors_fp(stderr);
           else {
                printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
                ShowCerts(ssl);                                /* get any certs */

                // defines
                AppHeader app_hdr(APPID_CPPM_APPLICATION, CPPM_APPLICATION_DATA);
                time_t now = time(0);
                Timestamp ts(localtime(&now));
                VehicleDescriptor v_desc(ford_config["vin"]);
                InVehicleAuthorizationData iva(resp);
                CPPMRawDataIE rd(alert);

                // build packet
                string pkt_buf = app_hdr.pack(ts.pack() + v_desc.pack() + rd.pack(iva.pack()));

                // send it
                cout << "sending cppm alert" << endl; 
                cout << "   sent: "; 
                dumpBytes((Byte *)pkt_buf.c_str(), pkt_buf.size());
                //send(sock, pkt_buf.c_str(), pkt_buf.size(), 0);
                SSL_write(ssl, pkt_buf.c_str(), pkt_buf.size());

                // get response
                //int rcv_len = recv(sock, rcvbuffer, sizeof(rcvbuffer), 0);
                int rcv_len = SSL_read(ssl, rcvbuffer, sizeof(rcvbuffer));
                cout << "   recv: "; 
                dumpBytes(rcvbuffer, rcv_len);

                SSL_free(ssl);                                   /* release connection state */
            }
            close(sock);
            SSL_CTX_free(ctx);                                   /* release context */
        }
        exit(0);
    }
}


void send_keyoff_trip_info() {
    if (forkit()) {
        SSL_CTX *ctx;
        SSL *ssl;

        ctx = InitCTX();
        Socket sock = open_server();
        if (sock > 0) {
           Byte rcvbuffer[1024];

           ssl = SSL_new(ctx);                                 /* create new SSL connection state */
           SSL_set_fd(ssl, sock);                              /* attach the socket descriptor */
           if ( SSL_connect(ssl) == FAIL )                     /* perform the connection */
               ERR_print_errors_fp(stderr);
           else {
                printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
                ShowCerts(ssl);                                /* get any certs */

                // defines
                AppHeader app_hdr(APPID_CPPM_APPLICATION, CPPM_APPLICATION_DATA);
                time_t now = time(0);
                Timestamp ts(localtime(&now));
                VehicleDescriptor v_desc(ford_config["vin"]);
//                GPSInfoBundle gps;
                CPPMRawDataIE rd(CPPM_TRIP_INFORMATION);
                KeyOffBundle kob(localtime(&now));

                // build packet
//                string pkt_buf = app_hdr.pack(ts.pack() + v_desc.pack() + rd.pack(kob.pack() + gps.pack() + KeyOnBundleSaved));
                string pkt_buf = app_hdr.pack(ts.pack() + v_desc.pack() + rd.pack(kob.pack() + KeyOnBundleSaved));

                // send it
                cout << "sending key off trip info" << endl; 
                cout << "    sent:" << endl;
                dumpBytesOffset((Byte *)pkt_buf.c_str(), pkt_buf.size(), 4);
                //send(sock, pkt_buf.c_str(), pkt_buf.size(), 0);
                SSL_write(ssl, pkt_buf.c_str(), pkt_buf.size());

                // get response
                //int rcv_len = recv(sock, rcvbuffer, sizeof(rcvbuffer), 0);
                int rcv_len = SSL_read(ssl, rcvbuffer, sizeof(rcvbuffer));
                cout << "    recv:" << endl; 
                dumpBytesOffset(rcvbuffer, rcv_len, 4);

                SSL_free(ssl);                                   /* release connection state */
            }
            close(sock);
            SSL_CTX_free(ctx);                                   /* release context */
        }
        exit(0);
    }
}

//void process_request(Socket sock, Byte *b, int len)
void process_request(SSL *ssl, Byte *b, int len)
{
    if ((b[0] & 0x3f) == REMOTE_VEHICLE_FUNCTION) {
        if ((b[1] & 0x1f) == RVF_COMMAND) {
            // bytes 8&9 are the control function
            // 10&11 are function command
            switch (b[9] & 0x7f) {
                case RVF_CONTROLLED_ENTITY_DOOR_LOCKS:
                    if (b[11] == RVF_FUNCTION_COMMAND_REQUEST) { 
                        if (b[14] == RVF_RAWDATA_REQUEST_LOCK_DOORS) { 
                            backtick("/usr/tmp/vicCarControl locks lock");
                        } else if (b[14] == RVF_RAWDATA_REQUEST_UNLOCK_DOORS) {
                            backtick("/usr/tmp/vicCarControl locks unlock");
                        }
                    }
 
                    // send response
                    //
 
                    break;
                case RVF_CONTROLLED_ENTITY_QUERY_REQUEST:
                case RVF_CONTROLLED_ENTITY_REMOTE_START:
                case RVF_CONTROLLED_ENTITY_ALARM_SYSTEM:
                    break;
                case RVF_CONTROLLED_ENTITY_VEHICLE_STATUS:
                    //send_vehicle_status(sock);
                    send_vehicle_status(ssl);
                    break;
                case RVF_CONTROLLED_ENTITY_CPPM_SERVICE:
                    break;
                case RVF_CONTROLLED_ENTITY_ADMINISTRATION:
                    if (b[11] == RVF_FUNCTION_COMMAND_REQUEST) { 
                        if (b[14] == RVF_RAWDATA_REQUEST_INITIAL_AUTHORIZATION) { 
                            //send_initial_auth_in_progress(sock);
                            send_initial_auth_in_progress(ssl);
                            
                            // do something with vic
                            string old_user, new_user;
                            old_user.assign((char *)&b[17], b[16]); 
                            new_user.assign((char *)&b[17 + b[16] +1], b[17+ b[16]]); 
printf("oldUser:%s,newUser:%s\n", old_user.c_str(), new_user.c_str());
                            sendVicAuthorizationRequest(VIC_AUTH_TYPE_INITIAL, old_user, new_user); 
                        }
                        else if (b[14] == RVF_RAWDATA_REQUEST_FINAL_AUTHORIZATION) { 
                            //send_final_auth_in_progress(sock);
                            send_final_auth_in_progress(ssl);

                            // do something with vic
                            string old_user, new_user;
                            old_user.assign((char *)&b[17], b[16]); 
                            new_user.assign((char *)&b[17 + b[16] +1], b[17+ b[16]]); 
printf("oldUser:%s,newUser:%s\n", old_user.c_str(), new_user.c_str());
                            sendVicAuthorizationRequest(VIC_AUTH_TYPE_FINAL, old_user, new_user); 
                        }
                    }
                    break;
                case RVF_CONTROLLED_ENTITY_CURRENT_GPS_POSITION:
                case RVF_CONTROLLED_ENTITY_SCHEDULER_DATA:
                case RVF_CONTROLLED_ENTITY_DRIVE_CONDITIONING_DATA:
                case RVF_CONTROLLED_ENTITY_FAVORITE_LOCATION_DATA:
                case RVF_CONTROLLED_ENTITY_TARGET_SOC:
                case RVF_CONTROLLED_ENTITY_PHEV:
                case RVF_CONTROLLED_ENTITY_CONFIGURATION:
                case RVF_CONTROLLED_ENTITY_PANIC_REQUEST:
                case RVF_CONTROLLED_ENTITY_CHIRP_REQUEST:
                default:
                   break;
            }
        }
    } 
}


bool forkit() {
    bool forked = false;
    int pid = fork();
    if (pid < 0) {
        cerr << "Fork failed" << endl;
    }
    else if (pid == 0) {
        forked = true;
    }
    else {
        children.push_back(pid);
    }
    return forked;
}


void sToBCD(Byte *p, string s) {
    int i;
    string b;

    // make it length of 16
    for (i = 0; i < 16-s.size(); i++) {
        b += '0';
    }
    b += s;
    //printf("s:%s,b:%s\n", s.c_str(), b.c_str());

    // copy bcd to passed buffer
    for (i = 0; i < 8; i++) {
        p[i] = (b[i*2]-'0' << 4) | (b[i*2+1]-'0');
    }
}


int main(int argc, char *argv[])
{
    string pppup;
    signal(SIGCHLD, &on_child);

    touchFile(NoSmsCommandsFile);

    printf("waiting for ppp\n");
    do {
        sleep(5);
        pppup = backtick("ifconfig | grep ppp0");
    } while (pppup == ""); 
    printf("ppp up\n");

    ford_config = readAssoc(FordConfigFile, " ");
    string vin = backtick("/usr/tmp/vicControl -t getvin | sed 's/VIN: //'");

    printf("ford_config:%s, vin:%s\n", ford_config["vin"].c_str(), vin.c_str());
    if ((ford_config["vin"] == "") && (vin == "(no vin given)")) {
        printf("no vin available\n");
        printf("waiting for CAN to become active to get vin\n");

        do {
            sleep(5);
            vin = backtick("/usr/tmp/vicControl -t getvin | sed 's/VIN: //'");
        } while (vin == "(no vin given)"); 
        printf("got vin\n");
    }

    if (vin != "(no vin given)") {
        if (vin != ford_config["vin"]) {
            ford_config["vin"] = vin;
            writeAssoc(FordConfigFile, ford_config, " ");
            cout << "new vin = " << ford_config["vin"] << endl;
            send_provisioning(ford_config["vin"]);
        }
    }

    // save key on bundle
    saveKeyOnBundle();

    while (true) {
        string sms = backtick("cell_shell checksms");
        if (sms.find("Yes") != string::npos) {
            printf("sms\n");
            sms = backtick("cell_shell clearsms");
            get_request();
        }

        if (fileExists("/tmp/smstest")) {
            unlink("/tmp/smstest");
            get_request();
        }
        if (fileExists("/tmp/provtest")) {
            unlink("/tmp/provtest");
            send_provisioning(ford_config["vin"]);
        }
        if (fileExists("/tmp/initialauth")) {
            unlink("/tmp/initialauth");
            send_cppm_alert(CPPM_INITIAL_AUTHORIZATION_ALERT, INVEHICLE_AUTHORIZATION_CONFIRMED);
        }
        if (fileExists("/tmp/finalauth")) {
            unlink("/tmp/finalauth");
            send_cppm_alert(CPPM_FINAL_AUTHORIZATION_ALERT, INVEHICLE_AUTHORIZATION_CONFIRMED);
        }
        if (fileExists("/tmp/initialauthreject")) {
            unlink("/tmp/initialauthreject");
            send_cppm_alert(CPPM_INITIAL_AUTHORIZATION_ALERT, INVEHICLE_AUTHORIZATION_DENY);
        }
        if (fileExists("/tmp/finalauthreject")) {
            unlink("/tmp/finalauthreject");
            send_cppm_alert(CPPM_FINAL_AUTHORIZATION_ALERT, INVEHICLE_AUTHORIZATION_DENY);
        }
        if (fileExists("/tmp/keyoff")) {
            unlink("/tmp/keyoff");
            send_keyoff_trip_info();
        }

        // rcv vic notifications and act on
        vicWait(1);

        sleep(1);
    }
}


////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

void vicWait(int timeout) {
//    monitor_flags |= IN_MONWAIT;
    openVic();
    time_t end_t = time(NULL) + timeout;
    while (timeout > 0) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        if (vic.checkForFrame()) {
            Byte buffer[64];
            int len = vic.getResp(buffer, 0);
            if (len > 0) {
                processVicMessage(len, buffer);
            }
        }
        int vic_socket = vic.getSocket();
        if (vic_socket >= 0) {
            FD_SET(vic_socket, &rd);
            nfds = max(nfds, vic_socket);
        }
        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if ((r == -1) && (errno == EINTR)) {
            timeout = end_t - time(NULL) + 1;
            continue;
        }
        if (r < 0) {
            perror("select()");
            exit(1);
        }
        if (r > 0) {
            if ((vic_socket >= 0) and (FD_ISSET(vic_socket, &rd))) {
                Byte buffer[64];
                int len = vic.getResp(buffer, 0);
                if (len > 0) {
                    processVicMessage(len, buffer);
                }
            }
        }
        if (r == 0) {
            break;
        }
        timeout = end_t - time(NULL) + 1;
    }
//    monitor_flags &= ~IN_MONWAIT;
}

void enableWatchdog() {
    updateFile(LastTimeFile, "Started");
}

void disableWatchdog() {
    unlink(LastTimeFile);
}


void openVic() {
    if (vic.getSocket() < 0) {
        vic.openSocket(sizeof(vic_notifications), vic_notifications);
    }
}

void checkVicVersion() {
    if (vic.build != vic.requested_build) {
        string binfile = backtick("ls /boot/VIC-"+vic.requested_build+"-*.dat");
        if (binfile != "") {
            disableWatchdog();
            logError("Updating VIC build: "+vic.requested_build);
            str_system("vic_loader -r "+binfile);
            enableWatchdog();
            getVicVersion();
        }
    }
}

void getVicVersion() {
    vic.getVersion(app_id);
//    car_state.vic_version = vic.long_version;
//    car_state.vic_version_short = vic.version;
//    car_state.vic_build = vic.build;
//    car_state.vic_requested_build = vic.requested_build;
//    logError("VIC version: "+car_state.vic_build+"-"+car_state.vic_version_short);
    logError("VIC version: "+vic.build+"-"+vic.version_str);
}

int getVicResponse(int reqid, int reqlen, Byte *buffer) {
//    monitor_flags |= IN_VICWAIT;
    int resplen = 0;
    openVic();
    if (vic.getSocket() != -1) {
        if (reqlen == 0) {
            vic.sendRequest(app_id, reqid);
        }
        else {
            vic.sendRequest(app_id, reqid, reqlen, buffer);
        }
        unsigned long end_time = Uptime + 15;
        while (true) {
            int len = vic.getResp(buffer, 1);
            if (len > 0) {
                if (buffer[1] == reqid) {
                    resplen = len;
//                    logError(string_printf("got %d bytes in response to 0x%02X command", len, reqid));
                    break;
                }
                else {
                    processVicMessage(len, buffer);
                }
            }
            else if (len == -1) {
                // socket error
                break;
            }
            else if (len < -1) {
                if (buffer[2] == reqid) {
                   logError("VIC error message");
                   resplen = 0;
                   break;
                }
            }
            Uptime = getUptime();
            if (Uptime > end_time) {
                break;
            }
        }
    }
//    monitor_flags &= ~IN_VICWAIT;
    return resplen;
}

void saveKeyOnBundle() 
{
    time_t now = time(0);
    Timestamp ts(localtime(&now));
//    GPSInfoBundle gps;
    KeyOnBundle kob(localtime(&now));

    // save packet
    //KeyOnBundleSaved = kob.pack() + gps.pack();
    KeyOnBundleSaved = kob.pack();
}

void processVicMessage(int len, Byte *buffer) {
    Byte appid = buffer[0];
    Byte type = buffer[1];
//    logError(string_printf("Got %d bytes for response 0x%02X",len,type));
    if (type == 0x20) {
        handleVicStatus(len, buffer);
    }
    else if (type == 0xFF) {
        // Error returned
    }
    else if (type == 0xF0) {
        // Emergency Notification
        logError("Got emergency notification");
        car_state.ecall_state = "starting";
    }
    else if (type == 0xF1) {
        // Ignition Change Notification
        bool ign = buffer[2];
        logError("Got ignition change notification:"+toString(ign));
//        if (car_state.ignition != ign) {
            car_state.ignition = ign;
//            doIgnitionProcess();
            if (car_state.ignition) {
                // ignition on
                saveKeyOnBundle();
            } else {
                // ignition off
                send_keyoff_trip_info();
            }
//        }
    }
    else if (type == 0xF2) {
        // Low Battery Notification
        logError("Got LowBattery Notification");
    }
    else if (type == 0xF4) {
        logError("Got QuickScan Notification");
        car_state.get_quickscan = true;
        car_state.quickscan_ecus = buffer[2];
        int year = buffer[3] + 2000;
        int mon = buffer[4];
        int day = buffer[5];
        int hour = buffer[6];
        int min = buffer[7];
        int sec = buffer[8];
        car_state.quickscan_idx = 0;
        car_state.quickscan_cnt = 0;
        car_state.quickscan_str = string_printf(" QSTime:%04d/%02d/%02dT%02d:%02d:%02d",
                             year, mon, day, hour, min, sec);
        if (car_state.quickscan_ecus == 0) {
            car_state.get_quickscan = false;
        }
    }
    else if (type == 0xF5) {
        car_state.check_fault_time = Uptime;
    }
    else if (type == 0xF6) {
        // Authorization Notification
        int which = buffer[2];
        int resp = buffer[3];
        logError("Got authorization notification:"+toString(resp));
        // todo: send cppm alerts
        if (which == VIC_AUTH_TYPE_INITIAL) {
            send_cppm_alert(CPPM_INITIAL_AUTHORIZATION_ALERT, (resp == VIC_AUTH_RESPONSE_REJECT) ? INVEHICLE_AUTHORIZATION_DENY : INVEHICLE_AUTHORIZATION_CONFIRMED);
        } else {
            send_cppm_alert(CPPM_FINAL_AUTHORIZATION_ALERT, (resp == VIC_AUTH_RESPONSE_REJECT) ? INVEHICLE_AUTHORIZATION_DENY : INVEHICLE_AUTHORIZATION_CONFIRMED);
        }
    }
}

void decodeVicStatus(int len, Byte *buffer) {
    Byte status = buffer[2];
    car_state.locks = buffer[3];
//printf("car_state.locks:%02x\n", car_state.locks);
    car_state.doors = buffer[4];
    car_state.ign_time = ((int)buffer[5] << 8) | buffer[6];
    //car_state.batval = buffer[7] / 10.;
    car_state.batval = buffer[7];
    car_state.vic_status = buffer[8];
    car_state.ignition = status & 0x01;
//printf("car_state.ignition:%d\n", car_state.ignition);
    car_state.can_active = status & 0x80;
    car_state.engine = status & 0x02;
    car_state.moving = status & 0x04;
    if (!car_state.can_active and fileExists("/tmp/moving")) {
        car_state.moving = (bool)stringToInt(getStringFromFile("/tmp/moving"));
    }
    if (car_state.engine) {
        car_state.engine_was_on = true;
    }
    if ((car_state.vic_status&0x7F) == 0x00) {
//        updateFile(VicStatusFile, "Passed");
    }
    else {
        string vic_status = "";
        if (car_state.vic_status & 0x01) {
            vic_status += ",RtcFail";
        }
        if (car_state.vic_status & 0x02) {
            vic_status += ",CellPowerFail";
        }
        if (car_state.vic_status & 0x04) {
            vic_status += ",VicTestFail";
        }
        if (car_state.vic_status & 0x08) {
            vic_status += ",CanBusFail";
        }
        if (car_state.vic_status & 0x70) {
            vic_status += ",UnknownFailure";
        }
        if (vic_status != "") {
            vic_status.erase(0, 1);
        }
//        updateFile(VicStatusFile, vic_status);
    }
    car_state.vic_reset = (car_state.vic_status & 0x80);
    car_state.alarm_state = 0x07; //SNA
    if (car_state.doors & 0x10) {
        car_state.trunk_opened = true;
    }
    if (len > 9) {
        int more_state = (buffer[9] << 8) | buffer[10];
//        if (car_state.vic_version > "000.027.000") {
        if (vic.protocol_version >= 0x0217) {
            car_state.alarm_state = (more_state & 0xE000) >> 13;
        }
        else {
            if (buffer[9] & 0x80) {
                car_state.alarm_state = 0x04; // Alarm on
            }
            else {
                car_state.alarm_state = 0x08; // Alarm off
            }
        }
        if (more_state & 0x1000) {
            car_state.trunk_opened = true;
//            logError("VIC reported trunk was opened");
        }
        int capabilities = (buffer[11] << 8) | buffer[12];
        int old_cap = car_state.capabilities;
        int new_cap = old_cap | capabilities;
        if (old_cap != new_cap) {
            car_state.capabilities = new_cap;
            car_state.make_capabilities_string();
            cerr << "old_cap:" << old_cap << endl;
            cerr << "new_cap:" << new_cap << endl;
            cerr << "capabilities: " << car_state.capabilities_str << endl;
//            updateFile(CapabilitiesFile, toString(new_cap));
//            backupAutonet();
        }
    }
}

void handleVicStatus(int len, Byte *buffer) {
}
/*
void handleVicStatus(int len, Byte *buffer) {
    bool last_ign = car_state.ignition;
    bool last_can = car_state.can_active;
    bool last_moving = car_state.moving;
    int last_alarm = car_state.alarm_state;
    decodeVicStatus(len, buffer);
    if (car_state.batval < car_state.lowbatval) {
        if (car_state.low_bat_time == 0) {
            car_state.low_bat_time = Uptime;
        }
        else if ((Uptime-car_state.low_bat_time) >= 120) {
            if (!car_state.low_battery) {
                string event = "CarState";
                string batval = string_printf("%.1f", car_state.batval);
                event += " Bat:" + batval;
                eventlog.append(event);
                if (canConnect()) {
                    cellinfo.events_to_send = true;
                    logError("Need to report Low Battery");
                }
                car_state.low_battery = true;
            }
            // shutdown in deep sleep
            //logError("Power down - low bat");
            //powerOff("LowBat", 0x02, 0);
        }
    }
    else if (car_state.low_bat_time != 0) {
        car_state.low_bat_time = 0;
    }
    if (last_ign != car_state.ignition) {
        doIgnitionProcess();
    }
    if (last_can != car_state.can_active) {
        if (car_state.can_active) {
            logError("CAN turned on");
            if (!fileExists(CarControlCommandFile) and (Uptime >= car_state.command_hold)) {
                logError("WokeOnCanFile set - CAN change");
                touchFile(WokeOnCanFile);
            }
        }
        else {
            logError("Can turned off");
            car_state.can_stopped = true;
            logError("WokeOnCanFile removed");
            unlink(WokeOnCanFile);
        }
    }
    if (car_state.engine) {
        car_state.engine_was_on = true;
    }
    if (car_state.remote_started and !car_state.engine) {
        car_state.remote_started = false;
//        unlink(RemoteStartedFile);
    }
    if (car_state.trunk_opened) {
        if (car_state.valet_mode and !car_state.trunk_opened_sent) {
            string event = "ValetAlert";
            event += " TrunkOpened";
            event += eventGpsPosition();
            eventlog.append(event);
            if (canConnect()) {
                cellinfo.events_to_send = true;
                logError("Need to report ValetAlert TrunkOpened");
            }
            car_state.trunk_opened_sent = true;
        }
        car_state.trunk_opened = false;
    }
    if (last_moving != car_state.moving) {
        if (car_state.moving) {
            int fuel = getFuelLevel();
            long int odom_x10 = getOdometerX10();
            if ( (fuel != -1) and (car_state.stopped_fuel_level != -1) and
                 (odom_x10 != -1) ) {
                int fuel_diff = fuel - car_state.stopped_fuel_level;
                if (fuel_diff > 5) {
                    string event = "CarState";
                    event += " Odom:" + string_printf("%.1f", odom_x10/10.);
                    if (car_state.obd2_unit) {
                        event += " Fuel%:" + toString(fuel);
                        event += " AddedFuel%:" + toString(fuel_diff);
                    }
                    else {
                        event += " Fuel:" + toString(fuel);
                        event += " AddedFuel:" + toString(fuel_diff);
                    }
                    eventlog.append(event);
                    logError("AddedFuel");
                    if (canConnect()) {
                        cellinfo.events_to_send = true;
                        logError("Need to report AddedFuel");
                    }
                }
            }
        }
        else {
            // We just stopped
            // Get odometer and fuel level
            car_state.stopped_fuel_level = getFuelLevel();
        }
    }
    if (last_alarm != car_state.alarm_state) {
//        logError("Alarm change: "+toString(last_alarm)+"->"+toString(car_state.alarm_state));
        if ( (car_state.alarm_state >= 0x3) and
             (car_state.alarm_state <= 0x6) ) {
            string event = "NotifyAlarm";
            event += " Alarm:" + toString(car_state.alarm_state);
            event += eventGpsPosition();
            eventlog.append(event);
            if (canConnect()) {
                cellinfo.events_to_send = true;
                logError("Need to report NotifyAlarm");
            }
        }
    }
}
*/

void sendVicAuthorizationRequest(int type, string old_user, string new_user) {
    Byte buffer[64];

    buffer[0] = type;
    buffer[1] = old_user.size();
    strncpy((char *)&buffer[2], old_user.c_str(), old_user.size());
    buffer[2 + old_user.size()] = new_user.size();
    strncpy((char *)&buffer[3 + old_user.size()], new_user.c_str(), new_user.size());
    getVicResponse(0x32, 3 + old_user.size() + new_user.size(), buffer);
}

long int getOdometerX10() {
    Byte buffer[64];
    long int odom = -1;
    int len = getVicResponse(0x28, 0, buffer);
    if (len == 5) {
        odom = (buffer[2]<<16) +
                   (buffer[3]<<8) +
                   buffer[4];
        if (odom == 0xFFFFFF) {
            odom = -1;
        }
    }
    string odomfile = "/tmp/odom";
    if (fileExists(odomfile)) {
        odom = stringToInt(getStringFromFile(odomfile));
    }
    return odom;
}

int getFuelLevel() {
    Byte buffer[64];
    int fuel = -1;
    int len = getVicResponse(0x25, 0, buffer);
    if (len == 3) {
        fuel = buffer[2];
        if (fuel == 0xFF) {
            fuel = -1;
        }
    }
    string fuelfile = "/tmp/fuel";
    if (fileExists(fuelfile)) {
        fuel = stringToInt(getStringFromFile(fuelfile));
    }
    return fuel;
}

/*
void doIgnitionProcess() {
    car_state.ignition_change_time = Uptime;
    if (car_state.ignition) {
        logError("Ignition turned on");
        car_state.ignition_delay = 5;
        car_state.ignition_off_time = 0;
        car_state.ignition_on_time = time(NULL);
        car_state.ignition_was_on = true;
        car_state.engine_was_on = false;
//        if (car_state.vic_version < "000.027.000") {
        if (vic.protocol_version <= 0x0216) {
            car_state.check_fault_time = Uptime + 10;
        }
//        unlink(RemoteStartedFile);
        unlink(IgnitionOffFile);
    }
    else {
        logError("Ignition turned off");
        car_state.ignition_delay = 2;
        car_state.ignition_off_time = time(NULL);
        if ( !car_state.engine_was_on and
             (car_state.ignition_on_time != 0) and
             ((car_state.ignition_off_time - car_state.ignition_on_time) < 90) ) {
            string event = "OnOff";
            eventlog.append(event);
        }
        car_state.ignition_on_time = 0;
//        car_state.check_fault_time = Uptime;
        system("pkill -HUP gps_mon");
        touchFile(IgnitionOffFile);
        car_state.stopped_fuel_level = -1;
    }
}

bool ignition_on() {
    return car_state.ignition and (car_state.ignition_change_time == 0);
}

bool ignition_off() {
    return !car_state.ignition and (car_state.ignition_change_time == 0);
}
*/

void on_child(int sig) {
    int stat = 0;
    int status = 0;
//    logError("Got SIGCHLD");
//    for (int i=0; i < commands.size(); i++) {
    for (int i=children.size()-1; i >= 0; i--) {
//        int pid = commands[i].pid;
        int pid = children[i];
        if (pid > 0) {
            pid_t child = waitpid(pid,&status,WNOHANG);
            if (child == pid) {
                children.erase(children.begin()+i);
//                logError("Removing pid: "+toString(pid)+" left:"+toString((int)children.size()));
//                commands[i].pid = 0;
//                commands[i].exited_time = time(NULL);
            }
        }
    }
//    wait(&stat);
    signal(SIGCHLD, on_child);
}



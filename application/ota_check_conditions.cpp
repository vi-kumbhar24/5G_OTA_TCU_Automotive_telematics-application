/*************************************************************************
 * File: check_conditions.cpp
 *
 * Routine to check for flashing pre-conditions. Input file is a xml file
 *
 * 2015/04/15 rwp
 *    Initial version
 * 2015/06/23 rwp
 *    Added other CAN related conditions (requires VIC version >= 0.45.0)
 * 2015/06/25 rwp
 *    Changed "not" to "nor"
 *    Added temperature (currently uses internal TCU temperature)
 **************************************************************************
 */

#include <iostream>
#include <regex.h>
#include "autonet_files.h"
#include "my_features.h"
#include "fileExists.h"
#include "getopts.h"
#include "getStringFromFile.h"
#include "grepFile.h"
#include "split.h"
#include "str_system.h"
#include "string_printf.h"
#include "ticpp.h"
#include "vic.h"
#define TIXML_USE_TICPP
using namespace std;

bool And(ticpp::Element *element);
bool Or(ticpp::Element *element);
bool Nor(ticpp::Element *element);
bool getResult(ticpp::Element *element);
bool EQ(ticpp::Element *element);
bool NE(ticpp::Element *element);
bool LT(ticpp::Element *element);
bool LE(ticpp::Element *element);
bool GT(ticpp::Element *element);
bool GE(ticpp::Element *element);
int compare(ticpp::Element *element);
string getValue(ticpp::Element *element);
void verify_it(string var, string list);
void getCarState();
void decodeStatus(Byte buffer[]);
void decodeTires(Byte buffer[]);
void decodeOil(Byte buffer[]);
void decodeFuel(Byte buffer[]);
void decodeSeats(Byte buffer[]);
void decodeOdom(Byte buffer[]);
void decodeSpeed(Byte buffer[]);
void decodeFuelUsage(Byte buffer[]);

string default_ecu = "";
bool debug = false;
bool verify = false;
#define SeatsFile "/tmp/seats"
#define DoorsFile "/tmp/doors"
#define MovingFile "/tmp/moving"
#define BatteryFile "/tmp/battery"
#define OtaEcusFile "/user/libdm/ecus.deployed"
bool error = false;

struct CarState {
    bool ignition;
    bool engine;
    bool moving;
    bool crash;
    bool check_engine;
    bool driver_seatbelt;
    bool panic;
    bool can_active;
    bool can_a_active;
    bool can_b_active;
    unsigned int ign_time;
    float batval;
    int locks;
    int doors;
    int vic_status;
    int ignition_status;
    int key_status;
    bool low_fuel;
    int alarm_state;
    int capabilities;
    int lf_tire_pressure;
    int rf_tire_pressure;
    int lr_tire_pressure;
    int rr_tire_pressure;
    int spare_tire_pressure;
    int front_ideal_pressure;
    int rear_ideal_pressure;
    int oil_life;
    int fuel_level;
    int seats;
    int seatbelts;
    long int odom;
    int speed_vic;
    unsigned long fuel_usage_raw_total;
    int fuel_usage_units;
    unsigned int fuel_usage_raw_current;
} carstate;

VIC vic;
int app_id = 30;

int main(int argc, char **argp) {
    GetOpts opts(argc, argp, "dv");
    debug = opts.exists("d");
    verify = opts.exists("v");
    if (argc < 2) {
        cout << "Usage: check_conditions condition_file [default_ecu]" << endl;
        exit(1);
    }
    string cond_file = argp[1];
    if (argc > 2) {
        default_ecu = argp[2];
    }
    getCarState();
    int exitstat = 1;
    bool result = false;
    try {
        ticpp::Document doc(cond_file);
        doc.LoadFile();
        ticpp::Element *child = doc.FirstChildElement("cond");
        result = And(child);
        if (debug) cout << "cond: " << result << endl;
        string cond = "failed";
        if (result and !error) {
            cond = "matched";
            exitstat = 0;
        }
        cout << "Condition: " << cond << endl;
    }
    catch(ticpp::Exception& ex) {
        cout << ex.what();
    }
    return exitstat;
}

bool And(ticpp::Element *element) {
    bool result = true;
    ticpp::Element *child = element->FirstChildElement(false);
    while (child) {
        result &= getResult(child);
        if (!result and !verify) {
            break;
        }
        child = child->NextSiblingElement(false);
    }
    return result;
}

bool Or(ticpp::Element *element) {
    bool result = false;
    ticpp::Element *child = element->FirstChildElement();
    while (child) {
        result |= getResult(child);
        if (result and !verify) {
            break;
        }
        child = child->NextSiblingElement(false);
    }
    return result;
}

bool Nor(ticpp::Element *element) {
    bool result = true;
    ticpp::Element *child = element->FirstChildElement();
    while (child) {
        result &= !getResult(child);
        if (!result and !verify) {
            break;
        }
        child = child->NextSiblingElement(false);
    }
    return result;
}

bool getResult(ticpp::Element *element) {
    bool result = false;
    string strName;
    element->GetValue(&strName);
    if (strName == "true") {
        result = true;
    }
    else if (strName == "false") {
        result = false;
    }
    else if (strName == "and") {
        result = And(element);
    }
    else if (strName == "or") {
        result = Or(element);
    }
    else if (strName == "nor") {
        result = Nor(element);
    }
    else if (strName == "eq") {
        result = EQ(element);
    }
    else if (strName == "ne") {
        result = NE(element);
    }
    else if (strName == "gt") {
        result = GT(element);
    }
    else if (strName == "ge") {
        result = GE(element);
    }
    else if (strName == "lt") {
        result = LT(element);
    }
    else if (strName == "le") {
        result = LE(element);
    }
    else if (strName == "ignitionState") {
        string onoff = element->GetText();
        verify_it(onoff, "on|off");
        string ign = "off";
        //if (fileExists(IgnitionOffFile)) {
        if (carstate.ignition) {
            ign = "on";
        }
        if (ign == onoff) {
            result = true;
        }
    }
    else if (strName == "engineState") {
        string onoff = element->GetText();
        verify_it(onoff, "on|off");
        string eng = "off";
        //string engine_state = getStringFromFile(EngineFile);
        //if (engine_state == "1") {
        if (carstate.engine) {
            eng = "on";
        }
        if (eng == onoff) {
            result = true;
        }
    }
    else if (strName == "vehicle_level") {
        result = true;
    }
    else if (strName == "gear_position") {
        string gear_state = element->GetText();
        verify_it(gear_state, "park|neutral");
        result = true;
    }
    else if (strName == "location") {
        string loc = element->GetText();
        verify_it(loc, "home");
        result = true;
    }
    else if (strName == "doors") {
        string door_state = element->GetText();
        verify_it(door_state, "locked|closed|opened");
        string doors = "opened";
        if (carstate.doors == 0x00) {
            doors = "closed";
            if (carstate.locks == 0x1F) {
                doors = "locked";
            }
        }
        //cout << "doors: " << doors << endl;
        if (doors == door_state) {
            result = true;
        }
    }
    else if (strName == "occupants") {
        string seats_state = element->GetText();
        verify_it(seats_state, "none|driver|any");
        string seats = "none";
        if (carstate.seats != 0x00) {
            seats = "any";
            if (carstate.seats == 0x01) {
                seats = "driver";
            }
        }
        if (seats_state == "any") {
           result = true;
        }
        else if (seats == seats_state) {
            result = true;
        }
    }
    else if (strName == "moving") {
        string t_f = element->GetText();
        verify_it(t_f, "true|false");
        if (t_f == "true") {
           if (carstate.moving) {
               result = true;
           }
        }
        else {
           if (!carstate.moving) {
               result = true;
           }
        }
    }
    else if (strName == "diagnostic_tool_present") {
        string t_f = element->GetText();
        verify_it(t_f, "true|false");
        if (t_f == "false") {
            result = true;
        }
    }
    else if (strName == "active_dtc") {
        string code = element->GetAttributeOrDefault("code", "");
        string except = element->GetAttributeOrDefault("except", "");
        if (fileExists(FaultsFile)) {
            if (code != "") {
                //cout << "checking dtc: " << code << endl;
                if (str_system(string("grep ")+code+" "+FaultsFile+" | grep -q Active") == 0) {
                    //cout << "found it" << endl;
                    result = true;
                }
            }
            else if (except != "") {
                Strings codes = split(except, ",");
                string cmd = string("cat ")+FaultsFile;
                for (int i=0; i < codes.size(); i++) {
                    cmd += " | grep -v "+codes[i];
                }
                cmd += " | grep -q Active";
                //cout << "Faults cmd: " << cmd << endl;
                if (str_system(cmd) == 0) {
                    //cout << "Other codes found" << endl;
                    result = true;
                }
            }
            else {
                if (str_system(string("grep -q Active ")+FaultsFile) == 0) {
                    //cout << "found Active line" << endl;
                    result = true;
                }
            }
        }
        else {
            //cout << "no faults file" << endl;
        }
    }
    else if (strName.substr(0,3) == "ecu") {
        string part = element->GetAttributeOrDefault("part", default_ecu);
        if (part == "") {
            cout << "Error: ECU not specified" << endl;
            exit(1);
        }
        if (strName == "ecuPresent") {
             string line = grepFile(OtaEcusFile, part);
             if (line != "") {
                 result = true;
             }
        }
        else {
            cout << "Unknown operand: " << strName << endl;
            exit(1);
        }
    }
    else if (strName == "active_dtc") {
        string code = element->GetAttributeOrDefault("code", "");
        if (code == "") {
        }
        else {
        }
    }
    else {
        cout << "Unknown operand: " << strName << endl;
        error = true;
    }
    if (debug) cout << "Operation: " << strName << " = " << result << endl;
    return result;
}

bool EQ(ticpp::Element *element) {
    bool result = false;
    if (compare(element) == 0) {
        result = true;
    }
    return result;
}

bool NE(ticpp::Element *element) {
    bool result = false;
    if (compare(element) != 0) {
        result = true;
    }
    return result;
}

bool LT(ticpp::Element *element) {
    bool result = false;
    if (compare(element) < 0) {
        result = true;
    }
    return result;
}

bool LE(ticpp::Element *element) {
    bool result = false;
    if (compare(element) <= 0) {
        result = true;
    }
    return result;
}

bool GT(ticpp::Element *element) {
    bool result = false;
    if (compare(element) > 0) {
        result = true;
    }
    return result;
}

bool GE(ticpp::Element *element) {
    bool result = false;
    if (compare(element) >= 0) {
        result = true;
    }
    return result;
}

int compare(ticpp::Element *element) {
    int result = 0;
    string type = element->GetAttributeOrDefault("type", "");
    ticpp::Element *first = element->FirstChildElement();
    ticpp::Element *second = first->NextSiblingElement();
    string first_val = getValue(first);
    string second_val = getValue(second);
    if ((type == "") or (type == "number")) {
        if ( (first_val.find(".") != string::npos) or
             (first_val.find(".") != string::npos) ) {
            float n1, n2;
            sscanf(first_val.c_str(), "%f", &n1);
            sscanf(second_val.c_str(), "%f", &n2);
            result = 0;
            if (n1 > n2) {
                result = 1;
            }
            else if (n1 < n2) {
                result = -1;
            }
        }
        else {
            int n1, n2;
            sscanf(first_val.c_str(), "%d", &n1);
            sscanf(second_val.c_str(), "%d", &n2);
            result = 0;
            if (n1 > n2) {
                result = 1;
            }
            else if (n1 < n2) {
                result = -1;
            }
        }
    }
    else if (type == "text") {
        result = strcmp(first_val.c_str(), second_val.c_str());
    }
    else if (type == "dotted") {
        Strings v1s = split(first_val, ".");
        Strings v2s = split(second_val, ".");
        int l1 = v1s.size();
        int l2 = v2s.size();
        int len = l1;
        if (l2 < len) {
            len = l2;
        }
        int d = 0;
        for (int i=0; i < len; i++) {
            int n1, n2;
            sscanf(v1s[i].c_str(), "%d", &n1);
            sscanf(v2s[i].c_str(), "%d", &n2);
            d = n1 - n2;
            if (d != 0) {
                break;
            }
        }
        if (d == 0) {
            d = l1 -l2;
        }
        if (d < 0) {
            result = -1;
        }
        else if (d > 0) {
            result = 1;
        }
    }
    else if (type == "regex") {
        string opcode;
        element->GetValue(&opcode);
        if ((opcode != "eq") and (opcode != "ne")) {
           cout << "Error: regex only is on 'ex' or 'ne' operations" << endl;
           exit(1);
        }
        result = 1;
        regex_t rgx;
        regcomp(&rgx, second_val.c_str(), 0);
        if (regexec(&rgx, first_val.c_str(), 0, NULL, 0) == 0) {
           result = 0;
        }
        regfree(&rgx);
    }
    else {
        cout << "Invalid comparison type: " << type << endl;
        error = true;
    }
    return result;
}

string getValue(ticpp::Element *element) {
    string value = "";
    string name;
    element->GetValue(&name);
    if (name == "value") {
        value = element->GetText();
    }
    else if (name == "batteryLevel") {
        if (fileExists(BatteryFile)) {
            value = getStringFromFile(BatteryFile);
        }
        else {
            value = string_printf("%.1f", carstate.batval);
        }
        //cout << "batval: " << value << endl;
    }
    else if (name == "temperature") {
        string units = element->GetAttributeOrDefault("units", "f");
        string location = element->GetAttributeOrDefault("location", "ext");
        verify_it(units, "f|c");
        if (location == "tcu") {
            string flags = "";
            if (units == "c") {
                flags = "-c";
            }
            value = backtick("getTemp "+flags);
        }
        else if (location == "ext") {
            if (units == "c") {
                value = "20.0";
            }
            else {
                value = "68.0";
            }
        }
        else {
            cout << "Unknown temperature location: " << location << endl;
            value = "0";
        }
    }
    else if (name.substr(0,3) == "ecu") {
        string part = element->GetAttributeOrDefault("part", default_ecu);
        string line = grepFile(OtaEcusFile, part);
        Strings fields;
        if (line != "") {
            fields = split(line, ",");
            int fld = 0;
            if (name == "ecuSwLevel") {
                fld = 5;
            }
            else if (name == "ecuHwLevel") {
                fld = 6;
            }
            else if (name == "ecuOrigin") {
                fld = 7;
            }
            else if (name == "ecuSupplierId") {
                fld = 8;
            }
            else if (name == "ecuVarientId") {
                fld = 9;
            }
            else if (name == "ecuDiagnosticVersion") {
                fld = 10;
            }
            else {
                cout << "Unknown operand: " << name << endl;
                exit(1);
            }
            if (fields.size() > fld) {
                value = fields[fld];
            }
        }
    }
    else {
        cout << "Unknown operand: " << name << endl;
        exit(1);
    }
    if (debug) cout << "Getval: " << name << " = " << value << endl;
    return value;
}

void verify_it(string var, string list) {
    Strings poss = split(list, "|");
    bool match = false;
    for (int i=0; i < poss.size(); i++) {
        if (var == poss[i]) {
            match = true;
            break;
        }
    }
    if (!match) {
        cout << "Invalid value: " << var << " not in list(" << list << ")" << endl;
        error = true;
    }
}

void getCarState() {
    Byte buffer[256];
    vic.openSocket();
    buffer[0] = 1;
    buffer[1] = (Byte)rand();
    vic.sendRequest(app_id, 0x35, 2, buffer);
    int len = vic.getResp(buffer, 5);
    if (len > 0) {
       int idx = 2;
       decodeStatus(buffer+idx);
       idx += 11;
       decodeTires(buffer+idx);
       idx += 7;
       decodeOil(buffer+idx);
       idx += 1;
       decodeFuel(buffer+idx);
       idx += 1;
       decodeSeats(buffer+idx);
       idx += 2;
       decodeFuelUsage(buffer+idx);
       idx += 14;
    }
    if (!carstate.can_active) {
        if (fileExists(SeatsFile)) {
            string seats_str = getStringFromFile(SeatsFile);
            int seats = 0;
            sscanf(seats_str.c_str(), "%x", &seats);
            carstate.seats = seats & 0x0F;
            carstate.seatbelts = (seats>>4) & 0x0F;
        }
        if (fileExists(DoorsFile)) {
            string doors_str = getStringFromFile(DoorsFile);
            sscanf(doors_str.c_str(), "%d/%d", &carstate.doors, &carstate.locks);
        }
        if (fileExists(MovingFile)) {
            carstate.moving = (bool)stringToInt(getStringFromFile("/tmp/moving"));
        }
        if (fileExists(EngineFile)) {
            carstate.engine = (bool)stringToInt(getStringFromFile(EngineFile));
        }
    }
}

void decodeStatus(Byte buffer[]) {
    Byte status = buffer[0];
    carstate.ignition = status & (1 << 0);
    carstate.engine = status & (1 << 1);
    carstate.moving = status & (1 << 2);
    carstate.crash = status & (1 << 3);
    carstate.check_engine = status & (1 << 4);
    carstate.driver_seatbelt = status & (1 << 5);
    carstate.panic = status & (1 << 6);
    carstate.can_active = status & (1 << 7);
    carstate.locks = buffer[1];
    carstate.doors = buffer[2];
    carstate.ign_time = ((int)buffer[3] << 8) | buffer[4];
    Byte batvalx10 = buffer[5];
    Features features;
    string pwrcntl = features.getFeature(PwrCntrl);
    Strings pwrvals = split(pwrcntl, "/");
    float batadj = 0.3;
    if (pwrvals.size() > 3) {
        batadj = stringToDouble(pwrvals[3]);
    }
    carstate.batval = batvalx10 / 10. + batadj;
    //cout << "batval: " << batvalx10 << " " << carstate.batval << endl;
    int vic_status_code = buffer[6];
    carstate.vic_status = vic_status_code;
    carstate.can_a_active = vic_status_code & 0x20;
    carstate.can_b_active = vic_status_code & 0x40;
    int more_state = (buffer[7] << 8) | buffer[8];
    carstate.ignition_status = (more_state & 0x0E00) >> 9;
    carstate.key_status = (more_state & 0x00C0) >> 6;
    carstate.low_fuel = more_state & 0x100;
    carstate.alarm_state = (more_state>>13)&0x07;
    carstate.capabilities = (buffer[9] << 8) | buffer[10];
}

void decodeTires(Byte buffer[]) {
    carstate.front_ideal_pressure = buffer[0];
    carstate.rear_ideal_pressure = buffer[0];
    carstate.lf_tire_pressure = buffer[2];
    carstate.rf_tire_pressure = buffer[3];
    carstate.lr_tire_pressure = buffer[4];
    carstate.rr_tire_pressure = buffer[5];
    carstate.spare_tire_pressure = buffer[6];
}

void decodeOil(Byte buffer[]) {
    carstate.oil_life = buffer[0];
}

void decodeFuel(Byte buffer[]) {
    carstate.fuel_level = buffer[0];
}

void decodeSeats(Byte buffer[]) {
    carstate.seats = buffer[0];
    carstate.seatbelts = buffer[1];
}

void decodeOdom(Byte buffer[]) {
    carstate.odom = ((long int)buffer[0]<<16) +
                    ((long int)buffer[1]<<8) +
                    buffer[2];
}

void decodeSpeed(Byte buffer[]) {
    carstate.speed_vic = ((int)buffer[0]<<8) | buffer[1];
}

void decodeFuelUsage(Byte buffer[]) {
    decodeOdom(buffer+9);
    decodeSpeed(buffer+12);
    unsigned long raw_total = 0;
    for (int i=0; i < 6; i++) {
        raw_total = (raw_total << 8) + buffer[i+0];
    }
    carstate.fuel_usage_raw_total = raw_total; 
    carstate.fuel_usage_units = buffer[6];
    carstate.fuel_usage_raw_current = (buffer[7] << 8) + buffer[8];
}

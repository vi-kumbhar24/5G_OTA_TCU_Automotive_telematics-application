#ifndef FEATURES_H
#define FEATURES_H

/**********************************************************************************
 * File: features.h
 * Class for handling features
 **********************************************************************************
 * 2015/02/20 rwp
 *    Added reading vehicle brand feature files
 * 2015/04/01 rwp
 *    Added Provisioned feature
 * 2015/07/02 rwp
 *    Added Ota feature
 * 2018/09/04 rwp
 *    Moved local config features to ConfigManager
 **********************************************************************************
 */

#include <map>
#include <string.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "backtick.h"
#include "fileExists.h"
#include "fileStats.h"
#include "readAssoc.h"
#include "string_convert.h"
using namespace std;

#define BootDelay "BootDelay"
#define PwrCntrl "PwrCntrl"
#define PwrSched "PwrSched"
#define VehicleBrand "VehicleBrand"
#define CarKeyFeature "carkey"
#define CarHealthFeature "carhealth"
#define FindMyCarFeature "findmycar"
#define FindMySpeedFeature "findmyspeed"
#define FleetFeature "fleet"
#define GeoFenceFeature "geofence"
#define RemoteFeature "remote"
#define WifiFeature "wifi"
#define DisableLocationFeature "disablelocation"
#define NoCan "NoCan"
#define ValetFeature "ValetMode"
#define SeatbeltsFeature "seatbelts"
#define SmsBlockingFeature "smsblocking"
#define GeoFences "GeoFences"
#define SpeedLimits "SpeedLimits"
#define RegFeature "Reg"
#define RateLimits "RateLimits"

class Features {
  private:
    Assoc features;
    Assoc gotten_features;
    time_t features_file_time;
    time_t local_features_file_time;
    map<string, bool> enables;
    map<string,unsigned long> enable_untils;

    void readFeatures() {
        if (fileExists(FeaturesFile)) {
            features_file_time = fileTime(FeaturesFile);
            features = readAssoc(FeaturesFile, "="); 
        }
        else {
            features_file_time = 0;
        }
        string featuresfile = backtick("getbranding features");
        if (featuresfile != "") {
            featuresfile = "/boot/features/"+featuresfile;
            if (fileExists(featuresfile)) {
                Assoc brand_features = readAssoc(featuresfile, "=");
                Assoc::iterator it;
                for (it=brand_features.begin(); it != brand_features.end(); it++) {
                    string var = (*it).first;
                    string val = (*it).second;
                    if (features.find(var) == features.end()) {
                        features[var] = val;
                    }
                }
            }
        }
        if (fileExists(LocalFeaturesFile)) {
            local_features_file_time = fileTime(LocalFeaturesFile);
            Assoc local_features = readAssoc(LocalFeaturesFile, "=");
            Assoc::iterator it;
            for (it=local_features.begin(); it != local_features.end(); it++) {
                string var = (*it).first;
                string val = (*it).second;
                if (var.substr(0,1) == "+") {
                    var.erase(0,1);
                    features[var] = val;
                }
                else if (features.find(var) == features.end()) {
                    features[var] = val;
                }
            }
        }
        else {
            local_features_file_time = 0;
        }
    }

    bool _exists(string var) {
        return (features.find(var) != features.end());
    }

  public:
    Features() {
        features_file_time = 0;
        local_features_file_time = 0;
        readFeatures();
    }

    bool exists(string var) {
        bool val = (features.find(var) != features.end());
        if (val) {
            gotten_features[var] = features[var];
        }
        else {
            gotten_features[var] = "";
        }
        return val;
    }

    string getFeature(string var) {
        string val = "";
        if (_exists(var)) {
            val = features[var];
        }
        gotten_features[var] = val;
        return val;
    }

    string getData(string var) {
        string val = "";
        string retval = "";
        if (_exists(var)) {
            val = features[var];
            if (strncmp(val.c_str(),"enabled", 7) == 0) {
                int pos = val.find(",");
                if (pos != string::npos) {
                    retval = val.substr(pos+1);
                }
            }
            else if (strncmp(val.c_str(),"enabuntil:", 10) == 0) {
                int pos = val.find(",");
                if (pos != string::npos) {
                    retval = val.substr(pos+1);
                }
            }
            else {
                retval = val;
            }
        }
        gotten_features[var] = val;
        return retval;
    }

    bool checkFeatures() {
        bool changed = false;
        if ( (fileTime(LocalFeaturesFile) != local_features_file_time) or
             (fileTime(FeaturesFile) != features_file_time) ) {
            //cerr << "Reading features" << endl;
            readFeatures();
            Assoc::iterator it;
            for (it=gotten_features.begin(); it != gotten_features.end(); it++) {
                string var = (*it).first;
                string oldval = (*it).second;
                string currval = "";
                if (_exists(var)) {
                    currval = features[var];
                }
                //cerr << "Checking " << var << " " << oldval << " -> " << currval << endl;
                if (currval != oldval) {
                    changed = true;
                    break;
                }
            }
        }
        if (!changed) {
            Assoc::iterator it;
            for (it=gotten_features.begin(); it != gotten_features.end(); it++) {
                string var = (*it).first;
                if (enable_untils.find(var) != enable_untils.end()) {
                    bool curr_enable = true;
                    if (time(NULL) > enable_untils[var]) {
                        curr_enable = false;
                    }
                    if (enables[var] != curr_enable) {
                        changed = true;
                        break;
                    }
                }
            }
        }
        return changed;
    }

    bool featureChanged(string var) {
        bool changed = false;
        if (gotten_features.find(var) == gotten_features.end()) {
            changed = true;
        }
        else {
            string oldval = gotten_features[var];
            string currval = "";
            if (_exists(var)) {
                currval = features[var];
            }
            //cerr << "Checking: " << var << " " << oldval << " = " << currval << endl;
            if (currval != oldval) {
                changed = true;
            }
            else if (enable_untils.find(var) != enable_untils.end()) {
                bool curr_enable = true;
                if (time(NULL) > enable_untils[var]) {
                    curr_enable = false;
                }
                if (enables[var] != curr_enable) {
                    changed = true;
                }
            }
        }
        //cerr << "Feature changed: " << var << "? " << changed << endl;
        return changed;
    }

    bool isEnabled(string var) {
        bool retval = false;
        string enab_str = getFeature(var);
        if (enab_str != "") {
            retval = true;
            if (strncmp(enab_str.c_str(), "enabuntil:", 10) == 0) {
                enab_str.erase(0, 10);
                size_t pos = enab_str.find(",");
                if (pos != string::npos) {
                    enab_str.erase(pos);
                }
                time_t until = stringToULong(enab_str);
                enable_untils[var] = until;
                if (time(NULL) > until) {
                    retval = false;
                }
            }
        }
        enables[var] = retval;
        return retval;
    }
};

#endif

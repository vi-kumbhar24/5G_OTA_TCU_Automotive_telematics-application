#ifndef READ_FILE_ARRAY_H
#define READ_FILE_ARRAY_H

#include <algorithm>
#include <cstring>
#include <string>
#include <stdlib.h>
#include "autonet_types.h"
#include "readFileArray.h"

Assoc readBuildVars(string file) {
    string build = "";
    Strings build_array;
    Assoc config;
    Strings bldvars = readFileArray(file);
    bool settings = false;
    for (int i=0; i < bldvars.size(); i++) {
        string bldvar = bldvars[i];
        size_t pos = bldvar.find(':');
        if (pos == string::npos) {
            continue;
        }
        string var = bldvar.substr(0, pos);
        string val = bldvar.substr(pos+1);
        std::transform (var.begin(), var.end(), var.begin(), ::tolower);
        while ((pos=var.find(' ')) != string::npos) {
            var.erase(pos, 1);
        }
        while ((var.size() > 0) and (val[0] == ' ')) {
            val.erase(0, 1);
        }
        if (var == "name") {
            string base_val = val;
            if ((pos=base_val.find('-')) != string::npos) {
                base_val = base_val.substr(0,pos);
            }
            config["BaseName"] = base_val;
        }
        if (var == "settings") {
            settings = true;
            continue;
        }
        if (val.size() == 0) {
            continue;
        }
        config[var] = val;
        if (settings) {
            if (val == "yes") {
                if (build.size() != 0) {
                    build += "-";
                }
                build += var;
                build_array.push_back(var);
            }
        }
    }
    if (build_array.size() != 0) {
        sort(build_array.begin(), build_array.end());
        string sorted_build = "";
        for (int i=0; i < build_array.size(); i++) {
            if (sorted_build.size() != 0) {
                sorted_build += "-";
            }
            sorted_build += build_array[i];
        }
        config["SortedBuild"] = sorted_build;
    }
    config["Build"] = build;
    string distro = config["BaseName"];
    if (config["SortedBuild"].size() > 0) {
        distro += "-" + config["SortedBuild"];
    }
    config["Distro"] = distro;
    return config;
}

#endif

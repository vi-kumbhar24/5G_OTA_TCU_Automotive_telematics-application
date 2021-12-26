#include <algorithm>
#include <iostream>
#include <vector>
#include <string.h>
#include <time.h>
#include "autonet_files.h"
#include "autonet_types.h"
#include "csv.h"
#include "fileExists.h"
#include "getopts.h"
#include "getStringFromFile.h"
#include "split.h"
#include "string_convert.h"
#include "readFileArray.h"
using namespace std;

void add_db_schedules();
int find_next();
int check_inside();

Strings schedules;
string translate_days = "smtwhfa";
bool debug = false;
bool checkInstall = false;
bool checkDownload = false;
bool no_schedule = false;

int main(int argc, char *argv[]) {
    int exitstat = 1;
    GetOpts opts(argc, argv, "dDiIn");
    debug = opts.exists("d");
    checkDownload = opts.exists("D");
    checkInstall = opts.exists("I");
    bool get_next = opts.exists("n");
    bool check_in = opts.exists("i");
    for (int i=1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "TODO") {
            no_schedule = true;
            continue;
        }
        Strings ents = split(arg, "/");
        for (int j=0; j < ents.size(); j++) {
            schedules.push_back(ents[j]);
        }
    }
    if (check_in or (!get_next and (argc > 1))) {
        if (check_in) {
            add_db_schedules();
        }
        exitstat = check_inside();
    }
    else {
        add_db_schedules();
        exitstat = find_next();
    }
    return exitstat;
}

void add_db_schedules() {
    if (!checkInstall and fileExists(DownloadedEcusFile)) {
        Strings download_db = readFileArray(DownloadedEcusFile);
        for (int i=0; i < download_db.size(); i++) {
            Strings db_ents = csvSplit(download_db[i], ",");
            string schedule = db_ents[5];
            if (schedule == "TODO") {
                no_schedule = true;
            }
            else {
                Strings ents = split(schedule, "/");
                for (int j=0; j < ents.size(); j++) {
                    schedules.push_back(ents[j]);
                }
            }
        }
    }
    if (!checkDownload and fileExists(DeliveredEcusExtFile)) {
        Strings delivered_db = readFileArray(DeliveredEcusExtFile);
        for (int i=0; i < delivered_db.size(); i++) {
            Strings db_ents = csvSplit(delivered_db[i], ",");
            string schedule = db_ents[5];
            if (schedule == "TODO") {
                no_schedule = true;
            }
            else {
                Strings ents = split(schedule, "/");
                for (int j=0; j < ents.size(); j++) {
                    schedules.push_back(ents[j]);
                }
            }
        }
    }
}

int find_next() {
    vector <int> days;
    time_t first_schedule_time = 0;
    string weekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int i=0; i < schedules.size(); i++) {
        string schedule = schedules[i];
        int pos = schedule.find(":");
        if (pos == string::npos) {
            continue;
        }
        string days_str = schedule.substr(0, pos);
        string times_str = schedule.substr(pos+1);
        transform(days_str.begin(), days_str.end(), days_str.begin(), ::tolower);
        vector<int> days;
        for (int j=0; j < days_str.size(); j++) {
            char ch = days_str.at(j);
            int v = translate_days.find(ch);
            if (v != string::npos) {
                days.push_back(v);
            }
        }
        int start_hr, start_min, end_time;
        sscanf(times_str.c_str(), "%2d%2d-%d",
                &start_hr, &start_min, &end_time);
        int start_time = start_hr*100 + start_min;
        int tzmin = 0;
        if (fileExists(TimezoneOffsetFile)) {
            tzmin = stringToInt(getStringFromFile(TimezoneOffsetFile));
        }
        time_t now = time(NULL) + tzmin*60;
        struct tm *tm;
        tm = gmtime(&now);
        int day = tm->tm_wday;
        int hhmm = tm->tm_hour*100 + tm->tm_min;
        int d_idx = 0;
        for (d_idx=0; d_idx < days.size(); d_idx++) {
            if (days[d_idx] >= day) {
                break;
            }
        }
        if (d_idx >= days.size()) {
            d_idx = 0;
        }
        bool sameday = false;
        if (days[d_idx] == day) {
            sameday = true;
            if (hhmm >= start_time) {
                sameday = false;
                d_idx++;
                if (d_idx >= days.size()) {
                    d_idx = 0;
                }
            }
        }
        int days_to_add = 0;
        if (!sameday) {
            days_to_add = days[d_idx] - day;
            if (days_to_add <= 0) {
                days_to_add += 7;
            }
        }
        tm->tm_hour = 0;
        tm->tm_min = 0;
        tm->tm_sec = 0;
        time_t next_time = timegm(tm) + days_to_add*86400 + start_hr*3600 + start_min*60;
        if (debug) {
            tm = gmtime(&next_time);
            printf("Next wakeup: %s, %d/%d/%d %02d:%02d\n",weekdays[tm->tm_wday].c_str(),tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
        }
        next_time -= tzmin*60;
        if ((first_schedule_time == 0) or (next_time < first_schedule_time)) {
            first_schedule_time = next_time;
        }
    }
    int exitstat = 1;
    if (first_schedule_time > 0) {
        exitstat = 0;
        if (debug) {
            struct tm *tm;
            tm = gmtime(&first_schedule_time);
            printf("Wakeup at: %s, %d/%d/%d %02d:%02d GMT\n",weekdays[tm->tm_wday].c_str(),tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min);
            time_t sleep_time = (first_schedule_time - time(NULL))/60 + 1;
            cout << "Wakeup in " << sleep_time << " minutes" << endl;
        }
    }
    cout << first_schedule_time << endl;
    return exitstat;
}

int check_inside() {
    //bool in_schedule = (schedules.size() == 0);
    bool in_schedule = no_schedule;
    for (int i=0; i < schedules.size(); i++) {
        string schedule = schedules[i];
        int pos = schedule.find(":");
        if (pos == string::npos) {
            continue;
        }
        string days_str = schedule.substr(0, pos);
        string times_str = schedule.substr(pos+1);
        transform(days_str.begin(), days_str.end(), days_str.begin(), ::tolower);
        int start_time, end_time;
        sscanf(times_str.c_str(), "%d-%d", &start_time, &end_time);
        int tzmin = 0;
        if (fileExists(TimezoneOffsetFile)) {
            tzmin = stringToInt(getStringFromFile(TimezoneOffsetFile));
        }
        time_t now = time(NULL) + tzmin*60;
        struct tm *tm;
        tm = gmtime(&now);
        int day = tm->tm_wday;
        int hhmm = tm->tm_hour*100 + tm->tm_min;
        if (end_time > start_time) {
            //cout << "checking in range" << endl;
            if ((hhmm >= start_time) and (hhmm < end_time)) {
                //cout << "in_time" << endl;
                char day_ltr = translate_days.at(day);
                if (days_str.find(day_ltr) != string::npos) {
                    //cout << "matched day:" << day_ltr << endl;
                    if (debug) {
                        cout << "In between: " << day_ltr << ":" << times_str << " : " << schedule << endl;
                    }
                    in_schedule = true;
                    break;
                }
            }
        }
        else {
            //cout << "checking split range" << endl;
            if (hhmm >= start_time) {
                //cout << "after start" << endl;
                char day_ltr = translate_days.at(day);
                if (days_str.find(day_ltr) != string::npos) {
                    //cout << "matched day:" << day_ltr << endl;
                    if (debug) {
                        cout << "After: " << day_ltr << ":" << start_time << " : " << schedule << endl;
                    }
                    in_schedule = true;
                    break;
                }
            }
            else if (hhmm < end_time) {
                //cout << "before end" << endl;
                char day_ltr = translate_days.at((day+6)%7);
                if (days_str.find(day_ltr) != string::npos) {
                    //cout << "matched day:" << day_ltr << endl;
                    if (debug) {
                        cout << "Before: " << day_ltr << ":" << end_time << " : " << schedule << endl;
                    }
                    in_schedule = true;
                    break;
                }
            }
        }
    }
    int exitstat = 1;
    if (in_schedule) {
        cout << "In schedule" << endl;
        exitstat = 0;
    }
    else {
        cout << "Not in schedule" << endl;
    }
    return exitstat;
}

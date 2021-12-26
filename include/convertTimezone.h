#ifndef CONVERTTIMEZONE_H
#define CONVERTTIMEZONE_H

#include <iostream>
#include <string>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "tzfile.h"
#include "ttinfo.h"
#include "autonet_files.h"
using namespace std;

const unsigned short int daysInYear[2][13] = {
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

class Timezone {
  private:

    struct Head {
        char version;
        size_t num_transitions;
        size_t num_types;
        size_t chars;
        size_t num_leaps;
        size_t num_isstd;
        size_t num_isgmt;
    };

    struct ttinfo {
        long int offset;            /* Seconds east of GMT.  */
        unsigned char isdst;        /* Used to set tm_isdst.  */
        unsigned char idx;          /* Index into `zone_names'.  */
        unsigned char isstd;        /* Transition times are in standard time.  */
        unsigned char isgmt;        /* Transition times are in GMT.  */
    };

    struct leap {
        time_t transition;          /* Time the transition takes effect.  */
        long int change;            /* Seconds of correction to apply.  */
    };

    struct tz_rule {
        string name;
        /* When to change.  */
        enum { J0, J1, M } type;    /* Interpretation of:  */
        unsigned short int m, n, d; /* Month, week, day.  */
        unsigned int secs;          /* Time of day.  */
        long int offset;            /* Seconds east of GMT (west if < 0).  */
        /* We cache the computed time of change for a
           given year so we don't have to recompute it.  */
        time_t change;      /* When to change to this zone.  */
        int computed_for;   /* Year above is computed for.  */
    };


    size_t _num_transitions;
    size_t _num_types;
    size_t _chars;
    size_t _num_leaps;
    size_t _num_isstd;
    size_t _num_isgmt;
    int _tzspec_size;
    time_t *_transitions;
    unsigned char *_type_idxs;
    struct ttinfo *_types;
    struct leap *_leaps;
    char *_zone_names;
    char *_tzspec;
    bool _use_gmt;
    bool _is_valid;
    string _tzname[2];
    unsigned long  _tz_offset;
    long int _tz_spec_offset;
    struct tz_rule _tz_rules[2];
    bool _daylight;
    unsigned long _rule_stdoff;
    unsigned long _rule_dstoff;

    unsigned long _decode32(char *p) {
        unsigned long val = 0;
        if (*p & 0x80) {
           for (int i=0; i < 4; i++) {
               val = (val << 8) | 0xFF;
           }
        }
        for (int i=0; i < 4; i++) {
            val = (val << 8) | (unsigned char)(*p++);
        }
        return val;
    }

    unsigned long _decode64(char *p) {
        unsigned long val = 0;
        for (int i=0; i < 8; i++) {
            val = (val << 8) | (unsigned char)(*p++);
        }
        return val;
    }

    bool _readHeader(FILE *fp, struct Head *head) {
        struct tzhead tzhead;
        bool error = false;
        int cnt = fread(&tzhead, sizeof(tzhead), 1, fp);
        if (cnt != 1) {
//            cerr << "File read error" << endl;
            error = true;
        }
        if (!error) {
            if (memcmp(tzhead.tzh_magic, TZ_MAGIC, sizeof(tzhead.tzh_magic)) != 0) {
//                cerr << "Invalid magic number" << endl;
                error = true;
            }
        }
        if (!error) {
            head->version = tzhead.tzh_version[0];
            head->num_transitions = (size_t)_decode32(tzhead.tzh_timecnt);
            head->num_types = (size_t)_decode32(tzhead.tzh_typecnt);
            head->chars = (size_t)_decode32(tzhead.tzh_charcnt);
            head->num_leaps = (size_t)_decode32(tzhead.tzh_leapcnt);
            head->num_isstd = (size_t)_decode32(tzhead.tzh_ttisstdcnt);
            head->num_isgmt = (size_t)_decode32(tzhead.tzh_ttisgmtcnt);
        }
        return error;
    }

    bool _readBody(FILE *fp, int trans_width) {
        bool error = false;
        if ( (_num_isstd > _num_types) or
             (_num_isgmt > _num_types) ) {
//            cerr << "num_isstd or num_isgmt wrong" << endl;
            error = true;
        }
        if (!error) {
            _transitions = (time_t *)malloc(_num_transitions * 8);
            _type_idxs = (unsigned char *)malloc(_num_transitions);
            _types = (struct ttinfo *)malloc(_num_types * sizeof(struct ttinfo));
            _zone_names = (char *)malloc(_chars);
            _leaps = (struct leap *)malloc(_num_leaps * sizeof(struct leap));
            if ( (_transitions == NULL) or
                 (_type_idxs == NULL) or
                 (_types == NULL) or
                 (_zone_names == NULL) or
                 (_leaps == NULL) ) {
//                cerr << "Failed to allocate memory" << endl;
                error = true;
            }
        }
        if (!error) {
            int cnt = fread(_transitions, trans_width, _num_transitions, fp);
            if (cnt != _num_transitions) {
//                cerr << "File read error" << endl;
                error = true;
            }
            else {
                if (trans_width == 4) {
                    int i = _num_transitions;
                    while (i-- > 0) {
                        _transitions[i] = (time_t)_decode32((char *)_transitions + i*4);
                    }
                }
                else {
                    for (int i=0; i < _num_transitions; i++) {
                        _transitions[i] = (time_t)_decode64((char *)_transitions + i*8);
                    }
                }
            }
        }
        if (!error) {
            int cnt = fread(_type_idxs, sizeof(char), _num_transitions, fp);
            if (cnt != _num_transitions) {
//                cerr << "File read error" << endl;
                error = true;
            }
            else {
                for (int i=0; i < _num_transitions; i++) {
                    if (_type_idxs[i] > _num_types) {
//                        cerr << "Bad type index" << endl;
                        error = true;
                        break;
                    }
                }
            }
        }
        if (!error) {
            struct {
                char offset[4];
                unsigned char isdst;
                unsigned char idx;
            } ttinfo1;
            for (int i=0; i < _num_types; i++) {
                int cnt = fread(&ttinfo1, sizeof(ttinfo1), 1, fp);
                if (cnt != 1) {
//                    cerr << "File read error" << endl;
                    error = true;
                    break;
                }
                _types[i].isdst = ttinfo1.isdst != 0;
                _types[i].offset = (long int)_decode32(ttinfo1.offset);
                if (ttinfo1.idx > _chars) {
//                    cerr << "Bad name index" << endl;
                    error = true;
                    break;
                }
//                cerr << "Type:" << i << "idx:" << (int)ttinfo1.idx << endl;
                _types[i].idx = ttinfo1.idx;
            }
        }
        if (!error) {
            int cnt = fread(_zone_names, sizeof(char), _chars, fp);
            if (cnt != _chars) {
//                cerr << "File read error" << endl;
                error = true;
            }
        }
        if (!error) {
            char val[8];
            for (int i=0; i < _num_leaps; i++) {
                int cnt = fread(val, trans_width, 1, fp);
                if (cnt != 1) {
//                    cerr << "File read error" << endl;
                    error = true;
                    break;
                }
                if (trans_width == 4) {
                    _leaps[i].transition = (time_t)_decode32(val);
                }
                else {
                    _leaps[i].transition = (time_t)_decode64(val);
                }
                cnt = fread(val, 4, 1, fp);
                if (cnt != 1) {
//                    cerr << "File read error" << endl;
                    error = true;
                    break;
                }
                _leaps[i].change = (time_t)_decode32(val);
            }
        }
        if (!error) {
            char c;
            int i = 0;
            for (int i=0; i < _num_isstd; i++) {
                int cnt = fread(&c, sizeof(char), 1, fp);
                if (cnt != 1) {
//                    cerr << "File read error" << endl;
                    error = true;
                    break;
                }
                _types[i].isstd = c != '\0';
            }
            while (i < _num_types) {
                _types[i++].isstd = 0;
            }
        }
        if (!error) {
            char c;
            int i = 0;
            for (int i=0; i < _num_isgmt; i++) {
                int cnt = fread(&c, sizeof(char), 1, fp);
                if (cnt != 1) {
//                    cerr << "File read error" << endl;
                    error = true;
                    break;
                }
                _types[i].isgmt = c != '\0';
            }
            while (i < _num_types) {
                _types[i++].isgmt = 0;
            }
        }
        return error;
    }

    bool _skipBody(FILE *fp, struct Head *head, int trans_width) {
        bool error = false;
        size_t to_skip = head->num_transitions * (trans_width + 1) +
                             head->num_types * 6 +
                             head->chars +
                             head->num_leaps * 8 +
                             head->num_isstd +
                             head->num_isgmt;
//        cerr << "Skipping:" << to_skip << endl;
        if (fseek(fp, to_skip, SEEK_CUR) != 0) {
//            cerr << "File seek error" << endl;
            error = true;
        }
        return error;
    }

    bool _tzParse() {
        bool error = false;
        string tzspec = _tzspec;
        string tzname = tzspec;
        string tzrules = "";
        string names[2];
        names[0] = "";
        names[1] = "";
        string rules[2];
        string offset_str = "";
        long int offset = 0;
        int pos = tzspec.find(",");
        if (pos != string::npos) {
            tzname = tzspec.substr(0, pos);
            tzrules = tzspec.substr(pos+1);
        }
        if (tzname[0] == '<') {
            pos = tzname.find(">");
            if (pos == string::npos) {
//                cerr << "Invalid TZ spec" << endl;
                error = true;
            }
            else {
                names[0] = tzname.substr(1, pos-1);
                tzname = tzname.substr(pos+1);
            }
        }
        if (!error) {
            pos = tzname.find_first_of("-+0123456789");
            if (pos != string::npos) {
                if (names[0] == "") {
                    names[0] = tzname.substr(0, pos);
                }
                offset_str = tzname.substr(pos);
                if (offset_str[offset_str.size()-1] == '>') {
                    pos = offset_str.find_last_of("<");
                    if (pos == string::npos) {
//                        cerr << "Invalid TZ spec" << endl;
                        error = true;
                    }
                    else {
                        names[1] = offset_str.substr(pos+1, offset_str.size()-pos-2);
                        offset_str = offset_str.substr(0, pos);
                    }
                }
                pos = offset_str.find_last_of("0123456789");
                if (pos == string::npos) {
//                    cerr << "Invalid offset in TZ name" << endl;
                    error = true;
                }
                else {
                    if (names[1] != "") {
                        names[1]= offset_str.substr(pos+1);
                    }
                    offset_str = offset_str.substr(0, pos+1);
                }
            }
            else if (names[0] == "") {
                names[0] = tzname;
            }
        }
        if (!error) {
            if (names[1] != "") {
                rules[0] = tzrules;
                rules[1] = "";
                pos = tzrules.find(",");
                if (pos != string::npos) {
                    rules[0] = tzrules.substr(0, pos);
                    rules[1] = tzrules.substr(pos+1);
                }
//                cerr << "TZ names[0]:" << names[0] << endl;
//                cerr << "TZ names[1]" << names[1]<< endl;
//                cerr << "TZ offset_str:" << offset_str << endl;
//                cerr << "TZ rules[0]:" << rules[0] << endl;
//                cerr << "TZ rules[1]:" << rules[1] << endl;
                for (int i=0; i < 2; i++) {
                     struct tz_rule *tzr = &_tz_rules[i];
                     string rule = rules[i];
                     if (rule == "") {
                         tzr->type = tz_rule::M;
                         if (i == 0) {
                             tzr->m = 4;
                             tzr->n = 1;
                             tzr->d = 0;
                         }
                         else {
                             tzr->type = tz_rule::M;
                             tzr->m = 10;
                             tzr->n = 5;
                             tzr->n = 0;
                         }
                     }
                     else if (rule[0] == 'M') {
                         tzr->type = tz_rule::M;
                         int cnt = sscanf(rule.c_str(), "M%hu.%hu.%hu",
                                          &tzr->m, &tzr->n, &tzr->d);
                         if ( (cnt != 3) or
                              (tzr->m < 1) or (tzr->m > 12) or
                              (tzr->n < 1) or (tzr->n > 5) or
                              (tzr->d > 6) ) {
//                            cerr << "Invalid TZ rule:" << rule << endl;
                            error = true;
                            break;
                         }
                     }
                     else {
                         string rule1 = rule;
                         if (rule[0] == 'J') {
                             tzr->type = tz_rule::J0;
                             rule1 = rule.substr(1);
                         }
                         else {
                             tzr->type = tz_rule::J1;
                         }
                         int cnt = sscanf(rule1.c_str(), "%hu", &tzr->d);
                         if ( (cnt != 1) or
                              (tzr->d > 365) ) {
//                             cerr << "Invalid TZ rule:" << rule << endl;
                             error = true;
                             break;
                         }
                     }
                     pos = rule.find("/");
                     int hh = 2;
                     int mm = 0;
                     int ss = 0;
                     if (pos != string::npos) {
                         string timestr = rule.substr(pos+1);
                         sscanf(timestr.c_str(), "%u:%u:%u",
                                &hh, &mm, &ss);
                     }
                     tzr->secs = hh*3600 + mm*60 +ss;
                     tzr->computed_for = -1;
                }
            }
        }
        if (!error) {
            offset = 0;
            long int offsign = 1;
            if (offset_str != "") {
                if ( (offset_str[0] == '+') or (offset_str[0] == '-') ) {
                    if (offset_str[0] == '-') {
                        offsign = -1;
                    }
                    offset_str = offset_str.substr(1);
                }
                int hh = 0;
                int mm = 0;
                int ss = 0;
                sscanf(offset_str.c_str(), "%u:%u:%u", &hh, &mm, &ss);
                offset = offsign * (hh*3600 + mm*60 +ss);
            }
            _tz_rules[0].offset = offset;
            _tz_rules[1].offset = offset + 3600;
//            cerr << "TZ offset:" << offset << endl;
            _tz_spec_offset = offset;
            _tz_rules[0].name = names[0];
            _tz_rules[1].name = names[1];
        }
        return error;
    }

    void _calcTime(time_t timer, struct tm *tp, string &tz) {
        long int offset = 0;
        if (_use_gmt) {
            gmtime_r(&timer, tp);
            tz = "GMT";
        }
        else if ( (_num_transitions == 0) or (timer < _transitions[0]) ) {
//            cerr << "Calculating before first transition" << endl;
            _calcTimeBeforeFirst(timer, tp, tz);
        }
        else if (timer >= _transitions[_num_transitions-1]) {
//            cerr << "Calculating after last transition" << endl;
            _calcTimeAfterLast(timer, tp, tz);
        }
        else {
//            cerr << "Calculating in transtitions" << endl;
            _calcTimeInbetween(timer, tp, tz);
        }
    }

    void _calcTimeBeforeFirst(time_t timer, struct tm *tp, string &tz) {
        long int offset = 0;
        size_t idx = 0;
        _tzname[0] = "";
        _tzname[1] = "";
        while ( (idx < _num_types) and _types[idx].isdst) {
            if (_tzname[1] == "") {
                _tzname[1] = _zone_names + _types[idx].idx;
                idx++;
            }
            if (idx == _num_types) {
                idx = 0;
            }
            _tzname[0] = _zone_names + _types[idx].idx;
            if (_tzname[1] == "") {
                int j = idx;
                while (j < _num_types) {
                    if (_types[j].isdst) {
                        _tzname[1] = _zone_names + _types[j].idx;
                        break;
                    }
                    j++;
                }
            }
        }
        timer += _types[idx].offset;
        tz = _zone_names + _types[idx].idx;
        gmtime_r(&timer, tp);
    }

    void _calcTimeAfterLast(time_t timer, struct tm *tp, string &tz) {
        long int offset = -_tz_spec_offset;
        size_t idx = 0;
        int isdst = 0;
        if ( (_tzspec == NULL) or (strlen(_tzspec) == 0) ) {
//            cerr << "No TZ spec" << endl;
            idx = _num_transitions - 1;
            struct ttinfo *info = &_types[_type_idxs[idx]];
            offset = info->offset;
            tz = _zone_names + info->idx;
            isdst = info->isdst;
        }
        else {
            if (_tz_rules[1].name == "") {
                tz = _tz_rules[0].name;
            }
            else {
//                cerr << "_tz_rules[0].name:" << _tz_rules[0].name << endl;
//                cerr << "_tz_rules[1].name:" << _tz_rules[1].name << endl;
                gmtime_r(&timer, tp);
                _calcChangeTime(&_tz_rules[0], tp->tm_year+1900);
                _calcChangeTime(&_tz_rules[1], tp->tm_year+1900);
                isdst = 0;
                /* If DST change time is greater than time to change back */
                if (_tz_rules[0].change > _tz_rules[1].change) {
                    if ( (timer < _tz_rules[0].change) or
                         (timer >= _tz_rules[1].change) ) {
                        isdst = 1;
                    }
                }
                else {
                    if ( (timer >= _tz_rules[0].change) or
                         (timer < _tz_rules[1].change) ) {
                        isdst = 1;
                    }
                }
                tz = _tz_rules[isdst].name;
                offset = _tz_rules[isdst].offset;
            }
        }
        timer += offset;
        gmtime_r(&timer, tp);
        tp->tm_isdst = isdst;
    }

    void _calcChangeTime(struct tz_rule *rule, int year) {
        time_t t;
        if ( (year == -1) or (year != rule->computed_for) ) {
            if (year > 1970) {
                t = ( (year-1970)*365
                      + ((year-1)/4 - 1970/4) 
                      - ((year-1)/100 - 1970/100)
                      + ((year-1)/400 - 1070/400) )*SECSPERDAY;
            }
            else {
                t = 0;
            }
        }
        switch (rule->type) {
          case tz_rule::J1:
            t += (rule->d - 1) * SECSPERDAY;
            if ( (rule->d >= 60) and isleap(year) ) {
                t += SECSPERDAY;
            }
            break;
          case tz_rule::J0:
            t += (rule->d - 1) * SECSPERDAY;
            break;
          case tz_rule::M:
            const unsigned short *mday = &daysInYear[isleap(year)][rule->m];
            t += mday[-1] * SECSPERDAY;

            int m1 = (rule->m + 0)%12 + 1;
            int yy0 = year;
            if (rule->m <= 2) {
                yy0 = year - 1;
            }
            int yy1 = yy0 / 100;
            int yy2 = yy0 % 100;
            /* Use Zeller's Congruence to get day of week of first day of month */
            int dow = ( (26*m1 - 2)/10 + 1
                        + yy2 + yy2/4 
                        + yy1/4 - 2*yy1 ) % 7;
            if (dow < 0) {
                dow += 7;
            }
            int d = rule->d - dow;
            if (d < 0) {
                d += 7;
            }
            for (int i=1; i < rule->n; i++) {
                if ((d+7) >= (int)(mday[0]-mday[1])) {
                    break;
                }
                d += 7;
            }
            t += d * SECSPERDAY;
            break;
        }
        rule->change = t = rule->offset + rule->secs;
        rule->computed_for = year;
    }

    void _calcTimeInbetween(time_t timer, struct tm *tp, string &tz) {
        size_t idx = 0;
        _tzname[0] = "";
        _tzname[1] = "";
        size_t lo = 0;
        size_t hi = _num_transitions - 1;
        idx = (_transitions[_num_transitions-1] - timer) / 15778476;
        bool found = false;
        if (idx< _num_transitions) {
            idx = _num_transitions - idx;
            if (timer < _transitions[idx]) {
                if ( (idx < 10) or (timer >- _transitions[idx-10]) ) {
                    while (timer < _transitions[idx-1]) {
                       idx--;
                    }
                    found = true;
                }
                hi = idx - 10;
            }
            else {
                if ( ((idx+10) > _num_transitions) or
                     (timer < _transitions[idx+10]) ) {
                    while (timer >= _transitions[idx]) {
                        idx++;
                    }
                    found = true;
                }
                lo = idx + 10;
            }
        }
        if (!found) {
            while (lo+1 < hi) {
                idx = (lo + hi) / 2;
                if (timer < _transitions[idx]) {
                    hi = idx;
                }
                else {
                    lo = idx;
                }
            }
            idx = hi;
        }
//        cerr << "Found index:" << idx << endl;
        struct ttinfo *info = &_types[_type_idxs[idx-1]];
        timer += info->offset;
//        cerr << "Found offset:" << info->offset << endl;
//        cerr << "Type.idx:" << (int)info->idx << endl;
        gmtime_r(&timer, tp);
        tp->tm_isdst = info->isdst;
        tz = _zone_names + info->idx;
    }

    void _calcLeapseconds(time_t timer, long int *leap_correct, int *leap_hit) {
        *leap_correct = 0L;
        *leap_hit = 0;
    }

  public:
    Timezone() {
        _use_gmt = true;
        _is_valid = false;
    }

    Timezone(string timezone) {
        _use_gmt = true;
        _is_valid = false;
        _tzname[0] = _tzname[1] = "GMT";
        _tz_offset = 0;
        _daylight = false;
        _transitions = NULL;
        int trans_width = 4;
        struct tzhead tzhead;
        bool error = false;
        if ( (sizeof(time_t) == 4) or (sizeof(time_t) == 8) ) {
            string zonefile = ZoneinfoDir;
            zonefile += "/" + timezone;
            FILE *fp = fopen(zonefile.c_str(), "r");
            if (fp == NULL) {
//                cerr << "Failed to open " << zonefile << endl;
                error = true;
            }
            else {
                struct Head head1;
                error = _readHeader(fp, &head1);
                if (!error) {
                    if ( (sizeof(time_t) == 8) and
                         (head1.version != '\0') ) {
                        error = _skipBody(fp, &head1, trans_width);
                        if (!error) {
                            error = _readHeader(fp, &head1);
                            trans_width = 8;
                        }
                    }
                }
                if (!error) {
//                     cerr << "Version:" << head1.version << endl;
//                     cerr << "Transitions:" << head1.num_transitions << endl;
//                     cerr << "Types:" << head1.num_types << endl;
//                     cerr << "Chars:" << head1.chars << endl;
//                     cerr << "Leaps:" << head1.num_leaps << endl;
//                     cerr << "Isstd:" << head1.num_isstd << endl;
//                     cerr << "Isgmt:" << head1.num_isgmt << endl;
                    _num_transitions = head1.num_transitions;
                    _num_types = head1.num_types;
                    _chars = head1.chars;
                    _num_leaps = head1.num_leaps;
                    _num_isstd = head1.num_isstd;
                    _num_isgmt = head1.num_isgmt;
                    error = _readBody(fp, trans_width);
                }
                if (!error) {
                    if ( (trans_width == 4) and 
                         (head1.version != '\0') ) {
                        struct Head head2;
                        error = _readHeader(fp, &head2);
                        if (!error) {
                           error = _skipBody(fp, &head2, 8);
                        }
                    }
                }
                if (!error) {
                    size_t pos = ftell(fp);
                    fseek(fp, 0, SEEK_END);
                    size_t end = ftell(fp);
                    fseek(fp, pos, SEEK_SET);
                    _tzspec_size = end - pos;
//                    cerr << "Remaining:" << _tzspec_size << endl;
                    if (_tzspec_size == 0) {
                        _tzspec = NULL;
                    }
                    else {
                        _tzspec = (char *)malloc(_tzspec_size+1);
                        if (_tzspec == NULL) {
//                            cerr << "Error in allocating memory" << endl;
                            error = true;
                        }
                        if (!error) {
                            int cnt = fread(_tzspec, sizeof(char), _tzspec_size, fp);
                            if (cnt != _tzspec_size) {
//                                cerr << "File read error" << endl;
                                error = true;
                            }
                            else {
                                _tzspec[_tzspec_size] = '\0';
                                if (_tzspec[0] != '\n') {
//                                    cerr << "Invalid tzspec" << endl;
                                    error = true;
                                }
                                else {
                                    strcpy(_tzspec, _tzspec+1);
                                    _tzspec_size--;
                                    if (_tzspec[_tzspec_size-1] == '\n') {
                                        _tzspec_size--;
                                        _tzspec[_tzspec_size] = '\0';
                                    }
//                                    cerr << "TZSpec:" << _tzspec << endl;
                                    _tzParse();
                                }
                            }
                        }
                    }
                }
                fclose(fp);
                if (!error) {
                    _tzname[0] = _tzname[1] = "";
                    for (int i=_num_transitions; i > 0;) {
                        int type = _type_idxs[--i];
                        int dst = _types[type].isdst;
                        if (_tzname[dst] == "") {
                            int idx = _types[type].idx;
                            _tzname[dst] = _zone_names + idx;
                            if (_tzname[1-dst] != "") {
                                break;
                            }
                        }
                    }
                    if (_tzname[0] == "") {
                        if (_num_types == 1) {
                            _tzname[0] = _zone_names;
                        }
                        else {
//                            cerr << "No transitions and more than one type" << endl;
                            error = true;
                        }
                    }
                }
                if (!error) {
                    if (_tzname[1] == "") {
                        _tzname[1] = _tzname[0];
                    }
                    if (_num_transitions == 0) {
                        _rule_stdoff = _rule_dstoff = _types[0].offset;
                    }
                    else {
                        bool stdoff_set = false;
                        bool dstoff_set = false;
                        for (int i=_num_transitions-1; i >= 0; i--) {
                            int idx = _type_idxs[i];
                            if (!stdoff_set and _types[idx].isdst) {
                                stdoff_set = true;
                                _rule_stdoff = _types[idx].offset;
                            }
                            else if (!dstoff_set and _types[idx].isdst) {
                                dstoff_set = true;
                                _rule_dstoff = _types[idx].offset;
                            }
                            if (stdoff_set && dstoff_set) {
                                break;
                            }
                        }
                        if (!dstoff_set) {
                            _rule_dstoff = _rule_stdoff;
                        }
                    }
                    _daylight = _rule_dstoff != _rule_stdoff;
                    _tz_offset = -_rule_stdoff;
//                    for (int i=0; i < _num_transitions; i++) {
//                        struct tm tm;
//                        gmtime_r(&_transitions[i], &tm);
//                        int idx = _type_idxs[i];
//                        char * tz = _zone_names + _types[idx].idx;
//                        int offset = _types[idx].offset / 3600;
//                        printf("%04d/%02d/%02d %02d:%02d:%02d %s %d\n",
//                               tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
//                               tm.tm_hour, tm.tm_min, tm.tm_sec, tz, offset);
//                    }
                }
                if (!error) {
                    _use_gmt = false;
                    _is_valid = true;
                }
            }
        }
    }

    void gettime(time_t timer, struct tm *tp, string &tz) {
        _calcTime(timer, tp, tz);
    }

    bool isValid() {
        return _is_valid;
    }
};

typedef map<string,Timezone> Timezones;

#endif

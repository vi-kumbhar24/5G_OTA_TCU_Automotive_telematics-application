#ifndef CSV_H
#define CVS_H

#include "autonet_types.h"
using namespace std;

string csvQuote(string instr) {
    string outstr = "\"";
    int pos = 0;
    string term = "\\\"\r\n\t`";
    while (pos < instr.size()) {
        int nextpos = instr.find_first_of(term.c_str(), pos);
        if (nextpos == string::npos) {
            break;
        }
        outstr += instr.substr(pos, (nextpos-pos));
        outstr += "\\";
        char ch = instr.at(nextpos);
        if (ch == '\n') {
            ch = 'n';
        }
        else if (ch == '\r') {
            ch = 'r';
        }
        else if (ch == '\t') {
            ch = 't';
        }
        outstr += ch;
        pos = nextpos + 1;
    }
    outstr += instr.substr(pos);
    outstr += "\"";
    return outstr;
}

string csvUnquote(string instr) {
    string outstr = instr;
    if ((instr.size() > 0) and (instr.at(0) == '"')) {
       outstr = "";
       instr = instr.substr(1, instr.size()-2);
       int pos = 0;
       while (pos < instr.size()) {
           int nextpos = instr.find_first_of("\\", pos);
           if (nextpos == string::npos) {
               outstr += instr.substr(pos);
               break;
           }
           outstr += instr.substr(pos, nextpos-pos);
           pos = nextpos;
           pos++;
           if (pos < instr.size()) {
               char ch = instr.at(pos);
               pos++;
               if (ch == 'n') {
                   ch = '\n';
               }
               else if (ch == 'r') {
                   ch = '\r';
               }
               else if (ch == 't') {
                   ch = '\t';
               }
               outstr += ch;
           }
        }
    }
    return outstr;
}

string csvJoin(Strings vals, string sep) {
    string retstr = "";
    for (int i=0; i < vals.size(); i++) {
        if (i != 0) {
            retstr += sep;
        }
        string val = vals[i];
        string term = sep + "\\\"\r\n\t '`";
        if (val.find_first_of(term.c_str()) != string::npos) {
            val  = csvQuote(val);
        }
        retstr += val;
    }
    return retstr;
}

Strings csvSplit(string instr, string sep) {
    Strings retstrs;
    int pos = 0;
    int next_pos = pos;
    while (pos < instr.size()) {
        if (instr.at(pos) == '"') {
            int start_pos = pos;
            pos++;
            string str = "";
            while (pos < instr.size()) {
                next_pos = instr.find_first_of("\\\"", pos);
                if (next_pos == string::npos) {
                    pos = instr.size();
                }
                else if (instr.at(next_pos) == '\\') {
                    pos = next_pos + 1;
                    if (pos < instr.size()) {
                        pos++;
                    }
                }
                else {
                    pos = next_pos;
                    break;
                }
            }
            if (pos < instr.size()) {
                pos++;
                str = instr.substr(start_pos, (pos-start_pos));
                retstrs.push_back(str);
                if (pos < instr.size()) {
                    if (instr.substr(pos,1).find_first_of(sep.c_str()) == string::npos) {
                        // not separator
                        break;
                    }
                }
            }
            else {
                str = instr.substr(start_pos);
                str += "\"";
                retstrs.push_back(str);
                break;
            }
        }
        else {
            next_pos = instr.find_first_of(sep.c_str(), pos);
            if (next_pos == string::npos) {
                string str = instr.substr(pos);
                retstrs.push_back(str);
                break;
            }
            else {
                string str = instr.substr(pos, (next_pos-pos));
                retstrs.push_back(str);
                pos = next_pos;
            }
        }
        if (pos > instr.size()) {
            retstrs.push_back("");
            break;
        }
        pos++;
    }
    return retstrs;
}

#endif

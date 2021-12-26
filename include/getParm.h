#ifndef GETPARM_H
#define GETPARM_H

string getParm(string str, string name, const char *term) {
    string res = "";
    int pos = str.find(name);
    if (pos != string::npos) {
        res = str.substr(pos+name.size());
        pos = res.find_first_of(term);
        if (pos != string::npos) {
            res = res.substr(0,pos);
        }
    }
    return res;
}

string getParm(string str, string name) {
    return getParm(str, name, " \n");
}

#endif

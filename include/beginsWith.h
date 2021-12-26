#ifndef BEGINSWITH_H
#define BEGINSWITH_H

bool beginsWith(string str, string match) {
    return (strncmp(str.c_str(), match.c_str(), match.size()) == 0);
}

#endif

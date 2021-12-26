#ifndef SSL_KEYS_H
#define SSL_KEYS_H

#include <sys/types.h>
#include <unistd.h>
#include "autonet_files.h"
#include "fileExists.h"
#include "fileStats.h"
#include "readFile.h"
#include "updateFile.h"
#include "grepFile.h"
#include "base64.h"
#include "getToken.h"
#include "string_printf.h"
using namespace std;

void generateKeys() {
    if (!fileExists(PrivateKeyFile) or (fileSize(PrivateKeyFile) == 0)) {
        string pvtfile = string_printf("%s.%d", PrivateKeyFile, getpid());
        string cmnd = "openssl genrsa -out " + pvtfile + " 1024;";
        cmnd += "chmod 400 " + pvtfile;
        system(cmnd.c_str());
        if (rename(pvtfile.c_str(), PrivateKeyFile) == 0) {
            cmnd = "openssl rsa -in " + string(PrivateKeyFile) + " -pubout >" + string(PublicKeyFile) + ";";
            cmnd += "chmod 444 " + string(PublicKeyFile);
            system(cmnd.c_str());
        }
        sync();
    }
}

string cleanKey(string keyfile) {
    string key = readFile(PublicKeyFile);
    string outkey = "";
    bool rsa_key = false;
    if (key.find("-----BEGIN RSA PUBLIC KEY-----") != string::npos) {
        rsa_key = true;
    }
    outkey = key.substr(key.find("\n")+1);
    outkey = outkey.substr(0,outkey.find_last_of("\n"));
    int pos;
    while ((pos=outkey.find("\n")) != string::npos) {
        outkey.replace(pos,1,"");
    }
    if (!rsa_key) {
        string decoded = base64_decode(outkey);
        decoded = decoded.substr(22);
        outkey = base64_encode((unsigned char *)decoded.c_str(), decoded.size());
    }
    return outkey;
}

bool createServerKeyFile(string server) {
    bool created = false;
    string serverline = grepFile(ServerKeysFile, server.c_str());
    if (serverline != "") {
        string key = getToken(serverline, 2);
        string deckey = base64_decode(key);
        char buffer[16];
        string str;
        str.assign("\x00", 1);
        deckey.insert(0, str);
        buffer[0] = (unsigned char)deckey.size();
        str.assign(buffer, 1);
        deckey.insert(0, str);
        deckey.insert(0, "\x03\x81");
        str.assign("\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05\x00", 15);
        deckey.insert(0, str);
        buffer[0] = (unsigned char)deckey.size();
        str.assign(buffer, 1);
        deckey.insert(0, str);
        deckey.insert(0, "\x30\x81");
        string newkey = base64_encode((unsigned char *)deckey.c_str(), deckey.size());
        for (int i=64; i < newkey.size(); i+=65) {
             newkey.insert(i, "\n");
        }
        newkey.insert(0, "-----BEGIN PUBLIC KEY-----\n");
        newkey += "\n-----END PUBLIC KEY-----";
        string filename = "/tmp/" + server + ".pubkey";
        updateFile(filename, newkey);
        created = true;
    }
    return created;
}

#endif

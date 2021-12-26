#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "json/json.h"
#include <string>
#include <iostream>
#include <fstream>
#include "autonet_types.h"
#include "backtick.h"
#include "str_system.h"
using namespace std;

void getInterfaces();
void checkIpV4V6(Json::Value interfaces);
bool handleEthernet(string ifName, Json::Value interfaceVal);
bool handleIpV4(string ifName, int index, Json::Value ruleVal);
bool handleIpV6(string ifName, int index, Json::Value ruleVal);
bool makeIpV4Address(string ifName, int index, string src, string dst, string &address);
bool makeIpV6Address(string ifName, int index, string src, string dst, string &address);
string makeAction(string action);
bool checkIpV4Range(string addr);
bool checkIpV4Address(string addr);
bool makeIpTcpUdpRule(string ifName, int index, string type, Json::Value layerVal, string &iptablesL4, bool rev);
bool makeIpAhEspRule(string ifName, int index, string type, Json::Value layerVal, string &iptablesL4, bool rev);
bool makeIpV4IcmpRule(string ifName, int index, string type, Json::Value layerVal, string &iptablesL4, bool rev);
bool makeIpV6IcmpRule(string ifName, int index, string protocol, Json::Value layerVal, string &iptablesL4, bool rev);
bool isInterface(string if_name);
string interfaceToIpV4(string if_name);
string interfaceToIpV6(string if_name);
bool isDomainName(string addr);
string domainNameToIpV4(string addr);
string domainNameToIpV6(string name);
bool checkIpPorts(string addr);
bool handleCan(string ifName, Json::Value interfaceVal);
Assoc v4interfaces;
Assoc v6interfaces;
Strings interfaces;
bool ipv4 = false;
bool ipv6 = false;

int main( int argc, const char* argv[] )
{
    getInterfaces();
    string rulesfile = "firewall.json";
    if (argc > 1) {
       rulesfile = argv[1];
    }
    Json::Value root;   
    Json::Reader reader;
    ifstream jsonFile(rulesfile, ifstream::in);
    if (!jsonFile){
        std::cerr << "Error opening file\n";
        return -1;
    }
    string strJson;
    strJson.assign( (istreambuf_iterator<char>(jsonFile) ), (istreambuf_iterator<char>() ) );
    jsonFile.close();
    bool parsingSuccessful = reader.parse( strJson.c_str(), root );     //parse process
    if ( !parsingSuccessful )
    {
        cout  << "Failed to parse"
               << reader.getFormattedErrorMessages();
        return 0;
    }
    if (!root.isObject()) {
        cout << "Document root is not an object" << endl;
        return -1;
    }
    Json::Value interfaces = root["interfaces"];
    if (interfaces.isNull()) {
        cout << "\"interfaces\" does not exist" << endl;
        return -1;
    }
    if (!interfaces.isArray()) {
        cout << "\"interfaces\" is not an array" << endl;
        return -1;
    }
    checkIpV4V6(interfaces);
    if (ipv4) {
        cout << "iptables -F" << endl;
        system("iptables -F");
        cout << "iptables -N LOG_DROP" << endl;
        system("iptables -N LOG_DROP 2>/dev/null");
        cout << "iptables -A LOG_DROP -j LOG" << endl;
        system("iptables -A LOG_DROP -j LOG");
        cout << "iptables -A LOG_DROP -j DROP" << endl;
        system("iptables -A LOG_DROP -j DROP");
    }
    if (ipv6) {
        cout << "ip6tables -F" << endl;
        system("ip6tables -F");
        cout << "ip6tables -N LOG_DROP" << endl;
        system("ip6tables -N LOG_DROP 2>/dev/null");
        cout << "ip6tables -A LOG_DROP -j LOG" << endl;
        system("ip6tables -A LOG_DROP -j LOG");
        cout << "ip6tables -A LOG_DROP -j DROP" << endl;
        system("ip6tables -A LOG_DROP -j DROP");
    }
    for ( int index = 0; index < interfaces.size(); ++index ) {
        Json::Value interfaceVal = interfaces[index];
        Json::Value nameVal = interfaceVal["name"];
        if (nameVal.isNull()) {
            cout << "Missing interface:" << index << " \"name\"" << endl;
            return -1;
        }
        if (!nameVal.isString()) {
            cout << "Interface:" << index << " \"name\" is not a string" << endl;
            return -1;
        }
        string ifName = nameVal.asString();
        if (interfaceToIpV4(ifName) == "") {
            cout << "Interface:" << index << " \"name\": " << ifName << " is not valid interface" << endl;
            continue;
        }
        Json::Value typeVal = interfaceVal["type"];
        if (typeVal.isNull()) {
            cout << "Missing interface:" << index << " \"type\"" << endl;
            return -1;
        }
        if (!typeVal.isString()) {
            cout << "Interface:" << index << " \"type\" is not a string" << endl;
            return -1;
        }
        string ifType = typeVal.asString();
        if (ifType == "ethernet") {
            if (!handleEthernet(ifName, interfaceVal)) {
                return -1;
            }
        }
        else if (ifType == "can") {
            if (!handleCan(ifName, interfaceVal)) {
                return -1;
            }
        }
        else {
            cout << "Invalid interface type: \"" << ifType << "\"" << endl;
            return -1;
        }
    }
    return 0;
}

void getInterfaces() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        family = ifa->ifa_addr->sa_family;
        string if_name = ifa->ifa_name;
        if (family == AF_INET || family == AF_INET6) {
            int addr_sz = sizeof(struct sockaddr_in);
            if (family == AF_INET6) {
                addr_sz = sizeof(struct sockaddr_in6);
            }
            s = getnameinfo(ifa->ifa_addr,
                    addr_sz,
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }
            string address = host;
            if (family == AF_INET) {
                if (interfaceToIpV4(if_name) == "") {
                    v4interfaces[if_name] = address;
                }
                else {
                    v4interfaces[if_name].append(","+address);
                }
            }
            else {
                if (interfaceToIpV6(if_name) == "") {
                    v6interfaces[if_name] = address;
                }
                else {
                    v6interfaces[if_name].append(","+address);
                }
            }
            if (!isInterface(if_name)) {
                interfaces.push_back(if_name);
            }
        }
    }
    freeifaddrs(ifaddr);
}

void checkIpV4V6(Json::Value interfaces) {
    for ( int index = 0; index < interfaces.size(); ++index ) {
        Json::Value interfaceVal = interfaces[index];
        Json::Value typeVal = interfaceVal["type"];
        if (!typeVal.isNull() and typeVal.isString()) {
            string ifType = typeVal.asString();
            if (ifType == "ethernet") {
                Json::Value rulesVal = interfaceVal["rules"];
                if (!rulesVal.isNull() and rulesVal.isArray()) {
                    for ( int index = 0; index < rulesVal.size(); ++index ) {
                        Json::Value ruleVal = rulesVal[index];
                        if (ruleVal.isObject()) {
                            Json::Value protocolVal = ruleVal["protocol"];
                            if (!protocolVal.isNull() and protocolVal.isString()) {
                                string prot = protocolVal.asString();
                                if (prot == "ipv4") {
                                    ipv4 = true;
                                }
                                else if (prot == "ipv6") {
                                    ipv6 = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

bool handleEthernet(string ifName, Json::Value interfaceVal) {
    bool retval = false;
    Json::Value defaultInVal = interfaceVal["default_input_action"];
    if (defaultInVal.isNull()) {
        cout << "Missing interface:" << ifName << " \"default_input_action\"" << endl;
        return false;
    }
    if (!defaultInVal.isString()) {
        cout << "Interface:" << ifName << " \"default_input_action\" is not a string" << endl;
        return false;
    }
    string defaultInputAction = defaultInVal.asString();
    if ((defaultInputAction != "drop") and (defaultInputAction != "allow") and (defaultInputAction != "log_drop")) {
        cout << "Interface:" << ifName << " \"default_input_action\" invalid value" << endl;
        return false;
    }
    Json::Value defaultOutVal = interfaceVal["default_output_action"];
    if (defaultOutVal.isNull()) {
        cout << "Missing interface:" << ifName << " \"default_output_action\"" << endl;
        return false;
    }
    if (!defaultOutVal.isString()) {
        cout << "Interface:" << ifName << " \"default_output_action\" is not a string" << endl;
        return false;
    }
    string defaultOutputAction = defaultOutVal.asString();
    if ((defaultOutputAction != "drop") and (defaultOutputAction != "allow") and (defaultOutputAction != "log_drop")) {
        cout << "Interface:" << ifName << " \"default_output_action\" invalid value" << endl;
        return false;
    }
    Json::Value rulesVal = interfaceVal["rules"];
    if (rulesVal.isNull()) {
        cout << "Interface:" << ifName << " \"rules\" does not exist" << endl;
        return false;
    }
    if (!rulesVal.isArray()) {
        cout << "Interface:" << ifName << " \"rules\" is not an array" << endl;
        return false;
    }
    for ( int index = 0; index < rulesVal.size(); ++index ) {
        Json::Value ruleVal = rulesVal[index];
        if (!ruleVal.isObject()) {
            cout << "Interface:" << ifName << " rule:" << index << "Rule is not an object" << endl;
            return false;
        }
        Json::Value protocolVal = ruleVal["protocol"];
        if (protocolVal.isNull()) {
            cout << "Interface:" << ifName << " rule:" << index << " Missing \"protocol\""  << endl;
            return false;
        }
        if (!protocolVal.isString()) {
            cout << "Interface:" << ifName << " rule:" << index << " \"protocol\" is not a string" << endl;
            return false;
        }
        string prot = protocolVal.asString();
        if (prot == "ipv4") {
            retval = handleIpV4(ifName, index, ruleVal);
        }
        else if (prot == "ipv6") {
            retval = handleIpV6(ifName, index, ruleVal);
        }
        else  {
            cout << "Interface:" << ifName << " rule:" << index << " Unknown protocol \"" << prot << "\"" << endl;
        }
    }
    if (ipv4) {
        if (defaultInputAction == "drop") {
            cout << "iptables -A INPUT -i " << ifName << " -j DROP" << endl;
            str_system(string("iptables -A INPUT -i ") + ifName + " -j DROP");
        }
        else if (defaultInputAction == "log_drop") {
            cout << "iptables -A INPUT -i " << ifName << " -j LOG_DROP" << endl;
            str_system(string("iptables -A INPUT -i ") + ifName + " -j LOG_DROP");
        }
        else {
            cout << "iptables -A INPUT -i " << ifName << " -j ACCEPT" << endl;
            str_system(string("iptables -A INPUT -i ") + ifName + " -j ACCEPT");
        }
        if (defaultOutputAction == "drop") {
            cout << "iptables -A OUTPUT -o " << ifName << " -j DROP" << endl;
            str_system(string("iptables -A OUTPUT -o ") + ifName + " -j DROP");
        }
        else if (defaultOutputAction == "log_drop") {
            cout << "iptables -A OUTPUT -o " << ifName << " -j LOG_DROP" << endl;
            str_system(string("iptables -A OUTPUT -o ") + ifName + " -j LOG_DROP");
        }
        else {
            cout << "iptables -A OUTPUT -o " << ifName << " -j ACCEPT" << endl;
            str_system(string("iptables -A OUTPUT -o ") + ifName + " -j ACCEPT");
        }
    }
    if (ipv6) {
        if (defaultInputAction == "drop") {
            cout << "ip6tables -A INPUT -i " << ifName << " -j DROP" << endl;
            str_system(string("ip6tables -A INPUT -i ") + ifName + " -j DROP");
        }
        else if (defaultInputAction == "log_drop") {
            cout << "ip6tables -A INPUT -i " << ifName << " -j LOG_DROP" << endl;
            str_system(string("ip6tables -A INPUT -i ") + ifName + " -j LOG_DROP");
        }
        else {
            cout << "ip6tables -A INPUT -i " << ifName << " -j ACCEPT" << endl;
            str_system(string("ip6tables -A INPUT -i ") + ifName + " -j ACCEPT");
        }
        if (defaultOutputAction == "drop") {
            cout << "ip6tables -A OUTPUT -o " << ifName << " -j DROP" << endl;
            str_system(string("ip6tables -A OUTPUT -o ") + ifName + " -j DROP");
        }
        else if (defaultOutputAction == "log_drop") {
            cout << "ip6tables -A OUTPUT -o " << ifName << " -j LOG_DROP" << endl;
            str_system(string("ip6tables -A OUTPUT -o ") + ifName + " -j LOG_DROP");
        }
        else {
            cout << "ip6tables -A OUTPUT -o " << ifName << " -j ACCEPT" << endl;
            str_system(string("ip6tables -A OUTPUT -o ") + ifName + " -j ACCEPT");
        }
    }
    return retval;
}

bool handleIpV4(string ifName, int index, Json::Value ruleVal) {
    bool retval = false;
    string src = "";
    string dst = "";
    string iptablesL4 = "";
    string rev_iptablesL4 = "";
    string src_port = "";
    string dst_port = "";
    string direction = "";
    string out_if = "";
    bool bidir = true;
    Json::Value actionVal = ruleVal["action"];
    if (actionVal.isNull()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"action\" does not exist" << endl;
        return false;
    }
    if (!actionVal.isString()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"action\" is not a string" << endl;
        return false;
    }
    string action = actionVal.asString();
    if ((action != "allow") and (action != "drop") and (action != "log_drop")) {
        cout << "Interface:" << ifName << " rule:" << index << "\"action\" invalid value \"" << action << "\"" << endl;
    }
    Json::Value dirVal = ruleVal["direction"];
    if (dirVal.isNull()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"direction\" does not exist" << endl;
        return false;
    }
    if (!dirVal.isString()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"direction\" is not a string" << endl;
        return false;
    }
    Json::Value outifVal = ruleVal["out_interface"];
    if (!outifVal.isNull()) {
        if (!outifVal.isString()) {
            cout << "Interface:" << ifName << " rule:" << index << "\"out_interface\" is not a string" << endl;
            return false;
        }
        string outifstr = outifVal.asString();
        if (interfaceToIpV4(outifstr) == "") {
            cout << "Interface:" << ifName << " rule:" << index << "\"out_interface\" is not an known interface" << endl;
            return false;
        }
        out_if = outifstr;
    }
    Json::Value unidirVal = ruleVal["unidirectional"];
    if (!unidirVal.isNull()) {
        if (!unidirVal.isBool()) {
            cout << "Interface:" << ifName << " rule:" << index << "\"unidirectional\" is not a boolean" << endl;
            return false;
        }
        bidir = !unidirVal.asBool();
    }
    string dir = dirVal.asString();
    if ((dir != "in") and (dir != "out")) {
        cout << "Interface:" << ifName << " rule:" << index << "\"direction\" invalid value \"" << action << "\"" << endl;
    }
    Json::Value paramsVal = ruleVal["params"];
    if (!paramsVal.isNull()) {
        if (!paramsVal.isObject()) {
            cout << "Interface:" << ifName << "rule:" << index << "\"params\" is not an object" << endl;
            return false;
        }
        Json::Value protocolVal = paramsVal["protocol"];
        if (protocolVal.isNull()) {
            cout << "Interface:" << ifName << " rule:" << index << " \"protocol\" does not exist" << endl;
            return false;
        }
        if (!protocolVal.isString()) {
            cout << "Interface:" << ifName << " rule:" << index << " \"protocol\" is not a string" << endl;
            return false;
        }
        string protocol = protocolVal.asString();
        Json::Value srcVal = paramsVal["source"];
        if (!srcVal.isNull()) {
            if (!srcVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " \"source\" is not a string" << endl;
                return false;
            }
            string ip = srcVal.asString();
            if (!checkIpV4Address(ip)) {
                cout << "Interface:" << ifName << " rule:" << index << " \"source\" invalid format: \"" << ip << "\"" << endl;
                return false;
            }
            src = ip;
        }
        Json::Value dstVal = paramsVal["destination"];
        if (!dstVal.isNull()) {
            if (!dstVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " \"destination\" is not a string" << endl;
                return false;
            }
            string ip = dstVal.asString();
            if (!checkIpV4Address(ip)) {
                cout << "Interface:" << ifName << " rule:" << index << " \"destination\" invalid format: \"" << ip << "\"" << endl;
                return false;
            }
            dst = ip;
        }
        Json::Value l4paramsVal = paramsVal["params"];
        if (!l4paramsVal.isNull() and !l4paramsVal.isObject()) {
            cout << "Interface:" << ifName << " rule:" << index << " \"params\" is not object" << endl;
            return false;
        }
        if ((protocol == "udp") or (protocol == "tcp")) {
            if (!makeIpTcpUdpRule(ifName, index, protocol, l4paramsVal, iptablesL4, false)) {
                return false;
            }
            if (!makeIpTcpUdpRule(ifName, index, protocol, l4paramsVal, rev_iptablesL4, true)) {
                return false;
            }
        }
        else if ((protocol == "ah") or (protocol == "esp")) {
            if (!makeIpAhEspRule(ifName, index, protocol, l4paramsVal, iptablesL4, false)) {
                return false;
            }
            if (!makeIpAhEspRule(ifName, index, protocol, l4paramsVal, rev_iptablesL4, true)) {
                return false;
            }
        }
        else if (protocol == "icmp") {
            if (!makeIpV4IcmpRule(ifName, index, protocol, l4paramsVal, iptablesL4, false)) {
                return false;
            }
            if (!makeIpV4IcmpRule(ifName, index, protocol, l4paramsVal, rev_iptablesL4, true)) {
                return false;
            }
        }
    }
    string ipV4addr = "";
    string rev_ipV4addr = "";
    if (!makeIpV4Address(ifName, index, src, dst, ipV4addr)) {
        return false;
    }
    if (!makeIpV4Address(ifName, index, dst, src, rev_ipV4addr)) {
        return false;
    }
    string act = makeAction(action);
    string rule = "iptables -A";
    if (dir == "in") {
        rule = rule + " INPUT -i ";
    }
    else {
        rule = rule + " OUTPUT -o ";
    }
    rule += ifName;
    rule += ipV4addr + iptablesL4 + act;
    cout << rule << endl;
    str_system(rule);
    if (bidir) {
        rule = "iptables -A";
        if (dir == "in") {
            rule = rule + " OUTPUT -o ";
        }
        else {
            rule = rule + " INPUT -i ";
        }
        rule += ifName;
        rule += rev_ipV4addr + rev_iptablesL4 + act;
        cout << rule << endl;
        str_system(rule);
    }
    if (out_if != "") {
        rule = "iptables -A";
        if (dir == "in") {
            rule = rule + " OUTPUT -o ";
        }
        else {
            rule = rule + " INPUT -i ";
        }
        rule += out_if;
        rule += rev_ipV4addr + rev_iptablesL4 + act;
        cout << rule << endl;
        str_system(rule);
        if (bidir) {
            rule = "iptables -A";
            if (dir == "in") {
                rule = rule + " INPUT -i ";
            }
            else {
                rule = rule + " OUTPUT -o ";
            }
            rule += out_if;
            rule += rev_ipV4addr + rev_iptablesL4 + act;
            cout << rule << endl;
            str_system(rule);
        }
    }
    return true;
}

bool handleIpV6(string ifName, int index, Json::Value ruleVal) {
    bool retval = false;
    string src = "";
    string dst = "";
    string iptablesL4 = "";
    string rev_iptablesL4 = "";
    string src_port = "";
    string dst_port = "";
    string direction = "";
    string out_if = "";
    bool bidir = true;
    Json::Value actionVal = ruleVal["action"];
    if (actionVal.isNull()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"action\" does not exist" << endl;
        return false;
    }
    if (!actionVal.isString()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"action\" is not a string" << endl;
        return false;
    }
    string action = actionVal.asString();
    if ((action != "allow") and (action != "drop") and (action != "log_drop")) {
        cout << "Interface:" << ifName << " rule:" << index << "\"action\" invalid value \"" << action << "\"" << endl;
    }
    Json::Value dirVal = ruleVal["direction"];
    if (dirVal.isNull()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"direction\" does not exist" << endl;
        return false;
    }
    if (!dirVal.isString()) {
        cout << "Interface:" << ifName << " rule:" << index << " \"direction\" is not a string" << endl;
        return false;
    }
    Json::Value outifVal = ruleVal["out_interface"];
    if (!outifVal.isNull()) {
        if (!outifVal.isString()) {
            cout << "Interface:" << ifName << " rule:" << index << "\"out_interface\" is not a string" << endl;
            return false;
        }
        string outifstr = outifVal.asString();
        if (interfaceToIpV4(outifstr) == "") {
            cout << "Interface:" << ifName << " rule:" << index << "\"out_interface\" is not an known interface" << endl;
            return false;
        }
        out_if = outifstr;
    }
    Json::Value unidirVal = ruleVal["unidirectional"];
    if (!unidirVal.isNull()) {
        if (!unidirVal.isBool()) {
            cout << "Interface:" << ifName << " rule:" << index << "\"unidirectional\" is not a boolean" << endl;
            return false;
        }
        bidir = !unidirVal.asBool();
    }
    string dir = dirVal.asString();
    if ((dir != "in") and (dir != "out")) {
        cout << "Interface:" << ifName << " rule:" << index << "\"direction\" invalid value \"" << action << "\"" << endl;
    }
    Json::Value paramsVal = ruleVal["params"];
    if (!paramsVal.isNull()) {
        if (!paramsVal.isObject()) {
            cout << "Interface:" << ifName << "rule:" << index << "\"params\" is not an object" << endl;
            return false;
        }
        Json::Value protocolVal = paramsVal["protocol"];
        if (protocolVal.isNull()) {
            cout << "Interface:" << ifName << " rule:" << index << " \"protocol\" does not exist" << endl;
            return false;
        }
        if (!protocolVal.isString()) {
            cout << "Interface:" << ifName << " rule:" << index << " \"protocol\" is not a string" << endl;
            return false;
        }
        string protocol = protocolVal.asString();
        Json::Value srcVal = paramsVal["source"];
        if (!srcVal.isNull()) {
            if (!srcVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " \"source\" is not a string" << endl;
                return false;
            }
            string ip = srcVal.asString();
            if (!checkIpV4Address(ip)) {
                cout << "Interface:" << ifName << " rule:" << index << " \"source\" invalid format: \"" << ip << "\"" << endl;
                return false;
            }
            src = ip;
        }
        Json::Value dstVal = paramsVal["destination"];
        if (!dstVal.isNull()) {
            if (!dstVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " \"destination\" is not a string" << endl;
                return false;
            }
            string ip = dstVal.asString();
            if (!checkIpV4Address(ip)) {
                cout << "Interface:" << ifName << " rule:" << index << " \"destination\" invalid format: \"" << ip << "\"" << endl;
                return false;
            }
            dst = ip;
        }
        Json::Value l4paramsVal = paramsVal["params"];
        if (!l4paramsVal.isNull() and !l4paramsVal.isObject()) {
            cout << "Interface:" << ifName << " rule:" << index << " \"params\" is not object" << endl;
            return false;
        }
        if ((protocol == "udp") or (protocol == "tcp")) {
            if (!makeIpTcpUdpRule(ifName, index, protocol, l4paramsVal, iptablesL4, false)) {
                return false;
            }
            if (!makeIpTcpUdpRule(ifName, index, protocol, l4paramsVal, rev_iptablesL4, true)) {
                return false;
            }
        }
        else if ((protocol == "ah") or (protocol == "esp")) {
            if (!makeIpAhEspRule(ifName, index, protocol, l4paramsVal, iptablesL4, false)) {
                return false;
            }
            if (!makeIpAhEspRule(ifName, index, protocol, l4paramsVal, rev_iptablesL4, true)) {
                return false;
            }
        }
        else if (protocol == "icmp") {
            if (!makeIpV6IcmpRule(ifName, index, protocol, l4paramsVal, iptablesL4, false)) {
                return false;
            }
            if (!makeIpV6IcmpRule(ifName, index, protocol, l4paramsVal, rev_iptablesL4, true)) {
                return false;
            }
        }
    }
    string ipV6addr = "";
    string rev_ipV6addr = "";
    if (!makeIpV6Address(ifName, index, src, dst, ipV6addr)) {
        return false;
    }
    if (!makeIpV6Address(ifName, index, dst, src, rev_ipV6addr)) {
        return false;
    }
    string act = makeAction(action);
    string rule = "ip6tables -A";
    if (dir == "in") {
        rule = rule + " INPUT -i ";
    }
    else {
        rule = rule + " OUTPUT -o ";
    }
    rule += ifName;
    rule += ipV6addr + iptablesL4 + act;
    cout << rule << endl;
    str_system(rule);
    if (bidir) {
        rule = "ip6tables -A";
        if (dir == "in") {
            rule = rule + " OUTPUT -o ";
        }
        else {
            rule = rule + " INPUT -i ";
        }
        rule += ifName;
        rule += rev_ipV6addr + rev_iptablesL4 + act;
        cout << rule << endl;
        str_system(rule);
    }
    if (out_if != "") {
        rule = "ip6tables -A";
        if (dir == "in") {
            rule = rule + " OUTPUT -o ";
        }
        else {
            rule = rule + " INPUT -i ";
        }
        rule += out_if;
        rule += rev_ipV6addr + rev_iptablesL4 + act;
        cout << rule << endl;
        str_system(rule);
        if (bidir) {
            rule = "ip6tables -A";
            if (dir == "in") {
                rule = rule + " INPUT -i ";
            }
            else {
                rule = rule + " OUTPUT -o ";
            }
            rule += out_if;
            rule += rev_ipV6addr + rev_iptablesL4 + act;
            cout << rule << endl;
            str_system(rule);
        }
    }
    return true;
}

bool makeIpV4Address(string ifName, int index, string src, string dst, string &address) {
    string rule = "";
    bool iprange = false;
    if (src != "") {
        string flag = "-s";
        if (checkIpV4Range(src)) {
            flag = "-m iprange --src-range";
            iprange = true;
        }
        else {
            string addr = interfaceToIpV4(src);
            if (addr != "") {
                src = addr;
            }
            else if (isDomainName(src)) {
                addr = domainNameToIpV4(src);
                if (addr == "") {
                    cout << "Interface:" << ifName << " rule:" << index << " \"source\": \"" << src << "\" did not resolve" << endl;
                    return false;
                }
                src = addr;
            }
        }
        rule = rule + " " + flag + " " + src;
    }
    if (dst != "") {
        string flag = "-d";
        if (checkIpV4Range(dst)) {
            flag = "--dst-range ";
            if (!iprange) {
                flag = string("-m iprange " ) + flag;
            }
        }
        else {
            string addr = interfaceToIpV4(dst);
            if (addr != "") {
                dst = addr;
            }
            else if (isDomainName(dst)) {
                addr = domainNameToIpV4(dst);
                if (addr == "") {
                    cout << "Interface:" << ifName << " rule:" << index << " \"destination\": \"" << dst << "\" did not resolve" << endl;
                    return false;
                }
                dst = addr;
            }
        }
        rule = rule + " " + flag + " " + dst;
    }
    address = rule;
    return true;
}

bool makeIpV6Address(string ifName, int index, string src, string dst, string &address) {
    string rule = "";
//    bool iprange = false;
    if (src != "") {
        string flag = "-s";
//        if (src.find("-") != string::npos) {
//            flag = "-m iprange --src-range";
//            iprange = true;
//        }
//        else {
            string addr = interfaceToIpV6(src);
            if (addr != "") {
                src = addr;
            }
            else if (isDomainName(src)) {
                addr = domainNameToIpV6(src);
                if (addr == "") {
                    cout << "Interface:" << ifName << " rule:" << index << " \"source\": \"" << src << "\" did not resolve" << endl;
                    return false;
                }
                src = addr;
            }
//        }
        rule = rule + " " + flag + " " + src;
    }
    if (dst != "") {
        string flag = "-d";
//        if (dst.find("-") != string::npos) {
//            flag = "--dst-range ";
//            if (!iprange) {
//                flag = string("-m iprange " ) + flag;
//            }
//        }
//        else {
            string addr = interfaceToIpV6(dst);
            if (addr != "") {
                dst = addr;
            }
            else if (isDomainName(dst)) {
                addr = domainNameToIpV6(dst);
                if (addr == "") {
                    cout << "Interface:" << ifName << " rule:" << index << " \"destination\": \"" << dst << "\" did not resolve" << endl;
                    return false;
                }
                dst = addr;
            }
//        }
        rule = rule + " " + flag + " " + dst;
    }
    address = rule;
    return true;
}

string makeAction(string action) {
    string act = "";
    if (action == "drop") {
        act = " -j DROP";
    }
    else if (action == "log_drop") {
        act = " -j LOG_DROP";
    }
    else {
        act =  " -j ACCEPT";
    }
    return act;
}

bool checkIpV4Range(string addr) {
    bool retval = false;
    int a1, a2, a3, a4, b1, b2, b3, b4;
    char ch;
    if (sscanf(addr.c_str(), "%d.%d.%d.%d-%d.%d.%d.%d%c",
                                   &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4, &ch) == 8) {
        if ( (a1 >= 0) and (a1 < 256) and
             (a2 >= 0) and (a2 < 256) and
             (a3 >= 0) and (a3 < 256) and
             (a4 >= 0) and (a4 < 256) and
             (b1 >= 0) and (b1 < 256) and
             (b2 >= 0) and (b2 < 256) and
             (b3 >= 0) and (b3 < 256) and
             (b4 >= 0) and (b4 < 256) ) {
            retval = true;
        }
    }
    return retval;
}

bool checkIpV4Address(string addr) {
    bool retval = false;
    int a1, a2, a3, a4, b1, b2, b3, b4;
    int sl;
    char ch;
    if (sscanf(addr.c_str(), "%d.%d.%d.%d%c", &a1, &a2, &a3, &a4, &ch) == 4) {
        if ( (a1 >= 0) and (a1 < 256) and
             (a2 >= 0) and (a2 < 256) and
             (a3 >= 0) and (a3 < 256) and
             (a4 >= 0) and (a4 < 256) ) {
            retval = true;
        }
    }
    else if (sscanf(addr.c_str(), "%d.%d.%d.%d/%d%c", &a1, &a2, &a3, &a4, &sl, &ch) == 5) {
        if ( (a1 >= 0) and (a1 < 256) and
             (a2 >= 0) and (a2 < 256) and
             (a3 >= 0) and (a3 < 256) and
             (a4 >= 0) and (a4 < 256) and
             (sl >= 0) and (sl < 25) ) {
            retval = true;
        }
    }
    else if (sscanf(addr.c_str(), "%d.%d.%d.%d-%d.%d.%d.%d%c",
                                   &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4, &ch) == 8) {
        if ( (a1 >= 0) and (a1 < 256) and
             (a2 >= 0) and (a2 < 256) and
             (a3 >= 0) and (a3 < 256) and
             (a4 >= 0) and (a4 < 256) and
             (b1 >= 0) and (b1 < 256) and
             (b2 >= 0) and (b2 < 256) and
             (b3 >= 0) and (b3 < 256) and
             (b4 >= 0) and (b4 < 256) ) {
            retval = true;
        }
    }
    else if (interfaceToIpV4(addr) != "") {
        retval = true;
    }
    else if (isDomainName(addr)) {
        retval = true;
    }
    return retval;
}

bool makeIpTcpUdpRule(string ifName, int index, string type, Json::Value layerVal, string &iptablesL4, bool rev) {
    string src_port = "";
    string dst_port = "";
    iptablesL4 = " -p " + type;
    if (!layerVal.isNull()) {
        Json::Value srcPortVal = layerVal["src_port"];
        if (!srcPortVal.isNull()) {
            if (!srcPortVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " \"src_port\" is not a string" << endl;
                return false;
            }
            src_port = srcPortVal.asString();
            if (!checkIpPorts(src_port)) {
                cout << "Interface:" << ifName << " rule:" << index << " \"src_port\" invalid format" << endl;
                return false;
            }
        }
        Json::Value dstPortVal = layerVal["dst_port"];
        if (!dstPortVal.isNull()) {
            if (!dstPortVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " \"dst_port\" is not a string" << endl;
                return false;
            }
            dst_port = dstPortVal.asString();
            if (!checkIpPorts(dst_port)) {
                cout << "Interface:" << ifName << " rule:" << index << " \"dst_port\" invalid format" << endl;
                return false;
            }
        }
    }
    if (rev) {
        string tmp_port = src_port;
        src_port = dst_port;
        dst_port = tmp_port;
    }
    if (src_port != "") {
        size_t pos = src_port.find("-");
        if (pos != string::npos) {
            src_port.replace(pos, 1, ":");
        }
        iptablesL4 = iptablesL4 + " --sport " + src_port;
    }
    if (dst_port != "") {
        size_t pos = dst_port.find("-");
        if (pos != string::npos) {
            dst_port.replace(pos, 1, ":");
        }
        iptablesL4 = iptablesL4 + " --dport " + dst_port;
    }
    if (type == "tcp") {
        if (rev) {
            iptablesL4 += " -m conntrack --ctstate ESTABLISHED";
        }
        else {
            iptablesL4 += " -m conntrack --ctstate NEW,ESTABLISHED";
        }
    }
    return true;
}

bool makeIpAhEspRule(string ifName, int index, string type, Json::Value layerVal, string &iptablesL4, bool rev) {
    iptablesL4 = " -p " + type;
    return true;
}

bool makeIpV4IcmpRule(string ifName, int index, string protocol, Json::Value layerVal, string &iptablesL4, bool rev) {
    string type = "";
    iptablesL4 = " -p " + protocol;
    if (!layerVal.isNull()) {
        Json::Value typeVal = layerVal["type"];
        if (!typeVal.isNull()) {
            if (!typeVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " icmp \"type\" is not a string" << endl;
                return false;
            }
            type = typeVal.asString();
            if ((type != "echo-request") and (type != "echo-response")) {
                cout << "Interface:" << ifName << " rule:" << index << " icmp type \"type\" invalid format" << endl;
                return false;
            }
        }
    }
    if (type != "") {
        iptablesL4 = iptablesL4 + " --icmp-type " + type;
    }
    return true;
}

bool makeIpV6IcmpRule(string ifName, int index, string protocol, Json::Value layerVal, string &iptablesL4, bool rev) {
    string type = "";
    iptablesL4 = " -p ipv6-icmp";
    if (!layerVal.isNull()) {
        Json::Value typeVal = layerVal["type"];
        if (!typeVal.isNull()) {
            if (!typeVal.isString()) {
                cout << "Interface:" << ifName << " rule:" << index << " icmp \"type\" is not a string" << endl;
                return false;
            }
            type = typeVal.asString();
            if ((type != "echo-request") and (type != "echo-response")) {
                cout << "Interface:" << ifName << " rule:" << index << " icmp type \"type\" invalid format" << endl;
                return false;
            }
        }
    }
    if (type != "") {
        iptablesL4 = iptablesL4 + " --icmpv6-type " + type;
    }
    return true;
}

bool isInterface(string if_name) {
    bool found = false;
    for (int i=0; i < interfaces.size(); i++) {
        if (interfaces[i] == if_name) {
            found = true;
            break;
        }
    }
    return found;
}

string interfaceToIpV4(string if_name) {
    string ipaddr = "";
    if (v4interfaces.find(if_name) != v4interfaces.end()) {
        ipaddr = v4interfaces[if_name];
    }
    return ipaddr;
}

string interfaceToIpV6(string if_name) {
    string ipaddr = "";
    if (v6interfaces.find(if_name) != v6interfaces.end()) {
        ipaddr = v6interfaces[if_name];
    }
    return ipaddr;
}

bool isDomainName(string addr) {
    bool retval = false;
    size_t pos = addr.find_last_of(".");
    if (pos != string::npos) {
        bool match = true;
        for (pos++; pos < addr.size(); pos++) {
            if (!isalpha(addr[pos])) {
                match = false;
                break;
            }
        }
        retval = match;
    }
    return retval;
}

string domainNameToIpV4(string name) {
    string ipaddr = "";
    ipaddr = backtick(string("resolvedns ")+name);
    size_t pos;
    while ((pos=ipaddr.find_first_of(" ")) != string::npos) {
        ipaddr.replace(pos, 1, ",");
    }
    return ipaddr;
}

string domainNameToIpV6(string name) {
    string ipaddr = "";
    ipaddr = backtick(string("resolvedns -6 ")+name);
    size_t pos;
    while ((pos=ipaddr.find_first_of(" ")) != string::npos) {
        ipaddr.replace(pos, 1, ",");
    }
    return ipaddr;
}

bool checkIpPorts(string addr) {
    bool retval = false;
    int p1, p2;
    char ch;
    if (sscanf(addr.c_str(), "%d%c", &p1, &ch) == 1) {
        retval = true;
    }
    else if (sscanf(addr.c_str(), "%d-%d%c", &p1, &p2, &ch) == 2) {
        retval = true;
    }
    else if (str_system("grep -q '^"+addr+"[ \t]' /etc/services") == 0) {
        retval = true;
    }
    return retval;
}

bool handleCan(string ifName, Json::Value interfaceVal) {
    bool retval = false;
    retval = true;
    return retval;
}

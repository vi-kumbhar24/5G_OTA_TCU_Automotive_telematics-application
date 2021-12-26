/******************************************************************************
 * File: check_eeprom
 *
 * Routine to check/fix eeprom if there is a checksum error
 *
 ******************************************************************************
 * Change log
 * 2012/10/06 rwp
 *    Inital coding
 ******************************************************************************
 */

#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include "autonet_types.h"
#include "autonet_files.h"
#include "backtick.h"
#include "getopts.h"
#include "split.h"
#include "str_system.h"
#include "string_convert.h"
#include "string_printf.h"
using namespace std;

bool ignore(int addr);
void read_file(string file, Byte *mem);

#define EEPROM_FILE "/tmp/eeprom"
#define SAVED_EEPROM_FILE "/autonet/admin/eeprom"
#define SIZE 0x1000
Byte eeprom[SIZE];
Byte in_eeprom[SIZE];
typedef struct {
    int start;
    int end;
} Ignore;
vector<Ignore> ignores;

int main(int argc, char *argv[]) {
    int retval = 0;
    for (int i=1; i < argc; i++) {
        string valstr = argv[i];
        Ignore limits;
        if (sscanf(argv[i], "%x-$x", &limits.start, &limits.end) == 1) {
            limits.end = limits.start;
        }
//        int pos = valstr.find("-");
//        if (pos == string::npos) {
//            limits.end = stringToInt(valstr.substr(pos+1));
//            valstr = valstr.substr(0, pos);
//            limits.start = stringToInt(valstr);
//        }
//        else {
//            limits.start = stringToInt(valstr);
//            limits.end = limits.start;
//        }
        ignores.push_back(limits);
        printf("Ignore: %X - %X\n", limits.start, limits.end);
    }
    read_file(EEPROM_FILE, in_eeprom);
    read_file(SAVED_EEPROM_FILE, eeprom);
//    for (int addr=0; addr < SIZE; addr+=32) {
//        string cmnd = string_printf("eeprom -a %x", addr);
//        string line = backtick(cmnd);
//        Strings bytes = split(line, " ");
//        for (int i=0; i < bytes.size(); i++) {
//             int val = 0;
//             sscanf(bytes[i].c_str(), "%x", &val);
//             in_eeprom[addr+i] = val;
//        }
//    }
    for (int addr=0; addr < SIZE; addr++) {
        if (eeprom[addr] != in_eeprom[addr]) {
            if (!ignore(addr)) {
                printf("%04X: %02X %02X\n", addr, eeprom[addr], in_eeprom[addr]);
                retval = 1;
            }
        }
    }
    return retval;
}

bool ignore(int addr) {
    bool retval = false;
    for (int i=0; i < ignores.size(); i++) {
        Ignore limit = ignores[i];
        if ( (addr >= limit.start) and (addr <= limit.end) ) {
            retval = true;
            break;
        }
    }
    return retval;
}

void read_file(string file, Byte *mem) {
    FILE *fp = fopen(file.c_str(), "r");
    if (!fp) {
       printf("Could not open %s\n", file.c_str());
       exit(1);
    }
    int n = fread(mem, 1, SIZE, fp);
    if (n != SIZE) {
        printf("Could not read full eeprom\n");
        exit(1);
    }
    fclose(fp);
}

#ifndef BCD_H
#define BCD_H

#include "autonet_types.h"

using namespace std;

int byte2bcd(int byte) {
    Byte bcd = 0;
    bcd += (byte%10);
    bcd += (byte/10) << 4;
    return bcd;
}

int word2bcd(int word) {
    int bcd = 0;
    bcd += (word%10);
    word /= 10;
    bcd += (word%10) << 4;
    word /= 10;
    bcd += (word%10) << 8;
    word /= 10;
    bcd += word << 12;
    return bcd;
}

int bcd2byte(int bcd) {
    int byte = 0;
    byte += bcd & 0x0F;
    byte += (bcd>>4) * 10;
    return byte;
}

int bcd2word(int bcd) {
    int word = 0;
    word += bcd & 0x0F;
    bcd >>= 4;
    word += (bcd&0x0F) * 10;
    bcd >>= 4;
    word += (bcd&0x0F) * 100;
    bcd >>= 4;
    word += bcd * 1000;
    return word;
}

#endif

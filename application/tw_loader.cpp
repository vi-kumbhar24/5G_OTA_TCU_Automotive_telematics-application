/******************************************************************************
 * File tw_loader.cpp
 *
 * Routine to load firmware into Telit Wireless cell module
 *
 ******************************************************************************
 * Change log:
 * 2013/12/26 rwp
 *   Initial version
 ******************************************************************************
 */

#include <map>
#include <cstring>
#include <string>
#include <fstream>
#include <iostream>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include "autonet_files.h"
#include "backtick.h"
#include "csv.h"
#include "crc16.h"
#include "dateTime.h"
#include "my_features.h"
#include "getField.h"
#include "getParm.h"
#include "getopts.h"
#include "getStringFromFile.h"
#include "readAssoc.h"
#include "split.h"
#include "string_convert.h"
#include "string_printf.h"
#include "touchFile.h"
#include "tw_loader.h"
#include "updateFile.h"
#include "writeAssoc.h"
using namespace std;

#define diag_device "celldiag"
#define modem_device "cellmodem"
#define NV_FILE "/tmp/backup_nv.bin"
#define PRL_FILE "/tmp/backup_prl.bin"

#define ONCE while(false)

//#define _POSIX_SOURCE 1         //POSIX compliant source

typedef map<int,bool> Readers;

void config_diag_port();
void config_modem_port();
bool backupNv();
bool restoreNv();
bool backupPrl(int nam);
bool restorePrl(int nam);
bool uploadRamLoader(string filename);
bool hello();
bool uploadBinaryFile(string binary_file);
bool resetModule();
void closeAndExit(int exitstat);
void openPort(string port);
void closePort();
bool lock(string dev);
void killConnection();
void resumeConnection();
string getAtResponse(string command);
string getAtResponse(string command, int timeout);
string getAtResponse(string command, string ans, int timeout);
string getRestOfLine();
bool isUnsolicitedEvent(string line);
void unsolicitedEvent(string unsol_line);
void sendDataPacket(PACKET &packet, bool head_flag);
bool getDataResponse(PACKET &out_packet, PACKET &in_packet, bool head_flag, int timeout, int tries);
bool receiveDataPacket(PACKET &recv_packet, bool t_head_flag, int timeout);
void makePacket(PACKET &in_packet, PACKET &out_packet, bool head_flag);
bool unpackPacket(PACKET &packet, PACKET &out_packet);
string hexdump(PACKET &packet);
bool beginsWith(string str, string match);
void stringToUpper(string &s);
void onTerm(int parm);
void onAlrm(int parm);

bool debug = false;
int ser_s = -1;		// Serial port descriptor
struct termios oldtio;	// Old port settings for serial port
string lockfile = "";
string last_serial_port = "";

int main(int argc, char *argv[])
{
    string ramloader_file = "";
    PACKET packet;
    PACKET recv_packet;
    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin", 1);
    signal(SIGTERM, onTerm);
    signal(SIGINT, onTerm);
    signal(SIGHUP, onTerm);
    signal(SIGABRT, onTerm);
    signal(SIGQUIT, onTerm);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, onAlrm);
    GetOpts opts(argc, argv, "dr:");
    if (opts.exists("r")) {
        ramloader_file = opts.get("r");
    }
    debug = opts.exists("d");
    if (argc < 2) {
        cerr << "Binary file must be provided" << endl;
        exit(1);
    }
    string binary_file = argv[1];
    if (!fileExists(binary_file)) {
        cerr << "Binary file '"  << binary_file << "' does not exist" << endl;
        exit(1);
    }

    openPort(diag_device);
    bool ok = true;
    bool done = false;
    hello();
    uploadBinaryFile(binary_file);
    closeAndExit(0);
    do /* ONCE */ {
        ok = false;
        cout << "Send status query" << endl;
        packet.Data[0] = 0x0C;
        packet.nSize = 1;
        if (!getDataResponse(packet, recv_packet, false, 1, 2)) {
            break;
        }
        cout << "Got Response: " << hexdump(recv_packet) << endl;
        if (recv_packet.Data[0] != 0x0D) {
            if ((recv_packet.Data[0] != 0x0C) and (recv_packet.Data[1] != 0x13)) {
                cout << "Error: Invalid mode" << endl;
                break;
            }
            cout << "Send SW Version Check" << endl;
            packet.Data[0] = 0x7C;
            packet.nSize = 1;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
            }
            cout << "Send Offline Mode" << endl;
            packet.Data[0] = 0x29;
            packet.Data[1] = 0x01;
            packet.Data[2] = 0x00;
            packet.nSize = 3;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
            }
            cout << "Send SPC Code" << endl;
            packet.Data[0] = 0x41;
            packet.Data[1] = 0x37;
            packet.Data[2] = 0x37;
            packet.Data[3] = 0x37;
            packet.Data[4] = 0x37;
            packet.Data[5] = 0x37;
            packet.Data[6] = 0x37;
            packet.nSize = 7;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
            }
            if (!backupNv()) {
                break;
            }
            if (!backupPrl(0)) {
                break;
            }
            cout << "Send Switch Downloader" << endl;
            packet.Data[0] = 0x3A;
            packet.nSize = 1;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
                closePort();
                sleep(1);
                openPort(diag_device);
            }
        }
        if (ramloader_file != "") {
            if (!uploadRamLoader(ramloader_file)) {
                break;
            }
        }
        if (hello()) {
            if (!uploadBinaryFile(binary_file)) {
//                break;
            }
        }
        if (!resetModule()) {
            break;
        }
        if (!restoreNv()) {
//            break;
        }
        if (!restorePrl(0)) {
            break;
        }
//        if (!resetModule()) {
//            break;
//        }
        done = true;
        ok = true;
    } ONCE;

    closeAndExit(0);
    return 0;
}

bool backupNv() {
    PACKET packet;
    PACKET recv_packet;
    FILE *fp = NULL;
    bool ok = true;
    bool done = false;
    do /* ONCE */ {
        ok = false;
        cout << "Send Feature Query" << endl;
        packet.Data[0] = 0x51;
        packet.nSize = 1;
        if (getDataResponse(packet, recv_packet, false, 1, 2)) {
            cout << "Got Response: " << hexdump(recv_packet) << endl;
        }
        cout << "Send Subsys Command 2" << endl;
        packet.Data[0] = 0x80;
        packet.Data[1] = 0x37;
        packet.Data[2] = 0x00;
        packet.Data[3] = 0x00;
        packet.nSize = 4;
        if (getDataResponse(packet, recv_packet, false, 1, 2)) {
            cout << "Got Response: " << hexdump(recv_packet) << endl;
            if (receiveDataPacket(recv_packet, false, 20)) {
                cout << "Got packet: " << hexdump(recv_packet) << endl;
            }
        }
        cout << "Send Sybsys Command(Build CNV file)" << endl;
        packet.Data[0] = 0x4B;
        packet.Data[1] = 0x13;
        packet.Data[2] = 0x02;
        packet.Data[3] = 0x00;
        packet.Data[4] = 0x00;
        packet.Data[5] = 0x00;
        packet.Data[6] = 0x00;
        packet.Data[7] = 0x00;
        packet.Data[8] = 0x24;
        packet.Data[9] = 0x01;
        packet.Data[10] = 0x00;
        packet.Data[11] = 0x00;
        bcopy("telit.bin", &packet.Data[12], 10);
        packet.nSize = 22;
        if (getDataResponse(packet, recv_packet, false, 1, 2)) {
            cout << "Got Response: " << hexdump(recv_packet) << endl;
        }
        sleep(3);
        cout << "Waiting for another packet" << endl;
        if (receiveDataPacket(recv_packet, false, 3)) {
            cout << "Got Packet: " << hexdump(recv_packet) << endl;
        }
        fp = fopen(NV_FILE, "wb");
        if (fp == NULL) {
            cout << "Failed to open NV file for writing" << endl;
            break;
        }
        int size = 516;
        int index = 0;
        while (true) {
            cout << "Send Subsys Command(Backup CNV file)" << endl;
            packet.Data[0] = 0x4B;
            packet.Data[1] = 0x13;
            packet.Data[2] = 0x04;
            packet.Data[3] = 0x00;
            packet.Data[4] = 0x00;
            packet.Data[5] = 0x00;
            packet.Data[6] = 0x00;
            packet.Data[7] = 0x00;
            packet.Data[8] = (Byte)(size & 0xFF);
            packet.Data[9] = (Byte)((size>>8) & 0xFF);
            packet.Data[10] = (Byte)((size>>16) & 0xFF);
            packet.Data[11] = (Byte)((size>>24) & 0xFF);
            packet.Data[12] = (Byte)(index & 0xFF);
            packet.Data[13] = (Byte)((index>>8) & 0xFF);
            packet.Data[14] = (Byte)((index>>16) & 0xFF);
            packet.Data[15] = (Byte)((index>>24) & 0xFF);
            packet.nSize = 16;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
                int got_size = (int)recv_packet.Data[12] |
                           (((int)recv_packet.Data[13]) << 8) |
                           (((int)recv_packet.Data[14]) << 16) |
                           (((int)recv_packet.Data[15]) << 24);
                cout << "Got NV bytes: " << got_size << endl;
                fwrite(&recv_packet.Data[20], sizeof(Byte), got_size, fp);
                if (got_size < size) {
                    fclose(fp);
                    fp = NULL;
                    done = true;
                    ok = true;
                    break;
                }
            }
            index += 516;
        }
        done = true;
    } ONCE;
    if (fp != NULL) {
        fclose(fp);
    }
    return ok;
}

bool restoreNv() {
    PACKET packet;
    PACKET recv_packet;
    FILE *fp = NULL;
    bool ok = true;
    bool done = false;
    do /* ONCE */ {
        ok = false;
        fp = fopen(NV_FILE, "rb");
        if (fp == NULL) {
            cout << "Failed to open NV file for reading" << endl;
            break;
        }
        cout << "Send Subsys Command(Restore CNV file 1)" << endl;
        packet.Data[0] = 0x4B;
        packet.Data[1] = 0x13;
        packet.Data[2] = 0x13;
        packet.Data[3] = 0x00;
        packet.Data[4] = 0x00;
        packet.nSize = 5;
        if (getDataResponse(packet, recv_packet, false, 1, 2)) {
            cout << "Got Response: " << hexdump(recv_packet) << endl;
        }
        cout << "Send Subsys Command(Restore CNV file 2)" << endl;
        packet.Data[0] = 0x4B;
        packet.Data[1] = 0x13;
        packet.Data[2] = 0x02;
        packet.Data[3] = 0x00;
        packet.Data[4] = 0x41;
        packet.Data[5] = 0x02;
        packet.Data[6] = 0x00;
        packet.Data[7] = 0x00;
        packet.Data[8] = 0xB6;
        packet.Data[9] = 0x01;
        packet.Data[10] = 0x00;
        packet.Data[11] = 0x00;
        bcopy("telit.bin", &packet.Data[12], 10);
        packet.nSize = 22;
        if (getDataResponse(packet, recv_packet, false, 1, 2)) {
            cout << "Got Response: " << hexdump(recv_packet) << endl;
        }
        usleep(500000);
        int size = 512;
        int index = 0;
        while (true) {
            int nread = fread(&packet.Data[12], 1, size, fp);
            if (nread <= 0) {
                fclose(fp);
                fp = NULL;
                done = true;
                ok = true;
                break;
            }
            cout << "Send Subsys Command(Restore CNV file 3)" << endl;
            packet.Data[0] = 0x4B;
            packet.Data[1] = 0x13;
            packet.Data[2] = 0x05;
            packet.Data[3] = 0x00;
            packet.Data[4] = 0x00;
            packet.Data[5] = 0x00;
            packet.Data[6] = 0x00;
            packet.Data[7] = 0x00;
            packet.Data[8] = (Byte)(index & 0xFF);
            packet.Data[9] = (Byte)((index>>8) & 0xFF);
            packet.Data[10] = (Byte)((index>>16) & 0xFF);
            packet.Data[11] = (Byte)((index>>24) & 0xFF);
            packet.nSize = 12 + nread;
            index += nread;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
                int uloadSize = (int)recv_packet.Data[12] |
                           (((int)recv_packet.Data[13]) << 8) |
                           (((int)recv_packet.Data[14]) << 16) |
                           (((int)recv_packet.Data[15]) << 24);
                Byte status = recv_packet.Data[16];
                cout << "Module received NV bytes: " << uloadSize << endl;
                if (uloadSize == size) {
                    continue;
                }
//                else if ((uloadSize == 0) and (status == 0x00)) {
//                    done = true;
//                    ok = true;
//                    break;
//                }
//                else {
//                    cout << "CNV Upload Error" << endl;
//                    break;
//                }
                else if ((uloadSize == 0) and (status != 0x00)) {
                    cout << "CNV Upload Error" << endl;
                    break;
                }
            }
        }
        cout << "Send Subsys Command(Restore CNV file 4)" << endl;
        packet.Data[0] = 0x4B;
        packet.Data[1] = 0x13;
        packet.Data[2] = 0x03;
        packet.Data[3] = 0x00;
        packet.Data[4] = 0x00;
        packet.Data[5] = 0x00;
        packet.Data[6] = 0x00;
        packet.Data[7] = 0x00;
        packet.nSize = 8;
        if (getDataResponse(packet, recv_packet, false, 1, 2)) {
            cout << "Got Response: " << hexdump(recv_packet) << endl;
        }
        done = true;
    } ONCE;
    if (fp != NULL) {
        fclose(fp);
    }
    return ok;
}

bool backupPrl(int nam) {
    PACKET packet;
    PACKET recv_packet;
    FILE *fp = NULL;
    bool ok = true;
    bool done = false;
    do /* ONCE */ {
        ok = false;
        int seq_num = 0;
        fp = fopen(PRL_FILE, "wb");
        if (fp == NULL) {
            cout << "Failed to open PRL file for writing" << endl;
            break;
        }
        while (true) {
            cout << "Send PRL Read" << endl;
            packet.Data[0] = 0x49;
            packet.Data[1] = (Byte)seq_num;
            packet.Data[2] = (Byte)nam;
            packet.nSize = 3;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
                if (recv_packet.Data[0] != 0x49) {
                    break;
                }
                int rl_status = recv_packet.Data[1];
                cout << "RL status: " << rl_status << endl;
                if (rl_status == 2) {
                    WORD nv_status = recv_packet.Data[2] | ((int)recv_packet.Data[3]<<8);
                    Byte recv_seq = recv_packet.Data[4];
                    Byte more = recv_packet.Data[5];
                    WORD num_bits = recv_packet.Data[6] | ((int)recv_packet.Data[7] << 8);
                    if (seq_num == 0) {
                        int prl_datasize = ((int)recv_packet.Data[8] << 8) | recv_packet.Data[9];
                        cout << "PRL data size: " << prl_datasize << endl;
                    }
                    int num_bytes = num_bits / 8;
                    cout << "Num Bytes: " << num_bytes << endl;
                    fwrite(&recv_packet.Data[8], sizeof(Byte), num_bytes, fp);
                    if (more == 0) {
                        done = true;
                        ok = true;
                        break;
                    }
                    seq_num += 1;
                }
                else {
                    cout << "PRL Read Error" << endl;
                    break;
                }
            }
        }
        done = true;
    } ONCE;
    if (fp != NULL) {
        fclose(fp);
    }
    return ok;
}

bool restorePrl(int nam) {
    PACKET packet;
    PACKET recv_packet;
    FILE *fp = NULL;
    bool ok = true;
    bool done = false;
    do /* ONCE */ {
        ok = false;
        int seq_num = 0;
        int prl_size = fileSize(PRL_FILE);
        int blk_size = 120;
        int num_blocks = (prl_size - 1)/blk_size + 1;
        fp = fopen(PRL_FILE, "rb");
        if (fp == NULL) {
            cout << "Failed to open PRL file for reading" << endl;
            break;
        }
        for (int seq_num=0; seq_num < num_blocks; seq_num++) {
            int bytes_read = fread(&packet.Data[6], 1, blk_size, fp);
            cout << "Read bytes: " << bytes_read << endl;
            int bits = bytes_read * 8;
            Byte more = 1;
            if ((seq_num+1) == num_blocks) {
                more = 0;
            }
            cout << "Send PRL Write" << endl;
            packet.Data[0] = 0x48;
            packet.Data[1] = (Byte)seq_num;
            packet.Data[2] = more;
            packet.Data[3] = (Byte)nam;
            packet.Data[4] = (Byte)(bits & 0xFF);
            packet.Data[5] = (Byte)((bits >> 8) & 0xFF);
            packet.nSize = 6 + bytes_read;
            if (getDataResponse(packet, recv_packet, false, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
                if (recv_packet.Data[0] != 0x48) {
                    break;
                }
                int rl_status = recv_packet.Data[1];
                cout << "RL status: " << rl_status << endl;
                if ((rl_status == 0) or (rl_status == 1)) {
                    if (!more) {
                        ok = true;
                    }
                    continue;
                }
                else {
                    cout << "PRL Write Error" << endl;
                    break;
                }
            }
            else {
                cout << "PRL Write Error" << endl;
                break;
            }
        }
        done = true;
    } ONCE;
    if (fp != NULL) {
        fclose(fp);
    }
    return ok;
}

bool uploadRamLoader(string filename) {
    PACKET packet;
    PACKET recv_packet;
    int max_record = 0;
    int loader_size = 0;
    int base_addr = 0;
    int start_addr = 0;
    bool ok = false;
    bool done = false;
    ifstream inf(filename.c_str());
    if (!inf.is_open()) {
        cout << "Error: Could not open RAM loader file" << endl;
    }
    else {
        cout << "Parsing RAM loader file: " << filename << endl;
        ok = true;
        int ln = 0;
        int next_addr = 0;
        while (true) {
            ln++;
            string line;
            getline(inf, line);
            if (inf.eof()) break;
            int pos;
            while ((pos=line.find_first_of("\r\n")) != string::npos) {
                line.erase(pos, 1);
            }
            if (line.length() < 11) {
                cout << "Error: Line too short(" << ln << ")" << endl;
                ok = false;
                break;
            }
            if ((line.length()%2) != 1) {
                cout << "Error: Invalid line length(" << ln << ")" << endl;
                ok = false;
                break;
            }
            if (line[0] != ':') {
                cout << "Error: Missing colon(" << ln << ")" << endl;
                ok = false;
                break;
            }
            if (line.find_first_not_of("0123456789abcdefABCDEF",1) != string::npos) {
                cout << "Error: Non-hex digit(" << ln << ")" << endl;
                ok = false;
                break;
            }
            int chksum = 0;
            for (int i=1; i < line.length(); i+=2) {
                int b;
                sscanf(&line.c_str()[i], "%2x", &b);
                chksum += b;
            }
            if ((chksum & 0xFF) != 0) {
                cout << "Error: Invalid checksum(" << ln << ")" << endl;
                ok = false;
                break;
            }
            int len;
            int type;
            int addr;
            sscanf(line.c_str(), ":%2x%4x%2x", &len, &addr, &type);
            if (line.length() != (len*2+11)) {
                cout << "Error: Incorrect line length(" << ln << ")" << endl;
                ok = false;
                break;
            }
//            printf("len:%d, type:%02X, addr:%04X\n", len, type, addr);
            if (type == 0x00) {
                loader_size += len;
                if (len > max_record) {
                    max_record = len;
                    cout << "max_record:" << max_record << endl;
                }
                if ((next_addr != -1) and (next_addr != addr)) {
                    cout << "Warning: Non-contiguaous records(" << ln << ")" << endl;
                }
                next_addr = addr + len;
            }
            else if (type == 0x01) {
                if (len != 0) {
                    cout << "Error: Invalid End Of File record(" << ln << ")" << endl;
                    ok = false;
                    break;
                }
            }
            else if (type == 0x04) {
                if (len != 2) {
                    cout << "Error: Invalid Extended Linear Address record(" << ln << ")" << endl;
                    ok = false;
                    break;
                }
                sscanf(&line.c_str()[9], "%4x", &base_addr);
            }
            else if (type == 0x05) {
                if (len != 4) {
                    cout << "Error: Invalid Start Adress record(" << ln << ")" << endl;
                    ok = false;
                    break;
                }
                sscanf(&line.c_str()[9], "%8x", &start_addr);
            }
            else {
                cout << "Error: Unknown record type(" << ln << ")" << endl;
                ok = false;
                break;
            }
        }
        inf.close();
    }
    int blk_size = 1017;
    Byte buffer[max(max_record,blk_size)+max_record];
    int buf_idx = 0;
    int bytes_in_buf = 0;
    if (ok) {
        ifstream inf(filename.c_str());
        if (inf.is_open()) {
            done = false;
            bool more_data = true;
            bool all_sent = false;
            int offset = 0;
            while (ok and !done) {
                while ((bytes_in_buf < blk_size) and more_data) {
                    string line;
                    getline(inf, line);
                    if (inf.eof()) {
                        more_data = false;
                        cout << "RAM loader file EOF" << endl;
                        break;
                    }
                    int pos;
                    while ((pos=line.find_first_of("\r\n")) != string::npos) {
                        line.erase(pos, 1);
                    }
                    int len;
                    int type;
                    int addr;
                    sscanf(line.c_str(), ":%2x%4x%2x", &len, &addr, &type);
                    if (type == 0x00) {
//                        printf("reading:");
                        for (int i=0; i < len; i++) {
                            int b;
                            sscanf(&line.c_str()[9+i*2], "%2x", &b);
                            buffer[bytes_in_buf++] = (Byte)b;
//                            printf(" %02X", b);
                        }
//                        printf("\n");
                    }
                }
                while (  (  (bytes_in_buf >= blk_size) or
                            (!more_data and (bytes_in_buf > 0)) ) and
                         !all_sent) {
                    int bytes_to_send = min(bytes_in_buf, blk_size);
                    packet.Data[0] = 0x0F;
                    packet.Data[1] = (Byte)((base_addr >> 8) & 0xFF);
                    packet.Data[2] = (Byte)(base_addr & 0xFF);
                    packet.Data[3] = (Byte)((offset >> 8) & 0xFF);
                    packet.Data[4] = (Byte)(offset & 0xFF);
                    packet.Data[5] = (Byte)((bytes_to_send >> 8) & 0xFF);
                    packet.Data[6] = (Byte)(bytes_to_send & 0xFF);
                    bcopy(buffer, &packet.Data[7], bytes_to_send);
                    packet.nSize = 7 + bytes_to_send;
                    if (getDataResponse(packet, recv_packet, true, 1, 2)) {
                        cout << "Got Response: " << hexdump(recv_packet) << endl;
                        if (recv_packet.Data[0] != 0x02) {
                            if (getDataResponse(packet, recv_packet, true, 1,1)) {
                                cout << "Got Response: " << hexdump(recv_packet) << endl;
                                if (recv_packet.Data[0] != 0x02) {
                                    cout << "Upload RAM Loader error" << endl;
                                    ok = false;
                                    break;
                                }
                            }
                        }
                    }
                    else {
                        cout << "Upload RAM Loader error" << endl;
                        ok = false;
                        break;
                    }
                    bytes_in_buf -= bytes_to_send;
                    offset += bytes_to_send;
                    if (bytes_in_buf == 0) {
                        if (!more_data) {
                            all_sent = true;
                            done = true;
                            cout << "RAM loader: all sent" << endl;
                            break;
                        }
                    }
                    for (int i=0; i < bytes_in_buf; i++) {
                        buffer[i] = buffer[bytes_to_send+i];
                    }
                }
            }
            done = true;
        }
    }
    if (ok) {
        sleep(1);
        packet.Data[0] = 0x05;
        packet.Data[1] = (Byte)((base_addr >> 8) & 0xFF);
        packet.Data[2] = (Byte)(base_addr & 0xFF);
        packet.Data[3] = (Byte)((base_addr >> 16) & 0xFF);
        packet.Data[4] = (Byte)((base_addr >> 24) & 0xFF);
        packet.nSize = 5;
//        if (getDataResponse(packet, recv_packet, true, 1, 2)) {
        if (getDataResponse(packet, recv_packet, true, 1, 1)) {
            cout << "Got Response: " << hexdump(recv_packet) << endl;
            if (recv_packet.Data[0] != 0x02) {
                if (getDataResponse(packet, recv_packet, true, 1, 1)) {
                    cout << "Got Response: " << hexdump(recv_packet) << endl;
                    if (recv_packet.Data[0] != 0x02) {
                        cout << "Upload RAM Loader error" << endl;
                        ok = false;
                    }
                }
                else {
                    cout << "Upload RAM Loader error" << endl;
                    ok = false;
                }
            }
        }
        sleep(2);
//        else {
//            cout << "Upload RAM Loader error" << endl;
//            ok = false;
//        }
    }
    return ok;
}

bool hello() {
    bool ok = false;
    PACKET packet;
    PACKET recv_packet;
    cout << "Sending 1st Hello" << endl;
    packet.Data[0] = 0x01;
    bcopy("QCOM fast download protocol host", &packet.Data[1], 32);
    packet.Data[33] = 0x03;
    packet.Data[34] = 0x03;
    packet.Data[35] = 0x09;
    packet.nSize = 36;
    if (getDataResponse(packet, recv_packet, 1, true, 1)) {
        cout << "Got Response to 1st Hello: " << hexdump(recv_packet) << endl;
        sleep(1);
    }
    cout << "Sending 2nd Hello" << endl;
    if (getDataResponse(packet, recv_packet, 1, true, 1)) {
        cout << "Got Response to 2nd Hello: " << hexdump(recv_packet) << endl;
        if (recv_packet.Data[0] == 0x02) {
            ok = true;
        }
    }
    return ok;
}

bool uploadBinaryFile(string binary_file) {
    PACKET packet;
    PACKET recv_packet;
    FILE *fp = NULL;
    bool ok = true;
    bool done = false;
    do /* ONCE */ {
        ok = false;
        int blk_size = 0x400;
        int index = 0;
        fp = fopen(binary_file.c_str(), "rb");
        if (fp == NULL) {
            cout << "Failed to open binary file for reading" << endl;
            break;
        }
        while (true) {
            int nread = fread(&packet.Data[5], 1, blk_size, fp);
            if (nread <= 0) {
                fclose(fp);
                fp = NULL;
                done = true;
                ok = true;
                break;
            }
            cout << "Send Write Binary File" << endl;
            packet.Data[0] = 0x07;
            packet.Data[1] = (Byte)(index & 0xFF);
            packet.Data[2] = (Byte)((index >> 8) & 0xFF);
            packet.Data[3] = (Byte)((index >> 16) & 0xFF);
            packet.Data[4] = (Byte)((index >> 24) & 0xFF);
            packet.nSize = 5 + nread;
            if (getDataResponse(packet, recv_packet, true, 1, 2)) {
                cout << "Got Response: " << hexdump(recv_packet) << endl;
                if (recv_packet.Data[0] != 0x08) {
                    cout << "Binary Write Error" << endl;
                    break;
                }
                else {
                    int write_addr = (int)recv_packet.Data[1] ||
                                     ((int)recv_packet.Data[2] << 8) ||
                                     ((int)recv_packet.Data[3] << 8) ||
                                     ((int)recv_packet.Data[4] << 8);
                    if (write_addr != index) {
                        cout << "Binary Write Error" << endl;
                        break;
                     }
                }
            }
            else {
                cout << "Binary PRL Write Error" << endl;
                break;
            }
            index += nread;
        }
        done = true;
    } ONCE;
    if (fp != NULL) {
        fclose(fp);
    }
    return ok;
}

bool resetModule() {
    bool ok = false;
    PACKET packet;
    PACKET recv_packet;
    cout << "Sending Reset" << endl;
    packet.Data[0] = 0x0B;
    packet.nSize = 1;
    if (getDataResponse(packet, recv_packet, true, 1, 2)) {
        cout << "Got response: " << hexdump(recv_packet) << endl;
        if (recv_packet.Data[0] == 0x0C) {
            cout << "Waiting 12 seconds for module to reset" << endl;
            closePort();
            sleep(12);
            openPort(diag_device);
            ok = true;
        }
    }
    return ok;
}

void closeAndExit(int exitstat) {
    closePort();
    exit(exitstat);
}

void openPort(string port) {
    string dev = "/dev/";
    dev += port;
    if (!lock(port)) {
        cout << "Serial device '" << port << "' already in use" << endl;
        closeAndExit(1);
    }
    ser_s = open(dev.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser_s < 0) {
        perror(diag_device);
        remove(lockfile.c_str());
        closeAndExit(-1);
    }
    last_serial_port = port;
    if (tcgetattr(ser_s,&oldtio)) {
        cerr << "Failed to get old port settings" << endl;
    }

    // set new port settings for canonical input processing 
    struct termios newtio;
    newtio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VMIN]=1;
    newtio.c_cc[VTIME]=1;
    if (tcsetattr(ser_s,TCSANOW,&newtio)) {
        cerr << "Failed to set new port settings" << endl;
    }
    if (tcflush(ser_s, TCIOFLUSH)) {
        cerr << "Failed to flush serial port" << endl;
    }
    if (fcntl(ser_s, F_SETFL, FNDELAY)) {
        cerr << "Failed to set fndelay on serial port" << endl;
    }
}

void closePort() {
    if (ser_s >= 0) {
        // restore old port settings
        tcsetattr(ser_s,TCSANOW,&oldtio);
        tcflush(ser_s, TCIOFLUSH);
        close(ser_s);        //close the com port
        if (lockfile != "") {
            remove(lockfile.c_str());
        }
        ser_s = -1;
    }
}

bool lock(string dev) {
    bool locked = false;
    lockfile = "/var/lock/LCK..";
    lockfile += dev;
    int fd = open(lockfile.c_str(), O_WRONLY | O_CREAT | O_EXCL);
    if (fd >= 0) {
        close(fd);
        locked = true;
    }
    return locked;
}

void killConnection() {
    cout << getDateTime() << " Disabling connections" << endl;
    touchFile(DontConnectFile);
    touchFile(NoCellCommandsFile);
    system("pkill pppd");
    sleep(1);
}

void resumeConnection() {
    cout << getDateTime() << " Re-enabling connections" << endl;
    unlink(NoCellCommandsFile);
    unlink(DontConnectFile);
}

string getAtResponse(string command) {
    return getAtResponse(command, "OK\n", 1);
}

string getAtResponse(string command, int timeout) {
    return getAtResponse(command, "OK\n", timeout);
}

string getAtResponse(string command, string ans, int timeout) {
    bool done = false;
    string outstr = "";
    string cmndstr = command;
    string expresp = command;
    if (expresp.length() >= 2) {
        expresp.erase(0,2);	// Get rid of "AT"
        if ((expresp.length() > 0) and (!isalpha(expresp.at(0)))) {
            expresp.erase(0,1);
        }
        size_t endpos = expresp.find_first_of("=?");
        if (endpos != string::npos) {
            expresp.erase(endpos);
        }
    }
    stringToUpper(expresp);
    cmndstr += "\r";
    time_t start_time = time(NULL);
    alarm(60);
    write(ser_s, cmndstr.c_str(), cmndstr.size());
    char last_ch = '\n';
    string line = "";
    while (!done) {
        fd_set rd, wr, er;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);
        FD_SET(ser_s, &rd);
        nfds = max(nfds, ser_s);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int r = select(nfds+1, &rd, &wr, &er, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (r > 0) {
            if (FD_ISSET(ser_s, &rd)) {
                char buf[255];
                int res = read(ser_s,buf,255);
                int i;
                if (res>0) {
                    for (i=0; i < res; i++) {
                        char ch = buf[i];
                        if (ch == '\r') {
                            ch = '\n';
                        }
                        if ( (ch == '\n') and (ch == last_ch) ) {
                            continue;
                        }
                        last_ch = ch;
                        line += ch;
                        string tmpout = outstr + line;
                        if (tmpout.find(ans) != string::npos) {
                            done = true;
                        }
                        if (ch == '\n') {
                            if ( ( (expresp.length() == 0 ) or
                                   (line.find(expresp) == string::npos) ) and
                                 isUnsolicitedEvent(line) ) {
                                unsolicitedEvent(line);
                            }
                            else {
                                outstr += line;
                            }
                            line = "";
                        }
                    }
                }  //end if res>0
            }
        }
        else {
            if ((time(NULL) - start_time) >= timeout) {
                outstr += line;
                line = "";
                if (debug) {
                    cerr << getDateTime() << " Timeout:" << command << endl;
                    while ((outstr.size() > 0) and (outstr.at(0) == '\n')) {
                         outstr.erase(0,1);
                    }
                    while ((outstr.size() > 0) and (outstr.at(outstr.size()-1) == '\n')) {
                        outstr.erase(outstr.size()-1);
                    }
                    size_t pos;
                    while ((pos=outstr.find("\n\n")) != string::npos) {
                        outstr.erase(pos,1);
                    }
                    cerr << outstr << endl;
                }
                done = true;
            }
        }
    }  //while (!done)
    outstr += line;
    alarm(0);
    while ((outstr.size() > 0) and (outstr.at(0) == '\n')) {
         outstr.erase(0,1);
    }
    while ((outstr.size() > 0) and (outstr.at(outstr.size()-1) == '\n')) {
        outstr.erase(outstr.size()-1);
    }
    size_t pos;
    while ((pos=outstr.find("\n\n")) != string::npos) {
        outstr.erase(pos,1);
    }
    cmndstr = command + "\n";
    if (strncmp(outstr.c_str(),cmndstr.c_str(),cmndstr.size()) == 0) {
        outstr = outstr.substr(cmndstr.size());
    }
//    if (outstr.find("ERROR") != string::npos) {
//        cerr << command << endl;
//    }
    if ((pos=outstr.find("\nOK")) != string::npos) {
        outstr.erase(pos);
    }
//    while ((pos=outstr.find("\n")) != string::npos) {
//        outstr.replace(pos,1,";");
//    }
    return outstr;
}

string getRestOfLine() {
    bool done = false;
    string outstr = "";
    alarm(60);
    while (!done) {
        fd_set rd;
        int nfds = 0;
        FD_ZERO(&rd);
        FD_SET(ser_s, &rd);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        nfds = max(nfds, ser_s);
        int r = select(nfds+1, &rd, NULL, NULL, &tv);
        if ((r == -1) && (errno == EINTR)) {
            continue;
        }
        if (r < 0) {
            perror ("selectt()");
            exit(1);
        }
        if (r > 0) {
            if (FD_ISSET(ser_s, &rd)) {
                char buf[255];
                int res = read(ser_s,buf,255);
                int i;
                if (res>0) {
                    for (i=0; i < res; i++) {
                        char ch = buf[i];
                        if (ch == '\r') {
                            ch = '\n';
                        }
                        if (ch == '\n') {
                            done = true;
                            break;
                        }
                        outstr += ch;
                    }
                }  //end if res>0
            }
        }
        else {
            done = 1;
        }
    }  //while (!done)
    alarm(0);
    return outstr;
}

bool isUnsolicitedEvent(string line) {
    bool retval = false;
    return retval;
}

void unsolicitedEvent(string unsol_line) {
    if (unsol_line[unsol_line.length()-1] == '\n') {
        unsol_line.erase(unsol_line.length()-1);
    }
    string datetime = getDateTime();
    cout << datetime << " " << unsol_line << endl;
}

void sendDataPacket(PACKET &packet, bool head_flag) {
    PACKET send_packet;
    bool ok = false;
    if (ser_s == -1) {
        openPort(last_serial_port);
    }
    if (ser_s != -1) {
        makePacket(packet, send_packet, head_flag);
        cout << "sending: " << hexdump(send_packet) << endl;
        write(ser_s, send_packet.Data, send_packet.nSize);
    }
}

bool getDataResponse(PACKET &packet, PACKET &out_packet, bool head_flag, int timeout, int tries) {
    PACKET send_packet;
    bool ok = false;
    if (ser_s == -1) {
        openPort(last_serial_port);
    }
    if (ser_s != -1) {
        makePacket(packet, send_packet, head_flag);
        while (!ok and (tries > 0)) {
            tries--;
            cout << "sending: " << hexdump(send_packet) << endl;
            write(ser_s, send_packet.Data, send_packet.nSize);
            ok = receiveDataPacket(out_packet, head_flag, timeout);
        }
    }
    return ok;
}

bool receiveDataPacket(PACKET &out_packet, bool t_head_flag, int timeout) {
    PACKET recv_packet;
    recv_packet.nSize = 0;
    bool ok = false;
    bool done = false;
    time_t start_time = time(NULL);
    if (ser_s == -1) {
        openPort(last_serial_port);
    }
    if (ser_s != -1) {
        alarm(60);
        while (!done) {
            fd_set rd, wr, er;
            int nfds = 0;
            FD_ZERO(&rd);
            FD_ZERO(&wr);
            FD_ZERO(&er);
            FD_SET(ser_s, &rd);
            nfds = max(nfds, ser_s);
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            int r = select(nfds+1, &rd, &wr, &er, &tv);
            if ((r == -1) && (errno == EINTR)) {
                continue;
            }
            if (r < 0) {
                perror ("selectt()");
                exit(1);
            }
            if (r > 0) {
                if (FD_ISSET(ser_s, &rd)) {
                    char buf[255];
                    int res = read(ser_s,buf,255);
                    int i;
                    if (res == 0) {
                        cout << "No data received" << endl;
                        closePort();
                        done = true;
                    }
                    if (res>0) {
                        for (i=0; i < res; i++) {
                            char ch = buf[i];
                            if (t_head_flag and (recv_packet.nSize == 0)) {
                                if (ch == 0x7E) {
                                    t_head_flag = false;
                                }
                                continue;
                            }
                            if (ch == 0x7E) {
                                done = true;
                                ok = true;
                                break;
                            }
                            recv_packet.Data[recv_packet.nSize++] = ch;
                        }
                    }
                }
            }
            else {
                if ((time(NULL) - start_time) >= timeout) {
                    if (debug) {
                        cerr << "Timeout received: " << hexdump(recv_packet) << endl;
                    }
                    done = true;
                }
            }
        }
        alarm(0);
        if (ok) {
            if (!unpackPacket(recv_packet, out_packet)) {
               ok = false;
            }
        }
    }
    return ok;
}

void makePacket(PACKET &in_packet, PACKET &out_packet, bool head_flag) {
    unsigned short crc = CRC_16_L_SEED;
    Byte data;
    int j = 0;
    if (head_flag) {
        out_packet.Data[j++] = (Byte)0x7E;
    }
    for (int i=0; i < in_packet.nSize; i++) {
        data = in_packet.Data[i];
        crc = (unsigned short)CRC_16_L_STEP(crc, data);
        if ((data == 0x7E) or (data == 0x7D)) {
            out_packet.Data[j++] = (Byte)0x7D;
            data ^= 0x20;
        }
        out_packet.Data[j++] = data;
    }
    crc = (unsigned short)(~crc);
    data = (Byte)(crc & 0x00FF);
    if ((data == 0x7E) or (data == 0x7D)) {
        out_packet.Data[j++] = (Byte)0x7D;
        data ^= 0x20;
    }
    out_packet.Data[j++] = data;
    data = (Byte)((crc>>8) & 0x00FF);
    if ((data == 0x7E) or (data == 0x7D)) {
        out_packet.Data[j++] = (Byte)0x7D;
        data ^= 0x20;
    }
    out_packet.Data[j++] = data;
    out_packet.Data[j++] = (Byte)0x7E;
    out_packet.nSize = j;
}

bool unpackPacket(PACKET &packet, PACKET &out_packet) {
    bool ok = false;
    int j = 0;
    unsigned short crc = CRC_16_L_SEED;
    for (int i=0; i < packet.nSize; i++) {
        Byte data = packet.Data[i];
        if (data == 0x7D) {
            data = packet.Data[++i] ^ 0x20;
        }
        out_packet.Data[j++] = data;
        crc = (unsigned short)CRC_16_L_STEP(crc, data);
    }
    crc = (unsigned short)(~crc);
    if (crc == CRC_16_L_OK) {
        ok = true;
    }
    out_packet.nSize = j - 2;
    return ok;
}

string hexdump(PACKET &packet) {
    string outstr = "\n";
    int j = 0;
    for (int i=0; i < packet.nSize; i++) {
        outstr += string_printf("%02X", packet.Data[i]);
        if ((i+1) != packet.nSize) {
            if (j == 16) {
                outstr += "\n";
                j = 0;
            }
            else {
                outstr += " ";
                j++;
            }
        }
    }
    return(outstr);
}

bool beginsWith(string str, string match) {
    return (strncmp(str.c_str(), match.c_str(), match.size()) == 0);
}

void stringToUpper(string &s) {
    for (int l=0; l < s.length(); l++) {
       s[l] = toupper(s[l]);
    }
}

void onTerm(int parm) {
    closeAndExit(0);
}

void onAlrm(int parm) {
    // Do nothing for now, just used to break out of blocking reads
    signal(SIGALRM, onAlrm);
}

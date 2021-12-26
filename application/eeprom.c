#include <errno.h>
#include <libgen.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SIZE 0x1000
#define EEPROM_FILE "/tmp/eeprom"
#define LOCK_FILE "/var/lock/LCK..eeprom"

void lock();
void unlock_exit(int exitval);
int check_checksum();
void write_checksum();
unsigned int calc_checksum();
int verify_eeprom();
void read_eeprom();
void open_eeprom();
void close_eeprom();
void read_bytes(unsigned int addr, int len, unsigned char buffer[]);
void write_bytes(unsigned int addr, int len, unsigned char buffer[]);
void open_eeprom();
void close_eeprom();
void read_file();
void write_file();

unsigned char eeprom[SIZE];

void print_usage(char *fn)
{
    printf("Usage: %s [flags] [-w bytes to write]\n", fn);
    printf(" description:\n");
    printf("   reads and writes helios eeprom. max read/write is 32 bytes currently.\n");
    printf("   be aware to not cross 32-byte page boundary when reading/writing.\n");
    printf("\n");
    printf(" flags:\n");
    printf("   -a: address to r/w in hexadecimal (default 0)\n");
    printf("   -c: checks the checksum\n");
    printf("   -d devNum: device number. default 2 (i.e /dev/i2c-2)\n");
    printf("   -h: this message\n");
    printf("   -n numBytes: number of bytes to read (default 32\n");
    printf("   -t: dont write checksum\n");
    printf("   -w: write bytes (default read)\n");
}

char filename[40];
int devAddr = 0x50;		// The I2C address of the eeprom (0xa0)
char *devNum="2";		// dev number (/dev/i2c-x)
int file = 0;

int main(int argc, char **argv)
{
    int i;
    int rwmode = 0;
    int addr = 0;		// eeprom address to r/w
    int numBytes = 32;		// default number of bytes to r/w
    char buf[40];		// buffer for reading/writing
    char *p;
    int check = 0;
    int no_chksum = 0;

    while ((i = getopt(argc, argv, "chtwd:a:n:")) >= 0) {
        switch (i) {
        case 'h':
            print_usage((char *)basename(argv[0]));
            exit(0);
            break;
        case 'd':
            devNum=optarg;
            break;
        case 'a':
            if (sscanf(optarg, "%x", &addr) != 1) {
                fprintf(stderr, "Cannot parse '%s' as hexadecimal address, (i.e: 1024)\n", optarg);
                exit(1);
            }
            if (addr >= SIZE) {
                fprintf(stderr, "Address exceeds size of EEPROM(%X)\n", SIZE);
                exit(1);
            }
            break;
        case 'n':
            if (sscanf(optarg, "%d", &numBytes) != 1) {
                fprintf(stderr, "Cannot parse '%s' as decmimal number, (i.e: 14)\n", optarg);
                exit(1);
            }
            if (numBytes > 32) {
                fprintf(stderr, "Cannot read more than 32 bytes\n");
                exit(1);
            }
            break;
        case 'w':
            rwmode = 1;
            break;
        case 'c':
            check = 1;
            break;
        case 't':
            no_chksum = 1;
            break;
        default:
            print_usage((char *)basename(argv[0]));
            exit(1);
            break;
        }
    }

    sprintf(filename,"/dev/i2c-%s", devNum);

    int just_read = 0;
    struct stat st;
    if (stat(EEPROM_FILE, &st) != 0) {
        read_eeprom();
        write_file();
        just_read = 1;
    }
    else {
        read_file();
    }

    lock();
    if (check) {
        if (!check_checksum(just_read)) {
            printf("Checksum failed\n");
            unlock_exit(1);
        }
        printf("Checksum matched\n");
    }
    else if (rwmode > 0) {

        i = 0;
        int left = argc - optind;
        if (left > 32) {
            fprintf(stderr, "Too many bytes to write\n");
            unlock_exit(1);
        }
        while (p = argv[optind++]) {
            if (sscanf(p, "%x", &buf[i++]) != 1) {
               fprintf(stderr, "Cannot parse '%s' as hex number, (i.e: fe)\n", p);
               unlock_exit(1);
            }
        }
        if ((addr+i) > SIZE) {
            fprintf(stderr, "Write exceeds size of EEPROM(%X)\n", SIZE);
            unlock_exit(1);
        }
        if ((addr & 0xFFE0) != ((addr+i-1) & 0xFFE0)) {
            fprintf(stderr, "Write cannot cross 0x20 boundary\n");
            unlock_exit(1);
        }
        open_eeprom();
        write_bytes(addr, i, buf);
        close_eeprom();
        if (! no_chksum) {
            write_checksum();
        }
        write_file();
    }
    else {
        if ((addr+numBytes) > SIZE) {
            fprintf(stderr, "Read exceeds size of EEPROM(%X)\n", SIZE);
            unlock_exit(1);
        }
        open_eeprom();
        read_bytes(addr, numBytes, buf);
        close_eeprom();
        int ok = 1;
        for (i=0; i < numBytes; i++) {
            if (eeprom[addr+i] != buf[i]) {
                ok = 0;
            }
        }
        if (ok) {
            for (i = 0; i < numBytes; i++)
                printf("%02x ", buf[i]);
            printf("\n");
        }
        else {
            fprintf(stderr, "Error in reading, mismatched value\n");
            unlock_exit(1);
        }
    }
    unlock_exit(0);
}

void lock() {
    int fd = -1;
    while (fd < 0) {
        fd = open(LOCK_FILE, O_WRONLY|O_CREAT|O_EXCL, 0);
        if (fd < 0) {
//            fprintf(stderr, "fd:%d %s\n", fd, strerror(errno)); 
            usleep(100000);
        }
    }
    close(fd);
}

void unlock_exit(int exitval) {
    unlink(LOCK_FILE);
    exit(exitval);
}

int check_checksum(int just_read) {
    int ok = 0;
    unsigned char buffer[2];
    unsigned int chksum = 0;
    unsigned int sv_chksum = 0;
    if (just_read) {
        chksum = calc_checksum();
        sv_chksum = eeprom[SIZE-2] + (eeprom[SIZE-1] << 8);
        if (chksum == sv_chksum) {
            ok = 1;
        }
    }
    else {
        if (verify_eeprom()) {
            chksum = calc_checksum();
            sv_chksum = eeprom[SIZE-2] + (eeprom[SIZE-1] << 8);
            if (chksum == sv_chksum) {
                ok = 1;
            }
        }
    }
    return ok;
}

void write_checksum() {
    unsigned char buffer[2];
    unsigned int chksum = 0;
    open_eeprom();
    chksum = calc_checksum();
    buffer[0] = chksum & 0xFF;
    buffer[1] = chksum >> 8;
    write_bytes(SIZE-2, 2, buffer);
    close_eeprom();
}

unsigned int calc_checksum() {
    unsigned int chksum = 0;
    unsigned char buffer[32];
    unsigned int addr = 0;
    int i, j, n;
    int highbit = 0;
    unsigned end = SIZE - 2;
    for (addr=0; addr < end; addr++) {
        highbit = (chksum >> 15) & 0x0001;
        chksum = ((chksum << 1) & 0xFFFF);
        chksum += highbit + eeprom[addr];
    }
//    printf("Chksum: %04X\n", chksum);
    return chksum;
}

int verify_eeprom() {
    int ok = 1;
    unsigned char buffer[32];
    unsigned int addr = 0;
    int i, j;
    open_eeprom();
    int cnt = SIZE / 32;
    for (i=1; i <= cnt; i++) {
        read_bytes(addr, 32, buffer);
        for (j=0; j < 32; j++) {
            if (eeprom[addr++] != buffer[j]) {
                ok = 0;
            }
        }
    }
    close_eeprom();
    return ok;
}

void read_eeprom() {
    unsigned char buffer[32];
    unsigned int addr = 0;
    int i, j;
    open_eeprom();
    int cnt = SIZE / 32;
    for (i=1; i <= cnt; i++) {
        read_bytes(addr, 32, buffer);
        for (j=0; j < 32; j++) {
            eeprom[addr++] = buffer[j];
        }
    }
    close_eeprom();
}

void open_eeprom() {
    if ((file = open(filename, O_RDWR)) < 0) {
        printf("Failed to open the dev (%s).\n", filename);
        /* ERROR HANDLING; you can check errno to see what went wrong */
        unlock_exit(1);
    }

    if (ioctl(file, I2C_SLAVE, devAddr) < 0) {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        printf("Failed to acquire bus access and/or talk to slave.\n");
        unlock_exit(1);
    }
}

void close_eeprom() {
    close(file);
}

void read_bytes(unsigned int addr, int len, unsigned char buffer[]) {
    unsigned char buf[4];
    buf[0] = addr >> 8;
    buf[1] = addr;
//    printf("write addr:%X, len:%d\n", addr, len);
    if (write(file, buf, 2) != 2) {
        /* ERROR HANDLING: i2c transaction failed */
         printf("Failed to write read address.\n");
         unlock_exit(1);
    }
    if (read(file, buffer, len) != len) {
        /* ERROR HANDLING: i2c transaction failed */
         printf("Failed to read.\n");
         unlock_exit(1);
    }
}

void write_bytes(unsigned int addr, int len, unsigned char buffer[]) {
    unsigned char buf[40];
    int i, j;
    buf[0] = addr >> 8;
    buf[1] = addr;
    j = 2;
    unsigned int a = addr;
    for (i=0; i < len; i++) {
        buf[j] = buffer[i];
        eeprom[a] = buffer[i];
        a++;
        j++;
    }
    // write to eeprom
    if (write(file, buf, j) != j) {
       /* ERROR HANDLING: i2c transaction failed */
       printf("Failed to write.\n");
       unlock_exit(1);
    }
    usleep(10000); // needed delay after a write operation
}

void read_file() {
    FILE *fp = fopen(EEPROM_FILE, "r");
    if (!fp) {
       printf("Could not open /tmp/eeprom\n");
       unlock_exit(1);
    }
    int n = fread(eeprom, 1, SIZE, fp);
    if (n != SIZE) {
        printf("Could not read full eprom\n");
        unlock_exit(1);
    }
    fclose(fp);
}

void write_file() {
    FILE *fp = fopen(EEPROM_FILE, "w");
    fwrite(eeprom, 1, SIZE, fp);
    fclose(fp);
}

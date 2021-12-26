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

void print_usage(char *fn)
{
    printf("Usage: %s [-f] [-c] [-h]\n", fn);
    printf("   -f: output in fahrenheit (default)\n");
    printf("   -c: output in celcius\n");
    printf("   -h: this message\n");
}


int main(int argc, char **argv)
{
    int fahr = 1;
    int i;
    while((i=getopt(argc,argv,"hfc"))>=0){
        switch(i){
        case 'h':
            print_usage((char *)basename(argv[0]));
            exit(0);
            break;
        case 'f':
            fahr = 1;
            break;
        case 'c':
            fahr = 0;
            break;
        default:
            print_usage((char *)basename(argv[0]));
            exit(1);
            break;
        }
    }


    int file;
    char filename[40];

    sprintf(filename,"/dev/i2c-2");
    if ((file = open(filename, O_RDWR)) < 0) {
        printf("Failed to open the bus.");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }

    int addr = 0x48;    // The I2C address of the lm75a (0x90)
    if (ioctl(file, I2C_SLAVE, addr) < 0) {
        printf("Failed to acquire bus access and/or talk to slave.\n");
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }

    char reg = 0x00; /* Device register to access */
    long temp;
    float ftemp;
    char buf[10];

    // write pointer byte 
    buf[0] = reg;
    if (write(file, buf, 1) != 1) {
      /* ERROR HANDLING: i2c transaction failed */
      printf("i2c failure\n");
      exit(1);
    }

    // read temp register, 2 bytes, use upper 11 bits, discard lower 5
    if (read(file, buf, 2) != 2) {
      /* ERROR HANDLING: i2c transaction failed */
      printf("i2c failure\n");
      exit(1);
    } else {
      /* buf[0] contains the MSB read byte */
      temp = (buf[0] << 8) + buf[1];
      temp >>= 5;
      ftemp = temp * .125;
      if (!(temp & 0x400)) {
         // temp is positive
         //  (°C) = +(Temp data) × 0.125 °C
         printf("%.1f\n", (!fahr) ? ftemp : (ftemp*1.8)+32);
      } else {
         // temp is negative 
         //  (°C) = −(2’s complement of Temp data) × 0.125 °C.
         printf("%.1f\n", (!fahr) ? -ftemp : (-ftemp*1.8)+32);
      }
    }

    exit(0);
}

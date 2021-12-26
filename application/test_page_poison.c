#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void hexdump(char *p, int len) {
    int i;
    for (i=0; i < len; i++) {
        printf("%02X ",*p++);
    }
    printf("\n");
}

int main() {
    int size = 1024;
    char *ptr = (char *)malloc(size);
    int len = 40;
    int i;
    int fail = 0;
    printf("l:%p\n", &&l1);
    for (i=0; i < size; i++) {
        if (ptr[i] != 0x0) {
            fail = 1;
            break;
        }
    }
l1:
    if (fail) {
        printf("Block not cleared\n");
    }
    printf("ptr=%p\n", ptr);
    hexdump(ptr, len);
    strcpy(ptr, "Now is the time for all good men to come to the aid of their country");
    hexdump(ptr, len);
    free(ptr);
    hexdump(ptr, len);
    return fail;
}

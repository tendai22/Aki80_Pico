//
// bin2c.c: binary file to C byte array source code
//

#include <stdio.h>

int main(int ac, char **av)
{
    int i = 0, c;
    while ((c = getchar()) != EOF) {
        if (i % 8 == 0)
            printf("/* %04X */", i);
        printf(" 0x%02x,", c);
        if ((i % 8) == 7)
            printf("\n");
        ++i;
    }
}

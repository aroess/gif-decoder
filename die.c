#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

/* exit gracefully and provide meaningful error message */
void die (const char *message) {
    if(errno) {
        perror(message);
    } else {
        printf("ERROR: %s\n", message);
    }
    exit(1);
}

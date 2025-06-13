//C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

//system headers
#include <unistd.h>


int main(int argc, char ** argv) {

    int ret;
    long usec;
    char * error;

    if (argc != 2) {
        printf("Use: usleep <usec>\n");
        return -1;
    }

    usec = atol(argv[1]);
    if (usec == 0) {
        printf("Error: <usec> is 0 or is not a number.\n");
        return -1;
    }

    ret = usleep(usec);
    if (ret != 0) {
        error = strerror(errno);
        printf("Failed to sleep: %s\n", error);
        return -1;
    }

    return 0;
}

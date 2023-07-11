#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <dirent.h>
#include <pthread.h>
#include <ctype.h>

#include "serial.h"


void finish(int s){
    fprintf(stderr, "\nExiting...\n");

    exit(EXIT_SUCCESS);
}

int main (int argc, char *argv[])
{
    int radio_type = RADIO_TYPE_ICOM;

    int serial_fd = open_serial_port("/dev/ttyUSB0");
    set_fixed_baudrate("19200", serial_fd);

    key_on(serial_fd, radio_type);



    return EXIT_SUCCESS;
}

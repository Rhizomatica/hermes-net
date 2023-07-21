#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int bus = open("/dev/i2c-2", O_RDWR);

    int addr = 0x08; /* The I2C address of the ATTiny85 */
    if (ioctl(bus,I2C_SLAVE,addr) < 0)
    {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }

    uint8_t response[32];

    int32_t count;
    count = i2c_smbus_read_i2c_block_data(bus, 0, 4, response);

    printf("count = %d, read = 0x%x 0x%x 0x%x 0x%x\n", count, response[0], response[1], response[2], response[3]);

    close(bus);

}

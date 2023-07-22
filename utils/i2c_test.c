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

    int addr_at = 0x08; /* The I2C address of the ATTiny85 */
    int addr_rtc = 0x68; /* The I2C address of the RTC */

    if (ioctl(bus,I2C_SLAVE,addr_at) < 0)
    {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }
    uint8_t response[32];

    int32_t count;

    i2c_smbus_write_quick(bus, 0);
    count = i2c_smbus_read_word_data(bus, 0);
//count = i2c_smbus_read_i2c_block_data(bus, 0, 4, response);
    printf("count = %d\n", count);

    if (ioctl(bus,I2C_SLAVE,addr_rtc) < 0)
    {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }

    uint8_t values = 0;
    count = i2c_smbus_write_block_data(bus, 0, 1, &values);
    count = i2c_smbus_read_i2c_block_data(bus, 0, 8, response);

    printf("count = %d, read = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", count, response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7]);

    close(bus);

}

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

uint8_t dec2bcd(uint8_t val){
	return ((val/10 * 16) + (val %10));
}

uint8_t bcd2dec(uint8_t val){
	return ((val/16 * 10) + (val %16));
}


int main(int argc, char *argv[])
{
    uint8_t response[32];
    uint8_t values = 0;
    int32_t count;

    int bus = open("/dev/i2c-2", O_RDWR);

    int addr_at = 0x08; /* The I2C address of the ATTiny85 */
    int addr_rtc = 0x68; /* The I2C address of the RTC */
    int addr_si5351 = 0x60; /* The I2C address of the Si5351 */


    // fwd / ref measurements (ATTiny85)
    printf("Reading FWD/REF\n");
    if (ioctl(bus,I2C_SLAVE,addr_at) < 0)
    {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }

    count = i2c_smbus_read_i2c_block_data(bus, 0, 4, response);
    printf("count = %d, read = 0x%x 0x%x 0x%x 0x%x\n", count, response[0], response[1], response[2], response[3]);


// Read from RTC
 	uint8_t rtc_time[10];
    printf("Reading RTC\n");
    if (ioctl(bus,I2C_SLAVE,addr_rtc) < 0)
    {
        /* ERROR HANDLING; you can check errno to see what went wrong */
        exit(1);
    }

    count = i2c_smbus_write_block_data(bus, 0, 1, &values);
    count = i2c_smbus_read_i2c_block_data(bus, 0, 8, response);

    // printf("count = %d, read = 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", count, response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7]);

    if (count <= 0)
        printf("RTC read error detected\n");

    for (int i = 0; i < 7; i++)
		rtc_time[i] = bcd2dec(rtc_time[i]);

	printf("RTC time is : year:%d month:%d day:%d hour:%d min:%d sec:%d\n",
           rtc_time[6] + 2000,
           rtc_time[5], rtc_time[4], rtc_time[2] & 0x3f, rtc_time[1],
           rtc_time[0] & 0x7f);


//convert to julian
	struct tm t;
	time_t gm_now;

	t.tm_year 	= rtc_time[6] + 2000 - 1900;
	t.tm_mon 	= rtc_time[5] - 1;
	t.tm_mday 	= rtc_time[4];
	t.tm_hour 	= rtc_time[2];
	t.tm_min	= rtc_time[1];
	t.tm_sec	= rtc_time[0];


	time_t tjulian = mktime(&t);

	tzname[0] = tzname[1] = "GMT";
	timezone = 0;
	daylight = 0;
	setenv("TZ", "UTC", 1);
	gm_now = mktime(&t);

	int32_t time_delta = difftime(gm_now, time(NULL));
	printf("time_delta = %d\n", time_delta);
	printf("rtc julian: %ld %ld\n", tjulian, time(NULL) - tjulian);

// Write to the RTC
//	rtc_time[0] = dec2bcd(seconds);
//	rtc_time[1] = dec2bcd(minutes);
//	rtc_time[2] = dec2bcd(hours);
//	rtc_time[3] = 0;
//	rtc_time[4] = dec2bcd(day);
//	rtc_time[5] = dec2bcd(month);
//	rtc_time[6] = dec2bcd(year - 2000);
#if 0
	for (uint8_t i = 0; i < 7; i++){
        int e = i2c_smbus_write_byte_data(bus, i, rtc_time[i]);
		if (e)
			printf("rtc_write: error writing ds1307 register at %d index\n", i);
	}
#endif

    //
    //i2c_smbus_write_byte_data(int file, __u8 command, __u8 value);

    close(bus);

}

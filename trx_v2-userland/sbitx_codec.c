/* sbitx_codec - see sbitx_codec.h.
 *
 * Copyright (C) 2026 Rhizomatica
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "sbitx_codec.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <stdio.h>
#include "sbitx_alsa.h"


/* Where the kernel lists I2C devices, as <bus>-<4-digit hex address>. */
#define I2C_SYSFS_DEVICES   "/sys/bus/i2c/devices"
#define CODEC_DRIVER_NAME   "wm8731"

/* WM8731 register 9: the digital audio interface, bit 0 ACTIVE. */
#define WM8731_REG_ACTIVE   9
#define WM8731_INACTIVE     0x000
#define WM8731_ACTIVE       0x001

/* How long the interface stays inactive: what cured a deaf codec on the
 * bench (2026-11-18). */
#define CODEC_INACTIVE_MS   200

/* How far the microphone's DC must lead the receiver's to call the order
 * (bench: 20 dB apart, either way). */
#define CODEC_ORDER_MARGIN_DB 10.0

/* Reactivations tried before giving up: each lands in the right order
 * about one time in four, so 40 fail together about once in 10^5. */
#define CODEC_REACTIVATE_MAX  40

/* How long one DC probe may take (its settle and window are in sbitx_alsa.c). */
#define CODEC_PROBE_TIMEOUT_MS 2000

bool sbitx_codec_find(const char *devices_dir, int *bus, int *addr)
{
    DIR *d = opendir(devices_dir);
    if (!d)
        return false;

    bool found = false;
    struct dirent *e;
    while (!found && (e = readdir(d)) != NULL)
    {
        int b, a;
        char tail;
        /* "1-001a", not the "i2c-1" bus entries */
        if (sscanf(e->d_name, "%d-%x%c", &b, &a, &tail) != 2)
            continue;
        char path[512], name[64] = "";
        snprintf(path, sizeof(path), "%s/%s/name", devices_dir, e->d_name);
        FILE *f = fopen(path, "r");
        if (!f)
            continue;
        if (fgets(name, sizeof(name), f))
            name[strcspn(name, "\n")] = '\0';
        fclose(f);
        if (strcmp(name, CODEC_DRIVER_NAME) == 0)
        {
            *bus = b;
            *addr = a;
            found = true;
        }
    }
    closedir(d);
    return found;
}

void sbitx_codec_encode(int reg, unsigned value, uint8_t out[2])
{
    out[0] = (uint8_t) ((reg << 1) | ((value >> 8) & 1));
    out[1] = (uint8_t) (value & 0xff);
}

/* One register write in one transfer (I2C_RDWR also reaches an address its
 * driver holds). */
static int codec_write(int fd, int addr, int reg, unsigned value)
{
    uint8_t buf[2];
    sbitx_codec_encode(reg, value, buf);
    struct i2c_msg msg = { .addr = (uint16_t) addr, .flags = 0, .len = sizeof(buf), .buf = buf };
    struct i2c_rdwr_ioctl_data data = { .msgs = &msg, .nmsgs = 1 };
    return ioctl(fd, I2C_RDWR, &data) < 0 ? -errno : 0;
}

codec_order sbitx_codec_order(double left_dc_dbfs, double right_dc_dbfs)
{
    if (right_dc_dbfs - left_dc_dbfs >= CODEC_ORDER_MARGIN_DB)
        return CODEC_ORDER_RIGHT;
    if (left_dc_dbfs - right_dc_dbfs >= CODEC_ORDER_MARGIN_DB)
        return CODEC_ORDER_SWAPPED;
    return CODEC_ORDER_UNKNOWN;
}

/* ACTIVE cleared, CODEC_INACTIVE_MS, set again.  0, or a negative errno. */
static int codec_cycle(int fd, int addr)
{
    int e = codec_write(fd, addr, WM8731_REG_ACTIVE, WM8731_INACTIVE);
    if (e == 0)
    {
        struct timespec ts = { CODEC_INACTIVE_MS / 1000, (CODEC_INACTIVE_MS % 1000) * 1000000L };
        nanosleep(&ts, NULL);
        e = codec_write(fd, addr, WM8731_REG_ACTIVE, WM8731_ACTIVE);
    }
    return e;
}

static codec_order probe_order(double *l, double *r)
{
    if (!sound_codec_probe_dc(l, r, CODEC_PROBE_TIMEOUT_MS))
        return CODEC_ORDER_UNKNOWN;
    return sbitx_codec_order(*l, *r);
}

bool sbitx_codec_reactivate(void)
{
    int bus, addr;
    if (!sbitx_codec_find(I2C_SYSFS_DEVICES, &bus, &addr))
    {
        fprintf(stderr, "codec: no %s under %s: the codec's interface is not reactivated\n",
              CODEC_DRIVER_NAME, I2C_SYSFS_DEVICES);
        return false;
    }

    /* The order must be readable before anything is cycled: a cycle that
     * cannot be checked lands swapped more often than not. */
    double l, r;
    if (probe_order(&l, &r) == CODEC_ORDER_UNKNOWN)
    {
        fprintf(stderr, "codec: %s at %d-%04x: channel order not readable from the DC (receiver %.1f, "
              "microphone %.1f dBFS): the interface is not reactivated\n", CODEC_DRIVER_NAME, bus, addr, l, r);
        return false;
    }

    char dev[32];
    snprintf(dev, sizeof(dev), "/dev/i2c-%d", bus);
    int fd = open(dev, O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "codec: cannot open %s (%s): the codec's interface is not reactivated\n", dev, strerror(errno));
        return false;
    }

    int swapped = 0, unreadable = 0;
    for (int n = 1; n <= CODEC_REACTIVATE_MAX; n++)
    {
        int e = codec_cycle(fd, addr);
        if (e < 0)
        {
            close(fd);
            fprintf(stderr, "codec: %s at %d-%04x: writing ACTIVE failed (%s); the receiver may stay deaf until a restart\n",
                  CODEC_DRIVER_NAME, bus, addr, strerror(-e));
            return false;
        }
        codec_order o = probe_order(&l, &r);
        if (o == CODEC_ORDER_RIGHT)
        {
            close(fd);
            fprintf(stderr, "codec: %s at %d-%04x: interface reactivated, channels in order after %d cycle%s "
                  "(%d swapped, %d unreadable; receiver DC %.1f, microphone %.1f dBFS)\n",
                  CODEC_DRIVER_NAME, bus, addr, n, n == 1 ? "" : "s", swapped, unreadable, l, r);
            return true;
        }
        if (o == CODEC_ORDER_SWAPPED)
            swapped++;
        else
            unreadable++;
    }
    close(fd);
    fprintf(stderr, "codec: %s at %d-%04x: channels not in order after %d reactivations (%d swapped, %d unreadable); "
          "the receiver may be deaf until a restart\n", CODEC_DRIVER_NAME, bus, addr, CODEC_REACTIVATE_MAX,
          swapped, unreadable);
    return false;
}

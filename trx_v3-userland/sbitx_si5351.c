/* sBitx core
 * Copyright (C) 2017-2023 Jerry Gaffke, KE7ER
 *
 * GPLv3 code from Jerry Gaffe, "An minimalist standalone set of Si5351
 * routines", later modified by Ashhar Farhan and Rafael Diniz.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <linux/types.h>
#include <stdint.h>

#include "sbitx_i2c.h"
#include "sbitx_si5351.h"
#include "sbitx_core.h"

#define SI_CLK0_CONTROL  16      // Register definitions
#define SI_CLK1_CONTROL 17
#define SI_CLK2_CONTROL 18
#define SI_SYNTH_PLL_A  26
#define SI_SYNTH_PLL_B  34
#define SI_SYNTH_MS_0   42
#define SI_SYNTH_MS_1   50
#define SI_SYNTH_MS_2   58
#define SI_PLL_RESET    177

#define SI_R_DIV_1    0b00000000      // R-division ratio definitions
#define SI_R_DIV_2    0b00010000
#define SI_R_DIV_4    0b00100000
#define SI_R_DIV_8    0b00110000
#define SI_R_DIV_16   0b01000000
#define SI_R_DIV_32   0b01010000
#define SI_R_DIV_64   0b01100000
#define SI_R_DIV_128    0b01110000

#define SI_CLK_SRC_PLL_A  0b00000000
#define SI_CLK_SRC_PLL_B  0b00100000

#define SI5351_CLK_PLL_SELECT_B (1<<5) 
#define SI5351_CLK_INTEGER_MODE (1<<6)   
#define SI5351_CLK_INPUT_MULTISYNTH_N  (3<<2)

#define SI5351_CLK_DRIVE_STRENGTH_MASK    (3<<0)
#define SI5351_CLK_DRIVE_STRENGTH_2MA   (0<<0)
#define SI5351_CLK_DRIVE_STRENGTH_4MA   (1<<0)
#define SI5351_CLK_DRIVE_STRENGTH_6MA   (2<<0)
#define SI5351_CLK_DRIVE_STRENGTH_8MA   (3<<0)

#define PLL_N 35
#define PLLFREQ (xtal_freq_calibrated * PLL_N)
//int xtal_freq_calibrated = 25012725; // crystal oscillator 
int xtal_freq_calibrated = 25000000; // tcxo

uint32_t plla_freq, pllb_freq;

// global radio handle pointer used for simplifying the function prototypes
radio *internal_radio_h;

void setup_oscillators(radio *radio_h)
{
    internal_radio_h = radio_h;

    //initialize the SI5351
    usleep(200000);
    si5351bx_init();
    usleep(200000);
    si5351bx_setfreq(1, radio_h->bfo_frequency);

    si5351_reset();
}

void si5351_reset()
{
    i2c_write_si5351(internal_radio_h, SI_PLL_RESET, 0xA0);
}

void si5351a_clkoff(uint8_t clk)
{
    i2c_write_si5351(internal_radio_h, clk, 0x80);   // Refer to SiLabs AN619 to see bit values - 0x80 turns off the output stage
}

/*
  Follow the AN619 application note for the Si5351
  a = mult
  b = num
  c = denom
*/

static void setup_pll(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
    uint32_t P1;          // PLL config register P1
    uint32_t P2;          // PLL config register P2
    uint32_t P3;          // PLL config register P3

    if (num == 0)
    {
        P1 = 128 * mult - 512;
        P2 = 0;
        P3 = 1;
    }
    else
    {
        P1 = (uint32_t)(128 * ((float)num / (float)denom));
        P1 = (uint32_t)(128 * (uint32_t)(mult) + P1 - 512);
        P2 = (uint32_t)(128 * ((float)num / (float)denom));
        P2 = (uint32_t)(128 * num - denom * P2);
        P3 = denom;
    }
    i2c_write_si5351(internal_radio_h, pll + 0, (P3 & 0x0000FF00) >> 8);
    i2c_write_si5351(internal_radio_h, pll + 1, (P3 & 0x000000FF));
    i2c_write_si5351(internal_radio_h, pll + 2, (P1 & 0x00030000) >> 16);
    i2c_write_si5351(internal_radio_h, pll + 3, (P1 & 0x0000FF00) >> 8);
    i2c_write_si5351(internal_radio_h, pll + 4, (P1 & 0x000000FF));
    i2c_write_si5351(internal_radio_h, pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
    i2c_write_si5351(internal_radio_h, pll + 6, (P2 & 0x0000FF00) >> 8);
    i2c_write_si5351(internal_radio_h, pll + 7, (P2 & 0x000000FF));
}

static void setup_multisynth(uint8_t clk, uint8_t pllSource, uint32_t divider,  uint32_t num, uint32_t denom, uint32_t rdiv,  uint8_t drive_strength)
{
    uint8_t synth, control;

    switch (clk){
    case 0:
        synth = 42;
        control = 16;
        break;
    case 1:
        synth = 50;
        control =17;
        break;
        //clock 2
    default:
        synth = 58;
        control = 18;
        break;
    }

    uint8_t dat;

    uint32_t P1;
    uint32_t P2;
    uint32_t P3;
    uint32_t div4 = 0;


    /* Output Multisynth Divider Equations
     * where: a = div, b = num and c = denom
     * P1 register is an 18-bit value using following formula:
     *  P1[17:0] = 128 * a + floor(128*(b/c)) - 512
     * P2 register is a 20-bit value using the following formula:
     *  P2[19:0] = 128 * b - c * floor(128*(b/c))
     * P3 register is a 20-bit value using the following formula:
     *  P3[19:0] = c
     */
    /* Set the main PLL config registers */
#define SI5351_DIVBY4     (3<<2)
  
    if (divider == 4)
    {
        div4 = SI5351_DIVBY4;
        P1 = P2 = 0;
        P3 = 1;
    }
    else if (num == 0)
    {
        /* Integer mode */
        P1 = 128 * divider - 512;
        P2 = 0;
        P3 = 1;
    }
    else
    {
        /* Fractional mode */
        P1 = 128 * divider + ((128 * num) / denom) - 512;
        P2 = 128 * num - denom * ((128 * num) / denom);
        P3 = denom;
    }

    i2c_write_si5351(internal_radio_h, synth + 0,   (P3 & 0x0000FF00) >> 8);
    i2c_write_si5351(internal_radio_h, synth + 1,   (P3 & 0x000000FF));
    i2c_write_si5351(internal_radio_h, synth + 2,   ((P1 & 0x00030000) >> 16) | div4 | rdiv);
    i2c_write_si5351(internal_radio_h, synth + 3,   (P1 & 0x0000FF00) >> 8);
    i2c_write_si5351(internal_radio_h, synth + 4,   (P1 & 0x000000FF));
    i2c_write_si5351(internal_radio_h, synth + 5,   ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
    i2c_write_si5351(internal_radio_h, synth + 6,   (P2 & 0x0000FF00) >> 8);
    i2c_write_si5351(internal_radio_h, synth + 7,   (P2 & 0x000000FF));

/* clock control register
 *  |    7    |    6    |    5    |    4    |   3    |  2   |   1   |  0   |
 *   pwr off?   ms_int?   pll b?    o/p inv? <--  source --> <-- drive ---->
 */

    /* Configure the clk control and enable the output */
    dat = drive_strength | SI5351_CLK_INPUT_MULTISYNTH_N;
    if (pllSource == SI_SYNTH_PLL_B)
        dat |= SI_CLK_SRC_PLL_B;
    if (num == 0)
        dat |= SI5351_CLK_INTEGER_MODE;
    i2c_write_si5351(internal_radio_h, control, dat);
}


static void set_freq_fixeddiv(int clk, int pll, uint32_t frequency, int divider,  uint8_t drive_strength){
    int32_t denom = 0x80000;
    int32_t pllfreq = frequency * divider;
    int32_t multi = pllfreq / xtal_freq_calibrated;
    int32_t num = ((uint64_t)(pllfreq % xtal_freq_calibrated) * 0x80000)/xtal_freq_calibrated;

    //printf("fixeddiv clk:%d, freq:%d, pll:%d, pllfreq:%d, div: %d\n", clk, frequency, pll, pllfreq, divider);
    setup_pll(pll, multi, num, denom);
    setup_multisynth(clk, pll, divider, 0, 1, SI_R_DIV_1, drive_strength);
}


void si5351bx_setfreq(uint8_t clk, uint32_t frequency){
    int pll;

    if (clk == 1)
        pll = SI_SYNTH_PLL_B;
    else
        pll = SI_SYNTH_PLL_A;

    int pll_div = 650000000l / frequency;

    //round to the next even integer
    if (pll_div * 650000000l != frequency)
        pll_div++;
 
    if (pll_div & 1)
        pll_div++;

    // TODO: parametrize drive power?
    set_freq_fixeddiv(clk, pll, frequency, pll_div,
                      SI5351_CLK_DRIVE_STRENGTH_8MA);
}


void si5351_set_calibration(int32_t cal)
{
    xtal_freq_calibrated = cal;
}

void si5351bx_init()
{
    si5351_reset();
    usleep(10000);
    si5351a_clkoff(SI_CLK0_CONTROL);
    si5351a_clkoff(SI_CLK1_CONTROL);
    si5351a_clkoff(SI_CLK2_CONTROL);
}

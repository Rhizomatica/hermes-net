/*
The initial sync between the gui values, the core radio values, settings, et al are manually set.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <math.h>
#include <fcntl.h>
#include <complex.h>
#include <fftw3.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <ncurses.h>
#include <glib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cairo.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/file.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include "sdr.h"
#include "sound.h"
#include "sdr_ui.h"
#include "ini.h"
#include "i2cbb.h"
#include "webserver.h"
#include "sbitx_controller.h"
#include "../include/radio_cmds.h"

extern controller_conn *tmp_connector;

/* Front Panel controls */
char pins[15] = {0, 2, 3, 6, 7, 
								10, 11, 12, 13, 14, 
								21, 22, 23, 25, 27};

#define ENC1_A (13)
#define ENC1_B (12)
#define ENC1_SW (14)

#define ENC2_A (0)
#define ENC2_B (2)
#define ENC2_SW (3)

#define SW5 (22)
#define PTT (7)
#define DASH (21)

#define ENC_FAST 1
#define ENC_SLOW 5

#define DS3231_I2C_ADD 0x68
//time sync, when the NTP time is not synced, this tracks the number of seconds 
//between the system cloc and the actual time set by \utc command
static long time_delta = 0;

//encoder state
struct encoder {
	int pin_a,  pin_b;
	int speed;
	int prev_state;
	int history;
};
void tuning_isr(void);


struct encoder enc_a, enc_b;

#define MAX_FIELD_LENGTH 128

#define FIELD_NUMBER 0
#define FIELD_BUTTON 1
#define FIELD_TOGGLE 2
#define FIELD_SELECTION 3
#define FIELD_TEXT 4
#define FIELD_STATIC 5
#define FIELD_CONSOLE 6

struct Queue q_web;

// event ids, some of them are mapped from gtk itself
#define FIELD_DRAW 0
#define FIELD_UPDATE 1 
#define FIELD_EDIT 2
#define MIN_KEY_UP 0xFF52
#define MIN_KEY_DOWN	0xFF54
#define MIN_KEY_LEFT 0xFF51
#define MIN_KEY_RIGHT 0xFF53
#define MIN_KEY_ENTER 0xFF0D
#define MIN_KEY_ESC	0xFF1B
#define MIN_KEY_BACKSPACE 0xFF08
#define MIN_KEY_TAB 0xFF09
#define MIN_KEY_CONTROL 0xFFE3
#define MIN_KEY_F1 0xFFBE
#define MIN_KEY_F2 0xFFBF
#define MIN_KEY_F3 0xFFC0
#define MIN_KEY_F4 0xFFC1
#define MIN_KEY_F5 0xFFC2
#define MIN_KEY_F6 0xFFC3
#define MIN_KEY_F7 0xFFC4
#define MIN_KEY_F8 0xFFC5
#define MIN_KEY_F9 0xFFC6
#define MIN_KEY_F9 0xFFC6
#define MIN_KEY_F10 0xFFC7
#define MIN_KEY_F11 0xFFC8
#define MIN_KEY_F12 0xFFC9
#define COMMAND_ESCAPE '\\'

void set_ui(int id);
void set_bandwidth(int hz);

struct field *active_layout = NULL;


struct field {
	char	*cmd;
	int		(*fn)(struct field *f, cairo_t *gfx, int event, int param_a, int param_b, int param_c);
	int		x, y, width, height;
	char	label[30];
	int 	label_width;
	char	value[MAX_FIELD_LENGTH];
	char	value_type; //NUMBER, SELECTION, TEXT, TOGGLE, BUTTON
	int 	font_index; //refers to font_style table
	char  selection[1000];
	long int	 	min, max;
  int step;
	char is_dirty;
	char update_remote;
};

#define STACK_DEPTH 4

struct band {
	char name[10];
	int	start;
	int	stop;
	//int	power;
	//int	max;
	int index;
	int	freq[STACK_DEPTH];
	int mode[STACK_DEPTH];
};

struct cmd {
	char *cmd;
	int (*fn)(char *args[]);
};


//variables to power up and down the tx

static int in_tx = TX_OFF;
static int key_down = 0;
static int tx_start_time = 0;

static int *tx_mod_buff = NULL;
static int tx_mod_index = 0;
static int tx_mod_max = 0;

// high swr protection
bool is_swr_protect_enabled = false;
bool connected_status = false;
bool led_status = false;
uint32_t serial_number = 0;
uint16_t reflected_threshold = 25; // vswr * 10
uint32_t radio_operation_result = 0;

char*mode_name[MAX_MODES] = {
	"USB", "LSB", "CW", "CWR", "NBFM", "AM", "FT8", "PSK31", "RTTY", 
	"DIGITAL", "2TONE" 
};

static int serial_fd = -1;
static int xit = 512; 
static long int tuning_step = 1000;
static int tx_mode = MODE_USB;


#define BAND80M	0
#define BAND40M	1
#define BAND30M 2	
#define BAND20M 3	
#define BAND17M 4	
#define BAND15M 5
#define BAND12M 6 
#define BAND10M 7 

struct band band_stack[] = {
	{"80m", 3500000, 4000000, 0, 
		{3500000,3574000,3600000,3700000},{MODE_CW, MODE_USB, MODE_CW,MODE_LSB}},
	{"40m", 7000000,7300000, 0,
		{7000000,7040000,7074000,7150000},{MODE_CW, MODE_CW, MODE_USB, MODE_LSB}},
	{"30m", 10100000, 10150000, 0,
		{10100000, 10100000, 10136000, 10150000}, {MODE_CW, MODE_CW, MODE_USB, MODE_USB}},
	{"20m", 14000000, 14400000, 0,
		{14010000, 14040000, 14074000, 14200000}, {MODE_CW, MODE_CW, MODE_USB, MODE_USB}},
	{"17m", 18068000, 18168000, 0,
		{18068000, 18100000, 18110000, 18160000}, {MODE_CW, MODE_CW, MODE_USB, MODE_USB}},
	{"15m", 21000000, 21500000, 0,
		{21010000, 21040000, 21074000, 21250000}, {MODE_CW, MODE_CW, MODE_USB, MODE_USB}},
	{"12m", 24890000, 24990000, 0,
		{24890000, 24910000, 24950000, 24990000}, {MODE_CW, MODE_CW, MODE_USB, MODE_USB}},
	{"10m", 28000000, 29700000, 0,
		{28000000, 28040000, 28074000, 28250000}, {MODE_CW, MODE_CW, MODE_USB, MODE_USB}},
};


#define VFO_A 0 
#define VFO_B 1 
//int	vfo_a_freq = 7000000;
//int	vfo_b_freq = 14000000;
char vfo_a_mode[10];
char vfo_b_mode[10];

//usefull data for macros, logging, etc
char contact_callsign[12];
char contact_grid[10];
char sent_rst[10];
char received_rst[10];
char received_exchange[10];

int	tx_id = 0;

//recording duration in seconds
time_t record_start = 0;
int	data_delay = 700;

#define MAX_RIT 25000

extern uint16_t fwdpower, vswr;
extern int bfo_freq;
extern int frequency_offset;

extern void read_hw_ini();
extern void save_hw_settings();


char settings_updated = 0;

void do_cmd(char *cmd);
void cmd_exec(char *cmd);

gboolean ui_tick(gpointer gook);

// the cmd fields that have '#' are not to be sent to the sdr
struct field main_controls[] = {
	{ "r1:freq", NULL, 600, 0, 150, 49, "FREQ", 5, "14000000", FIELD_NUMBER, FONT_LARGE_VALUE,
		"", 500000, 30000000, 100},

	// Main RX
	{ "r1:volume", NULL, 750, 330, 50, 50, "AUDIO", 40, "60", FIELD_NUMBER, FONT_FIELD_VALUE, 
		"", 0, 100, 1},
	{ "r1:mode", NULL, 500, 330, 50, 50, "MODE", 40, "USB", FIELD_SELECTION, FONT_FIELD_VALUE, 
		"USB/LSB/CW/CWR/FT8/PSK31/RTTY/DIGITAL/2TONE", 0,0, 0},
	{ "r1:low", NULL, 550, 330, 50, 50, "LOW", 40, "0", FIELD_NUMBER, FONT_FIELD_VALUE,
		"", 0,4000, 50},
	{ "r1:high", NULL, 600, 330, 50, 50, "HIGH", 40, "3000", FIELD_NUMBER, FONT_FIELD_VALUE, 
		"", 300, 10000, 50},

	{ "r1:agc", NULL, 650, 330, 50, 50, "AGC", 40, "OFF", FIELD_SELECTION, FONT_FIELD_VALUE,
		"OFF/SLOW/MED/FAST", 0, 1024, 1},
	{ "r1:gain", NULL, 700, 330, 50, 50, "IF", 40, "60", FIELD_NUMBER, FONT_FIELD_VALUE, 
		"", 0, 100, 1},

	//tx 
	{ "tx_power", NULL, 550, 430, 50, 50, "DRIVE", 40, "40", FIELD_NUMBER, FONT_FIELD_VALUE, 
		"", 1, 100, 1},
	{ "tx_gain", NULL, 550, 380, 50, 50, "MIC", 40, "50", FIELD_NUMBER, FONT_FIELD_VALUE, 
		"", 0, 100, 1},

	{ "#split", NULL, 750, 380, 50, 50, "SPLIT", 40, "OFF", FIELD_TOGGLE, FONT_FIELD_VALUE, 
		"ON/OFF", 0,0,0},
	{ "tx_compress", NULL, 600, 380, 50, 50, "COMP", 40, "0", FIELD_NUMBER, FONT_FIELD_VALUE, 
		"ON/OFF", 0,100,10},
	{"#rit", NULL, 550, 0, 50, 50, "RIT", 40, "OFF", FIELD_TOGGLE, FONT_FIELD_VALUE, 
		"ON/OFF", 0,0,0},
	{ "#tx_wpm", NULL, 650, 380, 50, 50, "WPM", 40, "12", FIELD_NUMBER, FONT_FIELD_VALUE, 
		"", 1, 50, 1},
	{ "rx_pitch", NULL, 700, 380, 50, 50, "PITCH", 40, "600", FIELD_NUMBER, FONT_FIELD_VALUE,
		"", 100, 3000, 10},
	
	{ "#web", NULL, 600, 430, 50, 50, "WEB", 40, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0, 0},

	{ "#tx", NULL, 1000, -1000, 50, 50, "TX", 40, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"RX/TX", 0,0, 0},

	{ "#rx", NULL, 650, 430, 50, 50, "RX", 40, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"RX/TX", 0,0, 0},
	
	{ "#record", NULL, 700, 430, 50, 50, "REC", 40, "OFF", FIELD_TOGGLE, FONT_FIELD_VALUE,
		"ON/OFF", 0,0, 0},

	// top row
	{"#step", NULL, 400, 0 ,50, 50, "STEP", 1, "10Hz", FIELD_SELECTION, FONT_FIELD_VALUE, 
		"1M/100K/10K/1K/100H/10H", 0,0,0},
	{"#vfo", NULL, 450, 0 ,50, 50, "VFO", 1, "A", FIELD_SELECTION, FONT_FIELD_VALUE, 
		"A/B", 0,0,0},
	{"#span", NULL, 500, 0 ,50, 50, "SPAN", 1, "25K", FIELD_SELECTION, FONT_FIELD_VALUE, 
		"25K/10K/6K/2.5K", 0,0,0},

	{"#status", NULL, 400, 51, 400, 29, "STATUS", 70, "7000 KHz", FIELD_STATIC, FONT_SMALL,
		"status", 0,0,0},  
	{"#console", NULL, 0, 0 , 400, 320, "CONSOLE", 70, "console box", FIELD_CONSOLE, FONT_LOG,
		"nothing valuable", 0,0,0},
	{"#log_ed", NULL, 0, 320, 400, 20, "", 70, "", FIELD_STATIC, FONT_LOG, 
		"nothing valuable", 0,128,0},
	{"#text_in", NULL, 0, 340, 398, 20, "TEXT", 70, "text box", FIELD_TEXT, FONT_LOG,
		"nothing valuable", 0,128,0},



	{"#close", NULL, 750, 430 ,50, 50, "_", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#off", NULL, 750, 0 ,50, 50, "x", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
  
  // other settings - currently off screen
  { "reverse_scrolling", NULL, 1000, -1000, 50, 50, "RS", 40, "ON", FIELD_TOGGLE, FONT_FIELD_VALUE,
    "ON/OFF", 0,0,0},
  { "tuning_acceleration", NULL, 1000, -1000, 50, 50, "TA", 40, "ON", FIELD_TOGGLE, FONT_FIELD_VALUE,
    "ON/OFF", 0,0,0},
  { "tuning_accel_thresh1", NULL, 1000, -1000, 50, 50, "TAT1", 40, "10000", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 100,99999,100},
  { "tuning_accel_thresh2", NULL, 1000, -1000, 50, 50, "TAT2", 40, "500", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 100,99999,100},
  { "mouse_pointer", NULL, 1000, -1000, 50, 50, "MP", 40, "LEFT", FIELD_SELECTION, FONT_FIELD_VALUE,
    "BLANK/LEFT/RIGHT/CROSSHAIR", 0,0,0},


	//moving global variables into fields 	
  { "#vfo_a_freq", NULL, 1000, -1000, 50, 50, "VFOA", 40, "14000000", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 500000,30000000,1},
  {"#vfo_b_freq", NULL, 1000, -1000, 50, 50, "VFOB", 40, "7000000", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 500000,30000000,1},
  {"#rit_delta", NULL, 1000, -1000, 50, 50, "RIT_DELTA", 40, "000000", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", -25000,25000,1},
	{"#mycallsign", NULL, 1000, -1000, 400, 149, "MYCALLSIGN", 70, "NOBODY", FIELD_TEXT, FONT_SMALL, 
		"", 3,10,1},
	{"#mygrid", NULL, 1000, -1000, 400, 149, "MYGRID", 70, "NOWHERE", FIELD_TEXT, FONT_SMALL, 
		"", 4,6,1},
  { "#cwinput", NULL, 1000, -1000, 50, 50, "CW_INPUT", 40, "KEYBOARD", FIELD_SELECTION, FONT_FIELD_VALUE,
		"KEYBOARD/IAMBIC/STRAIGHT", 0,0,0},
  { "#cwdelay", NULL, 1000, -1000, 50, 50, "CW_DELAY", 40, "300", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 50, 1000, 50},
	{ "#tx_pitch", NULL, 1000, -1000, 50, 50, "TX_PITCH", 40, "600", FIELD_NUMBER, FONT_FIELD_VALUE, 
    "", 300, 3000, 10},
	{ "sidetone", NULL, 1000, -1000, 50, 50, "SIDETONE", 40, "25", FIELD_NUMBER, FONT_FIELD_VALUE, 
    "", 0, 100, 5},
	{"#contact_callsign", NULL, 1000, -1000, 400, 149, "", 70, "NOBODY", FIELD_TEXT, FONT_SMALL, 
		"", 3,10,1},
	{"#sent_exchange", NULL, 1000, -1000, 400, 149, "", 70, "", FIELD_TEXT, FONT_SMALL, 
		"", 0,10,1},
  { "#contest_serial", NULL, 1000, -1000, 50, 50, "CONTEST_SERIAL", 40, "0", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 0,1000000,1},
	{"#current_macro", NULL, 1000, -1000, 400, 149, "MACRO", 70, "", FIELD_TEXT, FONT_SMALL, 
		"", 0,32,1},
  { "#fwdpower", NULL, 1000, -1000, 50, 50, "POWER", 40, "300", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 0,10000,1},
  { "#vswr", NULL, 1000, -1000, 50, 50, "REF", 40, "300", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 0,10000, 1},
  { "bridge", NULL, 1000, -1000, 50, 50, "BRIDGE", 40, "100", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 10,100, 1},

	//FT8 should be 4000 Hz
  {"#bw_voice", NULL, 1000, -1000, 50, 50, "BW_VOICE", 40, "3000", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 0, 3000, 50},
  {"#bw_cw", NULL, 1000, -1000, 50, 50, "BW_CW", 40, "400", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 0, 3000, 50},
  {"#bw_digital", NULL, 1000, -1000, 50, 50, "BW_DIGITAL", 40, "3000", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 0, 3000, 50},

	//FT8 controls
	{"#ft8_auto", NULL, 1000, -1000, 50, 50, "FT8_AUTO", 40, "ON", FIELD_TOGGLE, FONT_FIELD_VALUE, 
		"ON/OFF", 0,0,0},
	{"#ft8_tx1st", NULL, 1000, -1000, 50, 50, "FT8_TX1ST", 40, "ON", FIELD_TOGGLE, FONT_FIELD_VALUE, 
		"ON/OFF", 0,0,0},
  { "#ft8_repeat", NULL, 1000, -1000, 50, 50, "FT8_REPEAT", 40, "5", FIELD_NUMBER, FONT_FIELD_VALUE,
    "", 1, 10, 1},
	

	{"#passkey", NULL, 1000, -1000, 400, 149, "PASSKEY", 70, "123", FIELD_TEXT, FONT_SMALL, 
		"", 0,32,1},
	{"#telneturl", NULL, 1000, -1000, 400, 149, "TELNETURL", 70, "dxc.nc7j.com:7373", FIELD_TEXT, FONT_SMALL, 
		"", 0,32,1},


	/* band stack registers */
	{"#10m", NULL, 400, 330, 50, 50, "10M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#12m", NULL, 450, 330, 50, 50, "12M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#15m", NULL, 400, 380, 50, 50, "15M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#17m", NULL, 450, 380, 50, 50, "17M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#20m", NULL, 500, 380, 50, 50, "20M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#30m", NULL, 400, 430, 50, 50, "30M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#40m", NULL, 450, 430, 50, 50, "40M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},
	{"#80m", NULL, 500, 430, 50, 50, "80M", 1, "", FIELD_BUTTON, FONT_FIELD_VALUE, 
		"", 0,0,0},


	//the last control has empty cmd field
	{"", NULL, 0, 0 ,0, 0, "#", 1, "Q", FIELD_BUTTON, FONT_FIELD_VALUE, "", 0,0,0},
};

struct field *get_field(const char *cmd);
void update_field(struct field *f);
void tx_on();
void tx_off();

//#define MAX_CONSOLE_LINES 1000
//char *console_lines[MAX_CONSOLE_LINES];
int last_log = 0;


struct field *get_field(const char *cmd){
	for (int i = 0; active_layout[i].cmd[0] > 0; i++)
		if (!strcmp(active_layout[i].cmd, cmd))
			return active_layout + i;
	return NULL;
}

struct field *get_field_by_label(char *label){
	for (int i = 0; active_layout[i].cmd[0] > 0; i++)
		if (!strcasecmp(active_layout[i].label, label))
			return active_layout + i;
	return NULL;
}

int get_field_value(char *cmd, char *value){
	struct field *f = get_field(cmd);
	if (!f)
		return -1;
	strcpy(value, f->value);
	return 0;
}

int get_field_value_by_label(char *label, char *value){
	struct field *f = get_field_by_label(label);
	if (!f)
		return -1;
	strcpy(value, f->value);
	return 0;
}

int remote_update_field(int i, char *text){
	struct field * f = active_layout + i;

	if (f->cmd[0] == 0)
		return -1;
	
	//always send status afresh
	if (!strcmp(f->label, "STATUS")){
		//send time
		time_t now = time_sbitx();
		struct tm *tmp = gmtime(&now);
		sprintf(text, "STATUS %04d/%02d/%02d %02d:%02d:%02dZ",  
			tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec); 
		return 1;
	}

	strcpy(text, f->label);
	strcat(text, " ");
	strcat(text, f->value);
	int update = f->update_remote;
	f->update_remote = 0;

	//debug on
//	if (!strcmp(f->cmd, "#text_in") && strlen(f->value))
//		printf("#text_in [%s] %d\n", f->value, update);
	//debug off
	return update;
}

//set the field directly to a particuarl value, programmatically
int set_field(char *id, char *value){
	struct field *f = get_field(id);
	int debug = 0;

	if (!f){
		printf("*Error: field[%s] not found. Check for typo?\n", id);
		return 1;
	}
	
	if (f->value_type == FIELD_NUMBER){
		int	v = atoi(value);
		if (v < f->min)
			v = f->min;
		if (v > f->max)
			v = f->max;
		sprintf(f->value, "%d",  v);
	}
	else if (f->value_type == FIELD_SELECTION || f->value_type == FIELD_TOGGLE){
		// toggle and selection are the same type: toggle has just two values instead of many more
		char *p, *prev, b[100];
		//search the current text in the selection
		prev = NULL;
		strcpy(b, f->selection);
		p = strtok(b, "/");
		if (debug)
			printf("first token [%s]\n", p);
		while(p){
			if (!strcmp(value, p))
				break;
			else
				prev = p;
			p = strtok(NULL, "/");
			if (debug)
				printf("second token [%s]\n", p);
		}	
		//set to the first option
		if (p == NULL){
			if (prev)
				strcpy(f->value, prev);
			printf("*Error: setting field[%s] to [%s] not permitted\n", f->cmd, value);
			return 1;
		}
		else{
			if (debug)
				printf("Setting field to %s\n", value);
			strcpy(f->value, value);
		}
	}
	else if (f->value_type == FIELD_BUTTON){
		strcpy(f->value, value);	
		return 1;
	}
	else if (f->value_type == FIELD_TEXT){
		if (strlen(value) > f->max || strlen(value) < f->min){
			printf("*Error: field[%s] can't be set to [%s], improper size.\n", f->cmd, value);
			return 1;
		}
		else
			strcpy(f->value, value);
	}

	if (!strcmp(id, "#rit") || !strcmp(id, "#ft8_auto"))
		debug = 1; 

	//send a command to the radio 
	char buff[200];
	sprintf(buff, "%s=%s", f->cmd, f->value);
	do_cmd(buff);
	update_field(f);
	return 0;
}


void web_add_string(char *string){
	while (*string){
		q_write(&q_web, *string++);
	}
}

void  web_write(int style, char *data){
	char tag[20];

	switch(style){
		case FONT_FT8_RX:
			strcpy(tag, "WSJTX-RX");
			break;
		case FONT_FLDIGI_RX:
			strcpy(tag, "FLDIGI-RX");
			break;
		case FONT_CW_RX:
			strcpy(tag, "CW-RX");
			break;
		case FONT_FT8_TX:
			strcpy(tag, "WSJTX-TX");
			break;
		case FONT_FT8_QUEUED:
			strcpy(tag, "WSJTX-Q");
			break;
		case FONT_FLDIGI_TX:
			strcpy(tag, "FLDIGI-TX");
			break;
		case FONT_CW_TX:
			strcpy(tag, "CW-TX");
			break;
		case FONT_TELNET:
			strcpy(tag, "TELNET");
			break;
		default:
			strcpy(tag, "LOG");
	}

	web_add_string("<");
	web_add_string(tag);		
	web_add_string(">");
	while (*data){
				switch(*data){
				case '<':
					q_write(&q_web, '&');
					q_write(&q_web, 'l');
					q_write(&q_web, 't');
					q_write(&q_web, ';');
					break;
				case '>':
					q_write(&q_web, '&');
					q_write(&q_web, 'g');
					q_write(&q_web, 't');
					q_write(&q_web, ';');
					break;
			 	case '"':
					q_write(&q_web, '&');
					q_write(&q_web, 'q');
					q_write(&q_web, 'u');
					q_write(&q_web, 't');
					q_write(&q_web, 'e');
					q_write(&q_web, ';');
					break;
				case '\'':
					q_write(&q_web, '&');
					q_write(&q_web, 'a');
					q_write(&q_web, 'p');
					q_write(&q_web, 'o');
					q_write(&q_web, 's');
					q_write(&q_web, ';');
					break;
				case '\n':
					q_write(&q_web, '&');
					q_write(&q_web, '#');
					q_write(&q_web, 'x');
					q_write(&q_web, 'A');
					q_write(&q_web, ';');
					break;	
				default:
					q_write(&q_web, *data);
			}
			data++;
	}			
	web_add_string("</");
	web_add_string(tag);
	web_add_string(">");
}


static int mode_id(char *mode_str){
	if (!strcmp(mode_str, "CW"))
		return MODE_CW;
	else if (!strcmp(mode_str, "CWR"))
		return MODE_CWR;
	else if (!strcmp(mode_str, "USB"))
		return MODE_USB;
	else if (!strcmp(mode_str,  "LSB"))
		return MODE_LSB;
	else if (!strcmp(mode_str,  "FT8"))
		return MODE_FT8;
	else if (!strcmp(mode_str,  "PSK31"))
		return MODE_PSK31;
	else if (!strcmp(mode_str,  "RTTY"))
		return MODE_RTTY;
	else if (!strcmp(mode_str, "NBFM"))
		return MODE_NBFM;
	else if (!strcmp(mode_str, "AM"))
		return MODE_AM;
	else if (!strcmp(mode_str, "2TONE"))
		return MODE_2TONE;
	else if (!strcmp(mode_str, "DIGITAL"))
		return MODE_DIGITAL;
	return -1;
}

static void save_user_settings(int forced){
	static int last_save_at = 0;
	char file_path[200];	//dangerous, find the MAX_PATH and replace 200 with it

	//attempt to save settings only if it has been 30 seconds since the 
	//last time the settings were saved
	int now = millis();
	if ((now < last_save_at + 30000 ||  !settings_updated) && forced == 0)
		return;

	strcpy(file_path, "/etc/sbitx/user_settings.ini");

	FILE *f = fopen(file_path, "w");
	if (!f){
		printf("Unable to save %s : %s\n", file_path, strerror(errno));
		return;
	}

  // save the field values
	int i;
	for (i= 0; active_layout[i].cmd[0] > 0; i++){
		fprintf(f, "%s=%s\n", active_layout[i].cmd, active_layout[i].value);
	}

	//now save the band stack
	for (int i = 0; i < sizeof(band_stack)/sizeof(struct band); i++){
		fprintf(f, "\n[%s]\n", band_stack[i].name);
		//fprintf(f, "power=%d\n", band_stack[i].power);
		for (int j = 0; j < STACK_DEPTH; j++)
			fprintf(f, "freq%d=%d\nmode%d=%d\n", j, band_stack[i].freq[j], j, band_stack[i].mode[j]);
	}


	fclose(f);
	settings_updated = 0;
}


static int user_settings_handler(void* user, const char* section, 
            const char* name, const char* value)
{
    char cmd[1000];
    char new_value[200];

    strcpy(new_value, value);
    if (!strcmp(section, "r1")){
      sprintf(cmd, "%s:%s", section, name);
      set_field(cmd, new_value);
    }
    else if (!strcmp(section, "tx")){
      strcpy(cmd, name);
      set_field(cmd, new_value);
    }
    // if it is an empty section
    else if (strlen(section) == 0){
      sprintf(cmd, "%s", name);
      set_field(cmd, new_value); 
    }

		//band stacks
		int band = -1;
		if (!strcmp(section, "80m"))
			band = 0;
		else if (!strcmp(section, "40m"))
			band = 1;
		else if (!strcmp(section, "30m"))
			band = 2;
		else if (!strcmp(section, "20m"))
			band = 3;
		else if (!strcmp(section, "17m"))
			band = 4;
		else if (!strcmp(section, "15m"))
			band = 5;
		else if (!strcmp(section, "13m"))	
			band = 6;
		else if (!strcmp(section, "10m"))
			band = 7;	

		if (band >= 0  && !strcmp(name, "freq0"))
			band_stack[band].freq[0] = atoi(value);
		else if (band >= 0  && !strcmp(name, "freq1"))
			band_stack[band].freq[1] = atoi(value);
		else if (band >= 0  && !strcmp(name, "freq2"))
			band_stack[band].freq[2] = atoi(value);
		else if (band >= 0  && !strcmp(name, "freq3"))
			band_stack[band].freq[3] = atoi(value);
		else if (band >= 0 && !strcmp(name, "mode0"))
			band_stack[band].mode[0] = atoi(value);	
		else if (band >= 0 && !strcmp(name, "mode1"))
			band_stack[band].mode[1] = atoi(value);	
		else if (band >= 0 && !strcmp(name, "mode2"))
			band_stack[band].mode[2] = atoi(value);	
		else if (band >= 0 && !strcmp(name, "mode3"))
			band_stack[band].mode[3] = atoi(value);	

    return 1;
}
/* rendering of the fields */

void load_user_settings()
{
    char directory[200];	//dangerous, find the MAX_PATH and replace 200 with it
    strcpy(directory, "/etc/sbitx/user_settings.ini");

    if (ini_parse(directory, user_settings_handler, NULL)<0)
    {
        printf("Unable to load %s\n", directory);
        exit(EXIT_SUCCESS);
    }
}


char* freq_with_separators(char* freq_str){

  int freq = atoi(freq_str);
  int f_mhz, f_khz, f_hz;
  char temp_string[11];
  static char return_string[11];

  f_mhz = freq / 1000000;
  f_khz = (freq - (f_mhz*1000000)) / 1000;
  f_hz = freq - (f_mhz*1000000) - (f_khz*1000);

  sprintf(temp_string,"%d",f_mhz);
  strcpy(return_string,temp_string);
  strcat(return_string,".");
  if (f_khz < 100){
    strcat(return_string,"0");
  }
  if (f_khz < 10){
    strcat(return_string,"0");
  }
  sprintf(temp_string,"%d",f_khz);
  strcat(return_string,temp_string);
  strcat(return_string,".");
  if (f_hz < 100){
    strcat(return_string,"0");
  }
  if (f_hz < 10){
    strcat(return_string,"0");
  }
  sprintf(temp_string,"%d",f_hz);
  strcat(return_string,temp_string);
  return return_string;
}

void update_field(struct field *f){
	if (f->y >= 0)
		f->is_dirty = 1;
	f->update_remote = 1;
} 


// respond to a UI request to change the field value
static void edit_field(struct field *f, int action){

	if (f->fn){
		f->is_dirty = 1;
	 	f->update_remote = 1;
		if (f->fn(f, NULL, FIELD_EDIT, action, 0, 0))
			return;
	}
	
	if (f->value_type == FIELD_NUMBER){
		int	v = atoi(f->value);
		if (action == MIN_KEY_UP && v + f->step <= f->max)
			v += f->step;
		else if (action == MIN_KEY_DOWN && v - f->step >= f->min)
			v -= f->step;
		sprintf(f->value, "%d",  v);
	}
	else if (f->value_type == FIELD_SELECTION){
		char *p, *prev, b[100], *first, *last;
    // get the first and last selections
    strcpy(b, f->selection);
    p = strtok(b, "/");
    first = p;
    while(p){
      last = p;
      p = strtok(NULL, "/");
    }
		//search the current text in the selection
		prev = NULL;
		strcpy(b, f->selection);
		p = strtok(b, "/");
		while(p){
			if (!strcmp(p, f->value))
				break;
			else
				prev = p;
			p = strtok(NULL, "/");
		}	
		//set to the first option
		if (p == NULL){
			if (prev)
				strcpy(f->value, prev);
		}
		else if (action == MIN_KEY_DOWN){
			prev = p;
			p = strtok(NULL,"/");
			if (p)
				strcpy(f->value, p);
			else
        strcpy(f->value, first); // roll over
				//return;
				//strcpy(f->value, prev); 
		}
		else if (action == MIN_KEY_UP){
			if (prev)
				strcpy(f->value, prev);
			else
        strcpy(f->value, last); // roll over
				//return;
		}
	}
	else if (f->value_type == FIELD_TOGGLE){
		char *p, b[100];
		strcpy(b, f->selection);
		p = strtok(b, "/");
		while(p){
			if (strcmp(p, f->value))
				break;
			p = strtok(NULL, "/");
		}	
		strcpy(f->value, p);
	}
	else if (f->value_type == FIELD_BUTTON){
		NULL; // ah, do nothing!
	}

	//send a command to the receiver
	char buff[200];
	sprintf(buff, "%s=%s", f->cmd, f->value);
	do_cmd(buff);
	f->is_dirty = 1;
	f->update_remote = 1;
//	update_field(f);
	settings_updated++;
}

time_t time_sbitx(){
	if (time_delta)
		return  (millis()/1000l) + time_delta;
	else
		return time(NULL);
}


// setting the frequency is complicated by having to take care of the
// rit/split and power levels associated with each frequency
void set_operating_freq(int dial_freq, char *response){
    struct field *rit = get_field("#rit");
    struct field *split = get_field("#split");
//     struct field *vfo_a = get_field("#vfo_a_freq");
    struct field *vfo_b = get_field("#vfo_b_freq");
    struct field *rit_delta = get_field("#rit_delta");

    char freq_request[30];

    if (!strcmp(rit->value, "ON")){
        if (!in_tx)
            sprintf(freq_request, "r1:freq=%d", dial_freq + atoi(rit_delta->value));
        else
            sprintf(freq_request, "r1:freq=%d", dial_freq);
    }
    else if (!strcmp(split->value, "ON")){
        if (!in_tx)
            sprintf(freq_request, "r1:freq=%s", vfo_b->value);
        else
            sprintf(freq_request, "r1:freq=%d", dial_freq);
    }
    else
        sprintf(freq_request, "r1:freq=%d", dial_freq);

    //get back to setting the frequency
    sdr_request(freq_request, response);
}

void abort_tx(){
	set_field("#text_in", "");
	tx_off();
}


void update_log_ed(){
	struct field *f = get_field("#log_ed");
	struct field *f_sent_exchange = get_field("#sent_exchange");

	char *log_info = f->label;

	strcpy(log_info, "Log:");
	if (strlen(contact_callsign))
		strcat(log_info, contact_callsign);
	else {
		f->is_dirty = 1;
		return;
	}

	if (strlen(contact_grid)){
		strcat(log_info, " Grid:");
		strcat(log_info, contact_grid);
	}

	strcat(log_info, " Sent:");
	if (strlen(sent_rst)){	
				strcat(log_info, sent_rst);
		if (strlen(f_sent_exchange->value)){
			strcat(log_info, " ");
			strcat(log_info, f_sent_exchange->value);
		}
	}
	else	
		strcat(log_info, "-");


	strcat(log_info, " My:");
	if (strlen(received_rst)){
		strcat(log_info, received_rst);
	if (strlen(received_exchange)){
		strcat(log_info, " ");
		strcat(log_info, received_exchange);
	}
	}
	else
		strcat(log_info, "-");

	f->is_dirty = 1;
}


void tx_on(int trigger){
	char response[100];

	if (trigger != TX_SOFT && trigger != TX_PTT){
		puts("Error: tx_on trigger should be SOFT or PTT");
		return;
	}

    if (is_swr_protect_enabled)
    {
        puts("Error: tx_on trigger with SWR protection on, not turning tx on");
        return;
    }

	struct field *f = get_field("r1:mode");
	if (f){
		if (!strcmp(f->value, "CW"))
			tx_mode = MODE_CW;
		else if (!strcmp(f->value, "CWR"))
			tx_mode = MODE_CWR;
		else if (!strcmp(f->value, "USB"))
			tx_mode = MODE_USB;
		else if (!strcmp(f->value, "LSB"))
			tx_mode = MODE_LSB;
		else if (!strcmp(f->value, "NBFM"))
			tx_mode = MODE_NBFM;
		else if (!strcmp(f->value, "AM"))
			tx_mode = MODE_AM;
		else if (!strcmp(f->value, "2TONE"))
			tx_mode = MODE_2TONE;
		else if (!strcmp(f->value, "DIGITAL"))
			tx_mode = MODE_DIGITAL;
	}

	if (in_tx == 0){
		sdr_request("tx=on", response);	
		in_tx = trigger; //can be PTT or softswitch
		char response[20];
		struct field *freq = get_field("r1:freq");
		set_operating_freq(atoi(freq->value), response);
		update_field(get_field("r1:freq"));
		printf("TX on\n");
	}

	tx_start_time = millis();
}

void tx_off(){
	char response[100];

	if (in_tx){
		sdr_request("tx=off", response);	
		in_tx = 0;
		sdr_request("key=up", response);
		char response[20];
		struct field *freq = get_field("r1:freq");
		set_operating_freq(atoi(freq->value), response);
		update_field(get_field("r1:freq"));
        printf("TX off\n");
	}
	// we dont wanna change sound input here!
	//	sound_input(0); //it is a low overhead call, might as well be sure
}

int static cw_keydown = 0;
int	static cw_hold_until = 0;
int static cw_hold_duration = 150;

static void cw_key(int state){
	char response[100];
	if (state == 1 && cw_keydown == 0){
		sdr_request("key=down", response);
		cw_keydown = 1;
	}
	else if (state == 0 && cw_keydown == 1){
		cw_keydown = 0;
	}
	//printf("cw key = %d\n", cw_keydown);
}


void init_gpio_pins(){
	for (int i = 0; i < 15; i++){
		pinMode(pins[i], INPUT);
		pullUpDnControl(pins[i], PUD_UP);
	}

	pinMode(PTT, INPUT);
	pullUpDnControl(PTT, PUD_UP);
	pinMode(DASH, INPUT);
	pullUpDnControl(DASH, PUD_UP);
}

uint8_t dec2bcd(uint8_t val){
	return ((val/10 * 16) + (val %10));
}

uint8_t bcd2dec(uint8_t val){
	return ((val/16 * 10) + (val %16));
}

void rtc_read(){
	uint8_t rtc_time[10];

	i2cbb_write_i2c_block_data(DS3231_I2C_ADD, 0, 0, NULL);

	int e =  i2cbb_read_i2c_block_data(DS3231_I2C_ADD, 0, 8, rtc_time);
	if (e <= 0){
		printf("RTC not detected\n");
		return;
	}
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
	t.tm_min		= rtc_time[1];
	t.tm_sec		= rtc_time[0];		

	time_t tjulian = mktime(&t);
	
	tzname[0] = tzname[1] = "GMT";
	timezone = 0;
	daylight = 0;
	setenv("TZ", "UTC", 1);	
	gm_now = mktime(&t);

	printf("RTC detected\n");
	time_delta =(long)gm_now -(long)(millis()/1000l);
	printf("time_delta = %ld\n", time_delta);
	printf("rtc julian: %ld %ld\n", tjulian, time(NULL) - tjulian);

}


void rtc_write(int year, int month, int day, int hours, int minutes, int seconds){
	uint8_t rtc_time[10];

	rtc_time[0] = dec2bcd(seconds);
	rtc_time[1] = dec2bcd(minutes);
	rtc_time[2] = dec2bcd(hours);
	rtc_time[3] = 0;
	rtc_time[4] = dec2bcd(day);
	rtc_time[5] = dec2bcd(month);
	rtc_time[6] = dec2bcd(year - 2000);

	for (uint8_t i = 0; i < 7; i++){
  	int e = i2cbb_write_byte_data(DS3231_I2C_ADD, i, rtc_time[i]);
		if (e)
			printf("rtc_write: error writing ds1307 register at %d index\n", i);
	}

/*	int e =  i2cbb_write_i2c_block_data(DS1307_I2C_ADD, 0, 7, rtc_time);
	if (e < 0){
		printf("RTC not written: %d\n", e);
		return;
	}
*/
}

//this will copy the computer time
//to the rtc
void rtc_sync(){
	time_t t = time(NULL);
	struct tm *t_utc = gmtime(&t);

	printf("Checking for valid NTP time ...");
	if (system("ntpstat") != 0){
		printf(".. not found.\n");
		return;
	}
	printf("Syncing RTC to %04d-%02d-%02d %02d:%02d:%02d\n", 
		t_utc->tm_year + 1900,  t_utc->tm_mon + 1, t_utc->tm_mday, 
		t_utc->tm_hour, t_utc->tm_min, t_utc->tm_sec);

	rtc_write( t_utc->tm_year + 1900,  t_utc->tm_mon + 1, t_utc->tm_mday, 
		t_utc->tm_hour, t_utc->tm_min, t_utc->tm_sec);
}

int key_poll(){
	int key = 0;
	
	if (digitalRead(PTT) == LOW)
		key |= CW_DASH;
	if (digitalRead(DASH) == LOW)
		key |= CW_DOT;

	//printf("key %d\n", key);
	return key;
}

void enc_init(struct encoder *e, int speed, int pin_a, int pin_b){
	e->pin_a = pin_a;
	e->pin_b = pin_b;
	e->speed = speed;
	e->history = 5;
}

int enc_state (struct encoder *e) {
	return (digitalRead(e->pin_a) ? 1 : 0) + (digitalRead(e->pin_b) ? 2: 0);
}

int enc_read(struct encoder *e) {
  int result = 0; 
  int newState;
  
  newState = enc_state(e); // Get current state  
    
  if (newState != e->prev_state)
     delay (1);
  
  if (enc_state(e) != newState || newState == e->prev_state)
    return 0; 

  //these transitions point to the encoder being rotated anti-clockwise
  if ((e->prev_state == 0 && newState == 2) || 
    (e->prev_state == 2 && newState == 3) || 
    (e->prev_state == 3 && newState == 1) || 
    (e->prev_state == 1 && newState == 0)){
      e->history--;
      //result = -1;
    }
  //these transitions point to the enccoder being rotated clockwise
  if ((e->prev_state == 0 && newState == 1) || 
    (e->prev_state == 1 && newState == 3) || 
    (e->prev_state == 3 && newState == 2) || 
    (e->prev_state == 2 && newState == 0)){
      e->history++;
    }
  e->prev_state = newState; // Record state for next pulse interpretation
  if (e->history > e->speed){
    result = 1;
    e->history = 0;
  }
  if (e->history < -e->speed){
    result = -1;
    e->history = 0;
  }
  return result;
}

static int tuning_ticks = 0;
void tuning_isr(void){
	int tuning = enc_read(&enc_b);
	if (tuning < 0)
		tuning_ticks++;
	if (tuning > 0)
		tuning_ticks--;	
}

void hw_init(){
	wiringPiSetup();
	init_gpio_pins();

	enc_init(&enc_a, ENC_FAST, ENC1_B, ENC1_A);
	enc_init(&enc_b, ENC_FAST, ENC2_A, ENC2_B);

	g_timeout_add(1, ui_tick, NULL);

	wiringPiISR(ENC2_A, INT_EDGE_BOTH, tuning_isr);
	wiringPiISR(ENC2_B, INT_EDGE_BOTH, tuning_isr);
}

int get_cw_delay(){
	return atoi(get_field("#cwdelay")->value);
}

int get_cw_input_method(){
	struct field *f = get_field("#cwinput");
	if (!strcmp(f->value, "KEYBOARD"))
		return CW_KBD;
	else if (!strcmp(f->value, "IAMBIC"))
		return CW_IAMBIC;
	else
		return CW_STRAIGHT;
}

int get_pitch(){
	struct field *f = get_field("rx_pitch");
	return atoi(f->value);
}

int get_cw_tx_pitch(){
	struct field *f = get_field("#tx_pitch");
	return atoi(f->value);
}

int get_data_delay(){
	return data_delay;
}

int get_wpm(){
	struct field *f = get_field("#tx_wpm");
	return atoi(f->value);
}

long get_freq(){
	return atol(get_field("r1:freq")->value);
}

void set_bandwidth(int hz){
	char buff[10], bw_str[10];
	int low, high;
	struct field *f_mode = get_field("r1:mode");
	struct field *f_pitch = get_field("rx_pitch");

	switch(mode_id(f_mode->value)){
		case MODE_CW:
		case MODE_CWR:
			low = atoi(f_pitch->value) - hz/2;
			high = atoi(f_pitch->value) + hz/2;
			sprintf(bw_str, "%d", high - low);
			set_field("#bw_cw", bw_str);
			break;
		case MODE_LSB:
		case MODE_USB:
			low = 0;
			high = low + hz;
			sprintf(bw_str, "%d", high - low);
			set_field("#bw_voice", bw_str);
			break;
		case MODE_DIGITAL:
			low = atoi(f_pitch->value) - (hz/2);
			high = atoi(f_pitch->value) + (hz/2);
			sprintf(bw_str, "%d", high - low);
			set_field("#bw_digital", bw_str);
			break;
		case MODE_FT8:
			low = 0;
			high = 4000;
			break;
		default:
			low = 0;
			high = 3000;
	}

	if (low < 0)
		low = 0;
	if (high > 24000)
		high = 24000;

	//now set the bandwidth
	sprintf(buff, "%d", low);
	set_field("r1:low", buff);
	sprintf(buff, "%d", high);
	set_field("r1:high", buff);
}

void set_mode(char *mode){
	struct field *f = get_field("r1:mode");
	char umode[10];
	int i;

	for (i = 0; i < sizeof(umode) - 1 && *mode; i++)
		umode[i] = toupper(*mode++);
	umode[i] = 0;

	if(!set_field("r1:mode", umode)){
		int new_bandwidth = 3000;
	
		switch(mode_id(f->value)){
			case MODE_CW:
			case MODE_CWR:
				new_bandwidth = atoi(get_field("#bw_cw")->value);
				break;
			case MODE_USB:
			case MODE_LSB:
				new_bandwidth = atoi(get_field("#bw_voice")->value);
				break;
			case MODE_DIGITAL:
				new_bandwidth = atoi(get_field("#bw_digital")->value);
				break;
			case MODE_FT8:
				new_bandwidth = 4000;
				break;
		}
		set_bandwidth(new_bandwidth);
	}
}

void get_mode(char *mode){
	struct field *f = get_field("r1:mode");
	strcpy(mode, f->value);
}


void bin_dump(int length, uint8_t *data){
	printf("i2c: ");
	for (int i = 0; i < length; i++)
		printf("%x ", data[i]);
	printf("\n");
}

int  web_get_console(char *buff, int max){
	char c;
	int i;

	if (q_length(&q_web) == 0)
		return 0;
	strcpy(buff, "CONSOLE ");
	buff += strlen("CONSOLE ");
	for (i = 0; (c = q_read(&q_web)) && i < max; i++){
		if (c < 128 && c >= ' ')
			*buff++ = c;
	}
	*buff = 0;
	return i;
}

void meter_calibrate(){
	printf("starting meter calibration\n"
	"1. Attach a power meter and a dummy load to the antenna\n"
	"2. Adjust the drive until you see 40 watts on the power meter\n"
	"3. Press the tuning knob to confirm.\n");

	set_mode("CW");
	struct field *f_bridge = get_field("bridge");
	set_field("bridge", "100");
    printf("Setting to mode CW and bridge to 100.\n");
}

void processCATCommand(uint8_t *cmd, uint8_t *response)
{
    uint32_t frequency;
    char command[64];

    memset(response, 0, 5);

    switch(cmd[4]){

    case CMD_PTT_ON: // PTT On
        if (is_swr_protect_enabled)
        {
            response[0] = CMD_ALERT_PROTECTION_ON;
        }
        else if (in_tx == 0)
        {
            sound_input(1);
            tx_on(TX_SOFT);
            response[0] = CMD_RESP_PTT_ON_ACK;
        }
        else
        {
            response[0] = CMD_RESP_PTT_ON_NACK;
        }
        break;

    case CMD_PTT_OFF: // PTT OFF
        if (is_swr_protect_enabled)
        {
            response[0] = CMD_ALERT_PROTECTION_ON;
        }
        else
        if (in_tx != 0)
        {
            // sleep?
            // this will cause collateral damages - please FIX it
            usleep(100000);
            sound_input(0);
            tx_off();
            response[0] = CMD_RESP_PTT_OFF_ACK;
        }
        else
        {
            response[0] = CMD_RESP_PTT_OFF_NACK;
        }
        break;
    case CMD_GET_FREQ: // GET FREQUENCY
        response[0] = CMD_RESP_GET_FREQ_ACK;
        frequency = (uint32_t) get_freq();
        memcpy(response+1, &frequency, 4);
        break;

    case CMD_SET_FREQ: // SET FREQUENCY
        memcpy(&frequency, cmd, 4);

        sprintf(command, "r1:freq=%u", frequency);
        do_cmd(command);

        sprintf(command, "%u", frequency);
        set_field("r1:freq", command);

        response[0] = CMD_RESP_SET_FREQ_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_TXRX_STATUS: // GET TX/RX STATUS
        if (in_tx)
            response[0] = CMD_RESP_GET_TXRX_INTX;
        else
            response[0] = CMD_RESP_GET_TXRX_INRX;
        break;

    case CMD_SET_MODE: // set mode
        if (cmd[0] == 0x00 || cmd[0] == 0x03)
        {
            do_cmd("r1:mode=LSB");
            set_field("r1:mode", "LSB");
        }
        if (cmd[0] == 0x01)
        {
            do_cmd("r1:mode=USB");
            set_field("r1:mode", "USB");
        }
        if (cmd[0] == 0x04)
        {
            do_cmd("r1:mode=CW");
            set_field("r1:mode", "CW");
        }

        response[0] = CMD_RESP_SET_MODE_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_MODE: // GET SSB MODE
        if (rx_list->mode == MODE_USB)
            response[0] = CMD_RESP_GET_MODE_USB;
        if (rx_list->mode == MODE_LSB)
            response[0] = CMD_RESP_GET_MODE_LSB;
        if (rx_list->mode == MODE_CW)
            response[0] = CMD_RESP_GET_MODE_CW;
        break;

    case CMD_RESET_PROTECTION: // RESET PROTECTION
        response[0] = CMD_RESP_RESET_PROTECTION_ACK;
        is_swr_protect_enabled = false;
        break;


    case CMD_GET_PROTECTION_STATUS: // GET PROTECTION STATUS
        if (is_swr_protect_enabled)
            response[0] = CMD_RESP_GET_PROTECTION_ON;
        else
            response[0] = CMD_RESP_GET_PROTECTION_OFF;
        break;

    case CMD_GET_MASTERCAL: // GET MASTER CAL
        response[0] = CMD_RESP_GET_MASTERCAL_ACK;
        memcpy(response+1, &frequency_offset, 4);
        break;

    case CMD_SET_MASTERCAL: // SET MASTER CAL
        memcpy(&frequency_offset, cmd, 4);
        response[0] = CMD_RESP_SET_MASTERCAL_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_BFO: // GET BFO
        response[0] = CMD_RESP_GET_BFO_ACK;
        memcpy(response+1, &bfo_freq, 4);
        break;

    case CMD_SET_BFO: // SET BFO
        memcpy(&bfo_freq, cmd, 4);
        response[0] = CMD_RESP_SET_BFO_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_FWD: // GET FWD
        response[0] = CMD_RESP_GET_FWD_ACK;
        memcpy(response+1, &fwdpower, 2);
        break;

    case CMD_GET_REF: // GET REF
        response[0] = CMD_RESP_GET_REF_ACK;
        memcpy(response+1, &vswr, 2);
        break;

    case CMD_GET_LED_STATUS: // GET LED STATUS
        if (led_status)
            response[0] = CMD_RESP_GET_LED_STATUS_ON;
        else
            response[0] = CMD_RESP_GET_LED_STATUS_OFF;
        break;

    case CMD_SET_LED_STATUS: // SET LED STATUS
        led_status = cmd[0];
        response[0] = CMD_RESP_SET_LED_STATUS_ACK;
        break;

    case CMD_GET_CONNECTED_STATUS: // GET CONNECTED STATUS
        if (connected_status)
            response[0] = CMD_RESP_GET_CONNECTED_STATUS_ON;
        else
            response[0] = CMD_RESP_GET_CONNECTED_STATUS_OFF;
      break;

    case CMD_SET_CONNECTED_STATUS: // SET CONNECTED STATUS
        connected_status = cmd[0];
        response[0] = CMD_RESP_SET_CONNECTED_STATUS_ACK;
        break;

    case CMD_GET_SERIAL: // GET SERIAL NUMBER
        response[0] = CMD_RESP_GET_SERIAL_ACK;
        memcpy(response+1, &serial_number, 4);
      break;

    case CMD_SET_SERIAL: // SET SERIAL NUMBER
        memcpy(&serial_number, cmd, 4);
        response[0] = CMD_RESP_SET_SERIAL_ACK;
        save_user_settings(1);
        break;

    case CMD_SET_REF_THRESHOLD: // SET REF THRESHOLD
        memcpy(&reflected_threshold, cmd, 2);
        response[0] = CMD_RESP_SET_REF_THRESHOLD_ACK;
        save_user_settings(1);
        break;

    case CMD_GET_REF_THRESHOLD: // GET REF THRESHOLD
        response[0] = CMD_RESP_GET_REF_THRESHOLD_ACK;
        memcpy(response+1, &reflected_threshold, 2);
        break;

    case CMD_GPS_CALIBRATE: // CMD_GPS_CALIBRATE
        response[0] = CMD_RESP_GPS_NOT_PRESENT;
        break;

    case CMD_GET_STATUS: // CMD_GET_STATUS
        response[0] = CMD_RESP_GET_STATUS_ACK;
        memcpy(response+1, &radio_operation_result, 4);
        break;

    case CMD_SET_RADIO_DEFAULTS: // SET RADIO DEFAULTS
        save_user_settings(1);
        save_hw_settings();
        response[0] = CMD_RESP_SET_RADIO_DEFAULTS_ACK;
        break;

    case CMD_RESTORE_RADIO_DEFAULTS: // RESTORE RADIO DEFAULTS
        load_user_settings();
        read_hw_ini();
        response[0] = CMD_RESP_RESTORE_RADIO_DEFAULTS_ACK;
        break;
    case CMD_RADIO_RESET: // RADIO RESET
        exit(-1);
        break;
    default:
        response[0] = CMD_RESP_WRONG_COMMAND;
    }

}


gboolean ui_tick(gpointer gook){
	int static ticks = 0;
    uint8_t srv_cmd[5];
    uint8_t response[5];

	ticks++;

    if (tmp_connector->command_available == 1)
    {

        processCATCommand(tmp_connector->service_command, tmp_connector->response_service);

        tmp_connector->command_available = 0;
    }

	// check the tuning knob
	struct field *f = get_field("r1:freq");

	if (abs(tuning_ticks) > 5)
		tuning_ticks *= 4;
	while (tuning_ticks > 0){
        // TODO: freq down
		tuning_ticks--;
    //sprintf(message, "tune-\r\n");
    //write_console(FONT_LOG, message);

	}
	while (tuning_ticks < 0){
        // TODO: freq up
		tuning_ticks++;
    //sprintf(message, "tune+\r\n");
    //write_console(FONT_LOG, message);
	}

#if 1
	if (!(ticks % 30))
    {
        if(in_tx)
        {
            char buff[10];
            read_power();
			sprintf(buff,"%d", fwdpower);
			set_field("#fwdpower", buff);		
			sprintf(buff, "%d", vswr);
			set_field("#vswr", buff);
		}
    }
	if (!(ticks % 100))
    {
		if (digitalRead(ENC1_SW) == 0)
            printf("Button pressed?\n");
            //focus_field(get_field("r1:volume"));
    }
#endif

 
	f = get_field("r1:mode");
	//straight key in CW
	if (f && (!strcmp(f->value, "2TONE") || !strcmp(f->value, "LSB") || 
	!strcmp(f->value, "USB"))){
		if (digitalRead(PTT) == LOW && in_tx == 0)
			tx_on(TX_PTT);
		else if (digitalRead(PTT) == HIGH && in_tx  == TX_PTT)
			tx_off();
	}
#if 0
	int scroll = enc_read(&enc_a);
	if (scroll){
		if (scroll < 0)
            // TODO: volume down?
		else
            // TODO: volume up?
	}
#endif

	return TRUE;
}

void ui_init(int argc, char *argv[]){
 
    q_init(&q_web, 1000);

    webserver_start();
}

/* handle modem callbacks for more data */


int get_tx_data_byte(char *c){
	//take out the first byte and return it to the modem
	struct field *f = get_field("#text_in");
	int length = strlen(f->value);

	if (f->value[0] == '\\' || !length)
		return 0;
	if (length){
		*c = f->value[0];
		//now shift the buffer down, hopefully, this copies the trailing null too
		for (int i = 0; i < length; i++)
			f->value[i] = f->value[i+1];
	}
	f->is_dirty = 1;
	f->update_remote = 1;
	//update_field(f);
	return length;
}

int get_tx_data_length(){
	struct field *f = get_field("#text_in");

	if (strlen(f->value) == 0)
		return 0;

	if (f->value[0] != COMMAND_ESCAPE)
		return strlen(get_field("#text_in")->value);
	else
		return 0;
}

int is_in_tx(){
	return in_tx;
}

/* handle the ui request and update the controls */

void change_band(char *request){
	int i, old_band, new_band; 
	int max_bands = sizeof(band_stack)/sizeof(struct band);
	long new_freq, old_freq;
	char buff[100];

	//find the band that has just been selected, the first char is #, we skip it
	for (new_band = 0; new_band < max_bands; new_band++)
		if (!strcmp(request, band_stack[new_band].name))
			break;

	//continue if the band is legit
	if (new_band == max_bands)
		return;

	// find out the tuned frequency
	struct field *f = get_field("r1:freq");
	old_freq = atol(f->value);
	f = get_field("r1:mode");
	int old_mode = mode_id(f->value);
	if (old_mode == -1)
		return;

	//first, store this frequency in the appropriate bin
	for (old_band = 0; old_band < max_bands; old_band++)
		if (band_stack[old_band].start <= old_freq && old_freq <= band_stack[old_band].stop)
				break;

	int stack = band_stack[old_band].index;
	if (stack < 0 || stack >= STACK_DEPTH)
		stack = 0;
	if (old_band < max_bands){
		//update the old band setting 
		if (stack >= 0 && stack < STACK_DEPTH){
				band_stack[old_band].freq[stack] = old_freq;
				band_stack[old_band].mode[stack] = old_mode;
		}
	}

	//if we are still in the same band, move to the next position
	if (new_band == old_band){
		stack = ++band_stack[new_band].index;
		//move the stack and wrap the band around
		if (stack >= STACK_DEPTH)
			stack = 0;
		band_stack[new_band].index = stack;
	}
	stack = band_stack[new_band].index;
	sprintf(buff, "%d", band_stack[new_band].freq[stack]);
	char resp[100];
	set_operating_freq(band_stack[new_band].freq[stack], resp);
	set_field("r1:freq", buff);
	set_mode(mode_name[band_stack[new_band].mode[stack]]);	
	//set_field("r1:mode", mode_name[band_stack[new_band].mode[stack]]);	

  // this fixes bug with filter settings not being applied after a band change, not sure why it's a bug - k3ng 2022-09-03
  set_field("r1:low",get_field("r1:low")->value);
  set_field("r1:high",get_field("r1:high")->value);

	abort_tx();
}

void utc_set(char *args, int update_rtc){
	int n[7], i;
	char *p, *q;
	struct tm t;
	time_t gm_now;

	i = 0;
	p =  strtok(args, "-/;: ");
	if (p){
		n[0] = atoi(p);
		for (i = 1; i < 7; i++){
			p = strtok(NULL, "-/;: ");
			if (!p)
				break;
			n[i] = atoi(p);
		}
	}	

	if (i != 6 ){
			return;
	}

	rtc_write(n[0], n[1], n[2], n[3], n[4], n[5]);

	if (n[0] < 2000)
		n[0] += 2000;
	t.tm_year = n[0] - 1900;
	t.tm_mon = n[1] - 1;
	t.tm_mday = n[2]; 
	t.tm_hour = n[3];
	t.tm_min = n[4];
	t.tm_sec = n[5];

	tzname[0] = tzname[1] = "GMT";
	timezone = 0;
	daylight = 0;
	setenv("TZ", "UTC", 1);	
	gm_now = mktime(&t);

    time_delta =(long)gm_now -(long)(millis()/1000l);
	printf("time_delta = %ld\n", time_delta);
}


void do_cmd(char *cmd){	
	char request[1000], response[1000], buff[100];
	
	strcpy(request, cmd);			//don't mangle the original, thank you

    if (!strcmp(request, "#off")){
		tx_off();
		set_field("#record", "OFF");
		save_user_settings(1);
		exit(0);
	}
	else if (!strcmp(request, "#tx")){	
		tx_on(TX_SOFT);
	}
	else if (!strcmp(request, "#rx")){
		tx_off();
	}
	else if (!strncmp(request, "#rit", 4))
		update_field(get_field("r1:freq"));
	else if (!strncmp(request, "#split", 5)){
		update_field(get_field("r1:freq"));	
		if (!strcmp(get_field("#vfo")->value, "B"))
			set_field("#vfo", "A");
	}
	else if (!strcmp(request, "#vfo=B")){
		struct field *f = get_field("r1:freq");
		struct field *vfo = get_field("#vfo");
		struct field *vfo_a = get_field("#vfo_a_freq");
		struct field *vfo_b = get_field("#vfo_b_freq");
		if (!strcmp(vfo->value, "B")){
			//vfo_a_freq = atoi(f->value);
			strcpy(vfo_a->value, f->value);
			//sprintf(buff, "%d", vfo_b_freq);
			set_field("r1:freq", vfo_b->value);
			settings_updated++;
		}
	}
	else if (!strcmp(request, "#vfo=A")){
		struct field *f = get_field("r1:freq");
		struct field *vfo = get_field("#vfo");
		struct field *vfo_a = get_field("#vfo_a_freq");
		struct field *vfo_b = get_field("#vfo_b_freq");
		//printf("vfo old %s, new %s\n", vfo->value, request);
		if (!strcmp(vfo->value, "A")){
		//	vfo_b_freq = atoi(f->value);
			strcpy(vfo_b->value, f->value);
	//		sprintf(buff, "%d", vfo_a_freq);
			set_field("r1:freq", vfo_a->value);
			settings_updated++;
		}
	}
	//tuning step
  else if (!strcmp(request, "#step=1M"))
    tuning_step = 1000000;
	else if (!strcmp(request, "#step=100K"))
		tuning_step = 100000;
	else if (!strcmp(request, "#step=10K"))
		tuning_step = 10000;
	else if (!strcmp(request, "#step=1K"))
		tuning_step = 1000;
	else if (!strcmp(request, "#step=100H"))
		tuning_step = 100;
	else if (!strcmp(request, "#step=10H"))
		tuning_step = 10;

	//handle the band stacking
	else if (!strcmp(request, "#80m") || 
		!strcmp(request, "#40m") || 
		!strcmp(request, "#30m") || 
		!strcmp(request, "#20m") || 
		!strcmp(request, "#17m") || 
		!strcmp(request, "#15m") || 
		!strcmp(request, "#12m") || 
		!strcmp(request, "#10m")){
		change_band(request+1); //skip the hash		
	}
	else if (!strcmp(request, "#record=ON")){
		char fullpath[200];	//dangerous, find the MAX_PATH and replace 200 with it

		time(&record_start);
		struct tm *tmp = localtime(&record_start);
		sprintf(fullpath, "/etc/sbitx/audio/%04d%02d%02d-%02d%02d-%02d.wav",
			tmp->tm_year + 1900, tmp->tm_mon + 1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec); 

		char request[300], response[100];
		sprintf(request, "record=%s", fullpath);
		sdr_request(request, response);
	}
	else if (!strcmp(request, "#record=OFF")){
		sdr_request("record", "off");
		record_start = 0;
	}

	//this needs to directly pass on to the sdr core
	else if(request[0] != '#'){
		//translate the frequency of operating depending upon rit, split, etc.
		if (!strncmp(request, "r1:freq", 7))
			set_operating_freq(atoi(request+8), response);
		else
			sdr_request(request, response);
	}
}

void cmd_exec(char *cmd){
	int i, j;
	int mode = mode_id(get_field("r1:mode")->value);

	char args[MAX_FIELD_LENGTH];
	char exec[20];

  args[0] = 0;

	//copy the exec
	for (i = 0; *cmd > ' ' && i < sizeof(exec) - 1; i++)
		exec[i] = *cmd++;
	exec[i] = 0; 

	//skip the spaces
	while(*cmd == ' ')
		cmd++;

	j = 0;
	for (i = 0; *cmd && i < sizeof(args) - 1; i++){
		if (*cmd > ' ')
				j = i;
		args[i] = *cmd++;
	}
	args[++j] = 0;

	char response[100];
	if (!strcmp(exec, "callsign")){
		strcpy(get_field("#mycallsign")->value,args); 
		sprintf(response, "\n[Your callsign is set to %s]\n", get_field("#mycallsign")->value);
	}
	else if (!strcmp(exec, "metercal")){
		meter_calibrate();
	}
	else if (!strcmp(exec, "abort"))
		abort_tx();
	else if (!strcmp(exec, "rtc"))
		rtc_read();
	else if (!strcmp(exec, "txcal")){
		char response[10];
		sdr_request("txcal=", response);
	}
	else if (!strcmp(exec, "grid")){	
		set_field("#mygrid", args);
		sprintf(response, "\n[Your grid is set to %s]\n", get_field("#mygrid")->value);
	}
	else if (!strcmp(exec, "utc")){
		utc_set(args, 1);
	}
	else if (!strcmp(exec, "exchange")){
		set_field("#contest_serial", "0");
		set_field("#sent_exchange", "");

		if (strlen(args)){
			set_field("#sent_exchange", args);
			if (atoi(args))
				set_field("#contest_serial", args);
		}

	}
	else if(!strcmp(exec, "freq") || !strcmp(exec, "f")){
		long freq = atol(args);
		if (freq < 30000)
			freq *= 1000;

		if (freq > 0){
			char freq_s[20];
			sprintf(freq_s, "%ld",freq);
			set_field("r1:freq", freq_s);
		}
	}
  else if (!strcmp(exec, "exit")){
    tx_off();
    set_field("#record", "OFF");
    save_user_settings(1);
    exit(0);
  }
  else if ((!strcmp(exec, "lsb")) || (!strcmp(exec, "LSB"))){
    set_mode("LSB");
  }
  else if ((!strcmp(exec, "usb")) || (!strcmp(exec, "USB"))){
    set_mode("USB"); 
  }
  else if ((!strcmp(exec, "cw")) || (!strcmp(exec, "CW"))){
    set_mode("CW"); 
  }
  else if ((!strcmp(exec, "cwr")) || (!strcmp(exec, "CWR"))){
    set_mode("CWR");
  }
  else if ((!strcmp(exec, "rtty")) || (!strcmp(exec, "RTTY"))){
    set_mode("RTTY");
  }
  else if ((!strcmp(exec, "ft8")) || (!strcmp(exec, "FT8"))){
    set_mode("FT8");
  }
  else if ((!strcmp(exec, "psk")) || (!strcmp(exec, "psk31")) || (!strcmp(exec, "PSK")) || (!strcmp(exec, "PSK31"))){
    set_mode("PSK31");
  }
  else if ((!strcmp(exec, "digital")) || (!strcmp(exec, "DIGITAL"))  || (!strcmp(exec, "dig")) || (!strcmp(exec, "DIG"))){
    set_mode("DIGITAL");
  }
  else if ((!strcmp(exec, "2tone")) || (!strcmp(exec, "2TONE"))){
    set_mode("2TONE");
  }
  else if (!strcmp(exec, "band"))
      change_band(args);
  else if (!strcmp(exec, "bandwidth") || !strcmp(exec, "bw")){
      set_bandwidth(atoi(args));
  }
  else if (!strcmp(exec, "mode") || !strcmp(exec, "m") || !strcmp(exec, "MODE"))
      set_mode(args);
  else if (!strcmp(exec, "t"))
      tx_on(TX_SOFT);
  else if (!strcmp(exec, "r"))
      tx_off();
  else if (!strcmp(exec, "txpitch")){
      if (strlen(args)){
          int t = atoi(args);
          if (t > 100 && t < 4000)
              set_field("#tx_pitch", args);
          else
              printf("cw pitch should be 100-4000");
      }
      char buff[100];
      sprintf(buff, "txpitch is set to %d Hz\n", get_cw_tx_pitch());
  }
  else if (!strcmp(exec, "PITCH")){
      struct field *f = get_field_by_label(exec);
      if (f){
          set_field(f->cmd, args);
          int mode = mode_id(get_field("r1:mode")->value);
          int bw = 4000;
          switch(mode){
          case MODE_CW:
          case MODE_CWR:
              bw = atoi(get_field("#bw_cw")->value);
              set_bandwidth(bw);
              break;
          case MODE_DIGITAL:
              bw = atoi(get_field("#bw_digital")->value);
              set_bandwidth(bw);
              break;
          }
      }
  }
	save_user_settings(0);
}

GMainLoop *loop_g;

int main( int argc, char* argv[] ) {

	puts(VER_STR);
    active_layout = main_controls;

    loop_g = g_main_loop_new(NULL, 0);

	ui_init(argc, argv);
	hw_init();

	setup();

    // TODO split RTC code
	rtc_sync();

	//initialize the modulation display

	//set the radio to some decent defaults
	do_cmd("r1:freq=7100000");
	do_cmd("r1:mode=LSB");	
	do_cmd("#step=1000");	
	strcpy(vfo_a_mode, "USB");
	strcpy(vfo_b_mode, "LSB");

    // this is a constant multiplier to the tx level
	set_volume(20000000);

    load_user_settings();

	char buff[1000];

	//now set the frequency of operation and more to vfo_a
    //sprintf(buff, "%d", vfo_a_freq);
    set_field("r1:freq", get_field("#vfo_a_freq")->value);

	settings_updated = 0;

	printf("Reading rtc...");
	rtc_read();
	printf("done!\n");

    sbitx_controller();
    g_main_loop_run (loop_g);

  return 0;
}


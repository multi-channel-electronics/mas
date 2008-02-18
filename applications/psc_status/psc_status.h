/* psc_status.h
 * based on information from psu_monitor_gain.xls by Stan Knotek and
 * scuba2ps.h from psuc firmware.
 * Author: mandana@phas.ubc.ca
 *
*/
#include<stdio.h>
#include<float.h>

/********** PSU Data Block Settings  ***************/
// PSU Data Block POINTERS - defining this way prevents pointers from being reassigned dynamically
#define PSC_DATA_BLK_SIZE 72 

static unsigned char psc_data_blk[PSC_DATA_BLK_SIZE] = "";

#define SILICON_ID          0      // Read from DS18S20 LS 32 bits of 48
#define SOFTWARE_VERSION    4*2      // Software Version
#define FAN1_TACH           5*2      // Fan 1 speed /16   
#define FAN2_TACH           6*2      // Fan 2 speed /16
#define PSU_TEMP_1          7*2      // temperature 1 from DS18S20
#define PSU_TEMP_2          8*2      // temperature 2 from DS18S20
#define PSU_TEMP_3          9*2      // temperature 3 from DS18S20
#define ADC_OFFSET         10*2      // Grounded ADC input channel reading
#define V_VCORE            12*2      // +Vcore supply scaled 0 to +2V
#define V_VLVD             14*2      // +Vlvd supply scaled 0 to +2V
#define V_VAH              16*2      // +Vah supply scaled 0 to +2V
#define V_VA_PLUS          18*2      // +Va supply scaled 0 to +2V
#define V_VA_MINUS         20*2      // -Va supply scaled 0 to +2V
#define I_VCORE            22*2      // Current +Vcore supply scaled
#define I_VLVD             24*2      // Current +Vlvd supply scaled
#define I_VAH              26*2      // Current +Vah supply scaled
#define I_VA_PLUS          28*2      // Current +Va supply scaled
#define I_VA_MINUS         30*2      // Current -Va supply scaled
#define STATUS_WORD        32*2      // undefined place for status word
#define ACK_NAK            34*2      // either ACK or NAK
#define CHECK_BYTE         35*2      // checksum byte

static char *psc_data_blk_titles[]=
{"FAN1_TACH",
 "FAN2_TACH",
 "TEMP1 (PSUC)",
 "TEMP2 (PSU)",
 "TEMP3 (HS)",
 "ADC_OFFSET",
 "V_VCORE",
 "V_VLVD",
 "V_VAH",
 "V_VA+",
 "V_VA-",
 "I_VCORE",
 "I_VLVD",
 "I_VAH",
 "I_VA+",
 "I_VA-"};

static float psc_conversion[]=
{0.0, 
4.101,
6.161,
13.722,
8.488,
8.512,
21.312,
6.5476,
0.2461,
24.622,
3.2738};

static float psc_nominal[]=
{0.0,
3.0,
4.5,
10.0,
6.2,
6.2,
13.0,
4.00,
0.15,
15.0,
2.00};


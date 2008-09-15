/****************************************************************************
 * psc_status.c : utility to parse the reply to 'rb psc psc_status' command    
 *                as conformed by rev. 2.3 of psuc firmware and
 *                psu_monitor_gain.xls.
 * Author       : mandana@phas.ubc.ca
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <mce_library.h>
#include "options.h"
#include "psc_status.h"

#define MASCONFIG_FILE "/etc/mce/mas.cfg"
#define CMD_DEVICE "/dev/mce_cmd0"
#define DATA_DEVICE "/dev/mce_data0"
#define HARDWARE_FILE "/etc/mce/mce.cfg"

int main (int argc, char **argv){
   int index;
   int width;
   char temp[10];
   long int templ;
   float tempf;
   u32 data[20]; 
   char card_id[9];
 
   long int i;

   /* Define default MAS options */
   option_t options = {
     config_file:   MASCONFIG_FILE,
     cmd_device:    CMD_DEVICE,
     data_device:   DATA_DEVICE,
     hardware_file: HARDWARE_FILE,
     read_stdin:    0,
   };
   
   if (process_options(&options, argc, argv) < 0) {
     exit(1);
   }

   if (!options.read_stdin) {
     // Create MCE context 
     mce_context_t *mce = mcelib_create();
     
     // Load MCE hardware inforamtion
     if (mceconfig_open(mce, options.hardware_file, NULL) != 0) {
       fprintf(stderr, "Failed to open %s as hardware configuration file.", options.config_file);
       return 2;
     }
     // connect to an mce_cmd device 
     if (mcecmd_open(mce, options.cmd_device)) {
       fprintf(stderr, "Failed to open %s as command device.", options.cmd_device);
       return 3;     
     }
     
     // Lookup MCE parameters, or exit with error message
     mce_param_t m_psc_status;
     if (mcecmd_load_param (mce, &m_psc_status, "psc","psc_status") ) {   
       fprintf(stderr, "load param psc and psc_status failed.\n");
       return 1;
     }
     
     if ( mcecmd_read_block(mce, &m_psc_status, (PSC_DATA_BLK_SIZE)/8, data) ) {
       fprintf(stderr, "mce_cmd_read_block psc psc_status failed.\n");
       return 1;
     }     
   } else { //read_stdin
     printf("Reading ascii from stdin: ");
     for (index=0; index< PSC_DATA_BLK_SIZE / 8; index++) {
       scanf("%u", data + index);
     }
   }

   // print the value read back
   for (index=0 ; index< PSC_DATA_BLK_SIZE/8; index++){
   //    printf ("%d- data is %u %8.8x \n", index, data[index], data[index]);
       sprintf(psc_data_blk+(index*8), "%8.8x",data[index]);
   }
   //printf ("\n");
   
   // now parse the psc_status_block   
   strncpy (card_id, psc_data_blk+SILICON_ID, 8);    
   
   card_id[8] = '\0';
   printf ("card_id\t\t%s\n", card_id);
   printf("PSUC firmware version\t %c.%c\n", psc_data_blk[SOFTWARE_VERSION], psc_data_blk[SOFTWARE_VERSION+1]);

   index = FAN1_TACH;
   width = 2;
   for (i = 0; i<16; i++){
      if (i != 0 && i!=1 && i!=5){ //ignore fan speeds and adc_offset readings
         strncpy(temp, psc_data_blk+index, width);
         temp[width] = '\0'; 
         templ=strtol(temp, NULL, 16);
         printf ("%s\t\t%ld", psc_data_blk_titles[i], templ);
         if (i<5) 
            printf (" C");
         if (i> 5){
            tempf = (float)templ*(psc_conversion[i-5]/4096.0);
            printf(" units \t(%5.2f%c)\tNominal %5.2f", tempf, i<11?'V':'A',psc_nominal[i-5]);
         }
         printf("\n"); 
       }
       if (i> 4) width = 4;
       index = index + width;
       /*printf ("\t\t width is %d %d \n",width, index);*/
   }

   return 0;
}

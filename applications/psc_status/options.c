#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "options.h"

/*
  This is for MAS configuration file specification, mostly.  Since
  other arguments are processed by the main routines, the function
  must return the offset of the first unprocessed argument.
*/

#define USAGE_MESSAGE "" \
"  Initial options (MAS config):\n" \
"        -c <cmd device>         override default command device\n"\
"        -d <data device>        override default data device\n"\
"        -w <hardware file>      override default hardware configuration file\n"\
"        -m <MAS config file>    override default MAS configuration file\n"\
""

int process_options(option_t *options, int argc, char **argv)
{
  int option;
  // Note the "+" at the beginning of the options string forces the
  // first non-option string to return a -1, which is what we want.
  while ( (option = getopt(argc, argv, "+?hm:c:d:w:0123456789")) >=0) {

    switch(option) {
    case '?':
    case 'h':
      printf(USAGE_MESSAGE);
      return -1;

    case 'm':
      strcpy(options->config_file, optarg);
      break;

    case 'c':
      strcpy(options->cmd_device, optarg);
      break;

    case 'd':
      strcpy(options->data_device, optarg);
      break;

    case 'w':
      strcpy(options->hardware_file, optarg);
      break;

    case '0' ... '9':
      //It's a number! Get out of here!
      return optind-1;

    default:
      printf("Unimplemented option '-%c'!\n", option);
   }
  }
  return optind;
}

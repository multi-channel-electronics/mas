#ifndef _MASDATA_H_
#define _MASDATA_H_

#include <libmaslog.h>

#include "main.h"
#include "data.h"
#include "server.h"
#include "files.h"
#include "frames.h"


//typedef
struct params_struct {

	server_t server;
	datafile_t datafile;
	mce_comms_t mce_comms;
	data_thread_t data_data;
	frame_setup_t frame_setup;
	logger_t logger;

	int pflags;
#define PFLAG_QUIT  0x0001

};// params_t;


#endif
